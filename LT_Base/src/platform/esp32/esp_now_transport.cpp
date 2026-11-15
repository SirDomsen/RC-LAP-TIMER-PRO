#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <esp_now_transport.h>

namespace {
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
EspNowTransport::ReceiveCallback receiveCallback = nullptr;

constexpr size_t MAX_DYNAMIC_PEERS = 3;
struct DynamicPeerSlot {
  uint8_t targetMac[6];
  uint32_t lastSeenMs;
  bool occupied;
  uint8_t registeredMac[6];
  bool registered;
  wifi_interface_t registeredIfidx;
};

DynamicPeerSlot dynamicPeers[MAX_DYNAMIC_PEERS] = {};
portMUX_TYPE dynamicPeerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool pendingPeerSync = false;
bool broadcastPeerRegistered = false;
wifi_interface_t activeBroadcastIfidx = WIFI_IF_STA;

wifi_interface_t currentEspNowInterface() {
  return ((WiFi.getMode() & WIFI_MODE_AP) != 0) ? WIFI_IF_AP : WIFI_IF_STA;
}

bool isValidUnicastMac(const uint8_t* mac) {
  if (!mac) return false;
  if ((mac[0] & 0x01) != 0) return false; // Broadcast/Multicast ausschliessen
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) return true;
  }
  return false;
}

void recordSeenPeerMac(const uint8_t* mac) {
  if (!isValidUnicastMac(mac)) return;
  const uint32_t now = millis();

  portENTER_CRITICAL(&dynamicPeerMux);
  int freeIdx = -1;
  int oldestIdx = 0;
  uint32_t oldestAge = 0;

  for (size_t i = 0; i < MAX_DYNAMIC_PEERS; i++) {
    if (dynamicPeers[i].occupied) {
      if (memcmp(dynamicPeers[i].targetMac, mac, 6) == 0) {
        dynamicPeers[i].lastSeenMs = now;
        portEXIT_CRITICAL(&dynamicPeerMux);
        return;
      }
      uint32_t age = now - dynamicPeers[i].lastSeenMs;
      if (age >= oldestAge) {
        oldestAge = age;
        oldestIdx = static_cast<int>(i);
      }
    } else if (freeIdx < 0) {
      freeIdx = static_cast<int>(i);
    }
  }

  const int targetIdx = (freeIdx >= 0) ? freeIdx : oldestIdx;
  memcpy(dynamicPeers[targetIdx].targetMac, mac, 6);
  dynamicPeers[targetIdx].lastSeenMs = now;
  dynamicPeers[targetIdx].occupied = true;
  pendingPeerSync = true;
  portEXIT_CRITICAL(&dynamicPeerMux);
}

void forwardReceivedData(const uint8_t* mac, const uint8_t* data, int length) {
  // Ignore 10-byte BeaconSyncPacket frames so external ESPs cannot evict SZs from the 3-slot LRU.
  if (length != 10) {
    recordSeenPeerMac(mac);
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

static esp_err_t upsertPeer(const uint8_t* mac, wifi_interface_t ifidx) {
  if (esp_now_is_peer_exist(mac)) {
    esp_now_del_peer(mac);
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0; // 0 uses the current WiFi channel dynamically
  peerInfo.ifidx = ifidx;
  peerInfo.encrypt = false;
  return esp_now_add_peer(&peerInfo);
}

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
  const wifi_interface_t ifidx = currentEspNowInterface();
  const esp_err_t result = upsertPeer(broadcastAddress, ifidx);
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW] AddPeer Fehler: 0x%X\n", result);
  } else {
    activeBroadcastIfidx = ifidx;
    broadcastPeerRegistered = true;
  }
}

void maintain() {
  const wifi_interface_t ifidx = currentEspNowInterface();
  const bool ifidxChanged = (!broadcastPeerRegistered || activeBroadcastIfidx != ifidx);
  if (ifidxChanged) {
    addOrUpdateBroadcastPeer(0);
  }

  if (!pendingPeerSync && !ifidxChanged) return;

  portENTER_CRITICAL(&dynamicPeerMux);
  pendingPeerSync = false;
  portEXIT_CRITICAL(&dynamicPeerMux);

  for (size_t i = 0; i < MAX_DYNAMIC_PEERS; i++) {
    uint8_t target[6];
    bool occupied = false;
    portENTER_CRITICAL(&dynamicPeerMux);
    occupied = dynamicPeers[i].occupied;
    if (occupied) {
      memcpy(target, dynamicPeers[i].targetMac, 6);
    }
    portEXIT_CRITICAL(&dynamicPeerMux);

    if (!occupied) continue;

    const bool macChanged = !dynamicPeers[i].registered || memcmp(dynamicPeers[i].registeredMac, target, 6) != 0;
    const bool peerIfChanged = (dynamicPeers[i].registeredIfidx != ifidx);
    if (!macChanged && !peerIfChanged) continue;

    if (dynamicPeers[i].registered && memcmp(dynamicPeers[i].registeredMac, target, 6) != 0) {
      if (esp_now_is_peer_exist(dynamicPeers[i].registeredMac)) {
        esp_now_del_peer(dynamicPeers[i].registeredMac);
      }
    }

    const esp_err_t res = upsertPeer(target, ifidx);
    if (res == ESP_OK) {
      memcpy(dynamicPeers[i].registeredMac, target, 6);
      dynamicPeers[i].registered = true;
      dynamicPeers[i].registeredIfidx = ifidx;
      Serial.printf("[ESP-NOW] Peer[%u] aktiv: %02X:%02X:%02X:%02X:%02X:%02X (ifidx=%d)\n",
                    static_cast<unsigned>(i),
                    target[0], target[1], target[2], target[3], target[4], target[5],
                    static_cast<int>(ifidx));
    } else {
      Serial.printf("[ESP-NOW] Peer[%u] Add-Fehler: 0x%X\n", static_cast<unsigned>(i), res);
    }
  }
}

int sendBroadcast(uint8_t* data, size_t length) {
  return static_cast<int>(esp_now_send(broadcastAddress, data, length));
}

void logReady(uint8_t currentChannel) {
  Serial.printf("[ESP-NOW] Bereit! Kanal: %d | STA MAC: %s | AP MAC: %s | ifidx=%d\n",
                currentChannel,
                WiFi.macAddress().c_str(),
                WiFi.softAPmacAddress().c_str(),
                static_cast<int>(currentEspNowInterface()));
}

void logBeaconResult(int result, uint8_t currentChannel) {
  if (result != static_cast<int>(ESP_OK)) {
    Serial.printf("[ESP-NOW] Beacon Send-Fehler: 0x%X auf Ch %d\n", result, currentChannel);
  } else {
    Serial.printf("[ESP-NOW] Beacon gesendet auf Ch %d (OK)\n", currentChannel);
  }
}

}
