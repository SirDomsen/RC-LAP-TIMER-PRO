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

// Tracking für die Standzeit-Unterdrückung
static uint32_t lastSeenSignalTime[MAX_CARS + 1] = {0};
const uint32_t SECTOR_CLEARANCE_MS = 2000; // Mindestens 2 Sekunden Pause im Signal, um einen neuen Durchgang zu werten

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  if (len == sizeof(SyncPacket)) {
    SyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));
    localSyncOffsetMs = sync.baseTimeMs - millis();
    timeSynced = true;
  }
  else if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    #if STATION_ID == 2
    if (pkt.stationId == 3 && pkt.relayed == 0) {
      pkt.relayed = 1;
      esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
    }
    #endif

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
  pinMode(LED_SIGNAL, OUTPUT);
  digitalWrite(LED_SIGNAL, LOW); // Bereitschaft (Active LOW)

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
      digitalWrite(LED_SIGNAL, HIGH);
      delay(150);
      digitalWrite(LED_SIGNAL, LOW);
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

  static uint32_t lastAliveMs = 0;
  if (millis() - lastAliveMs > 4000) {
    lastAliveMs = millis();
    AlivePacket alive;
    alive.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  if (newCodeAvailable) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;

    if (detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      uint32_t nowMs = millis();

      // LED-Flackern bei JEDEM eintreffenden, validen IR-Signal (Ideal für spätere Buzzer-Anbindung)
      digitalWrite(LED_SIGNAL, HIGH);
      delayMicroseconds(40000);
      digitalWrite(LED_SIGNAL, LOW);

      // Prüfen, ob das Auto länger als SECTOR_CLEARANCE_MS KEIN Signal gesendet hat 
      bool isNewPass = (nowMs - lastSeenSignalTime[detectedCarId] > SECTOR_CLEARANCE_MS);

      // Letztmaligen Signal-Zeitpunkt aktualisieren
      lastSeenSignalTime[detectedCarId] = nowMs;

      // Nur bei einer echten Neuanfahrt ein Paket an die Basis funken
      if (isNewPass) {
        SectorPacket pkt;
        pkt.stationId = STATION_ID;
        pkt.carId = detectedCarId;
        pkt.timestamp = triggerTime;
        pkt.relayed = 0;
        pkt.eventId = micros();

        esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
      }
    }
  }
}