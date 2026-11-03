#ifndef ESP_NOW_TRANSPORT_H
#define ESP_NOW_TRANSPORT_H

#include <Arduino.h>

namespace EspNowTransport {
using ReceiveCallback = void (*)(const uint8_t* mac, const uint8_t* data, int length);

int init(ReceiveCallback callback);
uint8_t channel();
void addOrUpdateBroadcastPeer(uint8_t channel);
int sendBroadcast(uint8_t* data, size_t length);
void logReady(uint8_t channel);
void logBeaconResult(int result, uint8_t channel);
}

#endif
