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

#if STATION_ID == 1
  IPAddress local_IP(192, 168, 0, 11);
#elif STATION_ID == 2
  IPAddress local_IP(192, 168, 0, 12);
#elif STATION_ID == 3
  IPAddress local_IP(192, 168, 0, 13);
#endif

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;
  uint32_t eventId;
};

struct __attribute__((packed)) SyncPacket {
  uint32_t baseTimeMs;
  uint8_t stratum;
};

struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow;
};

// Das neue geheime Paket für den Wartungsmodus
struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
};
#define OTA_MAGIC_CODE 0x07A1234

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
uint8_t currentStratum = 255;
bool wifiConnected = false;
bool batteryLow = false;

// Status Flags für den Wartungsmodus
bool triggerMaintenance = false;
bool maintenanceMode = false;

static uint32_t ledPulseEndMs = 0;
static bool ledPulseActive = false;

static uint32_t lastSeenSignalTime[MAX_CARS + 1] = {0};
const uint32_t SECTOR_CLEARANCE_MS = 2000; 

#define RELAY_CACHE_SIZE 8
static uint32_t recentRelayedIds[RELAY_CACHE_SIZE] = {0};
static uint8_t relayCacheIndex = 0;

bool isAlreadyRelayed(uint32_t eventId) {
  if (eventId == 0) return false;
  for (uint8_t i = 0; i < RELAY_CACHE_SIZE; i++) {
    if (recentRelayedIds[i] == eventId) return true;
  }
  recentRelayedIds[relayCacheIndex] = eventId;
  relayCacheIndex = (relayCacheIndex + 1) % RELAY_CACHE_SIZE;
  return false;
}

uint32_t getSynchronizedTimeMs() {
  if (!timeSynced) return millis();
  return millis() + localSyncOffsetMs;
}

// Hilfsfunktion: Gelbe LED im 3Hz Takt blinken lassen (Active LOW)
void blinkSignalLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_SIGNAL, LOW); // AN
    delay(166);
    digitalWrite(LED_SIGNAL, HIGH); // AUS
    delay(166);
  }
}

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // OTA Trigger empfangen
  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      triggerMaintenance = true;
      #if STATION_ID < 3
      // An hintere Stationen weiterleiten
      esp_now_send(broadcastMac, (uint8_t *)&ota, sizeof(ota));
      #endif
    }
    return;
  }

  // 1. Kaskadierender Time-Sync
  if (len == sizeof(SyncPacket)) {
    SyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));

    if (sync.stratum < currentStratum || !timeSynced) {
      localSyncOffsetMs = sync.baseTimeMs - millis();
      timeSynced = true;
      currentStratum = sync.stratum + 1;

      #if STATION_ID < 3
      SyncPacket forwardSync;
      forwardSync.baseTimeMs = getSynchronizedTimeMs();
      forwardSync.stratum = currentStratum;
      esp_now_send(broadcastMac, (uint8_t *)&forwardSync, sizeof(forwardSync));
      #endif
    }
    return;
  }

  // 2. Alive-Pakete
  if (len == sizeof(AlivePacket)) {
    AlivePacket alive;
    memcpy(&alive, incomingData, sizeof(alive));
    
    #if STATION_ID < 3
    if (alive.stationId > STATION_ID) {
      esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
    }
    #endif
    return;
  }
  
  // 3. Sektor-Pakete
  if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    #if STATION_ID < 3
    if (pkt.stationId > STATION_ID && !isAlreadyRelayed(pkt.eventId)) {
      pkt.relayed = 1;
      esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
    }
    #endif
  }
}

void setup() {
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  pinMode(0, INPUT_PULLUP);

  digitalWrite(LED_SIGNAL, HIGH); // LED Initial aus

  // 1. Kurzes 333ms Aufblitzen als Boot-Indikator
  digitalWrite(LED_SIGNAL, LOW);
  delay(333);
  digitalWrite(LED_SIGNAL, HIGH);
  delay(100);

  WiFi.mode(WIFI_STA);
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);

  // 2. Versuch: Heim-WLAN für Schreibtisch-Modus
  WiFi.begin(HOME_SSID, HOME_PASS);
  uint32_t startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 4000) {
    delay(200);
  }

  uint8_t activeChannel = 1;

  if (WiFi.status() == WL_CONNECTED) {
    // Zuhause: WLAN bleibt an, IP ist aktiv, OTA bereit
    wifiConnected = true;
    activeChannel = WiFi.channel();
  } else {
    // Auf der Strecke: Heim-WLAN nicht gefunden -> Renn-Modus!
    blinkSignalLed(5); // 5x Blinken signalisiert den Wechsel in den Stealth-ESP-NOW-Modus
    
    WiFi.disconnect();
    delay(100);
    wifiConnected = false;

    // Kanal der Basisstation für ESP-NOW erschnüffeln
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).equals(HOME_SSID) || WiFi.SSID(i).equals(AP_SSID)) {
        activeChannel = WiFi.channel(i);
        break;
      }
    }
    wifi_set_channel(activeChannel);
  }

  // Erfolgreich hochgefahren -> LED dauerhaft an
  digitalWrite(LED_SIGNAL, LOW); 

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, activeChannel, NULL, 0);
  }

  initIRDecoder();

  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
  ArduinoOTA.begin();
}

void loop() {
  // Switch in den Wartungsmodus, wenn Befehl über ESP-NOW kam
  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    maintenanceMode = true;
    
    // WLAN reaktivieren und heimlich in die Basis einloggen
    WiFi.mode(WIFI_STA);
    WiFi.begin(AP_SSID, AP_PASS);
    // Wir lassen es im Hintergrund verbinden, OTA greift, sobald verbunden
  }

  // OTA Anfragen verarbeiten (funktioniert nur, wenn im Heimnetz oder im Wartungsmodus)
  if (wifiConnected || maintenanceMode) {
    ArduinoOTA.handle();
  }

  static uint32_t lastBatteryCheckMs = 0;
  if (millis() - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = millis();
    batteryLow = (digitalRead(0) == HIGH);
  }

  // Alive-Paket alle 3 Sekunden senden (im Wartungsmodus pausieren wir das Senden, um Störungen zu vermeiden)
  static uint32_t lastAliveMs = 0;
  if (!maintenanceMode && millis() - lastAliveMs > 3000) {
    lastAliveMs = millis();
    AlivePacket alive;
    alive.stationId = STATION_ID;
    alive.batteryLow = batteryLow ? 1 : 0;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  // === LED STATUS LOGIK ===
  if (maintenanceMode) {
    // Höchste Prio: 3Hz Dauerblinken im Wartungsmodus
    bool blinkState = (millis() / 166) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (batteryLow) {
    // Zweithöchste Prio: 0.5Hz Blinken bei leerem Akku
    bool blinkState = (millis() / 1000) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (ledPulseActive) {
    // Dritte Prio: Flackern bei IR-Durchfahrt
    if (millis() >= ledPulseEndMs) {
      digitalWrite(LED_SIGNAL, LOW); // Zurück auf Dauer-An
      ledPulseActive = false;
    }
  } else {
    // Normalzustand: Dauer-An (Active LOW)
    digitalWrite(LED_SIGNAL, LOW);
  }

  // IR-Verarbeitung nur im Renn-Modus zulassen (nicht beim Flashen stören!)
  if (newCodeAvailable && !maintenanceMode) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;
    uint8_t carIdInv      = code & 0x3F;

    bool isValidSignal = ((detectedCarId ^ carIdInv) == 0x3F);

    if (isValidSignal && detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      uint32_t nowMs = millis();

      if (!batteryLow) {
        digitalWrite(LED_SIGNAL, HIGH); // LED Kurz aus (Impuls)
        ledPulseEndMs = nowMs + 40;
        ledPulseActive = true;
      }

      bool isNewPass = (nowMs - lastSeenSignalTime[detectedCarId] > SECTOR_CLEARANCE_MS);
      lastSeenSignalTime[detectedCarId] = nowMs;

      if (isNewPass) {
        SectorPacket pkt;
        pkt.stationId = STATION_ID;
        pkt.carId = detectedCarId;
        pkt.timestamp = triggerTime;
        pkt.relayed = 0;
        pkt.eventId = ((uint32_t)STATION_ID << 24) | (micros() & 0x00FFFFFF);

        isAlreadyRelayed(pkt.eventId);
        esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
      }
    }
  }
}