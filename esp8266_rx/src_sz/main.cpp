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

#if STATION_ID == 1
  IPAddress local_IP(192, 168, 0, 11);
#elif STATION_ID == 2
  IPAddress local_IP(192, 168, 0, 12);
#elif STATION_ID == 3
  IPAddress local_IP(192, 168, 0, 13);
#endif

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) SectorPacket {
  uint8_t stationId;
  uint8_t carId;
  uint32_t timestamp;
  uint8_t relayed;
  uint32_t eventId;
};

struct __attribute__((packed)) SyncPacket {
  uint32_t baseTimeMs;
  uint8_t stratum;
};

struct __attribute__((packed)) AlivePacket {
  uint8_t stationId;
  uint8_t batteryLow;
};

struct __attribute__((packed)) OtaTriggerPacket {
  uint32_t magicCode;
};
#define OTA_MAGIC_CODE 0x07A1234

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

uint32_t localSyncOffsetMs = 0;
bool timeSynced = false;
uint8_t currentStratum = 255;
bool wifiConnected = false;
bool batteryLow = false;

bool triggerMaintenance = false;
bool maintenanceMode = false;

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

#define RELAY_QUEUE_SIZE 8
volatile SectorPacket relayQueue[RELAY_QUEUE_SIZE];
volatile uint8_t relayQueueHead = 0;
volatile uint8_t relayQueueTail = 0;

#define RELAY_CACHE_SIZE 16
static uint32_t recentRelayedIds[RELAY_CACHE_SIZE] = {0};
static uint8_t relayCacheIndex = 0;

void loadHotspotFromEEPROM() {
  EEPROM.begin(512);
  if (EEPROM.read(0) == EEPROM_MAGIC_HOTSPOT) {
    uint8_t ssidLen = EEPROM.read(1);
    if (ssidLen <= 32) {
      char s[33] = {0};
      for(int i=0; i<ssidLen; i++) s[i] = EEPROM.read(2+i);
      dynamicSsid = String(s);
    }
    uint8_t passLen = EEPROM.read(34);
    if (passLen <= 64) {
      char p[65] = {0};
      for(int i=0; i<passLen; i++) p[i] = EEPROM.read(35+i);
      dynamicPass = String(p);
    }
  }
}

void saveHotspotToEEPROM(String s, String p) {
  EEPROM.write(0, EEPROM_MAGIC_HOTSPOT);
  EEPROM.write(1, s.length());
  for(unsigned int i=0; i<s.length(); i++) EEPROM.write(2+i, s[i]);
  EEPROM.write(34, p.length());
  for(unsigned int i=0; i<p.length(); i++) EEPROM.write(35+i, p[i]);
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

void blinkSignalLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_SIGNAL, LOW); 
    delay(166);
    digitalWrite(LED_SIGNAL, HIGH); 
    delay(166);
  }
}

void executeWifiSwitch(uint8_t mode) {
  digitalWrite(LED_SIGNAL, HIGH); 
  WiFi.disconnect(true);
  delay(100);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  uint32_t startMs = millis();
  
  if (mode == 1) { 
    IPAddress gateway(192, 168, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);
    WiFi.begin(HOME_SSID, HOME_PASS);
    while(WiFi.status() != WL_CONNECTED && millis() - startMs < 10000) { delay(100); }
  } 
  else if (mode == 2 && dynamicSsid.length() > 0) { 
    WiFi.config(0u, 0u, 0u); 
    WiFi.begin(dynamicSsid.c_str(), dynamicPass.c_str());
    while(WiFi.status() != WL_CONNECTED && millis() - startMs < 15000) { delay(100); }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifi_set_channel(WiFi.channel());
    digitalWrite(LED_SIGNAL, LOW); 
  } else {
    wifiConnected = false;
    WiFi.disconnect(true);
    delay(100);
    int activeChannel = 1;
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).equals(HOME_SSID) || WiFi.SSID(i).equals(AP_SSID) || (dynamicSsid.length() > 0 && WiFi.SSID(i).equals(dynamicSsid))) {
        activeChannel = WiFi.channel(i);
        break;
      }
    }
    wifi_set_channel(activeChannel);
    digitalWrite(LED_SIGNAL, LOW); 
  }
}

void connectWlanSequence() {
  digitalWrite(LED_SIGNAL, HIGH); 
  WiFi.disconnect(true);
  delay(100);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G); 
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  IPAddress gateway(192, 168, 0, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);
  
  WiFi.begin(HOME_SSID, HOME_PASS);
  uint32_t startMs = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startMs < 4000) { delay(100); }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifi_set_channel(WiFi.channel());
    digitalWrite(LED_SIGNAL, LOW); 
    return;
  }
  
  blinkSignalLed(3);
  digitalWrite(LED_SIGNAL, HIGH); 
  
  if (dynamicSsid.length() > 0) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.setPhyMode(WIFI_PHY_MODE_11G);
    WiFi.mode(WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.config(0u, 0u, 0u); 
    
    WiFi.begin(dynamicSsid.c_str(), dynamicPass.c_str());
    startMs = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - startMs < 12000) { delay(100); }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      wifi_set_channel(WiFi.channel());
      digitalWrite(LED_SIGNAL, LOW); 
      return;
    }
    
    blinkSignalLed(5);
    digitalWrite(LED_SIGNAL, HIGH);
  }
  
  WiFi.disconnect(true);
  delay(100);
  wifiConnected = false;
  int activeChannel = 1;
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).equals(HOME_SSID) || WiFi.SSID(i).equals(AP_SSID) || (dynamicSsid.length() > 0 && WiFi.SSID(i).equals(dynamicSsid))) {
      activeChannel = WiFi.channel(i);
      break;
    }
  }
  wifi_set_channel(activeChannel);
  digitalWrite(LED_SIGNAL, LOW); 
}

void onDataSent(u8 *mac_addr, u8 sendStatus) {}

void onDataRecv(u8 *mac, u8 *incomingData, u8 len) {
  if (len == sizeof(WifiSwitchPacket)) {
    WifiSwitchPacket sw;
    memcpy(&sw, incomingData, sizeof(sw));
    if (sw.magicCode == WIFI_SWITCH_MAGIC) {
      targetWifiMode = sw.mode;
      pendingWifiSwitch = true;
      
      #if STATION_ID < 3
      esp_now_send(broadcastMac, (uint8_t *)&sw, sizeof(sw));
      #endif
    }
    return;
  }

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

  if (len == sizeof(OtaTriggerPacket)) {
    OtaTriggerPacket ota;
    memcpy(&ota, incomingData, sizeof(ota));
    if (ota.magicCode == OTA_MAGIC_CODE) {
      triggerMaintenance = true;
      #if STATION_ID < 3
      esp_now_send(broadcastMac, (uint8_t *)&ota, sizeof(ota));
      #endif
    }
    return;
  }

  if (len == sizeof(SyncPacket)) {
    SyncPacket sync;
    memcpy(&sync, incomingData, sizeof(sync));

    if (sync.stratum < currentStratum || !timeSynced) {
      localSyncOffsetMs = sync.baseTimeMs - millis();
      timeSynced = true;
      currentStratum = sync.stratum + 1;

      #if STATION_ID < 3
      SyncPacket forwardSync;
      forwardSync.baseTimeMs = getSynchronizedTimeMs();
      forwardSync.stratum = currentStratum;
      esp_now_send(broadcastMac, (uint8_t *)&forwardSync, sizeof(forwardSync));
      #endif
    }
    return;
  }

  if (len == sizeof(AlivePacket)) {
    AlivePacket alive;
    memcpy(&alive, incomingData, sizeof(alive));
    #if STATION_ID < 3
    if (alive.stationId > STATION_ID) {
      esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
    }
    #endif
    return;
  }
  
  if (len == sizeof(SectorPacket)) {
    SectorPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    
    #if STATION_ID < 3
    if (pkt.stationId > STATION_ID && !isAlreadyRelayed(pkt.eventId)) {
      uint8_t nextHead = (relayQueueHead + 1) % RELAY_QUEUE_SIZE;
      if (nextHead != relayQueueTail) {
        pkt.relayed = 1;
        // Sicheres Kopieren in die volatile Queue
        memcpy((void*)&relayQueue[relayQueueHead], &pkt, sizeof(SectorPacket));
        relayQueueHead = nextHead;
      }
    }
    #endif
  }
}

void setup() {
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  pinMode(0, INPUT_PULLUP);

  digitalWrite(LED_SIGNAL, HIGH); 
  digitalWrite(LED_SIGNAL, LOW);
  delay(333);
  digitalWrite(LED_SIGNAL, HIGH);
  delay(100);

  loadHotspotFromEEPROM();
  connectWlanSequence(); 

  if (esp_now_init() == 0) {
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_add_peer(broadcastMac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  }

  initIRDecoder();
  ArduinoOTA.setHostname(("SZ-" + String(STATION_ID)).c_str());
  ArduinoOTA.begin();
}

void loop() {
  if (pendingWifiSwitch) {
    pendingWifiSwitch = false;
    executeWifiSwitch(targetWifiMode);
  }

  if (relayQueueTail != relayQueueHead) {
    SectorPacket pktToRelay;
    // Sauberes Herauskopieren aus der volatile Queue
    memcpy(&pktToRelay, (void*)&relayQueue[relayQueueTail], sizeof(SectorPacket));
    relayQueueTail = (relayQueueTail + 1) % RELAY_QUEUE_SIZE;
    
    for (int i = 0; i < STATION_ID; i++) {
      esp_now_send(broadcastMac, (uint8_t *)&pktToRelay, sizeof(pktToRelay));
      delay(6);
    }
  }

  if (pendingHotspotAck) {
    pendingHotspotAck = false;
    HotspotAckPacket ack;
    ack.magicCode = HOTSPOT_ACK_MAGIC;
    ack.stationId = STATION_ID;
    esp_now_send(broadcastMac, (uint8_t *)&ack, sizeof(ack));
  }

  if (triggerMaintenance && !maintenanceMode) {
    triggerMaintenance = false;
    maintenanceMode = true;
    connectWlanSequence(); 
  }

  if (wifiConnected || maintenanceMode) {
    ArduinoOTA.handle();
  }

  static uint32_t lastBatteryCheckMs = 0;
  if (millis() - lastBatteryCheckMs > 10000) {
    lastBatteryCheckMs = millis();
    batteryLow = (digitalRead(0) == HIGH);
  }

  static uint32_t lastAliveMs = 0;
  if (!maintenanceMode && millis() - lastAliveMs > 3000) {
    lastAliveMs = millis();
    AlivePacket alive;
    alive.stationId = STATION_ID;
    alive.batteryLow = batteryLow ? 1 : 0;
    esp_now_send(broadcastMac, (uint8_t *)&alive, sizeof(alive));
  }

  if (maintenanceMode) {
    bool blinkState = (millis() / 166) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (batteryLow) {
    bool blinkState = (millis() / 1000) % 2 == 0;
    digitalWrite(LED_SIGNAL, blinkState ? LOW : HIGH);
  } else if (ledPulseActive) {
    if (millis() >= ledPulseEndMs) {
      digitalWrite(LED_SIGNAL, LOW); 
      ledPulseActive = false;
    }
  } else {
    digitalWrite(LED_SIGNAL, LOW);
  }

  if (newCodeAvailable && !maintenanceMode) {
    newCodeAvailable = false;
    
    uint32_t triggerTime = getSynchronizedTimeMs();
    uint16_t code = receivedCode & 0xFFF;
    uint8_t detectedCarId = (code >> 6) & 0x3F;
    uint8_t carIdInv      = code & 0x3F;

    if (((detectedCarId ^ carIdInv) == 0x3F) && detectedCarId >= 1 && detectedCarId <= MAX_CARS) {
      uint32_t nowMs = millis();

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
        pkt.relayed = 0;
        pkt.eventId = ((uint32_t)STATION_ID << 24) | (millis() & 0x00FFFFFF); 

        isAlreadyRelayed(pkt.eventId); 
        
        for (int i = 0; i < STATION_ID; i++) {
          esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
          delay(6);
        }
      }
    }
  }
}