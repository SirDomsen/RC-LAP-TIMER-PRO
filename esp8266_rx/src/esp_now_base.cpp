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

// Cache zur Duplikaterkennung (verhindert mehrfaches Verarbeiten von Relay-Paketen)
#define RECENT_EVENTS_MAX 10
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
    if (recentEvents[i].eventId == eventId && (now - recentEvents[i].receivedAt < 3000)) {
      return true;
    }
  }
  // Neu eintragen
  recentEvents[recentEventIndex] = {eventId, now};
  recentEventIndex = (recentEventIndex + 1) % RECENT_EVENTS_MAX;
  return false;
}

void onESPNowDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  }

  // 1. Heartbeat verarbeiten
  if (len == sizeof(AlivePacket)) {
    AlivePacket packet;
    memcpy(&packet, incomingData, sizeof(packet));
    
    unsigned long now = millis();
    if (packet.stationId == 1) sz1LastSeen = now;
    if (packet.stationId == 2) sz2LastSeen = now;
    if (packet.stationId == 3) sz3LastSeen = now;
    return;
  }

  // 2. Sektor-Paket verarbeiten
  if (len == sizeof(SectorPacket)) {
    SectorPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    // Duplikatprüfung anhand der Event-ID
    if (isDuplicateEvent(packet.eventId)) {
      return; 
    }

    Serial.printf("[ESP-NOW] Sektor SZ%d -> Auto ID %d bei %u ms (Relay: %d)\n", 
                  packet.stationId, packet.carId, packet.timestamp, packet.relayed);

    // Übergabe an die zentrale Runden- und Sektorlogik
    processSectorPacket(packet.stationId, packet.carId, packet.timestamp);
  }
}

void initESPNowBase() {
  WiFi.mode(WIFI_AP_STA);
  
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Fehler beim Initialisieren an der Basis!");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onESPNowDataRecv);
  
  Serial.println("[ESP-NOW] Basisstation erfolgreich als Master/Receiver gestartet.");
}

void handleTimeSyncBroadcast() {
  static uint32_t lastSyncTime = 0;
  if (millis() - lastSyncTime > 30000UL) {
    lastSyncTime = millis();
    
    SyncPacket syncPkt;
    syncPkt.baseTimeMs = millis();

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastAddress, (uint8_t *)&syncPkt, sizeof(syncPkt));
  }
}

bool isStationOnline(uint8_t stationId) {
  unsigned long now = millis();
  unsigned long timeout = 8000; 

  if (stationId == 1) return (now - sz1LastSeen < timeout);
  if (stationId == 2) return (now - sz2LastSeen < timeout);
  if (stationId == 3) return (now - sz3LastSeen < timeout);
  return false;
}