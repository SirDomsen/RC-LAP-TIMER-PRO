#include "web_server.h"
#include "web_handlers.h"
#include "lap_logic.h"
#include "storage.h"
#include "esp_now_base.h"
#include <status_leds.h>
#include <platform_runtime.h>
#include <network_platform.h>

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#if defined(ESP8266)
#include <Updater.h>
#elif defined(ESP32)
#include <Update.h>
#endif

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static IPAddress currentActiveIP(192, 168, 0, 8);

extern uint32_t perfMaxLoopUs;
extern uint32_t perfAvgLoopUs;

uint8_t currentRuntimeWifiMode = 0;

static bool pendingReset = false;
static uint32_t resetTimer = 0;

static bool pendingWifiSwitch = false;
static uint32_t wifiSwitchTimer = 0;
static uint8_t targetWifiMode = 0;

// --- GHOST-WLAN AUTO-OFF LOGIK ---
static bool ghostWlanActive = false;
static uint32_t ghostWlanOffTimer = 0;

// --- DYNAMISCHE HEAP-SCHUTZMAUER ---


static const char BUSY_HTML_PROGMEM[] PROGMEM = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<meta http-equiv='refresh' content='4'>"
  "<title>RC Lap Timer - Boxengasse voll</title>"
  "<style>body{background:#0a0c10;color:#00e5ff;font-family:sans-serif;text-align:center;padding:40px 20px;}"
  ".box{border:1px solid #00e5ff;background:#161b22;padding:20px;border-radius:10px;max-width:400px;margin:0 auto;box-shadow:0 0 15px rgba(0,229,255,0.3);}"
  "</style></head><body><div class='box'><h3>&#x23F3; Boxengasse belegt</h3>"
  "<p style='color:#f0f6fc;font-size:14px;'>Alle Pl&auml;tze in der Boxengasse sind aktuell belegt. Diese Seite l&auml;dt automatisch neu, sobald ein Platz frei wird...</p></div></body></html>";

bool isHeapCriticalForFileTransfer() {
  return PlatformRuntime::isHeapCriticalForFileTransfer(currentRuntimeWifiMode);
}

bool isHeapCriticalForLiveData() {
  return PlatformRuntime::isHeapCriticalForLiveData();
}

static const char* getContentType(const String& path) {
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".css"))  return "text/css; charset=utf-8";
  if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
  if (path.endsWith(".json")) return "application/json; charset=utf-8";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".webp")) return "image/webp";
  return "text/plain; charset=utf-8";
}

static String escapeJsonStr(const String& s) {
  String out = "";
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') continue;
    else out += c;
  }
  return out;
}

static void triggerGhostWlanEarlyShutdown() {
  if (ghostWlanActive && ghostWlanOffTimer == 0) {
    ghostWlanOffTimer = millis() + 15000UL; 
  }
}

static uint8_t activeStaticTransfers = 0;
static uint32_t lastStaticTransferMs = 0;

static bool serveStaticWithHeapGuard(AsyncWebServerRequest *request, String path) {
  if (path == "/" || path.length() == 0) path = "/index.html";
  if (path == "/admin" || path == "/admin/") path = "/admin.html";

  if (path == "/index.html" || path == "/admin.html") {
    triggerGhostWlanEarlyShutdown();
  }

  // Sicherheits-Reset, falls ein Disconnect-Callback im TCP-Stack verloren geht
  if (activeStaticTransfers > 0 && (millis() - lastStaticTransferMs > 10000UL)) {
    activeStaticTransfers = 0;
  }

  const uint8_t maxStaticSlots = PLATFORM_MAX_STATIC_SLOTS;

  bool isAsset = path.endsWith(".css") || path.endsWith(".js");
  bool heapCrit = isHeapCriticalForFileTransfer();
  bool slotsFull = (activeStaticTransfers >= maxStaticSlots) || (heapCrit && isAsset && activeStaticTransfers >= 1);

  if (slotsFull || (heapCrit && !isAsset)) {
    if (path.endsWith(".html")) {
      AsyncWebServerResponse *res = request->beginResponse(503, "text/html", BUSY_HTML_PROGMEM);
      res->addHeader("Retry-After", "3");
      res->addHeader("Connection", "close");
      request->send(res);
      return true;
    } else {
      AsyncWebServerResponse *res = request->beginResponse(503, "text/plain", "Busy");
      res->addHeader("Retry-After", isAsset ? "1" : "2");
      res->addHeader("Connection", "close");
      request->send(res);
      return true;
    }
  }

  String gzPath = path + ".gz";
  bool hasGz = LittleFS.exists(gzPath);
  bool hasRaw = !hasGz && LittleFS.exists(path);

  if (!hasGz && !hasRaw) return false;

  activeStaticTransfers++;
  lastStaticTransferMs = millis();
  request->onDisconnect([]() {
    if (activeStaticTransfers > 0) activeStaticTransfers--;
  });

  const char* mime = getContentType(path);
  AsyncWebServerResponse *response = request->beginResponse(LittleFS, hasGz ? gzPath : path, mime);
  if (hasGz) {
    response->addHeader("Content-Encoding", "gzip");
  }
  if (path.endsWith(".html")) {
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  } else {
    response->addHeader("Cache-Control", "public, max-age=604800, immutable");
  }
  response->addHeader("Connection", "close");
  request->send(response);
  return true;
}

void scheduleESPReset(uint16_t delayMs) {
  pendingReset = true;
  resetTimer = millis() + delayMs;
}

void triggerWifiSwitch(uint8_t mode) {
  targetWifiMode = mode;
  wifiSwitchTimer = millis() + 400; 
  pendingWifiSwitch = true;
}

// --- WEBSOCKET CLIENT-LIFECYCLE & SESSION-VERWALTUNG ---
struct WsTabSession {
  uint32_t clientId;
  char tabId[12];
  uint32_t stuckQueueSince;
};
static const size_t MAX_WS_SESSIONS = PLATFORM_MAX_WS_SESSIONS;
static WsTabSession wsSessions[MAX_WS_SESSIONS] = {};

static void addWsClient(uint32_t clientId) {
  for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
    if (wsSessions[i].clientId == clientId) {
      wsSessions[i].stuckQueueSince = 0;
      return;
    }
  }
  for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
    if (wsSessions[i].clientId == 0) {
      wsSessions[i].clientId = clientId;
      wsSessions[i].tabId[0] = '\0';
      wsSessions[i].stuckQueueSince = 0;
      return;
    }
  }
}

static void registerWsTab(uint32_t clientId, const char* tabId) {
  if (!tabId || tabId[0] == '\0') return;

  // 1. Wenn derselbe Tab sich neu verbindet -> vorherigen Socket dieses Tabs schließen
  for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
    if (wsSessions[i].clientId != 0 && wsSessions[i].clientId != clientId && strncmp(wsSessions[i].tabId, tabId, sizeof(wsSessions[i].tabId)) == 0) {
      uint32_t oldId = wsSessions[i].clientId;
      wsSessions[i].clientId = 0;
      wsSessions[i].tabId[0] = '\0';
      wsSessions[i].stuckQueueSince = 0;
      ws.close(oldId, 1000, "Tab reloaded");
      break;
    }
  }

  // 2. Tab-ID fuer diesen Client speichern
  for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
    if (wsSessions[i].clientId == clientId) {
      strncpy(wsSessions[i].tabId, tabId, sizeof(wsSessions[i].tabId) - 1);
      wsSessions[i].tabId[sizeof(wsSessions[i].tabId) - 1] = '\0';
      wsSessions[i].stuckQueueSince = 0;
      return;
    }
  }
}

static void unregisterWsClient(uint32_t clientId) {
  for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
    if (wsSessions[i].clientId == clientId) {
      wsSessions[i].clientId = 0;
      wsSessions[i].tabId[0] = '\0';
      wsSessions[i].stuckQueueSince = 0;
      return;
    }
  }
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    if (!PlatformRuntime::canAcceptWebSocket(ws.count(), currentRuntimeWifiMode)) {
      client->close(1013, "Boxengasse voll");
      return;
    }
    triggerGhostWlanEarlyShutdown();
    client->keepAlivePeriod(30);
    addWsClient(client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      char* str = (char*)data;
      if (strncmp(str, "{\"cmd\":\"hello\"", 14) == 0) {
        char* tabKey = strstr(str, "\"tab\":\"");
        if (tabKey) {
          tabKey += 7;
          char* end = strchr(tabKey, '\"');
          if (end) {
            *end = '\0';
            registerWsTab(client->id(), tabKey);
          }
        }
      } else if (strncmp(str, "{\"cmd\":\"ping\"", 13) == 0) {
        client->text(F("{\"cmd\":\"pong\"}"));
      } else if (strncmp(str, "{\"ping\":", 8) == 0) {
        client->text(str);
      }
    }
  } else if (type == WS_EVT_DISCONNECT) {
    unregisterWsClient(client->id());
  }
}

static void blinkGreenLED(int count) {
  for(int i = 0; i < count; i++) {
    LapStatusLeds::setNetworkReady(true);
    delay(166);
    LapStatusLeds::setNetworkReady(false);
    delay(166);
  }
}

static void setupGhostWLAN() {
  currentActiveIP = NetworkPlatform::stationIP();
  NetworkPlatform::prepareAccessPoint(true);
  
  String ipStr = currentActiveIP.toString();
  String connectedSsid = NetworkPlatform::stationSSID();
  
  int maxSsidLen = 31 - 4 - ipStr.length();
  if (maxSsidLen > 0 && connectedSsid.length() > (size_t)maxSsidLen) {
    connectedSsid = connectedSsid.substring(0, maxSsidLen);
  }
  String ipApName = "LT_" + connectedSsid + "-" + ipStr;
  NetworkPlatform::startAccessPoint(ipApName.c_str(), "Laptimer123", NetworkPlatform::channel(), 0, 1);

  ghostWlanActive = true;
  ghostWlanOffTimer = 0; 

  LapStatusLeds::setNetworkReady(true);
}

static void setupFallbackAP() {
  ghostWlanActive = false;
  NetworkPlatform::disconnect(true);
  delay(50);
  NetworkPlatform::prepareAccessPoint(false);
  
  IPAddress local_IP(192, 168, 0, 8);
  if (learnedApIP.length() > 0) {
    IPAddress parsed;
    if (parsed.fromString(learnedApIP) && parsed[0] != 0) {
      local_IP = parsed;
    }
  }
  IPAddress gateway = local_IP;
  IPAddress subnet(255, 255, 255, 0);
  NetworkPlatform::configureAccessPoint(local_IP, gateway, subnet);
  
  String activeApName = (apSSID.length() > 0) ? apSSID : getDefaultApSSID();
  const char* apKey = apPass.length() > 0 ? apPass.c_str() : AP_PASS;
  
  NetworkPlatform::startAccessPoint(activeApName.c_str(), apKey, 1, 0, PLATFORM_MAX_AP_CLIENTS);
  currentActiveIP = local_IP;
  currentRuntimeWifiMode = 0;
  LapStatusLeds::setNetworkReady(true);
}

static void connectNetworkCascade(uint8_t startMode) {
  if (startMode == 1) {
    String s = homeSSID.length() > 0 ? homeSSID : HOME_SSID;
    String p = homePass.length() > 0 ? homePass : HOME_PASS;
    
    LapStatusLeds::setNetworkReady(false);
    NetworkPlatform::disconnect(true);
    delay(50);
    String hostname = getDeviceHostname();
    NetworkPlatform::prepareStation(hostname.c_str());
    
    NetworkPlatform::beginStation(s.c_str(), p.c_str());
    uint32_t startMs = millis();
    while (!NetworkPlatform::isStationConnected() && (millis() - startMs < 10000)) {
      delay(100);
    }
    
    if (NetworkPlatform::isStationConnected()) {
      currentRuntimeWifiMode = 1;
      updateLearnedApIP(NetworkPlatform::stationIP());
      setupGhostWLAN();
      return; 
    } else {
      blinkGreenLED(3);
      delay(300);
      startMode = 2; 
    }
  }

  if (startMode == 2 && hotspotSSID.length() > 0) {
    LapStatusLeds::setNetworkReady(false);
    NetworkPlatform::disconnect(true);
    delay(50);
    String hostname = getDeviceHostname();
    NetworkPlatform::prepareStation(hostname.c_str());
    
    NetworkPlatform::beginStation(hotspotSSID.c_str(), hotspotPass.c_str());
    uint32_t startMs = millis();
    while (!NetworkPlatform::isStationConnected() && (millis() - startMs < 12000)) {
      delay(100);
    }
    
    if (NetworkPlatform::isStationConnected()) {
      currentRuntimeWifiMode = 2;
      setupGhostWLAN();
      return; 
    } else {
      blinkGreenLED(5);
    }
  }

  setupFallbackAP();
}

#include <memory>

#define MAX_FS_DIR_DEPTH 6

struct FilesStreamState {
  char pending[256];
  size_t pendingLen = 0;
  size_t pendingOff = 0;
  uint8_t phase = 0;
  bool first = true;
  String dirPaths[MAX_FS_DIR_DEPTH];
#if defined(ESP8266)
  Dir dirs[MAX_FS_DIR_DEPTH];
#elif defined(ESP32)
  File dirs[MAX_FS_DIR_DEPTH];
#endif
  int8_t depth = -1;
};

void initNetworkAndServer() {
  connectNetworkCascade(activeWifiMode);

  auto serveUpdatePage = [](AsyncWebServerRequest *request) {
    if (!serveStaticWithHeapGuard(request, "/update.html")) {
      request->send(200, "text/html", "<h2>Update</h2><form method='POST' action='/api/upload' enctype='multipart/form-data'><input type='file' name='data'><input type='submit' value='Upload'></form>");
    }
  };
  server.on("/update", HTTP_GET, serveUpdatePage);
  server.on("/update.html", HTTP_GET, serveUpdatePage);

  auto serveDebugPage = [](AsyncWebServerRequest *request) {
    if (!serveStaticWithHeapGuard(request, "/debug.html")) {
      request->send(404, "text/plain", "Debug-Seite nicht gefunden");
    }
  };
  server.on("/debug", HTTP_GET, serveDebugPage);
  server.on("/debug/", HTTP_GET, serveDebugPage);
  server.on("/debug.html", HTTP_GET, serveDebugPage);

  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Upload erfolgreich!");
  }, handleFileUpload);

  server.on("/api/update/firmware", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool hasError = Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(hasError ? 500 : 200, "text/plain", hasError ? "Flash-Fehler" : "OK");
    response->addHeader("Connection", "close");
    request->send(response);
    if (!hasError) {
      scheduleESPReset(1200);
    }
  }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
#if defined(ESP8266)
      Update.runAsync(true);
      size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#elif defined(ESP32)
      size_t maxSketchSpace = UPDATE_SIZE_UNKNOWN;
#endif
      if (!Update.begin(maxSketchSpace, U_FLASH)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (!Update.end(true)) {
        Update.printError(Serial);
      }
    }
  });

  server.on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    updateFsUsageCache();
    String json;
    json.reserve(128);
    json = "{\"total\":";
    json += cachedFsTotalBytes;
    json += ",\"used\":";
    json += cachedFsUsedBytes;
    size_t freeBytes = (cachedFsTotalBytes > cachedFsUsedBytes) ? (cachedFsTotalBytes - cachedFsUsedBytes) : 0;
    json += ",\"free\":";
    json += freeBytes;
    json += ",\"pct\":";
    json += cachedFsUsedPct;
    json += ",\"full\":";
    json += (storageFullLocked || cachedFsUsedPct >= 95 ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    flushCurrentSessionToIndex();
    updateFsUsageCache();

    auto st = std::make_shared<FilesStreamState>();
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
      [st](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        if (index == 0) {
          st->pendingLen = st->pendingOff = 0;
          st->phase = 0;
          st->first = true;
          st->depth = -1;
        }

        size_t written = 0;
        while (written < maxLen) {
          if (st->pendingOff < st->pendingLen) {
            size_t n = st->pendingLen - st->pendingOff;
            if (n > maxLen - written) n = maxLen - written;
            memcpy(buffer + written, st->pending + st->pendingOff, n);
            st->pendingOff += n;
            written += n;
            continue;
          }
          st->pendingLen = st->pendingOff = 0;

          if (st->phase == 0) {
            int n = snprintf(st->pending, sizeof(st->pending), "{\"total\":%u,\"used\":%u,\"files\":[",
                             (unsigned int)cachedFsTotalBytes, (unsigned int)cachedFsUsedBytes);
            if (n > 0) st->pendingLen = (size_t)n < sizeof(st->pending) ? (size_t)n : sizeof(st->pending) - 1;
            st->depth = 0;
            st->dirPaths[0] = "/";
#if defined(ESP8266)
            st->dirs[0] = LittleFS.openDir("/");
#elif defined(ESP32)
            st->dirs[0] = LittleFS.open("/");
            if (!st->dirs[0] || !st->dirs[0].isDirectory()) st->depth = -1;
#endif
            st->phase = 1;
          } else if (st->phase == 1) {
            if (st->depth < 0) {
              st->phase = 2;
              continue;
            }
#if defined(ESP8266)
            if (st->dirs[st->depth].next()) {
              String entryName = st->dirs[st->depth].fileName();
              String fullPath = (st->dirPaths[st->depth] == "/") ? ("/" + entryName) : (st->dirPaths[st->depth] + "/" + entryName);
              if (st->dirs[st->depth].isDirectory()) {
                if (st->depth + 1 < MAX_FS_DIR_DEPTH) {
                  st->depth++;
                  st->dirPaths[st->depth] = fullPath;
                  st->dirs[st->depth] = LittleFS.openDir(fullPath);
                }
              } else {
                const char* rel = fullPath.c_str() + (fullPath.startsWith("/") ? 1 : 0);
                int n = snprintf(st->pending, sizeof(st->pending), "%s{\"name\":\"%s\",\"size\":%u,\"time\":%lu}",
                                 st->first ? "" : ",", rel, (unsigned int)st->dirs[st->depth].fileSize(), (unsigned long)st->dirs[st->depth].fileTime());
                if (n > 0) {
                  st->pendingLen = (size_t)n < sizeof(st->pending) ? (size_t)n : sizeof(st->pending) - 1;
                  st->first = false;
                }
              }
            } else {
              st->dirPaths[st->depth] = "";
              st->depth--;
            }
#elif defined(ESP32)
            File file = st->dirs[st->depth].openNextFile();
            if (file) {
              String fullPath = file.path();
              if (file.isDirectory()) {
                if (st->depth + 1 < MAX_FS_DIR_DEPTH) {
                  st->depth++;
                  st->dirPaths[st->depth] = fullPath;
                  st->dirs[st->depth] = file;
                } else {
                  file.close();
                }
              } else {
                const char* rel = fullPath.c_str() + (fullPath.startsWith("/") ? 1 : 0);
                int n = snprintf(st->pending, sizeof(st->pending), "%s{\"name\":\"%s\",\"size\":%u,\"time\":%lu}",
                                 st->first ? "" : ",", rel, (unsigned int)file.size(), (unsigned long)file.getLastWrite());
                if (n > 0) {
                  st->pendingLen = (size_t)n < sizeof(st->pending) ? (size_t)n : sizeof(st->pending) - 1;
                  st->first = false;
                }
                file.close();
              }
            } else {
              st->dirs[st->depth].close();
              st->dirPaths[st->depth] = "";
              st->depth--;
            }
#endif
          } else if (st->phase == 2) {
            st->pending[0] = ']';
            st->pending[1] = '}';
            st->pendingLen = 2;
            st->phase = 3;
          } else {
            break;
          }
        }
        return written;
      });
    request->send(response);
  });

  server.on("/api/deletefile", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (isRaceDeleteLocked()) {
      request->send(423, "application/json", "{\"error\":\"locked_during_race\",\"message\":\"Dateiloeschen waehrend eines laufenden Rennens gesperrt\"}");
      return;
    }
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      filename.replace("%2F", "/");
      filename.replace("%2f", "/");
      if (!filename.startsWith("/")) filename = "/" + filename;

      if (filename.indexOf("..") >= 0) {
        request->send(400, "text/plain", "Ungueltiger Pfad");
        return;
      }

      // Loeschen (inkl. Eintrag in /sessions.json) laeuft im loop(), nicht im Webserver-Callback
      if (!queueFileDeletion(filename)) {
        request->send(503, "text/plain", "Busy");
        return;
      }
      request->send(200, "text/plain", "Geloescht");
      return;
    }
    request->send(400, "text/plain", "Datei nicht gefunden");
  });

  server.on("/api/download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String fn = request->getParam("file")->value();
      fn.replace("%2F", "/");
      fn.replace("%2f", "/");
      while (fn.startsWith("/")) fn = fn.substring(1);
      fn = "/" + fn;
      if (LittleFS.exists(fn)) {
        request->send(LittleFS, fn, "application/octet-stream", true);
        return;
      }
    }
    request->send(404, "text/plain", "Datei nicht gefunden");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP startet neu...");
    scheduleESPReset(600);
  });

  auto handleSystemMetrics = [](AsyncWebServerRequest *request) {
    String json;
    json.reserve(448);
    uint32_t uptime = millis() / 1000UL;
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t freePsram = (uint32_t)PlatformRuntime::getFreePsramBytes();
    uint8_t frag = PlatformRuntime::getHeapFragmentationPercent();
    uint32_t cpu = ESP.getCpuFreqMHz();
    float maxLoopMs = perfMaxLoopUs / 1000.0f;

    json = "{\"freeHeap\":";
    json += freeHeap;
    json += ",\"free_heap\":";
    json += freeHeap;
    json += ",\"freePsram\":";
    json += freePsram;
    json += ",\"frag\":";
    json += frag;
    json += ",\"maxLoopUs\":";
    json += perfMaxLoopUs;
    json += ",\"max_loop_ms\":";
    json += String(maxLoopMs, 1);
    json += ",\"avgLoopUs\":";
    json += perfAvgLoopUs;
    json += ",\"cpuMhz\":";
    json += cpu;
    json += ",\"cpu_mhz\":";
    json += cpu;
    json += ",\"uptimeSec\":";
    json += uptime;
    json += ",\"uptime\":";
    json += uptime;
    json += ",\"resetReason\":\"";
    json += escapeJsonStr(lastResetReason);
    json += "\",\"resetInfo\":\"";
    json += escapeJsonStr(lastResetInfo);
    json += "\",\"isCrash\":";
    json += (lastResetIsCrash ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  };
  server.on("/api/debug/system", HTTP_GET, handleSystemMetrics);
  server.on("/api/system/metrics", HTTP_GET, handleSystemMetrics);
  server.on("/api/debug/crashlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain; charset=utf-8", getCrashLog());
  });
  server.on("/api/debug/crashlog/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (clearCrashLog()) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(500, "text/plain", "Crash-Log konnte nicht gelöscht werden");
    }
  });

  server.on("/api/allconfig", HTTP_GET, handleGetAllConfig);
  server.on("/api/wificonfig", HTTP_GET, handleGetWifiConfig);
  server.on("/api/setwificonfig", HTTP_POST, handlePostSetWifiConfig);
  server.on("/api/switchwifi", HTTP_POST, handlePostSwitchWifi);
  server.on("/api/trigger_sz_ota", HTTP_POST, handlePostTriggerSzOta);

  server.on("/api/hotspot", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"ssid\":\"" + hotspotSSID + "\",\"pass\":\"" + hotspotPass + "\"}";
    request->send(200, "application/json", json);
  });
  server.on("/api/sethotspot", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid = request->hasParam("ssid") ? request->getParam("ssid")->value() : "";
    String pass = request->hasParam("pass") ? request->getParam("pass")->value() : "";
    saveHotspotConfigToFS(ssid, pass);
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/debug/packets", HTTP_GET, handleGetDebugPackets);
  server.on("/api/debug/status", HTTP_GET, handleGetDebugStatus);
  server.on("/api/debug/set", HTTP_POST, handlePostSetDebugMode);

  server.on("/api/laps", HTTP_GET, handleGetLaps);
  server.on("/api/stations", HTTP_GET, handleGetStationsStatus);
  server.on("/api/session", HTTP_POST, handlePostSession);
  server.on("/api/timeleft", HTTP_GET, handleGetTimeLeft);
  server.on("/api/recent", HTTP_GET, handleGetRecent);
  server.on("/api/deadtime", HTTP_GET, handleGetDeadtime);
  server.on("/api/filter", HTTP_GET, handleGetFilter);
  server.on("/api/names", HTTP_GET, handleGetNames);
  server.on("/api/version", HTTP_GET, handleGetVersion);
  server.on("/api/setfilter", HTTP_POST, handlePostSetFilter);
  server.on("/api/setdeadtime", HTTP_POST, handlePostSetDeadtime);
  server.on("/api/rename", HTTP_POST, handlePostRename);
  server.on("/api/reset", HTTP_POST, handlePostReset);

  server.on("/api/synctime", HTTP_POST, handlePostSyncTime);
  server.on("/api/sessions", HTTP_GET, handleGetSessions);
  server.on("/api/sessionfile", HTTP_GET, handleGetSessionFile);
  server.on("/api/deletelap", HTTP_POST, handlePostDeleteLap);
  server.on("/api/deletelapstatus", HTTP_GET, handleGetDeleteLapStatus);
  server.on("/api/deletedriverstats", HTTP_POST, handlePostDeleteDriverStats);
  server.on("/api/deleteallstats", HTTP_POST, handlePostDeleteAllStats);
  server.on("/api/deletesession", HTTP_POST, handlePostDeleteSession);


  server.on("/api/tracks", HTTP_GET, handleGetTracks);
  server.on("/api/tracks/select", HTTP_POST, handlePostSelectTrack);
  server.on("/api/tracks/add", HTTP_POST, handlePostAddTrack);
  server.on("/api/tracks/delete", HTTP_POST, handlePostDeleteTrack);
  server.on("/api/tracks/deleterecord", HTTP_POST, handlePostDeleteTrackRecord);

  server.on("/api/sectorconfig", HTTP_GET, handleGetSectorConfig);
  server.on("/api/setsectorconfig", HTTP_POST, handlePostSetSectorConfig);

  server.on("/api/buzzer", HTTP_POST, handlePostBuzzer);
  server.on("/api/buzzer", HTTP_GET, handleGetBuzzer);
  server.on("/api/countdown", HTTP_POST, handlePostCountdown);

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });
  server.on("/apple-touch-icon-precomposed.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    String url = request->url();

    if (url.startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"Not Found\"}");
      return;
    }
    if (url.startsWith("/apple-touch-icon") || url.endsWith(".map")) {
      request->send(204);
      return;
    }

    if (serveStaticWithHeapGuard(request, url)) {
      return;
    }

    request->send(404, "text/plain", "404: Not Found");
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  
  NetworkPlatform::beginMdns("laptimer");
  
  ArduinoOTA.setHostname(getOtaHostname().c_str());
  ArduinoOTA.begin();
}

void handleNetworkTasks() {
  if (pendingReset && millis() >= resetTimer) {
    ESP.restart();
  }

  if (ghostWlanActive && ghostWlanOffTimer > 0 && millis() >= ghostWlanOffTimer) {
    ghostWlanActive = false;
    NetworkPlatform::stopAccessPoint(true);
    NetworkPlatform::setStationMode();
  }

  if (pendingWifiSwitch && millis() >= wifiSwitchTimer) {
    pendingWifiSwitch = false;
    ws.closeAll();
    server.end();
    delay(150);
    ESP.restart(); 
  }

  NetworkPlatform::updateMdns();
  ArduinoOTA.handle();
  // Zyklischer Check auf verstaute Sockets (alle 3 Sekunden)
  static uint32_t lastClientStallCheck = 0;
  uint32_t now = millis();
  if (now - lastClientStallCheck >= 3000) {
    lastClientStallCheck = now;
    if (ws.count() > 0) {
      for (size_t i = 0; i < MAX_WS_SESSIONS; i++) {
        if (wsSessions[i].clientId != 0) {
          AsyncWebSocketClient* c = ws.client(wsSessions[i].clientId);
          if (c) {
            // Wenn der Client unquittierte Pakete staut (z. B. Handy im Standby/gesperrt)
            if (c->queueLen() > 0) {
              if (wsSessions[i].stuckQueueSince == 0) {
                wsSessions[i].stuckQueueSince = now;
              } else if (now - wsSessions[i].stuckQueueSince >= 8000) {
                uint32_t deadId = wsSessions[i].clientId;
                unregisterWsClient(deadId);
                c->close(1008, "Queue stalled");
              }
            } else {
              wsSessions[i].stuckQueueSince = 0;
            }
          } else {
            wsSessions[i].clientId = 0;
            wsSessions[i].tabId[0] = '\0';
            wsSessions[i].stuckQueueSince = 0;
          }
        }
      }
    }
  }

  // Bereinigt getrennte / geschlossene WebSockets aus dem Speicher (ohne bestehende Clients zu kicken)
  ws.cleanupClients();
}

void broadcastWebSocket(const String& msg) {
  if (ws.count() > 0) {
    ws.textAll(msg);
  }
}
