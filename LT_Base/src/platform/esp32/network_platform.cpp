#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <network_platform.h>

namespace NetworkPlatform {

void disconnect(bool eraseConfig) {
  WiFi.disconnect(eraseConfig);
}

void prepareAccessPoint(bool keepStation) {
  WiFi.mode(keepStation ? WIFI_AP_STA : WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_17dBm);
}

void prepareStation(const char* hostname) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname);
}

void setStationMode() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
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
}

}
