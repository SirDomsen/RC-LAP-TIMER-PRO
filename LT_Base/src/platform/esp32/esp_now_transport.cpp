#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <esp_now_transport.h>

namespace {
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
EspNowTransport::ReceiveCallback receiveCallback = nullptr;

void forwardReceivedData(const uint8_t* mac, const uint8_t* data, int length) {
  if (mac && length != 10) {
    Serial.printf("[ESP-NOW RX] Paket: %d Bytes von %02X:%02X:%02X:%02X:%02X:%02X\n",
                  length, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  if (receiveCallback) {
    receiveCallback(mac, data, length);
  }
}

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void onDataRecv(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int length) {
  const uint8_t* mac = recvInfo ? recvInfo->src_addr : nullptr;
  forwardReceivedData(mac, data, length);
}
#else
void onDataRecv(const uint8_t* mac, const uint8_t* data, int length) {
  forwardReceivedData(mac, data, length);
}
#endif
}

namespace EspNowTransport {

int init(ReceiveCallback callback) {
  receiveCallback = callback;
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_country_code("DE", true);
  if ((WiFi.getMode() & WIFI_MODE_STA) != 0) {
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  }

  const esp_err_t result = esp_now_init();
  if (result != ESP_OK) return static_cast<int>(result);

  esp_now_register_recv_cb(onDataRecv);
  return 0;
}

uint8_t channel() {
  return WiFi.channel();
}

void addOrUpdateBroadcastPeer(uint8_t) {
  if (esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_del_peer(broadcastAddress);
  }

  const wifi_mode_t mode = WiFi.getMode();
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, sizeof(broadcastAddress));
  peerInfo.channel = 0; // 0 uses the current WiFi channel dynamically
  peerInfo.ifidx = (mode == WIFI_AP) ? WIFI_IF_AP : WIFI_IF_STA;
  peerInfo.encrypt = false;
  const esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW] AddPeer Fehler: 0x%X\n", result);
  }
}

int sendBroadcast(uint8_t* data, size_t length) {
  return static_cast<int>(esp_now_send(broadcastAddress, data, length));
}

void logReady(uint8_t currentChannel) {
  Serial.printf("[ESP-NOW] Bereit! Kanal: %d | STA MAC: %s\n",
                currentChannel, WiFi.macAddress().c_str());
}

void logBeaconResult(int result, uint8_t currentChannel) {
  if (result != static_cast<int>(ESP_OK)) {
    Serial.printf("[ESP-NOW] Beacon Send-Fehler: 0x%X auf Ch %d\n", result, currentChannel);
  } else {
    Serial.printf("[ESP-NOW] Beacon gesendet auf Ch %d (OK)\n", currentChannel);
  }
}

}
