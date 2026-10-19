#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include "config.h"
#include "ir_decoder.h"

extern "C" {
  #include <espnow.h>
}

#ifndef u8
typedef uint8_t u8;
#endif

// --- Feste IP-Adressen für OTA/Wartung ---
#if STATION_ID == 1
  IPAddress local_IP(192, 168, 0, 11);
#elif STATION_ID == 2
  IPAddress local_IP(192, 168, 0, 12);
#elif STATION_ID == 3
  IPAddress local_IP(192, 168, 0, 13);
#endif

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ==========================================
// PAKETSTRUKTUREN
// ==========================================

struct __attribute__((packed)) SectorPacket {
  uint8_t  stationId;
  uint8_t  carId;
  uint32_t timestamp;
  uint16_t seqNumber;
  uint8_t  ttl;
};

#define BEACON_SYNC_MAGIC 0x53594E43 // "SYNC"
struct __attribute__((packed)) BeaconSyncPacket {
  uint32_t magicCode;
  uint32_t baseTimeMs;
  uint8_t  channel;
  uint8_t  stratum; 
};

struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow;
};

// OTA Wartungsmodus-Trigger (101 Bytes)
#define OTA_MAGIC_CODE 0x07A1234
struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
  uint8_t  targetMode; 
  char     ssid[32];
  char     pass[64];
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

// ==========================================
// GLOBALE STATUSVARIABLEN & EINSTELLUNGEN
// ==========================================

const uint32_t ESPNOW_HOP_LATENCY_MS = 1; 
const uint32_t BEACON_TIMEOUT_MS = 2500;     
const uint32_t CHANNEL_HOP_INTERVAL_MS = 45; 

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
uint8_t currentStratum = 255;
uint32_t lastBeaconReceivedMs = 0;
uint32_t lastStratumResetMs = 0;

uint8_t currentChannel = 1;
uint32_t lastChannelHopMs = 0;
bool channelLocked = false;

bool wifiConnected = false;
bool batteryLow = false;
bool maintenanceMode = false;

// Ziel-Netzwerk für OTA
volatile bool triggerMaintenance = false;
String otaTargetSsid = "";
String otaTargetPass = "";

String dynamicSsid = "";
String dynamicPass = "";
bool pendingHotspotAck = false;

volatile bool pendingWifiSwitch = false;
volatile uint8_t targetWifiMode = 0;

#define EEPROM_MAGIC_HOTSPOT 0x5B

static uint32_t ledPulseEndMs = 0;
static bool ledPulseActive = false;

static uint32_t lastSeenSignalTime[MAX_CARS + 1] = {0};
const uint32_t SECTOR_CLEARANCE_MS = 2000; 

static uint16_t localSeqNumber = 0;

#define RELAY_QUEUE_SIZE 8
volatile SectorPacket relayQueue[RELAY_QUEUE_SIZE];
volatile uint8_t relayQueueHead = 0;
volatile uint8_t relayQueueTail = 0;

#define RELAY_CACHE_SIZE 16
static uint32_t recentRelayedIds[RELAY_CACHE_SIZE] = {0};
static uint8_t relayCacheIndex = 0;

// ==========================================
// HILFSFUNKTIONEN
// ==========================================

void loadHotspotFromEEPROM() {
  EEPROM.begin(512);
  if (EEPROM.read(0) == EEPROM_MAGIC_HOTSPOT) {
    uint8_t ssidLen = EEPROM.read(1);
    if (ssidLen <= 32) {
      char s[33] = {0};
      for (int i = 0; i < ssidLen; i++) s[i] = EEPROM.read(2 + i);
      dynamicSsid = String(s);
    }
    uint8_t passLen = EEPROM.read(34);
    if (passLen <= 64) {
      char p[65] = {0};
      for (int i = 0; i < passLen; i++) p[i] = EEPROM.read(35 + i);
      dynamicPass = String(p);
    }
  }
}

void saveHotspotToEEPROM(String s, String p) {
  EEPROM.write(0, EEPROM_MAGIC_HOTSPOT);
  EEPROM.write(1, s.length());
  for (unsigned int i = 0; i < s.length(); i++) EEPROM.write(2 + i, s[i]);
  EEPROM.write(34, p.length());
  for (unsigned int i = 0; i < p.length(); i++) EEPROM.write(35 + i, p[i]);
  EEPROM.commit();
  dynamicSsid = s;
  dynamicPass = p;
}

bool isAlreadyRelayed(uint32_t eventId) {
  if (eventId == 0) return false;
  for (uint8_t i = 0; i < RELAY_CACHE_SIZE; i++) {
    if (recentRelayedIds[i] == eventId) return true;
  }
  recentRelayedIds[relayCacheIndex] = eventId;
  relayCacheIndex = (relayCacheIndex + 1) % RELAY_CACHE_SIZE;
  return false;
}

uint32_t getSynchronizedTimeMs() {
  if (!timeSynced) return millis();
  return millis() + localSyncOffsetMs;
}

void updateEspNowChannel(uint8_t ch) {
  currentChannel = ch;
  wifi_set_channel(currentChannel);
  if (esp_now_is_peer_exist(broadcastMac)) {
    esp_now_del_peer(broadcastMac);
  }
  esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
}

// OTA Netzwerk-Verbindung mit den dynamischen Daten der Basis
void connectOtaNetwork() {
  digitalWrite(LED_SIGNAL, HIGH); 
  WiFi.disconnect(true);
  delay(50);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  // Wenn keine Daten mitgeschickt wurden, Fallback auf Heim-WLAN
  String targetSsid = otaTargetSsid.length() > 0 ? otaTargetSsid : HOME_SSID;
  String targetPass = otaTargetPass.length() > 0 ? otaTargetPass : HOME_PASS;

  // Im Heim-WLAN statische IP erzwingen, im AP-Modus der Basis auf DHCP vertrauen
  if (targetSsid.equals(HOME_SSID)) {
    IPAddress gateway(192, 168, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);
  } else {
    WiFi.config(0u, 0u, 0u);
  }
  
  WiFi.begin(targetSsid.c_str(), targetPass.c_str());
  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 12000) { delay(100); }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    updateEspNowChannel(WiFi.channel());
  } else {
    wifiConnected = false;
  }
  digitalWrite(LED_SIGNAL, LOW); 
}

// ==========================================
// ESP-NOW CALLBACKS
// ==========================================

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // 1. BEACON & ZEITSYNCHRONISATION (Downstream)
  if (len == sizeof(BeaconSyncPacket)) {
    BeaconSyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));

    if (sync.magicCode == BEACON_SYNC_MAGIC) {
      lastBeaconReceivedMs = millis();
      channelLocked = true;

      if (sync.channel >= 1 && sync.channel <= 13 && sync.channel != currentChannel) {
        updateEspNowChannel(sync.channel);
      }

      if (sync.stratum < currentStratum || !timeSynced || (millis() - lastStratumResetMs > 5000)) {
        localSyncOffsetMs = (sync.baseTimeMs + ESPNOW_HOP_LATENCY_MS) - millis();
        timeSynced = true;
        currentStratum = sync.stratum + 1;
        lastStratumResetMs = millis();

        #if STATION_ID < 3
        BeaconSyncPacket forwardSync;
        forwardSync.magicCode = BEACON_SYNC_MAGIC;
        forwardSync.baseTimeMs = getSynchronizedTimeMs();
        forwardSync.channel = currentChannel;
        forwardSync.stratum = currentStratum;
        esp_now_send(broadcastMac, (uint8_t *)&forwardSync, sizeof(forwardSync));
        #endif
      }
    }
    return;
  }

  // 2. SEKTORZEIT-PAKET (Upstream zur Basis)
  if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    #if STATION_ID < 3
    if (pkt.stationId > STATION_ID) {
      uint32_t eventId = ((uint32_t)pkt.stationId << 16) | pkt.seqNumber;
      if (!isAlreadyRelayed(eventId) && pkt.ttl > 1) {
        pkt.ttl--;
        uint8_t nextHead = (relayQueueHead + 1) % RELAY_QUEUE_SIZE;
        if (nextHead != relayQueueTail) {
          memcpy((void*)&relayQueue[relayQueueHead], &pkt, sizeof(SectorPacket));
          relayQueueHead = nextHead;
        }
      }
    }
    #endif
    return;
  }

  // 3. ALIVE PACKET (Upstream)
  if (len == sizeof(AlivePacket)) {
    AlivePacket alive;
    memcpy(&alive, incomingData, sizeof(alive));
    #if STATION_ID < 3
    if (alive.stationId > STATION_ID) {
      static uint32_t lastRelayedAliveMs[4] = {0};
      if (millis() - lastRelayedAliveMs[alive.stationId] > 2000) {
        lastRelayedAliveMs[alive.stationId] = millis();
        esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
      }
    }
    #endif
    return;
  }

  // 4. OTA TRIGGER (Neue Logik mit SSID Übernahme)
  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      triggerMaintenance = true;
      otaTargetSsid = String(ota.ssid);
      otaTargetPass = String(ota.pass);
      #if STATION_ID < 3
      esp_now_send(broadcastMac, (uint8_t *)&ota, sizeof(ota));
      #endif
    }
    return;
  }

  // 5. HOTSPOT SYNC
  if (len == sizeof(HotspotSyncPacket)) {
    HotspotSyncPacket syncPkt;
    memcpy(&syncPkt, incomingData, sizeof(syncPkt));
    if (syncPkt.magicCode == HOTSPOT_SYNC_MAGIC) {
      saveHotspotToEEPROM(String(syncPkt.ssid), String(syncPkt.pass));
      pendingHotspotAck = true;
      #if STATION_ID < 3
      esp_now_send(broadcastMac, (uint8_t *)&syncPkt, sizeof(syncPkt));
      #endif
    }
    return;
  }
}

// ==========================================
// SETUP
// ==========================================

void setup() {
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  pinMode(0, INPUT_PULLUP);

  digitalWrite(LED_SIGNAL, LOW);
  delay(100);
  digitalWrite(LED_SIGNAL, HIGH);

  loadHotspotFromEEPROM();

  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(20.5); // Maximale Sendeleistung erzwingen
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
  }

  initIRDecoder();
  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  uint32_t nowMs = millis();

  // --- 1. KANAL-HOPPING ---
  if (!maintenanceMode) {
    if (nowMs - lastBeaconReceivedMs > BEACON_TIMEOUT_MS) {
      channelLocked = false;
      timeSynced = false;
      currentStratum = 255;

      if (nowMs - lastChannelHopMs >= CHANNEL_HOP_INTERVAL_MS) {
        lastChannelHopMs = nowMs;
        uint8_t nextCh = (currentChannel % 13) + 1;
        updateEspNowChannel(nextCh);
      }
    }
  }

  // --- 2. RELAY QUEUE ---
  if (relayQueueTail != relayQueueHead) {
    SectorPacket pktToRelay;
    memcpy(&pktToRelay, (void*)&relayQueue[relayQueueTail], sizeof(SectorPacket));
    relayQueueTail = (relayQueueTail + 1) % RELAY_QUEUE_SIZE;
    esp_now_send(broadcastMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
  }

  // --- 3. HOTSPOT ACK ---
  if (pendingHotspotAck) {
    pendingHotspotAck = false;
    HotspotAckPacket ack;
    ack.magicCode = HOTSPOT_ACK_MAGIC;
    ack.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));
  }

  // --- 4. WARTUNGSMODUS / OTA ---
  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    maintenanceMode = true;
    connectOtaNetwork(); 
    ArduinoOTA.begin();
  }

  if (maintenanceMode) {
    ArduinoOTA.handle();
    bool blinkState = (nowMs / 166) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  }

  // --- 5. BATTERIE-STATUS & ALIVE HEARTBEAT ---
  static uint32_t lastBatteryCheckMs = 0;
  if (nowMs - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = nowMs;
    batteryLow = (digitalRead(0) == HIGH);
  }

  static uint32_t lastAliveMs = 0;
  if (!maintenanceMode && channelLocked && (nowMs - lastAliveMs > 3000)) {
    lastAliveMs = nowMs;
    AlivePacket alive;
    alive.stationId = STATION_ID;
    alive.batteryLow = batteryLow ? 1 : 0;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  // --- 6. LED STATUS ---
  if (!maintenanceMode) {
    if (batteryLow) {
      bool blinkState = (nowMs / 1000) % 2 == 0;
      digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
    } else if (ledPulseActive) {
      if (nowMs >= ledPulseEndMs) {
        digitalWrite(LED_SIGNAL, LOW); 
        ledPulseActive = false;
      }
    } else {
      digitalWrite(LED_SIGNAL, LOW);
    }
  }

  // --- 7. IR-DECODER AUSWERTUNG ---
  if (newCodeAvailable && !maintenanceMode) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;
    uint8_t carIdInv      = code & 0x3F;

    if (((detectedCarId ^ carIdInv) == 0x3F) && detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      if (!batteryLow) {
        digitalWrite(LED_SIGNAL, HIGH); 
        ledPulseEndMs = nowMs + 40;
        ledPulseActive = true;
      }

      bool isNewPass = (nowMs - lastSeenSignalTime[detectedCarId] > SECTOR_CLEARANCE_MS);
      lastSeenSignalTime[detectedCarId] = nowMs;

      if (isNewPass) {
        SectorPacket pkt;
        pkt.stationId = STATION_ID;
        pkt.carId = detectedCarId;
        pkt.timestamp = triggerTime;
        pkt.seqNumber = ++localSeqNumber;
        pkt.ttl = 3;

        uint32_t eventId = ((uint32_t)STATION_ID << 16) | pkt.seqNumber;
        isAlreadyRelayed(eventId); 

        esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
      }
    }
  }
}