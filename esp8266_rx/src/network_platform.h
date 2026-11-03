#ifndef NETWORK_PLATFORM_H
#define NETWORK_PLATFORM_H

#include <Arduino.h>

namespace NetworkPlatform {
void disconnect(bool eraseConfig);
void prepareAccessPoint(bool keepStation);
void prepareStation(const char* hostname);
void setStationMode();
void beginStation(const char* ssid, const char* password);
bool isStationConnected();
IPAddress stationIP();
IPAddress accessPointIP();
String stationSSID();
uint8_t channel();
void configureAccessPoint(IPAddress localIP, IPAddress gateway, IPAddress subnet);
void startAccessPoint(const char* ssid, const char* password, uint8_t channel, uint8_t hidden, uint8_t maxConnections);
void stopAccessPoint(bool turnOff);
}

#endif
