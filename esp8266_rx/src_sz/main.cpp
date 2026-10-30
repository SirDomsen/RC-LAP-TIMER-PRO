#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "ir_decoder.h"
#include "secrets.h"

extern "C" {
  #include <espnow.h>
}

#ifndef u8
typedef uint8_t u8;
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef BASE_AP_SSID
  #define BASE_AP_SSID "LapTimer-Base"
#endif
#ifndef BASE_AP_PASS
  #define BASE_AP_PASS "12345678"
#endif

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t baseStationMac[6] = {0};
volatile bool baseMacKnown = false;
uint32_t lastDirectBaseBeaconMs = 0;
const uint32_t DIRECT_BASE_TIMEOUT_MS = 2500;

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

static SectorPacket lastUnicastPkt;
volatile bool pendingBroadcastFallback = false;

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

// ==========================================
// GLOBALE STATUSVARIABLEN
// ==========================================

const uint32_t ESPNOW_HOP_LATENCY_MS = 1; 
const uint32_t BEACON_TIMEOUT_MS = 15000;      
const uint32_t CHANNEL_HOP_INTERVAL_MS = 1200; 

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
volatile uint8_t currentStratum = 255;
uint32_t lastBeaconReceivedMs = 0;
uint32_t lastStratumResetMs = 0;

uint8_t currentChannel = 1;
uint32_t lastChannelHopMs = 0;
bool channelLocked = false;

bool batteryLow = false;
bool maintenanceMode = false;

volatile bool triggerMaintenance = false;
volatile bool pendingOtaAck = false; 
volatile uint8_t otaTargetMode = 1;
char otaTargetSsid[33] = {0};
char otaTargetPass[65] = {0};

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

  if (baseMacKnown) {
    if (esp_now_is_peer_exist(baseStationMac)) {
      esp_now_del_peer(baseStationMac);
    }
    esp_now_add_peer(baseStationMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
  }
}

bool tryConnectStation(const char* ssid, const char* pass, uint32_t timeoutMs, bool useStaticIp = false) {
  if (!ssid || strlen(ssid) == 0) {
    return false;
  }

  WiFi.mode(WIFI_OFF);
  delay(60);
  yield();

  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(20.5);
  WiFi.hostname("SZ-" + String(STATION_ID));

  if (useStaticIp) {
    IPAddress staticIp(192, 168, 0, 10 + STATION_ID);
    IPAddress gateway(192, 168, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 0, 1);
    WiFi.config(staticIp, gateway, subnet, dns);
  } else {
    WiFi.config(0u, 0u, 0u);
  }

  WiFi.begin(ssid, pass);

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs < timeoutMs)) {
    delay(100);
    yield();
    ESP.wdtFeed();
  }

  return (WiFi.status() == WL_CONNECTED);
}

// ==========================================
// 4-STUFEN-FAILSAFE WARTUNGSMODUS
// ==========================================

void startMaintenanceNetwork() {
  digitalWrite(LED_SIGNAL, HIGH);
  
  esp_now_deinit();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  yield();

  bool connected = false;

  // Stufe 1: Vorgabe von der Basis (statisch falls Heimnetz-SSID)
  if (strlen(otaTargetSsid) > 0) {
    bool isHomeNet = (strcmp(otaTargetSsid, WIFI_SSID) == 0);
    connected = tryConnectStation(otaTargetSsid, otaTargetPass, 6000, isHomeNet);
  }

  // Stufe 2: Heim-WLAN (secrets.h) -> immer statische IP
  if (!connected && strlen(WIFI_SSID) > 0) {
    connected = tryConnectStation(WIFI_SSID, WIFI_PASSWORD, 6000, true);
  }

  // Stufe 3: Basis-AP -> DHCP
  if (!connected && strlen(BASE_AP_SSID) > 0) {
    connected = tryConnectStation(BASE_AP_SSID, BASE_AP_PASS, 4000, false);
  }

  // Stufe 4: Autarker Rettungs-AP
  if (!connected) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    yield();

    WiFi.mode(WIFI_AP);
    WiFi.setOutputPower(20.5);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    String rescueSsid = "RescueSZ" + String(ESP.getChipId(), HEX);
    rescueSsid.toUpperCase();
    WiFi.softAP(rescueSsid.c_str(), "12345678", 1, false);
  }

  String hostName = "SZ-" + String(STATION_ID);
  ArduinoOTA.setHostname(hostName.c_str());

  ArduinoOTA.onEnd([]() { 
    delay(100);
    ESP.restart();
  });

  ArduinoOTA.onError([](ota_error_t error) { 
    ESP.restart(); 
  });

  ArduinoOTA.begin();

  maintenanceMode = true;
  digitalWrite(LED_SIGNAL, LOW);
}

// ==========================================
// ESP-NOW CALLBACKS
// ==========================================

void onDataSent(u8 *mac_addr, u8 sendStatus) {
  if (sendStatus != 0 && baseMacKnown && mac_addr != NULL) {
    if (memcmp(mac_addr, baseStationMac, 6) == 0) {
      baseMacKnown = false;
      if (currentStratum <= 1) {
        currentStratum = 255;
      }
      pendingBroadcastFallback = true;
    }
  }
}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  // 1. BEACON
  if (len == sizeof(BeaconSyncPacket)) {
    BeaconSyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));
    if (sync.magicCode == BEACON_SYNC_MAGIC) {
      lastBeaconReceivedMs = millis();
      channelLocked = true;
      
      if (sync.channel >= 1 && sync.channel <= 13 && sync.channel != currentChannel) {
        updateEspNowChannel(sync.channel);
      }
      
      if (sync.stratum == 0 && mac != NULL) {
        lastDirectBaseBeaconMs = millis();
        if (!baseMacKnown || memcmp(baseStationMac, mac, 6) != 0) {
          if (baseMacKnown && esp_now_is_peer_exist(baseStationMac)) {
            esp_now_del_peer(baseStationMac);
          }
          memcpy(baseStationMac, mac, 6);
          esp_now_add_peer(baseStationMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
          baseMacKnown = true;
        }
      }
      
      if (sync.stratum < currentStratum || !timeSynced || (!baseMacKnown && (millis() - lastStratumResetMs > 5000))) {
        localSyncOffsetMs = (sync.baseTimeMs + ESPNOW_HOP_LATENCY_MS) - millis();
        timeSynced = true;
        currentStratum = sync.stratum + 1;
        lastStratumResetMs = millis();
        
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

  // 2. SEKTORZEIT
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

  // 3. ALIVE RELAY
  if (len == sizeof(AlivePacket)) {
    AlivePacket alive;
    memcpy(&alive, incomingData, sizeof(alive));
    if (alive.stationId != STATION_ID && alive.stationId > 0 && alive.stationId <= 3) {
      static uint32_t lastRelayedAliveMs[4] = {0};
      if (millis() - lastRelayedAliveMs[alive.stationId] > 2000) {
        lastRelayedAliveMs[alive.stationId] = millis();
        if (baseMacKnown) esp_now_send(baseStationMac, (uint8_t *)&alive, sizeof(alive));
        else esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
      }
    }
    return;
  }

  // 4. OTA TRIGGER
  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      pendingOtaAck = true;
      triggerMaintenance = true;
      otaTargetMode = ota.targetMode;
      strncpy(otaTargetSsid, ota.ssid, 32);
      otaTargetSsid[32] = '\0';
      strncpy(otaTargetPass, ota.pass, 64);
      otaTargetPass[64] = '\0';
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
  digitalWrite(1, LOW); // TX-Pin hart auf Masse fuer Schaltung/Spannungsteiler
  pinMode(0, INPUT_PULLUP);

  digitalWrite(LED_SIGNAL, LOW);
  delay(100);
  digitalWrite(LED_SIGNAL, HIGH);

  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(20.5); 
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, currentChannel, NULL, 0);
  }

  initIRDecoder();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  uint32_t nowMs = millis();

  // --- 1. OTA ACK SENDEN ---
  if (pendingOtaAck) {
    pendingOtaAck = false;
    OtaAckPacket ack;
    ack.magicCode = OTA_ACK_MAGIC;
    ack.stationId = STATION_ID;
    if (baseMacKnown) esp_now_send(baseStationMac, (uint8_t *)&ack, sizeof(ack));
    esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));
  }

  // --- 2. WARTUNGSMODUS STARTEN ---
  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    startMaintenanceNetwork();
  }

  // --- 3. WARTUNGSMODUS AUSFÜHREN ---
  if (maintenanceMode) {
    ArduinoOTA.handle();
    yield();
    ESP.wdtFeed();
    
    bool blinkState = (millis() / 120) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
    return;
  }

  // --- 3b. DIRECT BASE LEASE TIMEOUT ---
  if (baseMacKnown && (nowMs - lastDirectBaseBeaconMs > DIRECT_BASE_TIMEOUT_MS)) {
    baseMacKnown = false;
    if (currentStratum <= 1) {
      currentStratum = 255;
    }
  }

  // --- 3c. FAILOVER FALLBACK SENDEN ---
  if (pendingBroadcastFallback) {
    pendingBroadcastFallback = false;
    esp_now_send(broadcastMac, (uint8_t *)&lastUnicastPkt, sizeof(lastUnicastPkt));
  }

  // --- 4. KANAL-HOPPING ---
  if (nowMs - lastBeaconReceivedMs > BEACON_TIMEOUT_MS) {
    channelLocked = false;
    timeSynced = false;
    currentStratum = 255;
    baseMacKnown = false;
    if (nowMs - lastChannelHopMs >= CHANNEL_HOP_INTERVAL_MS) {
      lastChannelHopMs = nowMs;
      uint8_t nextCh = (currentChannel % 13) + 1;
      updateEspNowChannel(nextCh);
    }
  }

  // --- 5. RELAY QUEUE ---
  if (relayQueueTail != relayQueueHead) {
    SectorPacket pktToRelay;
    memcpy(&pktToRelay, (void*)&relayQueue[relayQueueTail], sizeof(SectorPacket));
    relayQueueTail = (relayQueueTail + 1) % RELAY_QUEUE_SIZE;
    if (baseMacKnown) esp_now_send(baseStationMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
    else esp_now_send(broadcastMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
  }

  // --- 6. BATTERIESTATUS ---
  static uint32_t lastBatteryCheckMs = 0;
  if (nowMs - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = nowMs;
    batteryLow = (digitalRead(0) == HIGH);
  }

  // --- 7. ALIVE HEARTBEAT (3 Bytes, Unicast-Flag) ---
  static uint32_t lastAliveMs = 0;
  if (nowMs - lastAliveMs > 3000) {
    lastAliveMs = nowMs;
    AlivePacket alive;
    alive.stationId = STATION_ID;
    alive.batteryLow = batteryLow ? 1 : 0;
    alive.isUnicast = (baseMacKnown && channelLocked) ? 1 : 0;
    
    if (alive.isUnicast) {
      esp_now_send(baseStationMac, (uint8_t *)&alive, sizeof(alive));
    } else {
      esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
    }
  }

  // --- 8. LED STATUS ---
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

  // --- 9. IR-SIGNAL DECODIERUNG ---
  if (newCodeAvailable) {
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

        if (baseMacKnown) {
          lastUnicastPkt = pkt;
          pendingBroadcastFallback = false;
          esp_now_send(baseStationMac, (uint8_t *)&pkt, sizeof(pkt));
        } else {
          esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
        }
      }
    }
  }
}
