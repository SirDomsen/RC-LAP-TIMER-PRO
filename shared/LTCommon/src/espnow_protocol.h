#ifndef LT_ESPNOW_PROTOCOL_H
#define LT_ESPNOW_PROTOCOL_H

#include <stdint.h>

struct __attribute__((packed)) SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint16_t seqNumber;
  uint8_t ttl;
};

#define BEACON_SYNC_MAGIC 0x53594E43 // "SYNC"
struct __attribute__((packed)) BeaconSyncPacket {
  uint32_t magicCode;
  uint32_t baseTimeMs;
  uint8_t channel;
  uint8_t stratum;
};

struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow;
  uint8_t isUnicast;
};

#define OTA_MAGIC_CODE 0x07A1234
struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
  uint8_t targetMode;
  char ssid[32];
  char pass[64];
};

#define OTA_ACK_MAGIC 0x07A1235
struct __attribute__((packed)) OtaAckPacket {
  uint32_t magicCode;
  uint8_t stationId;
};

#define HOTSPOT_SYNC_MAGIC 0x08B2345
struct __attribute__((packed)) HotspotSyncPacket {
  uint32_t magicCode;
  char ssid[32];
  char pass[64];
};

#define HOTSPOT_ACK_MAGIC 0x08B2346
struct __attribute__((packed)) HotspotAckPacket {
  uint32_t magicCode;
  uint8_t stationId;
};

#define WIFI_SWITCH_MAGIC 0x08B2347
struct __attribute__((packed)) WifiSwitchPacket {
  uint32_t magicCode;
  uint8_t mode;
};

#endif
