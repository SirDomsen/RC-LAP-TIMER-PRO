#include "web_server.h"
#include "web_handlers.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

static AsyncWebServer server(80);

static void setupWLAN() {
  IPAddress local_IP(192, 168, 0, 8);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet)) Serial.println("[WLAN] STA Fehler!");

  WiFi.begin(HOME_SSID, HOME_PASS);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { delay(500); timeout++; }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WLAN] IP: " + WiFi.localIP().toString());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);
  }

  // --- mDNS Konfiguration ---
  if (MDNS.begin("laptimer")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] Responder gestartet: http://laptimer.local");
  } else {
    Serial.println("[mDNS] Fehler beim Starten!");
  }

  ArduinoOTA.setHostname("LapTimer-Base");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { 
      type = "filesystem";
      LittleFS.end(); 
    }
    Serial.println("OTA Start Update: " + type);
  });
  ArduinoOTA.begin();

  digitalWrite(LED_READY, HIGH);
}

void initNetworkAndServer() {
  setupWLAN();

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Streamt /update direkt aus dem Dateisystem
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/update.html")) {
      request->send(LittleFS, "/update.html", "text/html");
    } else {
      request->send(200, "text/html", 
        "<html><body><h2>Update</h2>"
        "<form method='POST' action='/api/upload' enctype='multipart/form-data'>"
        "<input type='file' name='data'><input type='submit' value='Upload'>"
        "</form></body></html>");
    }
  });

  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "OK");
  }, handleFileUpload);

  server.on("/api/laps", HTTP_GET, handleGetLaps);
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

  server.begin();
}

void handleNetworkTasks() {
  ArduinoOTA.handle();
  MDNS.update();
}