#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
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

struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
};
#define OTA_MAGIC_CODE 0x07A1234

// --- Pakete für die Hotspot-Synchronisation ---
#define HOTSPOT_SYNC_MAGIC 0x08B2345
struct __attribute__((packed)) HotspotSyncPacket {
  uint32_t magicCode;
  char ssid[32];
  char pass[64];
};

#define HOTSPOT_ACK_MAGIC 0x08B2346
struct __attribute__((packed)) HotspotAckPacket {
  uint32_t magicCode;
  uint8_t stationId;
};

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
uint8_t currentStratum = 255;
bool wifiConnected = false;
bool batteryLow = false;

bool triggerMaintenance = false;
bool maintenanceMode = false;

// Hotspot Variablen & Flags
String dynamicSsid = "";
String dynamicPass = "";
bool pendingHotspotAck = false;
bool triggerNewHotspot = false;

#define EEPROM_MAGIC_HOTSPOT 0x5B

static uint32_t ledPulseEndMs = 0;
static bool ledPulseActive = false;

static uint32_t lastSeenSignalTime[MAX_CARS + 1] = {0};
const uint32_t SECTOR_CLEARANCE_MS = 2000; 

#define RELAY_CACHE_SIZE 8
static uint32_t recentRelayedIds[RELAY_CACHE_SIZE] = {0};
static uint8_t relayCacheIndex = 0;

void loadHotspotFromEEPROM() {
  EEPROM.begin(512);
  if (EEPROM.read(0) == EEPROM_MAGIC_HOTSPOT) {
    uint8_t ssidLen = EEPROM.read(1);
    if (ssidLen <= 32) {
      char s[33] = {0};
      for(int i=0; i<ssidLen; i++) s[i] = EEPROM.read(2+i);
      dynamicSsid = String(s);
    }
    uint8_t passLen = EEPROM.read(34);
    if (passLen <= 64) {
      char p[65] = {0};
      for(int i=0; i<passLen; i++) p[i] = EEPROM.read(35+i);
      dynamicPass = String(p);
    }
  }
}

void saveHotspotToEEPROM(String s, String p) {
  EEPROM.write(0, EEPROM_MAGIC_HOTSPOT);
  EEPROM.write(1, s.length());
  for(unsigned int i=0; i<s.length(); i++) EEPROM.write(2+i, s[i]);
  EEPROM.write(34, p.length());
  for(unsigned int i=0; i<p.length(); i++) EEPROM.write(35+i, p[i]);
  EEPROM.commit();
  dynamicSsid = s;
  dynamicPass = p;
}

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

void blinkSignalLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_SIGNAL, LOW); // AN
    delay(166);
    digitalWrite(LED_SIGNAL, HIGH); // AUS
    delay(166);
  }
}

// Hybride Verbindungssequenz (Statisch daheim, DHCP im Hotspot)
void connectWlanSequence() {
  digitalWrite(LED_SIGNAL, HIGH); // LED Initial aus
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  
  // 1. Heim-WLAN suchen (MIT fester IP für OTA)
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);
  
  WiFi.begin(HOME_SSID, HOME_PASS);
  uint32_t startMs = millis();
  
  while(WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) {
    delay(100);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifi_set_channel(WiFi.channel());
    digitalWrite(LED_SIGNAL, LOW); // Erfolg -> Dauerhaft an
    return;
  }
  
  // Heim-WLAN fehlgeschlagen
  blinkSignalLed(3);
  digitalWrite(LED_SIGNAL, HIGH); 
  
  // 2. Dynamischen Hotspot versuchen (falls vorhanden)
  if (dynamicSsid.length() > 0) {
    WiFi.disconnect(true);
    delay(100);
    
    // DHCP für das Handy aktivieren!
    WiFi.config(0u, 0u, 0u); 
    
    WiFi.begin(dynamicSsid.c_str(), dynamicPass.c_str());
    startMs = millis();
    
    while(WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) {
      delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      wifi_set_channel(WiFi.channel());
      digitalWrite(LED_SIGNAL, LOW); // Erfolg -> Dauerhaft an
      return;
    }
    
    // Hotspot fehlgeschlagen
    blinkSignalLed(5);
    digitalWrite(LED_SIGNAL, HIGH);
  }
  
  // 3. Fallback: Offline-Rennen (Kanal via Scan suchen, aber NICHT verbinden!)
  WiFi.disconnect(true);
  delay(100);
  wifiConnected = false;
  int activeChannel = 1;
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).equals(HOME_SSID) || WiFi.SSID(i).equals(AP_SSID) || (dynamicSsid.length() > 0 && WiFi.SSID(i).equals(dynamicSsid))) {
      activeChannel = WiFi.channel(i);
      break;
    }
  }
  wifi_set_channel(activeChannel);
  digitalWrite(LED_SIGNAL, LOW); // Scan beendet -> Dauerhaft an
}

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // Hotspot Sync empfangen
  if (len == sizeof(HotspotSyncPacket)) {
    HotspotSyncPacket syncPkt;
    memcpy(&syncPkt, incomingData, sizeof(syncPkt));
    if (syncPkt.magicCode == HOTSPOT_SYNC_MAGIC) {
      saveHotspotToEEPROM(String(syncPkt.ssid), String(syncPkt.pass));
      pendingHotspotAck = true;
      triggerNewHotspot = true;
      
      #if STATION_ID < 3
      esp_now_send(broadcastMac, (uint8_t *)&syncPkt, sizeof(syncPkt));
      #endif
    }
    return;
  }

  // OTA Trigger empfangen
  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      triggerMaintenance = true;
      #if STATION_ID < 3
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
  digitalWrite(LED_SIGNAL, LOW);
  delay(333);
  digitalWrite(LED_SIGNAL, HIGH);
  delay(100);

  loadHotspotFromEEPROM();

  connectWlanSequence(); 

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  }

  initIRDecoder();

  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
  ArduinoOTA.begin();
}

void loop() {
  if (pendingHotspotAck) {
    pendingHotspotAck = false;
    HotspotAckPacket ack;
    ack.magicCode = HOTSPOT_ACK_MAGIC;
    ack.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));
    delay(50); // Kurz warten vor dem WLAN Neustart
  }

  if (triggerNewHotspot) {
    triggerNewHotspot = false;
    connectWlanSequence();
  }

  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    maintenanceMode = true;
    connectWlanSequence(); 
  }

  if (wifiConnected || maintenanceMode) {
    ArduinoOTA.handle();
  }

  static uint32_t lastBatteryCheckMs = 0;
  if (millis() - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = millis();
    batteryLow = (digitalRead(0) == HIGH);
  }

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
    bool blinkState = (millis() / 166) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (batteryLow) {
    bool blinkState = (millis() / 1000) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (ledPulseActive) {
    if (millis() >= ledPulseEndMs) {
      digitalWrite(LED_SIGNAL, LOW); // Zurück auf Dauer-An
      ledPulseActive = false;
    }
  } else {
    digitalWrite(LED_SIGNAL, LOW);
  }

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
        digitalWrite(LED_SIGNAL, HIGH); 
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