#include "web_server.h"
#include "web_handlers.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <DNSServer.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
DNSServer dnsServer;

const byte DNS_PORT = 53;
static IPAddress currentActiveIP(192, 168, 0, 8);

extern uint32_t perfMaxLoopUs;
extern uint32_t perfAvgLoopUs;

static bool pendingReset = false;
static uint32_t resetTimer = 0;

void scheduleESPReset(uint16_t delayMs) {
  pendingReset = true;
  resetTimer = millis() + delayMs;
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WebSocket] Client #%u verbunden\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      if (strncmp((char*)data, "{\"ping\":", 8) == 0) {
        client->text((char*)data);
      }
    }
  }
}

const char CAPTIVE_HTML_FALLBACK[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RC Lap Timer Pro</title>
  <style>
    body { font-family: sans-serif; background: #121212; color: #fff; text-align: center; padding: 40px 20px; }
    .btn { display: block; padding: 16px; background: #00e676; color: #000; text-decoration: none; font-weight: bold; border-radius: 8px; margin-top: 20px; }
  </style>
</head>
<body>
  <h2>RC Lap Timer Pro</h2>
  <p>Für Sprachausgabe bitte im Browser öffnen:</p>
  <a id="lnk" href="#" class="btn">Zum RC Laptimer Pro</a>
  <script>
    var ip = "%LAPTIMER_IP%";
    if(ip.startsWith("%")) ip = window.location.hostname;
    document.getElementById("lnk").href = "http://" + ip + "/";
  </script>
</body>
</html>
)rawliteral";

String captiveProcessor(const String& var) {
  if (var == "LAPTIMER_IP") {
    return currentActiveIP.toString();
  }
  return String();
}

static void setupWLAN() {
  IPAddress local_IP(192, 168, 0, 8);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.config(local_IP, gateway, subnet)) Serial.println("[WLAN] STA Fehler!");

  WiFi.begin(HOME_SSID, HOME_PASS);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { delay(250); timeout++; }

  if (WiFi.status() == WL_CONNECTED) {
    currentActiveIP = WiFi.localIP();
    Serial.println("[WLAN] Verbunden! IP: " + currentActiveIP.toString());
  } else {
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);
    currentActiveIP = local_IP;
    dnsServer.start(DNS_PORT, "*", currentActiveIP);
    Serial.println("[DNS] Captive Portal DNS-Server gestartet.");
  }

  if (MDNS.begin("laptimer")) {
    MDNS.addService("http", "tcp", 80);
  }

  ArduinoOTA.setHostname("LapTimer-Base");
  ArduinoOTA.begin();

  digitalWrite(LED_READY, HIGH);
}

void initNetworkAndServer() {
  setupWLAN();

  // serveStatic erkennt automatisch vorhandene .gz Dateien und liefert den Content-Encoding Header mit
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // --- UPDATE-SEITE (unter /update oder /update.html) ---
  auto serveUpdatePage = [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/update.html.gz")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/update.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    } else if (LittleFS.exists("/update.html")) {
      request->send(LittleFS, "/update.html", "text/html");
    } else {
      request->send(200, "text/html", "<h2>Update</h2><form method='POST' action='/api/upload' enctype='multipart/form-data'><input type='file' name='data'><input type='submit' value='Upload'></form>");
    }
  };
  server.on("/update", HTTP_GET, serveUpdatePage);
  server.on("/update.html", HTTP_GET, serveUpdatePage);

  // --- DEBUG-SEITE (unter /debug oder /debug.html) ---
  auto serveDebugPage = [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/debug.html.gz")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/debug.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    } else if (LittleFS.exists("/debug.html")) {
      request->send(LittleFS, "/debug.html", "text/html");
    } else {
      request->send(404, "text/plain", "Debug-Seite nicht im Flash gefunden");
    }
  };
  server.on("/debug", HTTP_GET, serveDebugPage);
  server.on("/debug.html", HTTP_GET, serveDebugPage);

  // --- UPLOAD & DATEI-MANAGER ---
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Upload erfolgreich!");
  }, handleFileUpload);

  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    String json;
    json.reserve(2048);
    json = "{\"total\":";
    json += fs_info.totalBytes;
    json += ",\"used\":";
    json += fs_info.usedBytes;
    json += ",\"files\":[";

    Dir dir = LittleFS.openDir("/");
    bool first = true;
    while (dir.next()) {
      if (!first) json += ",";
      json += "{\"name\":\"";
      json += dir.fileName();
      json += "\",\"size\":";
      json += dir.fileSize();
      json += ",\"time\":";
      json += (uint32_t)dir.fileTime();
      json += "}";
      first = false;
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  server.on("/api/deletefile", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      if (!filename.startsWith("/")) filename = "/" + filename;
      if (LittleFS.exists(filename)) {
        LittleFS.remove(filename);
        request->send(200, "text/plain", "Geloescht");
        return;
      }
    }
    request->send(400, "text/plain", "Datei nicht gefunden");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "ESP startet neu...");
    scheduleESPReset(600);
  });

  // --- SYSTEM & DIAGNOSE ---
  server.on("/api/debug/system", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json;
    json.reserve(160);
    json = "{\"freeHeap\":";
    json += ESP.getFreeHeap();
    json += ",\"frag\":";
    json += ESP.getHeapFragmentation();
    json += ",\"maxLoopUs\":";
    json += perfMaxLoopUs;
    json += ",\"avgLoopUs\":";
    json += perfAvgLoopUs;
    json += ",\"cpuMhz\":";
    json += ESP.getCpuFreqMHz();
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/debug/packets", HTTP_GET, handleGetDebugPackets);
  server.on("/api/debug/status", HTTP_GET, handleGetDebugStatus);
  server.on("/api/debug/set", HTTP_POST, handlePostSetDebugMode);

  // --- RENNLEITUNG & ZEITNAHME ---
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

  // --- STATISTIKEN & HISTORIE ---
  server.on("/api/synctime", HTTP_POST, handlePostSyncTime);
  server.on("/api/stats", HTTP_GET, handleGetStats);
  server.on("/api/deletelap", HTTP_POST, handlePostDeleteLap);
  server.on("/api/deletedriverstats", HTTP_POST, handlePostDeleteDriverStats);
  server.on("/api/deleteallstats", HTTP_POST, handlePostDeleteAllStats);
  server.on("/api/sessions", HTTP_GET, handleGetSessions);
  server.on("/api/savesessionresult", HTTP_POST, handlePostSaveSessionResult);
  server.on("/api/deletesession", HTTP_POST, handlePostDeleteSession);
  server.on("/api/sectorconfig", HTTP_GET, handleGetSectorConfig);
  server.on("/api/setsectorconfig", HTTP_POST, handlePostSetSectorConfig);

 // --- CATCH-ALL & CAPTIVE PORTAL ---
  auto serveCaptivePage = [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/captive.html.gz")) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/captive.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    } else if (LittleFS.exists("/captive.html")) {
      request->send(LittleFS, "/captive.html", "text/html", false, captiveProcessor);
    } else {
      request->send(200, "text/html", CAPTIVE_HTML_FALLBACK, captiveProcessor);
    }
  };

  server.onNotFound([serveCaptivePage](AsyncWebServerRequest *request) {
    String url = request->url();
    if (url.startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"Not Found\"}");
      return;
    }

    String host = request->host();
    if (host == currentActiveIP.toString() || host == "laptimer.local" || host.indexOf("192.168.") >= 0) {
      if (LittleFS.exists(url)) {
        request->send(LittleFS, url, "text/html");
        return;
      } 
      if (LittleFS.exists(url + ".gz")) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, url + ".gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        return;
      } 
      if (url == "/" || url == "/index.html") {
        if (LittleFS.exists("/index.html.gz")) {
          AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
          response->addHeader("Content-Encoding", "gzip");
          request->send(response);
        } else {
          request->send(LittleFS, "/index.html", "text/html");
        }
        return;
      }
      request->send(404, "text/plain", "404: Not Found");
      return;
    }

    // Android / Windows Erkennung
    if (url.indexOf("generate_204") >= 0 || url.indexOf("gen_204") >= 0) {
      serveCaptivePage(request);
      return;
    }
    
    // Apple iOS / macOS Erkennung
    if (url.indexOf("captive.apple.com") >= 0 || url.indexOf("hotspot-detect") >= 0) {
      request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
      return;
    }

    // Standard Captive Portal Aufruf
    serveCaptivePage(request);
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

void handleNetworkTasks() {
  if (pendingReset && millis() >= resetTimer) {
    ESP.restart();
  }
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  MDNS.update();
  ws.cleanupClients();
}

void broadcastWebSocket(const String& msg) {
  if (ws.count() > 0) {
    ws.textAll(msg);
  }
}