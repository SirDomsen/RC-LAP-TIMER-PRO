#ifndef ESP_NOW_BASE_H
#define ESP_NOW_BASE_H

#include <Arduino.h>

struct SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;      // 0 = direkt, 1 = via Relay weitergeleitet
  uint32_t eventId;     // Eindeutige ID zur Duplikaterkennung
};

struct SyncPacket {
  uint32_t baseTimeMs;
};

struct AlivePacket {
  uint8_t stationId;
};

void initESPNowBase();
void handleTimeSyncBroadcast();
bool isStationOnline(uint8_t stationId);

// Debug-Steuerung & Export
void setDebugMode(bool active);
bool getDebugMode();
String getDebugPacketsJSON();

#endif