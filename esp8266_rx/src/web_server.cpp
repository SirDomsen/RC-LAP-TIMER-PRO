#include "web_server.h"
#include "web_handlers.h"
#include "storage.h"
#include "esp_now_base.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

extern "C" {
  #include <espnow.h>
}

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static IPAddress currentActiveIP(192, 168, 0, 8);

extern uint32_t perfMaxLoopUs;
extern uint32_t perfAvgLoopUs;

static bool pendingReset = false;
static uint32_t resetTimer = 0;

static bool pendingGhostApDisable = false;

static bool pendingWifiSwitch = false;
static uint32_t wifiSwitchTimer = 0; // NEU: Timer für den verzögerten Kamikaze-Disconnect
static uint8_t targetWifiMode = 0;

static String cacheHeaderStr; 

void scheduleESPReset(uint16_t delayMs) {
  pendingReset = true;
  resetTimer = millis() + delayMs;
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WebSocket] Client #%u verbunden\n", client->id());
    pendingGhostApDisable = true;
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
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); 
  
  String ipStr = currentActiveIP.toString();
  String connectedSsid = WiFi.SSID();
  
  int maxSsidLen = 31 - 4 - ipStr.length();
  if (connectedSsid.length() > maxSsidLen) {
    connectedSsid = connectedSsid.substring(0, maxSsidLen);
  }
  
  String ipApName = "LT_" + connectedSsid + "-" + ipStr;
  
  WiFi.softAP(ipApName.c_str(), "Laptimer123", WiFi.channel(), 0, 1);
  digitalWrite(LED_READY, HIGH); 
  Serial.println("[WLAN] Verbunden! Geister-WLAN: " + ipApName);
}

static void setupFallbackAP() {
  WiFi.disconnect(true);
  delay(50);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  IPAddress local_IP(192, 168, 0, 8);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);
  currentActiveIP = local_IP;
  digitalWrite(LED_READY, HIGH); 
  Serial.println("[WLAN] Reiner AP-Modus aktiv! IP: " + currentActiveIP.toString());
}

static void setupWLAN() {
  digitalWrite(LED_READY, LOW); 
  WiFi.disconnect(true);
  delay(100);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  IPAddress local_IP(192, 168, 0, 8);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);
  
  WiFi.begin(HOME_SSID, HOME_PASS);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 16) {
    delay(250);
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setupGhostWLAN();
  } else {
    blinkGreenLED(3);
    digitalWrite(LED_READY, LOW);
    delay(300);

    if (hotspotSSID.length() > 0) {
      WiFi.disconnect(true);
      delay(100);
      WiFi.setPhyMode(WIFI_PHY_MODE_11G);
      WiFi.mode(WIFI_STA);
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
      WiFi.config(0u, 0u, 0u); 
      
      WiFi.begin(hotspotSSID.c_str(), hotspotPass.c_str());
      timeout = 0;
      while (WiFi.status() != WL_CONNECTED && timeout < 48) { 
        delay(250);
        timeout++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        setupGhostWLAN();
      } else {
        blinkGreenLED(5);
        setupFallbackAP();
      }
    } else {
      setupFallbackAP();
    }
  }

  if (MDNS.begin("laptimer")) {
    MDNS.addService("http", "tcp", 80);
  }

  ArduinoOTA.setHostname("LapTimer-Base");
  ArduinoOTA.begin();
}

void initNetworkAndServer() {
  setupWLAN();

  cacheHeaderStr = webCacheEnabled ? "max-age=86400" : "no-cache";

  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl(cacheHeaderStr.c_str());

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
  server.on("/debug/", HTTP_GET, serveDebugPage);
  server.on("/debug.html", HTTP_GET, serveDebugPage);

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

  // --- NEU: Korrigierte Switch-Logik (Erst Daten, dann Befehl, dann warten) ---
  server.on("/api/switchwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    uint8_t mode = 0;
    if (request->hasParam("mode")) {
      mode = request->getParam("mode")->value().toInt();
    }
    
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // 1. Wenn Modus = 2 (Hotspot), MÜSSEN wir den SZs vorher die SSID mitteilen!
    if (mode == 2) {
      HotspotSyncPacket syncPkt;
      syncPkt.magicCode = HOTSPOT_SYNC_MAGIC;
      strncpy(syncPkt.ssid, hotspotSSID.c_str(), sizeof(syncPkt.ssid) - 1);
      syncPkt.ssid[sizeof(syncPkt.ssid) - 1] = '\0';
      strncpy(syncPkt.pass, hotspotPass.c_str(), sizeof(syncPkt.pass) - 1);
      syncPkt.pass[sizeof(syncPkt.pass) - 1] = '\0';

      for(int i=0; i<3; i++) {
        esp_now_send(broadcastMac, (uint8_t *)&syncPkt, sizeof(syncPkt));
        delay(40);
      }
      delay(150); // Den SZs Zeit geben, das in den EEPROM zu brennen
    }

    // 2. Danach erst den eigentlichen Befehl zum sofortigen Umschalten senden
    struct __attribute__((packed)) WifiSwitchPacket {
      uint32_t magicCode;
      uint8_t mode; 
    };
    
    WifiSwitchPacket pkt;
    pkt.magicCode = 0x08B2347; 
    pkt.mode = mode;
    
    for(int i=0; i<3; i++) {
      esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
      delay(30);
    }
    
    // 3. Basisstation merkt sich den Wechsel, kappt aber erst in 400ms ihr WLAN!
    targetWifiMode = mode;
    wifiSwitchTimer = millis() + 400; 
    pendingWifiSwitch = true;
    
    request->send(200, "text/plain", "Switch initiated");
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
  server.on("/api/stats", HTTP_GET, handleGetStats);
  server.on("/api/deletelap", HTTP_POST, handlePostDeleteLap);
  server.on("/api/deletedriverstats", HTTP_POST, handlePostDeleteDriverStats);
  server.on("/api/deleteallstats", HTTP_POST, handlePostDeleteAllStats);
  server.on("/api/sessions", HTTP_GET, handleGetSessions);
  server.on("/api/savesessionresult", HTTP_POST, handlePostSaveSessionResult);
  server.on("/api/deletesession", HTTP_POST, handlePostDeleteSession);
  server.on("/api/sectorconfig", HTTP_GET, handleGetSectorConfig);
  server.on("/api/setsectorconfig", HTTP_POST, handlePostSetSectorConfig);
  
  server.on("/api/cacheconfig", HTTP_GET, handleGetCacheConfig);
  server.on("/api/setcacheconfig", HTTP_POST, handlePostSetCacheConfig);

  server.onNotFound([](AsyncWebServerRequest *request) {
    String url = request->url();

    if (url.startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"Not Found\"}");
      return;
    }

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
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
}

void handleNetworkTasks() {
  if (pendingReset && millis() >= resetTimer) {
    ESP.restart();
  }

  if (pendingGhostApDisable) {
    pendingGhostApDisable = false;
    if (WiFi.getMode() == WIFI_AP_STA) {
      WiFi.mode(WIFI_STA);
    }
  }
  
  // --- NEU: Basisstation kappt erst NACH dem 400ms Timer ihr WLAN ---
  if (pendingWifiSwitch && millis() >= wifiSwitchTimer) {
    pendingWifiSwitch = false;
    
    digitalWrite(LED_READY, LOW); 
    WiFi.disconnect(true);
    delay(100);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    uint32_t startMs = millis();
    
    if (targetWifiMode == 1) { 
      IPAddress local_IP(192, 168, 0, 8);
      IPAddress gateway(192, 168, 0, 1);
      IPAddress subnet(255, 255, 255, 0);
      WiFi.config(local_IP, gateway, subnet);
      WiFi.begin(HOME_SSID, HOME_PASS);
      while(WiFi.status() != WL_CONNECTED && millis() - startMs < 10000) { delay(100); }
      if (WiFi.status() == WL_CONNECTED) { setupGhostWLAN(); } 
      else { setupFallbackAP(); }
    } 
    else if (targetWifiMode == 2) { 
      WiFi.config(0u, 0u, 0u); 
      WiFi.begin(hotspotSSID.c_str(), hotspotPass.c_str());
      while(WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) { delay(100); }
      if (WiFi.status() == WL_CONNECTED) { setupGhostWLAN(); } 
      else { setupFallbackAP(); }
    } 
    else { 
      setupFallbackAP();
    }
  }

  ArduinoOTA.handle();
  MDNS.update();
  ws.cleanupClients();
}

void broadcastWebSocket(const String& msg) {
  if (ws.count() > 0) {
    ws.textAll(msg);
  }
}