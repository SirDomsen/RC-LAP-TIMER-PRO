#include "storage.h"
#include "lap_logic.h"
#include <EEPROM.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

String driverNames[MAX_CARS + 1];
String carNames[MAX_CARS + 1];

static const String defaultDrivers[MAX_CARS + 1] = {
  "-", "Fahrer 1", "Fahrer 2", "Fahrer 3", "Fahrer 4", "Fahrer 5",
  "Fahrer 6", "Fahrer 7", "Fahrer 8", "Fahrer 9", "Fahrer 10",
  "Fahrer 11", "Fahrer 12", "Fahrer 13", "Fahrer 14", "Fahrer 15"
};

static const String defaultCars[MAX_CARS + 1] = {
  "-", "Arrma Gorgon", "Arrma Typhon Grom", "Auto 3", "Auto 4", "Auto 5",
  "Auto 6", "Auto 7", "Auto 8", "Auto 9", "Auto 10",
  "Auto 11", "Auto 12", "Auto 13", "Auto 14", "Auto 15"
};

// Queue für asynchrones Flash-Schreiben
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

  if (EEPROM.read(0) != EEPROM_MAGIC) {
    EEPROM.write(0, EEPROM_MAGIC);
    
    EEPROM.write(301, (uint8_t)(DEFAULT_MIN_LAP_MS & 0xFF));
    EEPROM.write(302, (uint8_t)((DEFAULT_MIN_LAP_MS >> 8) & 0xFF));
    EEPROM.write(303, 1);
    EEPROM.commit();
    
    minLapTimeMs = DEFAULT_MIN_LAP_MS;
    requireTwoFrames = true;
  } else {
    uint16_t low = EEPROM.read(301);
    uint16_t high = EEPROM.read(302);
    minLapTimeMs = low | (high << 8);
    if (minLapTimeMs < 500 || minLapTimeMs > 30000) {
      minLapTimeMs = DEFAULT_MIN_LAP_MS;
    }

    uint8_t filterVal = EEPROM.read(303);
    requireTwoFrames = (filterVal != 0);
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

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint8_t position, uint8_t mode, uint64_t timestampMs) {
  uint8_t nextHead = (queueHead + 1) % QUEUE_MAX;
  if (nextHead != queueTail) { // Puffer nicht voll
    lapQueue[queueHead] = {carId, lapNum, lapTimeMs, position, mode, timestampMs};
    queueHead = nextHead;
  }
}

void processStorageQueue() {
  if (queueHead == queueTail) return; // Queue leer

  PendingLap lap = lapQueue[queueTail];
  queueTail = (queueTail + 1) % QUEUE_MAX;

  saveLapToHistory(lap.carId, lap.lapNum, lap.lapTimeMs, lap.position, lap.mode, lap.timestampMs);
}

void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint8_t position, uint8_t mode, uint64_t timestampMs) {
  String filepath = "/stats_" + String(carId) + ".json";
  
  JsonDocument doc;
  if (LittleFS.exists(filepath)) {
    File f = LittleFS.open(filepath, "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
  }

  JsonArray laps = doc["laps"].as<JsonArray>();
  if (!laps) {
    laps = doc["laps"].to<JsonArray>();
  }

  // Begrenzung: Max. 100 Runden pro Fahrzeug speichern, um den RAM beim Laden/Parsen nicht zu überlasten
  if (laps.size() >= 100) {
    laps.remove(0);
  }

  JsonObject newLap = laps.add<JsonObject>();
  newLap["lap"] = lapNum;
  newLap["time"] = lapTimeMs;
  newLap["mode"] = mode;
  newLap["ts"] = timestampMs;
  if (mode == 2 || mode == 3) {
    newLap["pos"] = position;
  }

  File f = LittleFS.open(filepath, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

String getDriverHistoryJson(uint8_t carId) {
  String filepath = "/stats_" + String(carId) + ".json";
  if (!LittleFS.exists(filepath)) return "{\"laps\":[]}";

  File f = LittleFS.open(filepath, "r");
  if (!f) return "{\"laps\":[]}";
  String content = f.readString();
  f.close();
  return content;
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

  // Begrenzung: Max. 20 Sessions speichern
  if (sessions.size() >= 20) {
    sessions.remove(0);
  }

  JsonDocument newSessionDoc;
  deserializeJson(newSessionDoc, sessionJsonStr);
  sessions.add(newSessionDoc.as<JsonObject>());

  File f = LittleFS.open(filepath, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

String getSessionsJson() {
  String filepath = "/sessions.json";
  if (!LittleFS.exists(filepath)) return "{\"sessions\":[]}";

  File f = LittleFS.open(filepath, "r");
  if (!f) return "{\"sessions\":[]}";
  String content = f.readString();
  f.close();
  return content;
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