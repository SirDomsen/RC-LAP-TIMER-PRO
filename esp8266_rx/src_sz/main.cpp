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
  
  // TX (GPIO1) als dauerhafte "intelligente Masse" für den Spannungsteiler konfigurieren
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);

  // GPIO0 als digitaler Eingang für die Auswertung mit internem Pullup
  pinMode(0, INPUT_PULLUP);

  // 1. Kurzer Blink beim Einschalten (Active LOW: LOW = an, HIGH = aus)[cite: 1]
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
      digitalWrite(LED_SIGNAL, LOW);  // Blinken bei Verbindungsversuch[cite: 1]
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

  // 2. Ab hier bleibt die LED permanent AN (Active LOW -> LOW)[cite: 1]
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

  // 1. Regelmäßige Akkuspannungsprüfung alle 10 Sekunden über GPIO0 (BC337 Schwellenwert)
  static uint32_t lastBatteryCheckMs = 0;
  if (millis() - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = millis();
    // Wenn die Akkuspannung sinkt, sperrt der BC337 und der Pullup zieht GPIO0 auf HIGH
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

  // 3. LED-Steuerung (Normalbetrieb vs. 0.5Hz Unterspannungs-Blinken)
  if (batteryLow) {
    // 0.5 Hz Blinken: 1000ms an / 1000ms aus (Periode = 2000ms)
    bool blinkState = (millis() / 1000) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH); // Active LOW: LOW = an
  }

  // 4. IR-Signale verarbeiten
  if (newCodeAvailable) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;

    if (detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      uint32_t nowMs = millis();

      // LED flackert kurz bei IR-Empfang (nur wenn Akku ok ist, sonst blinkt er ohnehin langsam im 0.5Hz-Takt)[cite: 1]
      if (!batteryLow) {
        digitalWrite(LED_SIGNAL, HIGH);
        delayMicroseconds(40000);
        digitalWrite(LED_SIGNAL, LOW);
      }

      // Prüfen, ob das Auto länger als SECTOR_CLEARANCE_MS KEIN Signal gesendet hat[cite: 1]
      bool isNewPass = (nowMs - lastSeenSignalTime[detectedCarId] > SECTOR_CLEARANCE_MS);

      // Letztmaligen Signal-Zeitpunkt aktualisieren[cite: 1]
      lastSeenSignalTime[detectedCarId] = nowMs;

      // Nur bei einer echten Neuanfahrt ein Paket an die Basis funken[cite: 1]
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