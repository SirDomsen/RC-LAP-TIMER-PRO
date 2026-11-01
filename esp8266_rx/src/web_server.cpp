#include "web_server.h"
#include "web_handlers.h"
#include "storage.h"
#include "esp_now_base.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
extern "C" {
  #include <espnow.h>
}
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_now.h>
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

static const char* cacheHeaderStr = "max-age=86400";
// --- DYNAMISCHE HEAP-SCHUTZMAUER ---
const size_t HEAP_CRITICAL_MIN_BYTES = 12000;
const uint8_t HEAP_CRITICAL_MAX_FRAG = 55;
const size_t HEAP_CRITICAL_MIN_BLOCK = 3072;

static const char BUSY_HTML_PROGMEM[] PROGMEM = 
  "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<meta http-equiv='refresh' content='4'>"
  "<title>RC Lap Timer - Warteschlange</title>"
  "<style>body{background:#0a0c10;color:#00e5ff;font-family:sans-serif;text-align:center;padding:40px 20px;}"
  ".box{border:1px solid #00e5ff;background:#161b22;padding:20px;border-radius:10px;max-width:400px;margin:0 auto;box-shadow:0 0 15px rgba(0,229,255,0.3);}"
  "</style></head><body><div class='box'><h3>&#x23F3; Verbindungs-Warteschlange</h3>"
  "<p style='color:#f0f6fc;font-size:14px;'>Mehrere Geräte laden gleichzeitig. Verbinde automatisch in wenigen Sekunden...</p></div></body></html>";

bool isHeapCriticalForFileTransfer() {
#if defined(ESP8266)
  return (ESP.getFreeHeap() < HEAP_CRITICAL_MIN_BYTES || ESP.getHeapFragmentation() > HEAP_CRITICAL_MAX_FRAG || ESP.getMaxFreeBlockSize() < HEAP_CRITICAL_MIN_BLOCK);
#elif defined(ESP32)
  return (ESP.getFreeHeap() < HEAP_CRITICAL_MIN_BYTES);
#endif
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

static void triggerGhostWlanEarlyShutdown() {
  if (ghostWlanActive && ghostWlanOffTimer == 0) {
    ghostWlanOffTimer = millis() + 15000UL; 
  }
}

static bool serveStaticWithHeapGuard(AsyncWebServerRequest *request, String path) {
  if (path == "/" || path.length() == 0) path = "/index.html";
  if (path == "/admin" || path == "/admin/") path = "/admin.html";

  if (path == "/index.html" || path == "/admin.html") {
    triggerGhostWlanEarlyShutdown();
  }

  if (isHeapCriticalForFileTransfer()) {
    if (path.endsWith(".html")) {
      AsyncWebServerResponse *res = request->beginResponse(503, "text/html", BUSY_HTML_PROGMEM);
      res->addHeader("Retry-After", "4");
      res->addHeader("Connection", "close");
      request->send(res);
      return true;
    } else if (!path.endsWith(".css") && !path.endsWith(".js")) {
      AsyncWebServerResponse *res = request->beginResponse(503, "text/plain", "Busy");
      res->addHeader("Retry-After", "4");
      res->addHeader("Connection", "close");
      request->send(res);
      return true;
    }
  }

  String gzPath = path + ".gz";
  bool hasGz = LittleFS.exists(gzPath);
  bool hasRaw = !hasGz && LittleFS.exists(path);

  if (!hasGz && !hasRaw) return false;

  const char* mime = getContentType(path);
  AsyncWebServerResponse *response = request->beginResponse(LittleFS, hasGz ? gzPath : path, mime);
  if (hasGz) {
    response->addHeader("Content-Encoding", "gzip");
  }
  response->addHeader("Cache-Control", cacheHeaderStr);
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
static const size_t MAX_WS_SESSIONS = 4;
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
    if (isHeapCriticalForFileTransfer()) {
      client->close(1013, "Server busy");
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
    digitalWrite(LED_READY, HIGH);
    delay(166);
    digitalWrite(LED_READY, LOW);
    delay(166);
  }
}

static void setupGhostWLAN() {
  currentActiveIP = WiFi.localIP();
#if defined(ESP8266)
  WiFi.setPhyMode(WIFI_PHY_MODE_11N); 
  WiFi.mode(WIFI_AP_STA);
  WiFi.setOutputPower(20.5);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); 
#elif defined(ESP32)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
#endif
  
  String ipStr = currentActiveIP.toString();
  String connectedSsid = WiFi.SSID();
  
  int maxSsidLen = 31 - 4 - ipStr.length();
  if (maxSsidLen > 0 && connectedSsid.length() > (size_t)maxSsidLen) {
    connectedSsid = connectedSsid.substring(0, maxSsidLen);
  }
  
  String ipApName = "LT_" + connectedSsid + "-" + ipStr;
  WiFi.softAP(ipApName.c_str(), "Laptimer123", WiFi.channel(), 0, 1);

  ghostWlanActive = true;
  ghostWlanOffTimer = 0; 

  digitalWrite(LED_READY, HIGH); 
}

static void setupFallbackAP() {
  ghostWlanActive = false;
  WiFi.disconnect(true);
  delay(50);
#if defined(ESP8266)
  WiFi.setPhyMode(WIFI_PHY_MODE_11N); 
  WiFi.mode(WIFI_AP);
  WiFi.setOutputPower(20.5);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); 
#elif defined(ESP32)
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
#endif
  
  IPAddress local_IP(192, 168, 0, 8);
  if (learnedApIP.length() > 0) {
    IPAddress parsed;
    if (parsed.fromString(learnedApIP) && parsed[0] != 0) {
      local_IP = parsed;
    }
  }
  IPAddress gateway = local_IP;
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  String activeApName = (apSSID.length() > 0) ? apSSID : getDefaultApSSID();
  const char* apKey = apPass.length() > 0 ? apPass.c_str() : AP_PASS;
  
  WiFi.softAP(activeApName.c_str(), apKey, 1, 0, 4);
  currentActiveIP = local_IP;
  currentRuntimeWifiMode = 0;
  digitalWrite(LED_READY, HIGH); 
}

static void connectNetworkCascade(uint8_t startMode) {
  if (startMode == 1) {
    String s = homeSSID.length() > 0 ? homeSSID : HOME_SSID;
    String p = homePass.length() > 0 ? homePass : HOME_PASS;
    
    digitalWrite(LED_READY, LOW);
    WiFi.disconnect(true);
    delay(50);
#if defined(ESP8266)
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(20.5);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.config(0u, 0u, 0u);
    WiFi.hostname(getDeviceHostname());
#elif defined(ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(getDeviceHostname().c_str());
#endif
    
    WiFi.begin(s.c_str(), p.c_str());
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 10000)) {
      delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      currentRuntimeWifiMode = 1;
      updateLearnedApIP(WiFi.localIP());
      setupGhostWLAN();
      return; 
    } else {
      blinkGreenLED(3);
      delay(300);
      startMode = 2; 
    }
  }

  if (startMode == 2 && hotspotSSID.length() > 0) {
    digitalWrite(LED_READY, LOW);
    WiFi.disconnect(true);
    delay(50);
#if defined(ESP8266)
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(20.5);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.config(0u, 0u, 0u);
    WiFi.hostname(getDeviceHostname());
#elif defined(ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(getDeviceHostname().c_str());
#endif
    
    WiFi.begin(hotspotSSID.c_str(), hotspotPass.c_str());
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 12000)) {
      delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      currentRuntimeWifiMode = 2;
      setupGhostWLAN();
      return; 
    } else {
      blinkGreenLED(5);
    }
  }

  setupFallbackAP();
}

#if defined(ESP8266)
static void listFilesRecursive(const String& basePath, AsyncResponseStream *response, bool& first) {
  Dir dir = LittleFS.openDir(basePath);
  while (dir.next()) {
    String entryName = dir.fileName();
    String fullPath = (basePath == "/") ? ("/" + entryName) : (basePath + "/" + entryName);
    if (dir.isDirectory()) {
      listFilesRecursive(fullPath, response, first);
    } else {
      if (!first) response->print(",");
      String relName = fullPath.startsWith("/") ? fullPath.substring(1) : fullPath;
      response->printf("{\"name\":\"%s\",\"size\":%u,\"time\":%lu}",
                       relName.c_str(), (unsigned int)dir.fileSize(), (unsigned long)dir.fileTime());
      first = false;
    }
  }
}
#elif defined(ESP32)
static void listFilesRecursive(const String& basePath, AsyncResponseStream *response, bool& first) {
  File dir = LittleFS.open(basePath);
  if (!dir || !dir.isDirectory()) return;
  File file = dir.openNextFile();
  while (file) {
    String fullPath = file.path();
    if (file.isDirectory()) {
      listFilesRecursive(fullPath, response, first);
    } else {
      if (!first) response->print(",");
      String relName = fullPath.startsWith("/") ? fullPath.substring(1) : fullPath;
      response->printf("{\"name\":\"%s\",\"size\":%u,\"time\":%lu}",
                       relName.c_str(), (unsigned int)file.size(), (unsigned long)file.getLastWrite());
      first = false;
    }
    file = dir.openNextFile();
  }
}
#endif

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

  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    flushCurrentSessionToIndex();
#if defined(ESP8266)
    FSInfo fs_info;
    LittleFS.info(fs_info);
    size_t totalBytes = fs_info.totalBytes;
    size_t usedBytes = fs_info.usedBytes;
#elif defined(ESP32)
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
#endif
    updateFsUsageCache();

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->printf("{\"total\":%u,\"used\":%u,\"files\":[",
                     (unsigned int)totalBytes, (unsigned int)usedBytes);

    bool first = true;
    listFilesRecursive("/", response, first);

    response->print("]}");
    request->send(response);
  });

  server.on("/api/deletefile", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      filename.replace("%2F", "/");
      filename.replace("%2f", "/");
      if (!filename.startsWith("/")) filename = "/" + filename;

      // Falls es eine Session-Datei aus einem Unterordner ist, auch sauber aus /sessions.json entfernen
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash > 0 && filename.endsWith(".json")) {
        String baseName = filename.substring(lastSlash + 1, filename.length() - 5);
        deleteSessionById(baseName);
      }

      if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
      }
      updateFsUsageCache();
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

  server.on("/api/debug/system", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json;
    json.reserve(200);
    json = "{\"freeHeap\":";
    json += ESP.getFreeHeap();
    json += ",\"frag\":";
#if defined(ESP8266)
    json += ESP.getHeapFragmentation();
#else
    json += 0;
#endif
    json += ",\"maxLoopUs\":";
    json += perfMaxLoopUs;
    json += ",\"avgLoopUs\":";
    json += perfAvgLoopUs;
    json += ",\"cpuMhz\":";
    json += ESP.getCpuFreqMHz();
    json += ",\"uptimeSec\":";
    json += (millis() / 1000UL);
    json += "}";
    request->send(200, "application/json", json);
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
  
  if (MDNS.begin("laptimer")) {
    MDNS.addService("http", "tcp", 80);
  }
  
  ArduinoOTA.setHostname(getOtaHostname().c_str());
  ArduinoOTA.begin();
}

void handleNetworkTasks() {
  if (pendingReset && millis() >= resetTimer) {
    ESP.restart();
  }

  if (ghostWlanActive && ghostWlanOffTimer > 0 && millis() >= ghostWlanOffTimer) {
    ghostWlanActive = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#elif defined(ESP32)
    WiFi.setSleep(false);
#endif
  }

  if (pendingWifiSwitch && millis() >= wifiSwitchTimer) {
    pendingWifiSwitch = false;
    ws.closeAll();
    server.end();
    delay(150);
    ESP.restart(); 
  }

#if defined(ESP8266)
  MDNS.update();
#endif
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

  // Haelt max. 3 WebSockets aktiv, damit min. 2 TCP-PCBs fuer HTTP-Assets frei bleiben
  ws.cleanupClients(3);
}

void broadcastWebSocket(const String& msg) {
  if (ws.count() > 0) {
    ws.textAll(msg);
  }
}
