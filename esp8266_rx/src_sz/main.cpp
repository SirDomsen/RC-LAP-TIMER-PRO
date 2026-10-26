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

// Smart-Parent Routing (Zero Heap: 7 Bytes RAM)
uint8_t parentMac[6] = {0};
bool hasParent = false;
uint8_t parentStratum = 255;
uint32_t lastParentSeenMs = 0;

static SectorPacket pendingFallbackPkt;
static bool hasPendingFallback = false;
volatile bool lastTxFailed = false;

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
  uint8_t isUnicast;
};

// OTA Wartungsmodus-Trigger (101 Bytes)
#define OTA_MAGIC_CODE 0x07A1234
struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
  uint8_t  targetMode; 
  char     ssid[32];
  char     pass[64];
};

#define OTA_ACK_MAGIC 0x07A1235
struct __attribute__((packed)) OtaAckPacket {
  uint32_t magicCode;
  uint8_t  stationId;
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
const uint32_t BEACON_TIMEOUT_MS = 15000;      // Beruhigt: 15 Sekunden Verweildauer statt 2,5s
const uint32_t CHANNEL_HOP_INTERVAL_MS = 1200; // Beruhigt: 1200 ms pro Kanal lauschen statt 45ms

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
static uint32_t otaStartTimeMs = 0;
static bool otaTransferInProgress = false;

// Ziel-Netzwerk für OTA
volatile bool triggerMaintenance = false;
volatile uint8_t otaTargetMode = 1;
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

  if (hasParent) {
    if (esp_now_is_peer_exist(parentMac)) {
      esp_now_del_peer(parentMac);
    }
    esp_now_add_peer(parentMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
  }
}

// OTA Netzwerk-Verbindung mit den dynamischen Daten der Basis und Pre-OTA Sicherheitsnetz
void connectOtaNetwork() {
  digitalWrite(LED_SIGNAL, HIGH); 
  WiFi.disconnect(true);
  delay(50);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); // 802.11g für zuverlässige Router-Verbindung
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  String targetSsid = otaTargetSsid;
  String targetPass = otaTargetPass;

  if (targetSsid.length() == 0) {
    targetSsid = HOME_SSID;
    targetPass = HOME_PASS;
  }

  // Stufe 1: Zuerst dynamisch per DHCP probieren (Standard)
  WiFi.config(0u, 0u, 0u);
  WiFi.hostname(String("SZ-") + String(STATION_ID));
  WiFi.begin(targetSsid.c_str(), targetPass.c_str());

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 12000)) { 
    delay(100); 
  }
  
  // Stufe 2: Fallback auf feste IP, falls DHCP (z.B. im Heimnetz) hängt
  if (WiFi.status() != WL_CONNECTED && otaTargetMode == 1) {
    IPAddress gateway(192, 168, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(targetSsid.c_str(), targetPass.c_str());
    
    startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 6000)) { 
      delay(100); 
    }
  }

  // Stufe 3: Notfall-AP als Rettungsanker gegen Softbricks (öffnen und für OTA bereitstellen)
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    char rescueSsid[32];
    snprintf(rescueSsid, sizeof(rescueSsid), "SZ-RESCUE-%d", STATION_ID);
    WiFi.softAP(rescueSsid, "12345678");
    wifiConnected = true; 
  } else {
    wifiConnected = true;
    updateEspNowChannel(WiFi.channel());
  }

  otaStartTimeMs = millis();
  otaTransferInProgress = false;
  digitalWrite(LED_SIGNAL, LOW); 
}

// ==========================================
// ESP-NOW CALLBACKS
// ==========================================

void onDataSent(u8 *mac_addr, u8 sendStatus) {
  if (sendStatus != 0) {
    lastTxFailed = true;
  } else {
    lastTxFailed = false;
    hasPendingFallback = false; // Unicast erfolgreich per Hardware-ACK quittiert!
  }
}

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

      // Smart-Parent Auswahl: Bevorzuge niedrigeres Stratum (Hop-Count zur Basis)
      // Hört SZ die Basis direkt (Stratum 0) -> sofort Parent = Basis.
      // Hört SZ3 nur SZ2 (Stratum 1) -> Parent = SZ2.
      // Wenn der bisherige Parent > 6 Sekunden stumm war, darf ein anderer übernehmen.
      if (mac != NULL) {
        bool isBetterStratum = (sync.stratum < parentStratum);
        bool isCurrentParent = (hasParent && memcmp(parentMac, mac, 6) == 0);
        bool parentTimedOut  = (hasParent && (millis() - lastParentSeenMs > 6000));

        if (isBetterStratum || isCurrentParent || !hasParent || parentTimedOut) {
          if (!hasParent || memcmp(parentMac, mac, 6) != 0) {
            if (hasParent && esp_now_is_peer_exist(parentMac)) {
              esp_now_del_peer(parentMac);
            }
            memcpy(parentMac, mac, 6);
            esp_now_add_peer(parentMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
            hasParent = true;
          }
          parentStratum = sync.stratum;
          lastParentSeenMs = millis();
        }
      }

      // Stratum-Logik verhindert Echo-Schleifen (Basis = 0, SZ1..3 = 1..x)
      if (sync.stratum < currentStratum || !timeSynced || (millis() - lastStratumResetMs > 5000)) {
        localSyncOffsetMs = (sync.baseTimeMs + ESPNOW_HOP_LATENCY_MS) - millis();
        timeSynced = true;
        currentStratum = sync.stratum + 1;
        lastStratumResetMs = millis();

        // JEDE Station leitet das Beacon weiter (Flood-Routing)
        BeaconSyncPacket forwardSync;
        forwardSync.magicCode = BEACON_SYNC_MAGIC;
        forwardSync.baseTimeMs = getSynchronizedTimeMs();
        forwardSync.channel = currentChannel;
        forwardSync.stratum = currentStratum;
        esp_now_send(broadcastMac, (uint8_t *)&forwardSync, sizeof(forwardSync));
      }
    }
    return;
  }

  // 2. SEKTORZEIT-PAKET (Upstream zur Basis)
  if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    if (pkt.stationId != STATION_ID && pkt.stationId > 0) {
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
    return;
  }

  // 3. ALIVE PACKET (Upstream)
  if (len == sizeof(AlivePacket)) {
    AlivePacket alive;
    memcpy(&alive, incomingData, sizeof(alive));
    
    if (alive.stationId != STATION_ID && alive.stationId > 0 && alive.stationId <= 3) {
      static uint32_t lastRelayedAliveMs[4] = {0};
      if (millis() - lastRelayedAliveMs[alive.stationId] > 2000) {
        lastRelayedAliveMs[alive.stationId] = millis();
        if (hasParent) {
          esp_now_send(parentMac, (uint8_t *)&alive, sizeof(alive));
        } else {
          esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
        }
      }
    }
    return;
  }

  // 4. OTA TRIGGER (Arm-and-Confirm vor dem Switch)
  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      // Arm-and-Confirm Bestätigung an Parent und Broadcast
      OtaAckPacket ack;
      ack.magicCode = OTA_ACK_MAGIC;
      ack.stationId = STATION_ID;
      if (hasParent) {
        esp_now_send(parentMac, (uint8_t *)&ack, sizeof(ack));
      }
      esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));

      triggerMaintenance = true;
      otaTargetMode = ota.targetMode;
      otaTargetSsid = String(ota.ssid);
      otaTargetPass = String(ota.pass);
      
      static uint32_t lastOtaTrigger = 0;
      if (millis() - lastOtaTrigger > 5000) {
        lastOtaTrigger = millis();
        esp_now_send(broadcastMac, (uint8_t *)&ota, sizeof(ota));
      }
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
      
      static uint32_t lastHotspotSync = 0;
      if (millis() - lastHotspotSync > 5000) {
        lastHotspotSync = millis();
        esp_now_send(broadcastMac, (uint8_t *)&syncPkt, sizeof(syncPkt));
      }
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
  WiFi.setPhyMode(WIFI_PHY_MODE_11B); // 802.11b für maximale Reichweite & Empfindlichkeit (-98 dBm) im Rennbetrieb
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

  static char otaHostname[16];
  snprintf(otaHostname, sizeof(otaHostname), "SZ-%d", STATION_ID);
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.onStart([]() {
    otaTransferInProgress = true;
  });
  ArduinoOTA.onEnd([]() {
    otaTransferInProgress = false;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ESP.restart();
  });
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  uint32_t nowMs = millis();

  // --- 1. KANAL-HOPPING (BERUHIGT) ---
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

  // --- 1b. FAILOVER CHECK: Falls Unicast fehlschlug (kein ACK), sofort Broadcast abfeuern ---
  if (hasPendingFallback && lastTxFailed) {
    hasPendingFallback = false;
    lastTxFailed = false;
    esp_now_send(broadcastMac, (uint8_t *)&pendingFallbackPkt, sizeof(SectorPacket));
  }

  // --- 2. RELAY QUEUE ---
  if (relayQueueTail != relayQueueHead) {
    SectorPacket pktToRelay;
    memcpy(&pktToRelay, (void*)&relayQueue[relayQueueTail], sizeof(SectorPacket));
    relayQueueTail = (relayQueueTail + 1) % RELAY_QUEUE_SIZE;
    if (hasParent) {
      esp_now_send(parentMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
    } else {
      esp_now_send(broadcastMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
    }
  }

  // --- 3. HOTSPOT ACK ---
  if (pendingHotspotAck) {
    pendingHotspotAck = false;
    HotspotAckPacket ack;
    ack.magicCode = HOTSPOT_ACK_MAGIC;
    ack.stationId = STATION_ID;
    if (hasParent) {
      esp_now_send(parentMac, (uint8_t *)&ack, sizeof(ack));
    } else {
      esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));
    }
  }

  // --- 4. WARTUNGSMODUS / OTA (MIT TOTMANNSCHALTUNG) ---
  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    maintenanceMode = true;
    otaStartTimeMs = millis(); // Sofort Zeitstempel setzen gegen Frühauslösung!
    connectOtaNetwork(); 
    ArduinoOTA.begin();
  }

  if (maintenanceMode) {
    ArduinoOTA.handle();
    bool blinkState = (nowMs / 166) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);

    // Totmannschaltung: Nach 120 Sekunden Inaktivität automatischer Neustart zurück in ESP-NOW
    if (!otaTransferInProgress && otaStartTimeMs > 0 && (nowMs - otaStartTimeMs > 120000UL)) {
      ESP.restart();
    }
  }

  // --- 5. BATTERIE-STATUS & ALIVE HEARTBEAT ---
  static uint32_t lastAliveMs = 0;
  if (!maintenanceMode && channelLocked && (nowMs - lastAliveMs > 3000)) {
    lastAliveMs = nowMs;
    batteryLow = (digitalRead(0) == HIGH);

    AlivePacket alive;
    alive.stationId = STATION_ID;
    alive.batteryLow = batteryLow ? 1 : 0;
    alive.isUnicast = hasParent ? 1 : 0;
    if (hasParent) {
      esp_now_send(parentMac, (uint8_t *)&alive, sizeof(alive));
    } else {
      esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
    }
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

        // Primär: Unicast an den gewählten Parent (Hardware-ACK & bis zu 7 Retries)
        if (hasParent) {
          memcpy(&pendingFallbackPkt, &pkt, sizeof(SectorPacket));
          hasPendingFallback = true;
          lastTxFailed = false;
          esp_now_send(parentMac, (uint8_t *)&pkt, sizeof(pkt));
        } else {
          // Kein Parent bekannt: Broadcast
          esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
        }
      }
    }
  }
}
