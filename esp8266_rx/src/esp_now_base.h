#ifndef ESP_NOW_BASE_H
#define ESP_NOW_BASE_H

#include <Arduino.h>

struct __attribute__((packed)) SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;
  uint32_t eventId;
};

struct __attribute__((packed)) SyncPacket {
  uint32_t baseTimeMs;
  uint8_t stratum; // 0 = Basis, 1 = SZ1, 2 = SZ2
};

struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow; // 0 = OK, 1 = Akku leer
};

void initESPNowBase();
void handleTimeSyncBroadcast();
bool isStationOnline(uint8_t stationId);
bool isStationBatteryLow(uint8_t stationId);

// Debug-Steuerung & Export
void setDebugMode(bool active);
bool getDebugMode();
String getDebugPacketsJSON();

#endif