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

static bool debugModeEnabled = false;

#define MAX_DEBUG_PACKETS 20
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

String getDebugPacketsJSON() {
  String json = "[";
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

#define RECENT_EVENTS_MAX 16
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
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  }

  // 1. Alive / Heartbeat verarbeiten
  if (len == sizeof(AlivePacket)) {
    AlivePacket packet;
    memcpy(&packet, incomingData, sizeof(packet));
    
    unsigned long now = millis();
    if (packet.stationId == 1) { sz1LastSeen = now; sz1BatLow = (packet.batteryLow != 0); }
    if (packet.stationId == 2) { sz2LastSeen = now; sz2BatLow = (packet.batteryLow != 0); }
    if (packet.stationId == 3) { sz3LastSeen = now; sz3BatLow = (packet.batteryLow != 0); }

    logDebugPacket(packet.stationId, 0, packet.batteryLow);
    return;
  }

  // 2. Sektor-Paket verarbeiten
  if (len == sizeof(SectorPacket)) {
    SectorPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    // Auch ein Rundenpaket beweist, dass die Station lebt
    unsigned long now = millis();
    if (packet.stationId == 1) sz1LastSeen = now;
    if (packet.stationId == 2) sz2LastSeen = now;
    if (packet.stationId == 3) sz3LastSeen = now;

    logDebugPacket(packet.stationId, packet.carId, packet.timestamp);

    if (isDuplicateEvent(packet.eventId)) {
      return; 
    }

    processSectorPacket(packet.stationId, packet.carId, packet.timestamp);
  }
}

void initESPNowBase() {
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Fehler bei Initialisierung!");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);

  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  }
  
  Serial.printf("[ESP-NOW] Bereit auf Funkkanal %d\n", WiFi.channel());
}

void handleTimeSyncBroadcast() {
  static uint32_t lastSyncTime = 0;
  // Alle 3 Sekunden Master-Sync (Stratum 0) aussenden
  if (millis() - lastSyncTime > 3000UL) {
    lastSyncTime = millis();
    
    SyncPacket syncPkt;
    syncPkt.baseTimeMs = millis();
    syncPkt.stratum = 0; // Master

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcastAddress)) {
      esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
    }
    esp_now_send(broadcastAddress, (uint8_t *)&syncPkt, sizeof(syncPkt));
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