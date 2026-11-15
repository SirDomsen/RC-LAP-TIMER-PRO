#ifndef ESP_NOW_BASE_H
#define ESP_NOW_BASE_H

#include <Arduino.h>
#include <espnow_protocol.h>

void initESPNowBase();
void handleTimeSyncBroadcast();
bool isStationOnline(uint8_t stationId);
bool isStationBatteryLow(uint8_t stationId);
bool isStationUnicast(uint8_t stationId);

// Sendet den OTA-Trigger mit den Ziel-WLAN-Daten an alle SZs
void sendSZOtaTrigger(uint8_t targetMode, const char* ssid, const char* pass);

// Debug-Steuerung & Export
void setDebugMode(bool active);
bool getDebugMode();
const String& getDebugPacketsJSON();

extern uint32_t espNowRxPackets;
extern uint32_t espNowRxBytes;
extern uint32_t espNowTxPackets;
extern uint32_t espNowTxErrors;
extern uint32_t espNowDupDrops;

#endif
