#include "storage.h"
#include "lap_logic.h"
#include <EEPROM.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

String driverNames[MAX_CARS + 1];
String carNames[MAX_CARS + 1];

bool webCacheEnabled = true;

// WLAN Variablen mit Default-Werten aus config.h
String homeSSID = HOME_SSID;
String homePass = HOME_PASS;
String hotspotSSID = "";
String hotspotPass = "";
String apSSID = AP_SSID;
String apPass = AP_PASS;
uint8_t activeWifiMode = 1; // Standard: 1 = Heim-WLAN

static const String defaultDrivers[MAX_CARS + 1] = {
  "-", "Fahrer 1", "Fahrer 2", "Fahrer 3", "Fahrer 4", "Fahrer 5",
  "Fahrer 6", "Fahrer 7", "Fahrer 8", "Fahrer 9", "Fahrer 10",
  "Fahrer 11", "Fahrer 12", "Fahrer 13", "Fahrer 14", "Fahrer 15"
};

static const String defaultCars[MAX_CARS + 1] = {
  "-", "Auto 1", "Auto 2", "Auto 3", "Auto 4", "Auto 5",
  "Auto 6", "Auto 7", "Auto 8", "Auto 9", "Auto 10",
  "Auto 11", "Auto 12", "Auto 13", "Auto 14", "Auto 15"
};

#define QUEUE_MAX 30
static PendingLap lapQueue[QUEUE_MAX];
static uint8_t queueHead = 0;
static uint8_t queueTail = 0;

void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("[FS] Fehler beim Mounten von LittleFS");
  } else {
    Serial.println("[FS] LittleFS erfolgreich gestartet.");
  }
}

void loadWifiConfigFromFS() {
  if (LittleFS.exists("/wifi.json")) {
    File f = LittleFS.open("/wifi.json", "r");
    if (f) {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, f);
      f.close();
      if (!err) {
        if (doc["homeSSID"].is<String>()) homeSSID = doc["homeSSID"].as<String>();
        if (doc["homePass"].is<String>()) homePass = doc["homePass"].as<String>();
        if (doc["hotspotSSID"].is<String>()) hotspotSSID = doc["hotspotSSID"].as<String>();
        else if (doc["ssid"].is<String>()) hotspotSSID = doc["ssid"].as<String>();
        if (doc["hotspotPass"].is<String>()) hotspotPass = doc["hotspotPass"].as<String>();
        else if (doc["pass"].is<String>()) hotspotPass = doc["pass"].as<String>();
        if (doc["apSSID"].is<String>()) apSSID = doc["apSSID"].as<String>();
        if (doc["apPass"].is<String>()) apPass = doc["apPass"].as<String>();
        if (doc["mode"].is<uint8_t>()) activeWifiMode = doc["mode"].as<uint8_t>();
      }
    }
  }
}

void saveWifiConfigToFS(uint8_t mode, const String& hSsid, const String& hPass, const String& hsSsid, const String& hsPass, const String& aSsid, const String& aPass) {
  activeWifiMode = mode;
  homeSSID = hSsid;
  homePass = hPass;
  hotspotSSID = hsSsid;
  hotspotPass = hsPass;
  if (aSsid.length() > 0) apSSID = aSsid;
  if (aPass.length() > 0) apPass = aPass;

  JsonDocument doc;
  doc["mode"] = activeWifiMode;
  doc["homeSSID"] = homeSSID;
  doc["homePass"] = homePass;
  doc["hotspotSSID"] = hotspotSSID;
  doc["hotspotPass"] = hotspotPass;
  doc["apSSID"] = apSSID;
  doc["apPass"] = apPass;
  
  // Abwärtskompatibilität
  doc["ssid"] = hotspotSSID;
  doc["pass"] = hotspotPass;

  File f = LittleFS.open("/wifi.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void saveHotspotConfigToFS(const String& ssid, const String& pass) {
  hotspotSSID = ssid;
  hotspotPass = pass;
  saveWifiConfigToFS(activeWifiMode, homeSSID, homePass, hotspotSSID, hotspotPass, apSSID, apPass);
}

void loadNamesFromFS() {
  for (int i = 1; i <= MAX_CARS; i++) {
    driverNames[i] = defaultDrivers[i];
    carNames[i] = defaultCars[i];
  }

  if (LittleFS.exists("/names.json")) {
    File f = LittleFS.open("/names.json", "r");
    if (f) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, f);
      f.close();

      if (!error) {
        for (int i = 1; i <= MAX_CARS; i++) {
          String idStr = String(i);
          if (doc[idStr].is<JsonObject>()) {
            if (doc[idStr]["driver"].is<String>()) {
              driverNames[i] = doc[idStr]["driver"].as<String>();
            }
            if (doc[idStr]["car"].is<String>()) {
              carNames[i] = doc[idStr]["car"].as<String>();
            }
          }
        }
      }
    }
  }
}

void saveAllNamesToFS() {
  JsonDocument doc;
  for (int i = 1; i <= MAX_CARS; i++) {
    doc[String(i)]["driver"] = driverNames[i];
    doc[String(i)]["car"] = carNames[i];
  }

  File f = LittleFS.open("/names.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void loadSettingsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  loadNamesFromFS();
  loadWifiConfigFromFS();

  if (EEPROM.read(0) != EEPROM_MAGIC) {
    EEPROM.write(0, EEPROM_MAGIC);
    EEPROM.write(301, (uint8_t)(DEFAULT_MIN_LAP_MS & 0xFF));
    EEPROM.write(302, (uint8_t)((DEFAULT_MIN_LAP_MS >> 8) & 0xFF));
    EEPROM.write(303, 1);
    EEPROM.write(304, 1);
    EEPROM.write(305, 1);
    EEPROM.write(306, 1);
    EEPROM.write(307, 0);
    EEPROM.write(308, 0);
    EEPROM.write(309, 1);
    EEPROM.write(310, 1);
    EEPROM.commit();
    
    minLapTimeMs = DEFAULT_MIN_LAP_MS;
    requireTwoFrames = true;
    useSz1 = true;
    useSz2 = true;
    useSz3 = true;
    checkpointMode = false;
    dragraceMode = false;
    sectorAudioFeedback = true;
    webCacheEnabled = true;
  } else {
    uint16_t low = EEPROM.read(301);
    uint16_t high = EEPROM.read(302);
    minLapTimeMs = low | (high << 8);
    if (minLapTimeMs < 500 || minLapTimeMs > 30000) {
      minLapTimeMs = DEFAULT_MIN_LAP_MS;
    }

    requireTwoFrames = (EEPROM.read(303) != 0);
    useSz1 = (EEPROM.read(304) != 0);
    useSz2 = (EEPROM.read(305) != 0);
    useSz3 = (EEPROM.read(306) != 0);
    checkpointMode = (EEPROM.read(307) != 0);
    dragraceMode = (EEPROM.read(308) != 0);
    sectorAudioFeedback = (EEPROM.read(309) != 0);
    webCacheEnabled = (EEPROM.read(310) != 0);
  }
}

void saveNamesToEEPROM(int id, const String& driver, const String& car) {
  if (id >= 1 && id <= MAX_CARS) {
    driverNames[id] = driver;
    carNames[id] = car;
    saveAllNamesToFS();
  }
}

void saveDeadtimeToEEPROM(uint16_t ms) {
  int addr = 301;
  EEPROM.write(addr, (uint8_t)(ms & 0xFF));
  EEPROM.write(addr + 1, (uint8_t)((ms >> 8) & 0xFF));
  EEPROM.commit();
}

void saveFilterToEEPROM(bool enable) {
  int addr = 303;
  EEPROM.write(addr, enable ? 1 : 0);
  EEPROM.commit();
}

void saveCacheConfigToEEPROM(bool enable) {
  EEPROM.write(310, enable ? 1 : 0);
  EEPROM.commit();
}

void saveSectorConfigToEEPROM(bool s1, bool s2, bool s3, bool cp, bool dr, bool audio) {
  EEPROM.write(304, s1 ? 1 : 0);
  EEPROM.write(305, s2 ? 1 : 0);
  EEPROM.write(306, s3 ? 1 : 0);
  EEPROM.write(307, cp ? 1 : 0);
  EEPROM.write(308, dr ? 1 : 0);
  EEPROM.write(309, audio ? 1 : 0);
  EEPROM.commit();
}

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs) {
  uint8_t nextHead = (queueHead + 1) % QUEUE_MAX;
  if (nextHead != queueTail) { 
    lapQueue[queueHead] = {carId, lapNum, lapTimeMs, sz1, sz2, sz3, sz4, position, mode, timestampMs};
    queueHead = nextHead;
  }
}

void processStorageQueue() {
  static uint32_t lastStorageWrite = 0;
  if (millis() - lastStorageWrite < 120) return;

  if (queueHead != queueTail) { 
    PendingLap lap = lapQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_MAX;
    saveLapToHistory(lap.carId, lap.lapNum, lap.lapTimeMs, lap.sz1, lap.sz2, lap.sz3, lap.sz4, lap.position, lap.mode, lap.timestampMs);
    lastStorageWrite = millis();
  }
}

void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs) {
  String filepath = "/stats_" + String(carId) + ".json";
  
  char newLapJson[256];
  snprintf(newLapJson, sizeof(newLapJson), "{\"lap\":%u,\"time\":%lu,\"sz1\":%lu,\"sz2\":%lu,\"sz3\":%lu,\"sz4\":%lu,\"mode\":%u,\"ts\":%llu,\"pos\":%u}",
           lapNum, lapTimeMs, sz1, sz2, sz3, sz4, mode, timestampMs, position);

  if (!LittleFS.exists(filepath)) {
    File f = LittleFS.open(filepath, "w");
    if (f) {
      f.print("{\"laps\":[");
      f.print(newLapJson);
      f.print("]}");
      f.close();
    }
    return;
  }

  File f = LittleFS.open(filepath, "r+");
  if (!f) return;

  size_t size = f.size();
  
  if (size > 15000) {
    f.close();
    JsonDocument doc;
    File fRead = LittleFS.open(filepath, "r");
    if (fRead) {
      deserializeJson(doc, fRead);
      fRead.close();
    }

    JsonArray laps = doc["laps"].as<JsonArray>();
    if (!laps) laps = doc["laps"].to<JsonArray>();

    for (JsonObject existingLap : laps) {
      if (existingLap["lap"] == lapNum && existingLap["time"] == lapTimeMs && existingLap["mode"] == mode) {
        return; 
      }
    }

    if (laps.size() >= 200) {
      laps.remove(0);
    }

    JsonObject newLap = laps.add<JsonObject>();
    newLap["lap"] = lapNum;
    newLap["time"] = lapTimeMs;
    newLap["sz1"] = sz1;
    newLap["sz2"] = sz2;
    newLap["sz3"] = sz3;
    newLap["sz4"] = sz4;
    newLap["mode"] = mode;
    newLap["ts"] = timestampMs;
    newLap["pos"] = position;

    File fWrite = LittleFS.open(filepath, "w");
    if (fWrite) {
      serializeJson(doc, fWrite);
      fWrite.close();
    }
    return;
  }

  f.seek(size - 2, SeekSet);
  char lastChars[2];
  f.readBytes(lastChars, 2);

  if (lastChars[0] == '[' && lastChars[1] == ']') {
     f.seek(size - 1, SeekSet);
     f.print(newLapJson);
     f.print("]}");
  } else {
     f.seek(size - 2, SeekSet);
     f.print(",");
     f.print(newLapJson);
     f.print("]}");
  }
  f.close();
}

void deleteLapFromHistory(uint8_t carId, uint16_t lapNum) {
  String filepath = "/stats_" + String(carId) + ".json";
  if (!LittleFS.exists(filepath)) return;

  JsonDocument doc;
  File f = LittleFS.open(filepath, "r");
  if (f) {
    deserializeJson(doc, f);
    f.close();
  }

  JsonArray laps = doc["laps"].as<JsonArray>();
  if (laps) {
    for (size_t i = 0; i < laps.size(); i++) {
      if (laps[i]["lap"] == lapNum) {
        laps.remove(i);
        break;
      }
    }
  }

  File fWrite = LittleFS.open(filepath, "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
}

void deleteDriverHistory(uint8_t carId) {
  String filepath = "/stats_" + String(carId) + ".json";
  if (LittleFS.exists(filepath)) {
    LittleFS.remove(filepath);
  }
}

void deleteAllHistory() {
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    deleteDriverHistory(i);
  }
  if (LittleFS.exists("/sessions.json")) {
    LittleFS.remove("/sessions.json");
  }
}

void saveSessionToHistory(const String& sessionJsonStr) {
  String filepath = "/sessions.json";
  JsonDocument doc;
  
  if (LittleFS.exists(filepath)) {
    File f = LittleFS.open(filepath, "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
  }

  JsonArray sessions = doc["sessions"].as<JsonArray>();
  if (!sessions) {
    sessions = doc["sessions"].to<JsonArray>();
  }

  JsonDocument incomingSessionDoc;
  DeserializationError err = deserializeJson(incomingSessionDoc, sessionJsonStr);
  if (err) return;

  JsonObject incomingObj = incomingSessionDoc.as<JsonObject>();
  uint64_t incomingTs = incomingObj["ts"] | 0;

  bool updated = false;
  if (incomingTs > 0) {
    for (size_t i = 0; i < sessions.size(); i++) {
      if (sessions[i]["ts"] == incomingTs) {
        sessions[i] = incomingObj;
        updated = true;
        break;
      }
    }
  }

  if (!updated) {
    if (sessions.size() >= 30) {
      sessions.remove(0);
    }
    sessions.add(incomingObj);
  }

  File f = LittleFS.open(filepath, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void deleteSessionFromHistory(uint64_t sessionTs) {
  String filepath = "/sessions.json";
  if (!LittleFS.exists(filepath)) return;

  JsonDocument doc;
  File f = LittleFS.open(filepath, "r");
  if (f) {
    deserializeJson(doc, f);
    f.close();
  }

  JsonArray sessions = doc["sessions"].as<JsonArray>();
  if (sessions) {
    for (size_t i = 0; i < sessions.size(); i++) {
      if (sessions[i]["ts"] == sessionTs) {
        sessions.remove(i);
        break;
      }
    }
  }

  File fWrite = LittleFS.open(filepath, "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
}