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

// Automatische IP-Zuweisung basierend auf dem Build-Flag STATION_ID
#if STATION_ID == 1
  IPAddress local_IP(192, 168, 0, 11);
#elif STATION_ID == 2
  IPAddress local_IP(192, 168, 0, 12);
#elif STATION_ID == 3
  IPAddress local_IP(192, 168, 0, 13);
#endif

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;
  uint32_t eventId;
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
bool batteryLow = false; // Status für Unterspannung

// Nicht-blockierender Timer für das LED-Feedback
static uint32_t ledPulseEndMs = 0;
static bool ledPulseActive = false;

// Tracking für die Standzeit-Unterdrückung
static uint32_t lastSeenSignalTime[MAX_CARS + 1] = {0};
const uint32_t SECTOR_CLEARANCE_MS = 2000; 

// Ringpuffer gegen Relay-Bumerangs und Endlosschleifen
#define RELAY_CACHE_SIZE 8
static uint32_t recentRelayedIds[RELAY_CACHE_SIZE] = {0};
static uint8_t relayCacheIndex = 0;

bool isAlreadyRelayed(uint32_t eventId) {
  if (eventId == 0) return false;
  for (uint8_t i = 0; i < RELAY_CACHE_SIZE; i++) {
    if (recentRelayedIds[i] == eventId) {
      return true;
    }
  }
  recentRelayedIds[relayCacheIndex] = eventId;
  relayCacheIndex = (relayCacheIndex + 1) % RELAY_CACHE_SIZE;
  return false;
}

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  if (len == sizeof(SyncPacket)) {
    SyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));
    localSyncOffsetMs = sync.baseTimeMs - millis();
    timeSynced = true;
    return;
  }
  
  if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    #if STATION_ID < 3
    // Jede Station leitet Pakete von weiter hinten liegenden Stationen in Richtung Basis weiter
    // SZ2 leitet SZ3 weiter. SZ1 leitet SZ2 und SZ3 weiter (egal ob direkt oder via SZ2 empfangen).
    if (pkt.stationId > STATION_ID && !isAlreadyRelayed(pkt.eventId)) {
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
  pinMode(LED_SIGNAL, OUTPUT);
  
  // TX (GPIO1) als Masse für den Spannungsteiler
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);

  // GPIO0 als digitaler Eingang für die Unterspannungsprüfung
  pinMode(0, INPUT_PULLUP);

  // Kurzer Blink beim Einschalten (Active LOW: LOW = an, HIGH = aus)
  digitalWrite(LED_SIGNAL, LOW);
  delay(200);
  digitalWrite(LED_SIGNAL, HIGH);

  WiFi.mode(WIFI_STA);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);

  WiFi.begin(HOME_SSID, HOME_PASS);
  uint32_t startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_SIGNAL, LOW);
      delay(150);
      digitalWrite(LED_SIGNAL, HIGH);
      delay(150);
    }
    WiFi.disconnect();
    delay(100);
    WiFi.begin(AP_SSID, AP_PASS);
    startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000) {
      delay(250);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
  } else {
    wifiConnected = false;
    wifi_set_channel(1);
  }

  // Dauerhaft AN im Normalbetrieb
  digitalWrite(LED_SIGNAL, LOW);

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  }

  initIRDecoder();

  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  // 1. Akkuspannungsprüfung alle 10 Sekunden (BC337 Schwellenwert)
  static uint32_t lastBatteryCheckMs = 0;
  if (millis() - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = millis();
    batteryLow = (digitalRead(0) == HIGH);
  }

  // 2. Alive-Pakete an die Basis senden
  static uint32_t lastAliveMs = 0;
  if (millis() - lastAliveMs > 4000) {
    lastAliveMs = millis();
    AlivePacket alive;
    alive.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  // 3. LED-Steuerung (Normalbetrieb vs. 0.5Hz Blinken vs. 40ms IR-Feedback)
  if (batteryLow) {
    bool blinkState = (millis() / 1000) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (ledPulseActive) {
    if (millis() >= ledPulseEndMs) {
      digitalWrite(LED_SIGNAL, LOW); // Wieder an
      ledPulseActive = false;
    }
  }

  // 4. IR-Signale verarbeiten
  if (newCodeAvailable) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;
    uint8_t carIdInv      = code & 0x3F;

    // Protokoll-Integritätsprüfung (Komplement-Check)
    bool isValidSignal = ((detectedCarId ^ carIdInv) == 0x3F);

    if (isValidSignal && detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      uint32_t nowMs = millis();

      // LED-Flackern ohne delayMicroseconds
      if (!batteryLow) {
        digitalWrite(LED_SIGNAL, HIGH); // Kurz ausschalten
        ledPulseEndMs = nowMs + 40;
        ledPulseActive = true;
      }

      // Standzeit-Unterdrückung
      bool isNewPass = (nowMs - lastSeenSignalTime[detectedCarId] > SECTOR_CLEARANCE_MS);
      lastSeenSignalTime[detectedCarId] = nowMs;

      if (isNewPass) {
        SectorPacket pkt;
        pkt.stationId = STATION_ID;
        pkt.carId = detectedCarId;
        pkt.timestamp = triggerTime;
        pkt.relayed = 0;
        // Stations-ID in obere 8 Bit codieren, um Kollisionen zu verhindern
        pkt.eventId = ((uint32_t)STATION_ID << 24) | (micros() & 0x00FFFFFF);

        // Eigenes Paket im Cache registrieren, damit Bumerangs ignoriert werden
        isAlreadyRelayed(pkt.eventId);

        esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
      }
    }
  }
}