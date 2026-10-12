#ifndef ESP_NOW_BASE_H
#define ESP_NOW_BASE_H

#include <Arduino.h>

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

void initESPNowBase();
void handleTimeSyncBroadcast();
bool isStationOnline(uint8_t stationId);

#endif