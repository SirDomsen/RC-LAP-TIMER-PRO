#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "ir_decoder.h"

extern "C" {
  #include <espnow.h>
}

#ifndef u8
typedef uint8_t u8;
#endif

// --- KONFIGURATION FÜR DIESE STATION ---
#define STATION_ID 2  // SZ1 (Für SZ2 auf 2 und SZ3 auf 3 stellen)

#if STATION_ID == 1
  IPAddress local_IP(192, 168, 0, 11);
#elif STATION_ID == 2
  IPAddress local_IP(192, 168, 0, 12);
#elif STATION_ID == 3
  IPAddress local_IP(192, 168, 0, 13);
#endif

// Universelle Broadcast-Adresse anstelle einer festen MAC
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


struct SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;
};

struct SyncPacket {
  uint32_t baseTimeMs;
};

struct AlivePacket {
  uint8_t stationId;
};

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
bool wifiConnected = false;

// LED-Blink-Steuerung (non-blocking)
uint32_t ledPulseStart = 0;
bool ledIsBlinking = false;

void onDataSent(u8 *mac_addr, u8 sendStatus) {
  // Frei für Callbacks
}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // 1. Zeit-Sync Paket von der Basis verarbeiten (gilt für ALLE SZ)
  if (len == sizeof(SyncPacket)) {
    SyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));
    localSyncOffsetMs = sync.baseTimeMs - millis();
    timeSynced = true;
  }
  
  // 2. Relay-Logik
  else if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    // SZ2 horcht nach SZ3 und leitet weiter
    #if STATION_ID == 2
    if (pkt.stationId == 3 && pkt.relayed == 0) {
      pkt.relayed = 1;
      esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
    }
    #endif

    // Falls SZ1 z.B. auch noch als Relay für SZ2 dienen müsste (bei extremer Distanz):
    #if STATION_ID == 1
    if (pkt.stationId == 2 && pkt.relayed == 0) {
      pkt.relayed = 1;
      esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
    }
    #endif
  }
}

uint32_t getSynchronizedTimeMs() {
  if (!timeSynced) return millis();
  return millis() + localSyncOffsetMs;
}

void setup() {
  // Pins konfigurieren (nutzt LED_SIGNAL aus der config.h)
  pinMode(LED_SIGNAL, OUTPUT);
  digitalWrite(LED_SIGNAL, HIGH); // LED zunächst aus beim Booten (Active LOW)

  WiFi.mode(WIFI_STA);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);

  // --- 1. VERSUCH: Heim-WLAN ---
  WiFi.begin(HOME_SSID, HOME_PASS);
  uint32_t startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
    delay(250);
  }

  // --- WENN HEIM-WLAN FEHLT: 3x kurz blinken ---
  if (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_SIGNAL, LOW);  // LED an (Active LOW)
      delay(150);
      digitalWrite(LED_SIGNAL, HIGH); // LED aus
      delay(150);
    }

    // --- 2. VERSUCH: Basis-AP suchen ---
    WiFi.disconnect();
    delay(100);
    WiFi.begin(AP_SSID, AP_PASS); // AP-Zugangsdaten der Basis
    
    startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
      delay(250);
    }
  }

  // Status auswerten
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    digitalWrite(LED_SIGNAL, LOW); // LED dauerhaft an (WLAN steht – entweder Heim oder Basis-AP!)
  } else {
    wifiConnected = false;
    wifi_set_channel(1);           // Reiner ESP-NOW Fallback auf Kanal 1
    digitalWrite(LED_SIGNAL, HIGH); // LED bleibt aus (weder Heim noch Basis-AP gefunden)
  }

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
  }

  // IR-Decoder initialisieren
  initIRDecoder();

  // OTA Setup (funktioniert sowohl im Heim-WLAN als auch im Basis-AP!)
  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  // LED-Impuls nach 40ms beenden und je nach WLAN-Status zurückschalten
  if (ledIsBlinking && (millis() - ledPulseStart > 40)) {
    digitalWrite(LED_SIGNAL, wifiConnected ? LOW : HIGH); 
    ledIsBlinking = false;
  }

  // Alle 4 Sekunden ein Heartbeat/Lebenszeichen an die Basis senden
  static uint32_t lastAliveMs = 0;
  if (millis() - lastAliveMs > 4000) {
    lastAliveMs = millis();
    
    AlivePacket alive;
    alive.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  // Prüfen, ob der Decoder ein valides IR-Paket erkannt hat
  if (newCodeAvailable) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    uint8_t detectedCarId = (uint8_t)(receivedCode & 0xFF); 

    SectorPacket pkt;
    pkt.stationId = STATION_ID;
    pkt.carId = detectedCarId;
    pkt.timestamp = triggerTime;
    pkt.relayed = 0;

    // Paket an Basis senden
    esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));

    // Blaue Onboard-LED kurz aufblitzten lassen (High-Puls, da Active LOW -> kurz aus)
    digitalWrite(LED_SIGNAL, HIGH); 
    ledPulseStart = millis();
    ledIsBlinking = true;
  }
}