#include "esp_now_base.h"
#include "lap_logic.h"
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}

#ifndef u8
typedef uint8_t u8;
#endif

static unsigned long sz1LastSeen = 0;
static unsigned long sz2LastSeen = 0;
static unsigned long sz3LastSeen = 0;

static bool sz1BatLow = false;
static bool sz2BatLow = false;
static bool sz3BatLow = false;

static bool sz1Unicast = false;
static bool sz2Unicast = false;
static bool sz3Unicast = false;

static bool debugModeEnabled = false;

#define MAX_DEBUG_PACKETS 8
struct DebugPacketLog {
  uint32_t timestampMs;
  uint8_t stationId;
  uint8_t carId;
  uint32_t rawData;
};

static DebugPacketLog debugPackets[MAX_DEBUG_PACKETS];
static uint8_t debugPacketIndex = 0;
static uint8_t debugPacketCount = 0;

void setDebugMode(bool active) {
  debugModeEnabled = active;
  if (!active) {
    debugPacketCount = 0;
    debugPacketIndex = 0;
  }
}

bool getDebugMode() {
  return debugModeEnabled;
}

void logDebugPacket(uint8_t stationId, uint8_t carId, uint32_t rawData) {
  if (!debugModeEnabled) return;
  
  debugPackets[debugPacketIndex].timestampMs = millis();
  debugPackets[debugPacketIndex].stationId = stationId;
  debugPackets[debugPacketIndex].carId = carId;
  debugPackets[debugPacketIndex].rawData = rawData;

  debugPacketIndex = (debugPacketIndex + 1) % MAX_DEBUG_PACKETS;
  if (debugPacketCount < MAX_DEBUG_PACKETS) debugPacketCount++;
}

const String& getDebugPacketsJSON() {
  static String json;
  static bool memoryReserved = false;
  
  if (!memoryReserved) {
    json.reserve(512);
    memoryReserved = true;
  }
  
  json = "[";

  if (debugModeEnabled && debugPacketCount > 0) {
    uint8_t start = (debugPacketCount < MAX_DEBUG_PACKETS) ? 0 : debugPacketIndex;
    for (uint8_t i = 0; i < debugPacketCount; i++) {
      uint8_t idx = (start + i) % MAX_DEBUG_PACKETS;
      if (i > 0) json += ",";
      json += "{\"time\":";
      json += debugPackets[idx].timestampMs;
      json += ",\"st\":";
      json += debugPackets[idx].stationId;
      json += ",\"car\":";
      json += debugPackets[idx].carId;
      json += ",\"raw\":";
      json += debugPackets[idx].rawData;
      json += "}";
    }
  }
  json += "]";
  return json;
}

#define RECENT_EVENTS_MAX 24
struct RecentEvent {
  uint32_t eventId;
  unsigned long receivedAt;
};
static RecentEvent recentEvents[RECENT_EVENTS_MAX];
static uint8_t recentEventIndex = 0;

bool isDuplicateEvent(uint32_t eventId) {
  if (eventId == 0) return false;
  unsigned long now = millis();
  for (int i = 0; i < RECENT_EVENTS_MAX; i++) {
    if (recentEvents[i].eventId == eventId && (now - recentEvents[i].receivedAt < 2500)) {
      return true;
    }
  }
  recentEvents[recentEventIndex] = {eventId, now};
  recentEventIndex = (recentEventIndex + 1) % RECENT_EVENTS_MAX;
  return false;
}

void onESPNowDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // 1a. Hotspot ACK abfangen
  if (len == sizeof(HotspotAckPacket)) {
    HotspotAckPacket ack;
    memcpy(&ack, incomingData, sizeof(ack));
    if (ack.magicCode == HOTSPOT_ACK_MAGIC) {
      
    }
    return;
  }

  // 1b. OTA ACK (Arm-and-Confirm) abfangen
  if (len == sizeof(OtaAckPacket)) {
    OtaAckPacket oAck;
    memcpy(&oAck, incomingData, sizeof(oAck));
    if (oAck.magicCode == OTA_ACK_MAGIC) {
      logDebugPacket(oAck.stationId, 0, 0x07A);
    }
    return;
  }

  // 2. Alive / Heartbeat verarbeiten
  if (len == sizeof(AlivePacket)) {
    AlivePacket packet;
    memcpy(&packet, incomingData, sizeof(packet));
    
    unsigned long now = millis();
    if (packet.stationId == 1) { sz1LastSeen = now; sz1BatLow = (packet.batteryLow != 0); sz1Unicast = (packet.isUnicast != 0); }
    if (packet.stationId == 2) { sz2LastSeen = now; sz2BatLow = (packet.batteryLow != 0); sz2Unicast = (packet.isUnicast != 0); }
    if (packet.stationId == 3) { sz3LastSeen = now; sz3BatLow = (packet.batteryLow != 0); sz3Unicast = (packet.isUnicast != 0); }

    logDebugPacket(packet.stationId, 0, packet.batteryLow);
    return;
  }

  // 3. Sektor-Paket verarbeiten
  if (len == sizeof(SectorPacket)) {
    SectorPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    unsigned long now = millis();
    if (packet.stationId == 1) sz1LastSeen = now;
    if (packet.stationId == 2) sz2LastSeen = now;
    if (packet.stationId == 3) sz3LastSeen = now;

    // Eindeutige Event-ID aus Stations-ID und lokaler Sequenznummer
    uint32_t eventId = ((uint32_t)packet.stationId << 16) | packet.seqNumber;

    // Duplikate filtern
    if (isDuplicateEvent(eventId)) {
      return; 
    }

    logDebugPacket(packet.stationId, packet.carId, packet.timestamp);
    processSectorPacket(packet.stationId, packet.carId, packet.timestamp);
  }
}

void initESPNowBase() {
  if (esp_now_init() != 0) {
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);

  uint8_t currentCh = WiFi.channel();
  if (currentCh == 0) currentCh = 1;

  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, currentCh, NULL, 0);
  }
}

void handleTimeSyncBroadcast() {
  static uint32_t lastSyncTime = 0;
  // Beacon-Frequenz auf 500 ms verdoppelt für blitzschnellen Kanal-Lock & Ausfallsicherheit
  if (millis() - lastSyncTime >= 500UL) {
    lastSyncTime = millis();
    
    uint8_t currentCh = WiFi.channel();
    if (currentCh == 0) currentCh = 1;

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Funkkanal des Broadcast-Peers dynamisch nachführen
    static uint8_t registeredChannel = 0;
    if (currentCh != registeredChannel) {
      if (esp_now_is_peer_exist(broadcastAddress)) {
        esp_now_del_peer(broadcastAddress);
      }
      esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, currentCh, NULL, 0);
      registeredChannel = currentCh;
    }
    
    BeaconSyncPacket syncPkt;
    syncPkt.magicCode = BEACON_SYNC_MAGIC;
    syncPkt.baseTimeMs = millis();
    syncPkt.channel = currentCh;
    syncPkt.stratum = 0;

    esp_now_send(broadcastAddress, (uint8_t *)&syncPkt, sizeof(syncPkt));
  }
}

void sendSZOtaTrigger(uint8_t targetMode, const char* ssid, const char* pass) {
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // 1. NEUER TRIGGER (101 Bytes) für die Zukunft (inklusive Ziel-WLAN Zugangsdaten)
  OtaTriggerPacket pkt;
  pkt.magicCode = OTA_MAGIC_CODE;
  pkt.targetMode = targetMode;
  memset(pkt.ssid, 0, sizeof(pkt.ssid));
  memset(pkt.pass, 0, sizeof(pkt.pass));
  strncpy(pkt.ssid, ssid, sizeof(pkt.ssid) - 1);
  strncpy(pkt.pass, pass, sizeof(pkt.pass) - 1);

  for (int i = 0; i < 3; i++) {
    esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(pkt));
    delay(25);
  }
  
  delay(50);

  // 2. LEGACY TRIGGER (4 Bytes) - Rettungsanker für die aktuell geflashten SZs
  struct __attribute__((packed)) LegacyOtaPacket {
    uint32_t magicCode;
  };
  LegacyOtaPacket legacyPkt;
  legacyPkt.magicCode = OTA_MAGIC_CODE;

  for (int i = 0; i < 3; i++) {
    esp_now_send(broadcastAddress, (uint8_t *)&legacyPkt, sizeof(legacyPkt));
    delay(25);
  }
}

bool isStationOnline(uint8_t stationId) {
  unsigned long now = millis();
  unsigned long timeout = 7000; 

  if (stationId == 1) return (now - sz1LastSeen < timeout);
  if (stationId == 2) return (now - sz2LastSeen < timeout);
  if (stationId == 3) return (now - sz3LastSeen < timeout);
  return false;
}

bool isStationBatteryLow(uint8_t stationId) {
  if (stationId == 1) return sz1BatLow;
  if (stationId == 2) return sz2BatLow;
  if (stationId == 3) return sz3BatLow;
  return false;
}

bool isStationUnicast(uint8_t stationId) {
  if (!isStationOnline(stationId)) return false;
  if (stationId == 1) return sz1Unicast;
  if (stationId == 2) return sz2Unicast;
  if (stationId == 3) return sz3Unicast;
  return false;
}
