#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}
#include <esp_now_transport.h>

namespace {
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
EspNowTransport::ReceiveCallback receiveCallback = nullptr;

void onDataRecv(uint8_t* mac, uint8_t* data, uint8_t length) {
  if (receiveCallback) {
    receiveCallback(mac, data, length);
  }
}
}

namespace EspNowTransport {

int init(ReceiveCallback callback) {
  receiveCallback = callback;
  const int result = esp_now_init();
  if (result != 0) return result;

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
  return 0;
}

uint8_t channel() {
  return WiFi.channel();
}

void addOrUpdateBroadcastPeer(uint8_t peerChannel) {
  if (esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_del_peer(broadcastAddress);
  }
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, peerChannel, NULL, 0);
}

int sendBroadcast(uint8_t* data, size_t length) {
  return esp_now_send(broadcastAddress, data, static_cast<uint8_t>(length));
}

void logReady(uint8_t) {}

void logBeaconResult(int, uint8_t) {}

}
