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

#endif
