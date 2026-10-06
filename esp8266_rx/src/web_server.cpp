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
DNSServer dnsServer;

const byte DNS_PORT = 53;
static IPAddress currentActiveIP(192, 168, 0, 8);

// Fallback HTML für das Captive Portal (falls captive.html nicht in LittleFS liegt)
const char CAPTIVE_HTML_FALLBACK[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RC Laptimer Pro</title>
  <style>
    body { font-family: sans-serif; background: #121212; color: #fff; text-align: center; padding: 40px 20px; }
    .btn { display: block; padding: 16px; background: #00e676; color: #000; text-decoration: none; font-weight: bold; border-radius: 8px; margin-top: 20px; }
  </style>
</head>
<body>
  <h2>RC Laptimer Pro</h2>
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

// Template Processor ersetzt %LAPTIMER_IP% dynamisch durch die reale IP
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

  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet)) Serial.println("[WLAN] STA Fehler!");

  WiFi.begin(HOME_SSID, HOME_PASS);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { delay(500); timeout++; }

  if (WiFi.status() == WL_CONNECTED) {
    currentActiveIP = WiFi.localIP();
    Serial.println("[WLAN] IP: " + currentActiveIP.toString());
  } else {
    // Access Point starten, wenn keine Verbindung zum Heim-WLAN besteht
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);
    currentActiveIP = local_IP;

    // DNS-Server starten: Fängt alle Domain-Abfragen (*) ab und leitet sie auf den Laptimer um
    dnsServer.start(DNS_PORT, "*", currentActiveIP);
    Serial.println("[DNS] Captive Portal DNS-Server gestartet.");
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

  // 1. Statische Dateien aus LittleFS servieren
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

  // --- API Endpunkte ---
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

  // --- Captive Portal & Connectivity-Checks Handler ---
  server.onNotFound([](AsyncWebServerRequest *request) {
    String url = request->url();

    // WICHTIG: API-Aufrufe dürfen NIEMALS im Captive Portal landen!
    if (url.startsWith("/api/")) {
      request->send(404, "application/json", "{\"error\":\"Not Found\"}");
      return;
    }

    String host = request->host();

    // A) Wenn die Anfrage an die IP-Adresse des ESP oder laptimer.local geht:
    if (host == currentActiveIP.toString() || host == "laptimer.local" || host.indexOf("192.168.") >= 0) {
      if (LittleFS.exists(url)) {
        request->send(LittleFS, url, "text/html");
      } else if (url == "/" || url == "/index.html") {
        request->send(LittleFS, "/index.html", "text/html");
      } else {
        request->send(404, "text/plain", "404: Not Found");
      }
      return;
    }

    // B) Android Connectivity Probe
    if (url.indexOf("generate_204") >= 0 || url.indexOf("gen_204") >= 0) {
      if (LittleFS.exists("/captive.html")) {
        request->send(LittleFS, "/captive.html", "text/html", false, captiveProcessor);
      } else {
        request->send(200, "text/html", CAPTIVE_HTML_FALLBACK, captiveProcessor);
      }
      return;
    }
    
    // C) Apple Connectivity Probe
    if (url.indexOf("captive.apple.com") >= 0 || url.indexOf("hotspot-detect") >= 0) {
      request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
      return;
    }

    // D) Alle sonstigen Captive-Portal Aufrufe
    if (LittleFS.exists("/captive.html")) {
      request->send(LittleFS, "/captive.html", "text/html", false, captiveProcessor);
    } else {
      request->send(200, "text/html", CAPTIVE_HTML_FALLBACK, captiveProcessor);
    }
  });

  server.begin();
}

void handleNetworkTasks() {
  dnsServer.processNextRequest(); // Verarbeitet DNS-Anfragen für das Captive Portal
  ArduinoOTA.handle();
  MDNS.update();
}