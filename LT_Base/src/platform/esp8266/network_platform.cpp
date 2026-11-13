#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <network_platform.h>

namespace NetworkPlatform {

void disconnect(bool eraseConfig) {
  WiFi.disconnect(eraseConfig);
}

void prepareAccessPoint(bool keepStation) {
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.mode(keepStation ? WIFI_AP_STA : WIFI_AP);
  WiFi.setOutputPower(20.5);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

void prepareStation(const char* hostname) {
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(20.5);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.config(0u, 0u, 0u);
  WiFi.hostname(hostname);
}

void setStationMode() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

void beginStation(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
}

bool isStationConnected() {
  return WiFi.status() == WL_CONNECTED;
}

IPAddress stationIP() {
  return WiFi.localIP();
}

IPAddress accessPointIP() {
  return WiFi.softAPIP();
}

String stationSSID() {
  return WiFi.SSID();
}

uint8_t channel() {
  return WiFi.channel();
}

void configureAccessPoint(IPAddress localIP, IPAddress gateway, IPAddress subnet) {
  WiFi.softAPConfig(localIP, gateway, subnet);
}

void startAccessPoint(const char* ssid, const char* password, uint8_t apChannel, uint8_t hidden, uint8_t maxConnections) {
  WiFi.softAP(ssid, password, apChannel, hidden, maxConnections);
}

void stopAccessPoint(bool turnOff) {
  WiFi.softAPdisconnect(turnOff);
}

void beginMdns(const char* hostname) {
  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
  }
}

void updateMdns() {
  MDNS.update();
}

}
