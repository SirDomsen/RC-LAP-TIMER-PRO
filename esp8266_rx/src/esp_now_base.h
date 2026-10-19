#ifndef ESP_NOW_BASE_H
#define ESP_NOW_BASE_H

#include <Arduino.h>

// Sektor-Zeitstempel (Upstream zur Basis)
struct __attribute__((packed)) SectorPacket {
  uint8_t  stationId;
  uint8_t  carId;
  uint32_t timestamp;
  uint16_t seqNumber;
  uint8_t  ttl;
};

// Kombiniertes Beacon & Zeit-Sync (Downstream von der Basis)
#define BEACON_SYNC_MAGIC 0x53594E43 // "SYNC"
struct __attribute__((packed)) BeaconSyncPacket {
  uint32_t magicCode;
  uint32_t baseTimeMs;
  uint8_t  channel;
  uint8_t  stratum; // 0 = Basis, 1 = SZ1, 2 = SZ2, 3 = SZ3
};

// Heartbeat / Alive
struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow; // 0 = OK, 1 = Akku leer
};

// OTA Wartungsmodus-Trigger mit Ziel-Netzwerkdaten
#define OTA_MAGIC_CODE 0x07A1234
struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
  uint8_t  targetMode; // 0 = AP-Modus, 1 = Heim-WLAN
  char     ssid[32];
  char     pass[64];
};

// Hotspot Synchronisation
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

#define WIFI_SWITCH_MAGIC 0x08B2347
struct __attribute__((packed)) WifiSwitchPacket {
  uint32_t magicCode;
  uint8_t mode; 
};

void initESPNowBase();
void handleTimeSyncBroadcast();
bool isStationOnline(uint8_t stationId);
bool isStationBatteryLow(uint8_t stationId);

// Sendet den OTA-Trigger mit den Ziel-WLAN-Daten an alle SZs
void sendSZOtaTrigger(uint8_t targetMode, const char* ssid, const char* pass);

// Debug-Steuerung & Export
void setDebugMode(bool active);
bool getDebugMode();
const String& getDebugPacketsJSON();

#endif