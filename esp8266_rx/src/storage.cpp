#include "storage.h"
#include "lap_logic.h"
#include "web_server.h"
#include <EEPROM.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

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
String learnedApIP = "";
uint8_t activeWifiMode = 1; // Standard: 1 = Heim-WLAN

// Eindeutige Hardware-Identifikatoren
String getChipIdSuffix() {
  char buf[8];
  snprintf(buf, sizeof(buf), "%04X", (uint16_t)(ESP.getChipId() & 0xFFFF));
  return String(buf);
}

String getDefaultApSSID() {
  return String(AP_SSID) + "-" + getChipIdSuffix();
}

String getDeviceHostname() {
  return String("RCLaptimerPro-") + getChipIdSuffix();
}

String getOtaHostname() {
  return String("LapTimer-Base-") + getChipIdSuffix();
}

// Aktive Strecke & Speicherstatus
String activeTrackId = "standard";
String activeTrackName = "Test-Strecke";
uint8_t cachedFsUsedPct = 0;
uint32_t cachedFsUsedBytes = 0;
uint32_t cachedFsTotalBytes = 0;
bool storageFullLocked = false;
int32_t clientTzOffsetSec = 3600; // Default UTC+1 (MEZ), wird vom Browser exakt synchronisiert

// Top-5 Rekordlisten der aktiven Strecke
RecordEntry activeAllTimeRecords[MAX_TRACK_RECORDS];
uint8_t activeAllTimeCount = 0;
RecordEntry activeDailyRecords[MAX_TRACK_RECORDS];
uint8_t activeDailyCount = 0;

// Laufende Session-Verwaltung
static String currentSessionId = "";
static String currentSessionFilePath = "";
static bool currentSessionActive = false;
static uint64_t currentSessionStartSec = 0;
static uint64_t sessionLastEndSec = 0;
static uint8_t currentSessionMode = 0;
static uint32_t currentSessionIndexOffset = 0;
static size_t currentSessionIndexLen = 0;
static bool sessionIndexDirty = false;
static uint32_t lastIndexFlushMs = 0;

static uint16_t sessionCarLaps[MAX_CARS + 1] = {0};
static uint32_t sessionCarBest[MAX_CARS + 1] = {0};
static uint16_t sessionCarValidLaps[MAX_CARS + 1] = {0};
static uint64_t sessionCarTimeSum[MAX_CARS + 1] = {0};
static uint32_t sessionCarAvg[MAX_CARS + 1] = {0};

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

static const char* getModeFolderName(uint8_t mode) {
  switch (mode) {
    case 0: return "training";
    case 1: return "quali";
    case 2: return "race_laps";
    case 3: return "race_time";
    case 4: return "drag";
    default: return "training";
  }
}

static const char* getModeDisplayName(uint8_t mode) {
  switch (mode) {
    case 0: return "Freies Training";
    case 1: return "Qualifying";
    case 2: return "Rennen (Runden)";
    case 3: return "Rennen (Zeit)";
    case 4: return "Dragrace";
    default: return "Freies Training";
  }
}

static String formatTimestampId(uint64_t tsSec) {
  time_t localT = (time_t)(tsSec + clientTzOffsetSec);
  struct tm *tm_info = gmtime(&localT);
  char buf[24];
  if (tm_info) {
    snprintf(buf, sizeof(buf), "%02d-%02d-%02d_%02d-%02d-%02d",
             (tm_info->tm_year + 1900) % 100,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
  } else {
    snprintf(buf, sizeof(buf), "%llu", tsSec);
  }
  return String(buf);
}

String getCurrentLocalDateStr() {
  if (baseUnixTimestampSec == 0) return "";
  uint64_t nowSec = baseUnixTimestampSec + ((millis() - baseMillis) / 1000UL);
  time_t localT = (time_t)(nowSec + clientTzOffsetSec);
  struct tm *tm_info = gmtime(&localT);
  if (!tm_info) return "";
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d-%02d-%02d",
           (tm_info->tm_year + 1900) % 100,
           tm_info->tm_mon + 1,
           tm_info->tm_mday);
  return String(buf);
}

static void clearRecordList(RecordEntry* list, uint8_t& count) {
  count = 0;
  for (uint8_t i = 0; i < MAX_TRACK_RECORDS; i++) {
    list[i].lapMs = 0;
    list[i].carId = 0;
    list[i].driver = "";
    list[i].car = "";
    list[i].date = "";
    list[i].prevLapMs = 0;
    list[i].prevDate = "";
  }
}

static void sortRecordList(RecordEntry* list, uint8_t& count) {
  for (uint8_t i = 0; i < count; i++) {
    for (uint8_t j = i + 1; j < count; j++) {
      if (list[j].lapMs > 0 && (list[i].lapMs == 0 || list[j].lapMs < list[i].lapMs)) {
        RecordEntry tmp = list[i];
        list[i] = list[j];
        list[j] = tmp;
      }
    }
  }
  while (count > 0 && list[count - 1].lapMs == 0) {
    count--;
  }
}

void syncTopRecordVars() {
  if (activeAllTimeCount > 0 && activeAllTimeRecords[0].lapMs > 0) {
    allTimeBestLapMs = activeAllTimeRecords[0].lapMs;
    allTimeBestCarId = activeAllTimeRecords[0].carId;
    allTimeBestDriver = activeAllTimeRecords[0].driver;
    allTimeBestCar = activeAllTimeRecords[0].car;
    allTimeBestDate = activeAllTimeRecords[0].date;
  } else {
    allTimeBestLapMs = 0;
    allTimeBestCarId = 0;
    allTimeBestDriver = "";
    allTimeBestCar = "";
    allTimeBestDate = "";
  }

  if (activeDailyCount > 0 && activeDailyRecords[0].lapMs > 0) {
    dailyBestLapMs = activeDailyRecords[0].lapMs;
    dailyBestCarId = activeDailyRecords[0].carId;
    dailyBestDriver = activeDailyRecords[0].driver;
    dailyBestCar = activeDailyRecords[0].car;
    dailyBestDate = activeDailyRecords[0].date;
  } else {
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    String today = getCurrentLocalDateStr();
    if (today.length() > 0) dailyBestDate = today;
  }
}

static bool updateRecordList(RecordEntry* list, uint8_t& count, uint32_t lapMs, uint8_t carId, const String& driver, const String& car, const String& date) {
  if (lapMs == 0) return false;

  // 1. Prüfen, ob Kombination (Fahrer + Fahrzeug) bereits in der Liste existiert
  int8_t existingIdx = -1;
  for (uint8_t i = 0; i < count; i++) {
    if (list[i].driver.equalsIgnoreCase(driver) && list[i].car.equalsIgnoreCase(car)) {
      existingIdx = (int8_t)i;
      break;
    }
  }

  if (existingIdx >= 0) {
    if (lapMs < list[existingIdx].lapMs) {
      list[existingIdx].prevLapMs = list[existingIdx].lapMs;
      list[existingIdx].prevDate = list[existingIdx].date;
      list[existingIdx].lapMs = lapMs;
      list[existingIdx].carId = carId;
      list[existingIdx].driver = driver;
      list[existingIdx].car = car;
      list[existingIdx].date = date;
      sortRecordList(list, count);
      return true;
    }
    return false;
  }

  // 2. Kombination noch nicht vertreten -> Top 5 prüfen
  if (count < MAX_TRACK_RECORDS) {
    list[count].lapMs = lapMs;
    list[count].carId = carId;
    list[count].driver = driver;
    list[count].car = car;
    list[count].date = date;
    list[count].prevLapMs = 0;
    list[count].prevDate = "";
    count++;
    sortRecordList(list, count);
    return true;
  } else if (lapMs < list[MAX_TRACK_RECORDS - 1].lapMs) {
    list[MAX_TRACK_RECORDS - 1].lapMs = lapMs;
    list[MAX_TRACK_RECORDS - 1].carId = carId;
    list[MAX_TRACK_RECORDS - 1].driver = driver;
    list[MAX_TRACK_RECORDS - 1].car = car;
    list[MAX_TRACK_RECORDS - 1].date = date;
    list[MAX_TRACK_RECORDS - 1].prevLapMs = 0;
    list[MAX_TRACK_RECORDS - 1].prevDate = "";
    sortRecordList(list, count);
    return true;
  }

  return false;
}

static void loadRecordArrayFromJson(JsonObject trackObj, const char* key, RecordEntry* list, uint8_t& count) {
  clearRecordList(list, count);
  if (trackObj[key].is<JsonArray>()) {
    JsonArray arr = trackObj[key].as<JsonArray>();
    for (JsonObject item : arr) {
      if (count >= MAX_TRACK_RECORDS) break;
      uint32_t lap = item["lap"] | 0;
      if (lap == 0) continue;
      list[count].lapMs = lap;
      list[count].carId = item["carId"] | 0;
      list[count].driver = item["driver"] | "";
      list[count].car = item["car"] | "";
      list[count].date = item["date"] | "";
      list[count].prevLapMs = item["prevLap"] | 0;
      list[count].prevDate = item["prevDate"] | "";
      count++;
    }
    sortRecordList(list, count);
  }
}

static void saveRecordArrayToJson(JsonObject trackObj, const char* key, const RecordEntry* list, uint8_t count) {
  JsonArray arr = trackObj[key].to<JsonArray>();
  for (uint8_t i = 0; i < count && i < MAX_TRACK_RECORDS; i++) {
    if (list[i].lapMs == 0) continue;
    JsonObject item = arr.add<JsonObject>();
    item["lap"] = list[i].lapMs;
    item["carId"] = list[i].carId;
    item["driver"] = list[i].driver;
    item["car"] = list[i].car;
    item["date"] = list[i].date;
    if (list[i].prevLapMs > 0) {
      item["prevLap"] = list[i].prevLapMs;
      item["prevDate"] = list[i].prevDate;
    }
  }
}

static void loadActiveTrackRecordsFromJsonObj(JsonObject foundObj, bool& needsSaveBack) {
  String today = getCurrentLocalDateStr();

  loadRecordArrayFromJson(foundObj, "allTimeRecords", activeAllTimeRecords, activeAllTimeCount);
  if (activeAllTimeCount == 0) {
    uint32_t legacyRec = foundObj["recordLap"] | (foundObj["bestLap"] | 0);
    if (legacyRec > 0) {
      activeAllTimeRecords[0].lapMs = legacyRec;
      activeAllTimeRecords[0].carId = foundObj["recordCarId"] | (foundObj["bestCarId"] | 0);
      activeAllTimeRecords[0].driver = foundObj["recordDriver"] | (foundObj["bestDriver"] | "");
      activeAllTimeRecords[0].car = foundObj["recordCar"] | (foundObj["bestCar"] | "");
      activeAllTimeRecords[0].date = foundObj["recordDate"] | (foundObj["bestDate"] | today);
      activeAllTimeRecords[0].prevLapMs = 0;
      activeAllTimeRecords[0].prevDate = "";
      activeAllTimeCount = 1;
      needsSaveBack = true;
    }
  }

  String savedDailyDate = foundObj["bestDate"] | "";
  if (today.length() > 0 && savedDailyDate.length() > 0 && savedDailyDate != today) {
    clearRecordList(activeDailyRecords, activeDailyCount);
    foundObj["bestLap"] = 0;
    foundObj["bestCarId"] = 0;
    foundObj["bestDriver"] = "";
    foundObj["bestCar"] = "";
    foundObj["bestDate"] = today;
    foundObj["dailyRecords"].to<JsonArray>();
    needsSaveBack = true;
  } else {
    loadRecordArrayFromJson(foundObj, "dailyRecords", activeDailyRecords, activeDailyCount);
    if (activeDailyCount == 0) {
      uint32_t legacyDaily = foundObj["bestLap"] | 0;
      if (legacyDaily > 0 && (today.length() == 0 || savedDailyDate.length() == 0 || savedDailyDate == today)) {
        activeDailyRecords[0].lapMs = legacyDaily;
        activeDailyRecords[0].carId = foundObj["bestCarId"] | 0;
        activeDailyRecords[0].driver = foundObj["bestDriver"] | "";
        activeDailyRecords[0].car = foundObj["bestCar"] | "";
        activeDailyRecords[0].date = savedDailyDate.length() > 0 ? savedDailyDate : today;
        activeDailyRecords[0].prevLapMs = 0;
        activeDailyRecords[0].prevDate = "";
        activeDailyCount = 1;
        needsSaveBack = true;
      }
    }
  }

  syncTopRecordVars();
  if (dailyBestDate.length() == 0 && savedDailyDate.length() > 0) {
    dailyBestDate = savedDailyDate;
  }
}

void checkDailyBestDateRollover() {
  static uint32_t lastCheckMs = 0;
  if (millis() - lastCheckMs < 5000) return;
  lastCheckMs = millis();
  if (baseUnixTimestampSec == 0) return;

  String today = getCurrentLocalDateStr();
  if (today.length() == 0) return;

  if (dailyBestDate.length() == 0) {
    dailyBestDate = today;
    for (uint8_t i = 0; i < activeDailyCount; i++) {
      if (activeDailyRecords[i].date.length() == 0) activeDailyRecords[i].date = today;
    }
    for (uint8_t i = 0; i < activeAllTimeCount; i++) {
      if (activeAllTimeRecords[i].date.length() == 0) activeAllTimeRecords[i].date = today;
    }
    return;
  }

  if (dailyBestDate != today) {
    clearRecordList(activeDailyRecords, activeDailyCount);
    dailyBestDate = today;
    syncTopRecordVars();
    saveTracksToFS();

    char buf[160];
    snprintf(buf, sizeof(buf), "{\"type\":\"track_record\",\"trackId\":\"%s\",\"trackName\":\"%s\",\"bestLap\":0}",
             activeTrackId.c_str(), activeTrackName.c_str());
    broadcastWebSocket(buf);
  }
}

void checkAndUpdateTrackRecords(uint8_t carId, uint32_t lapTimeMs, bool& isNewAllTimeBest, bool& isNewDailyBest) {
  isNewAllTimeBest = false;
  isNewDailyBest = false;
  if (lapTimeMs == 0 || carId < 1 || carId > MAX_CARS) return;

  String today = getCurrentLocalDateStr();
  if (today.length() > 0 && dailyBestDate.length() > 0 && dailyBestDate != today) {
    clearRecordList(activeDailyRecords, activeDailyCount);
    dailyBestDate = today;
    syncTopRecordVars();
  }

  String dName = driverNames[carId];
  String cName = carNames[carId];

  bool wasNewP1AllTime = (activeAllTimeCount == 0 || lapTimeMs < activeAllTimeRecords[0].lapMs);
  bool wasNewP1Daily = (activeDailyCount == 0 || lapTimeMs < activeDailyRecords[0].lapMs);

  bool changedAllTime = updateRecordList(activeAllTimeRecords, activeAllTimeCount, lapTimeMs, carId, dName, cName, today);
  bool changedDaily = updateRecordList(activeDailyRecords, activeDailyCount, lapTimeMs, carId, dName, cName, today);

  if (changedAllTime || changedDaily) {
    if (changedAllTime && wasNewP1AllTime) isNewAllTimeBest = true;
    if (changedDaily && wasNewP1Daily) isNewDailyBest = true;
    syncTopRecordVars();
    saveTracksToFS();

    char buf[160];
    snprintf(buf, sizeof(buf), "{\"type\":\"track_record\",\"trackId\":\"%s\",\"recordLap\":%lu,\"bestLap\":%lu}",
             activeTrackId.c_str(), allTimeBestLapMs, dailyBestLapMs);
    broadcastWebSocket(buf);
  }
}

static bool removeEntryFromList(RecordEntry* list, uint8_t& count, int8_t index, uint32_t& deletedLapOut, uint8_t& deletedCarIdOut, uint32_t& restoredLapOut) {
  deletedLapOut = 0;
  deletedCarIdOut = 0;
  restoredLapOut = 0;

  if (index < 0) {
    clearRecordList(list, count);
    return true;
  }
  if ((uint8_t)index >= count) return false;

  deletedLapOut = list[index].lapMs;
  deletedCarIdOut = list[index].carId;
  restoredLapOut = list[index].prevLapMs;

  if (list[index].prevLapMs > 0) {
    list[index].lapMs = list[index].prevLapMs;
    list[index].date = list[index].prevDate;
    list[index].prevLapMs = 0;
    list[index].prevDate = "";
    sortRecordList(list, count);
  } else {
    list[index].lapMs = 0;
    sortRecordList(list, count);
  }
  return true;
}

bool deleteTrackRecordEntry(const String& trackId, bool isDaily, int8_t index) {
  String targetId = trackId.length() > 0 ? trackId : activeTrackId;
  uint32_t deletedLap = 0;
  uint8_t deletedCarId = 0;
  uint32_t restoredLap = 0;

  if (targetId == activeTrackId) {
    bool ok = false;
    if (isDaily) {
      ok = removeEntryFromList(activeDailyRecords, activeDailyCount, index, deletedLap, deletedCarId, restoredLap);
    } else {
      ok = removeEntryFromList(activeAllTimeRecords, activeAllTimeCount, index, deletedLap, deletedCarId, restoredLap);
    }
    if (!ok) return false;

    if (deletedCarId >= 1 && deletedCarId <= MAX_CARS && deletedLap > 0 && bestLapDuration[deletedCarId] == deletedLap) {
      bestLapDuration[deletedCarId] = restoredLap;
    }

    syncTopRecordVars();
    saveTracksToFS();
  } else {
    if (!LittleFS.exists("/tracks.json")) return false;
    File f = LittleFS.open("/tracks.json", "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) {
      f.close();
      return false;
    }
    f.close();

    JsonArray arr = doc["tracks"].as<JsonArray>();
    bool found = false;
    for (JsonObject t : arr) {
      if (t["id"].as<String>() == targetId) {
        RecordEntry tempList[MAX_TRACK_RECORDS];
        uint8_t tempCount = 0;
        const char* arrKey = isDaily ? "dailyRecords" : "allTimeRecords";
        loadRecordArrayFromJson(t, arrKey, tempList, tempCount);
        if (!removeEntryFromList(tempList, tempCount, index, deletedLap, deletedCarId, restoredLap)) {
          return false;
        }
        saveRecordArrayToJson(t, arrKey, tempList, tempCount);
        if (isDaily) {
          if (tempCount > 0 && tempList[0].lapMs > 0) {
            t["bestLap"] = tempList[0].lapMs;
            t["bestCarId"] = tempList[0].carId;
            t["bestDriver"] = tempList[0].driver;
            t["bestCar"] = tempList[0].car;
            t["bestDate"] = tempList[0].date;
          } else {
            t["bestLap"] = 0;
            t["bestCarId"] = 0;
            t["bestDriver"] = "";
            t["bestCar"] = "";
          }
        } else {
          if (tempCount > 0 && tempList[0].lapMs > 0) {
            t["recordLap"] = tempList[0].lapMs;
            t["recordCarId"] = tempList[0].carId;
            t["recordDriver"] = tempList[0].driver;
            t["recordCar"] = tempList[0].car;
            t["recordDate"] = tempList[0].date;
          } else {
            t["recordLap"] = 0;
            t["recordCarId"] = 0;
            t["recordDriver"] = "";
            t["recordCar"] = "";
            t["recordDate"] = "";
          }
        }
        found = true;
        break;
      }
    }
    if (!found) return false;

    File fW = LittleFS.open("/tracks.json", "w");
    if (fW) {
      serializeJson(doc, fW);
      fW.close();
    }
  }

  char buf[160];
  snprintf(buf, sizeof(buf), "{\"type\":\"track_record\",\"trackId\":\"%s\",\"recordLap\":%lu,\"bestLap\":%lu}",
           activeTrackId.c_str(), allTimeBestLapMs, dailyBestLapMs);
  broadcastWebSocket(buf);
  return true;
}

static String escapeJsonStr(const String& s) {
  String out;
  out.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c >= 32) out += c;
  }
  return out;
}

void updateFsUsageCache() {
  FSInfo fs_info;
  if (LittleFS.info(fs_info) && fs_info.totalBytes > 0) {
    cachedFsTotalBytes = fs_info.totalBytes;
    cachedFsUsedBytes = fs_info.usedBytes;
    cachedFsUsedPct = (uint8_t)((fs_info.usedBytes * 100ULL) / fs_info.totalBytes);
    if (cachedFsUsedPct < 95) {
      storageFullLocked = false;
    }
  }
}

void initFS() {
  if (LittleFS.begin()) {
    updateFsUsageCache();
  }
}

// --- STRECKEN-VERWALTUNG (/tracks.json) ---
String sanitizeTrackSlug(const String& rawName) {
  String s = rawName;
  s.trim();
  s.replace("ä", "ae"); s.replace("Ä", "ae");
  s.replace("ö", "oe"); s.replace("Ö", "oe");
  s.replace("ü", "ue"); s.replace("Ü", "ue");
  s.replace("ß", "ss");

  String slug = "";
  bool lastUnderscore = false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      slug += c;
      lastUnderscore = false;
    } else if (!lastUnderscore && slug.length() > 0) {
      slug += '_';
      lastUnderscore = true;
    }
    if (slug.length() >= 14) break;
  }
  while (slug.endsWith("_")) {
    slug.remove(slug.length() - 1);
  }
  if (slug.length() == 0) slug = "strecke";
  return slug;
}

void loadTracksFromFS() {
  bool loaded = false;
  bool needsSaveBack = false;
  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, f);
      f.close();
      if (!err && doc["tracks"].is<JsonArray>()) {
        String act = doc["active"] | "standard";
        JsonArray arr = doc["tracks"].as<JsonArray>();
        if (arr.size() > 0) {
          JsonObject foundObj;
          for (JsonObject t : arr) {
            if (t["id"].as<String>() == act) {
              foundObj = t;
              break;
            }
          }
          if (foundObj.isNull()) {
            foundObj = arr[0];
          }
          activeTrackId = foundObj["id"] | "standard";
          activeTrackName = foundObj["name"] | "Test-Strecke";
          loadActiveTrackRecordsFromJsonObj(foundObj, needsSaveBack);
          loaded = true;
        }
      }
    }
  }

  if (!loaded) {
    activeTrackId = "standard";
    activeTrackName = "Test-Strecke";
    clearRecordList(activeAllTimeRecords, activeAllTimeCount);
    clearRecordList(activeDailyRecords, activeDailyCount);
    syncTopRecordVars();
    saveTracksToFS();
  } else if (needsSaveBack) {
    saveTracksToFS();
  }
}

void saveTracksToFS() {
  JsonDocument doc;
  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
  }

  doc["active"] = activeTrackId;
  JsonArray arr = doc["tracks"].as<JsonArray>();
  if (!arr) {
    arr = doc["tracks"].to<JsonArray>();
  }

  String today = getCurrentLocalDateStr();
  syncTopRecordVars();

  bool found = false;
  for (JsonObject t : arr) {
    if (t["id"].as<String>() == activeTrackId) {
      t["name"] = activeTrackName;
      t["recordLap"] = allTimeBestLapMs;
      t["recordCarId"] = allTimeBestCarId;
      t["recordDriver"] = allTimeBestDriver;
      t["recordCar"] = allTimeBestCar;
      t["recordDate"] = allTimeBestDate;
      t["bestLap"] = dailyBestLapMs;
      t["bestCarId"] = dailyBestCarId;
      t["bestDriver"] = dailyBestDriver;
      t["bestCar"] = dailyBestCar;
      t["bestDate"] = dailyBestDate;
      saveRecordArrayToJson(t, "allTimeRecords", activeAllTimeRecords, activeAllTimeCount);
      saveRecordArrayToJson(t, "dailyRecords", activeDailyRecords, activeDailyCount);
      found = true;
    } else if (today.length() > 0) {
      // Auch bei inaktiven Strecken abgelaufene Tagesbestzeiten von gestern bereinigen
      String tDate = t["bestDate"] | "";
      if (tDate.length() > 0 && tDate != today) {
        t["bestLap"] = 0;
        t["bestCarId"] = 0;
        t["bestDriver"] = "";
        t["bestCar"] = "";
        t["bestDate"] = today;
        t["dailyRecords"].to<JsonArray>();
      }
    }
  }

  if (!found) {
    JsonObject t = arr.add<JsonObject>();
    t["id"] = activeTrackId;
    t["name"] = activeTrackName;
    t["recordLap"] = allTimeBestLapMs;
    t["recordCarId"] = allTimeBestCarId;
    t["recordDriver"] = allTimeBestDriver;
    t["recordCar"] = allTimeBestCar;
    t["recordDate"] = allTimeBestDate;
    t["bestLap"] = dailyBestLapMs;
    t["bestCarId"] = dailyBestCarId;
    t["bestDriver"] = dailyBestDriver;
    t["bestCar"] = dailyBestCar;
    t["bestDate"] = dailyBestDate;
    saveRecordArrayToJson(t, "allTimeRecords", activeAllTimeRecords, activeAllTimeCount);
    saveRecordArrayToJson(t, "dailyRecords", activeDailyRecords, activeDailyCount);
  }

  File fWrite = LittleFS.open("/tracks.json", "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
  updateFsUsageCache();
}

void saveActiveTrackBestToFS() {
  saveTracksToFS();
}

bool selectTrackById(const String& trackId) {
  if (!LittleFS.exists("/tracks.json")) return false;
  File f = LittleFS.open("/tracks.json", "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  JsonArray arr = doc["tracks"].as<JsonArray>();
  for (JsonObject t : arr) {
    if (t["id"].as<String>() == trackId) {
      startNewSessionFrame((uint8_t)currentMode);
      activeTrackId = t["id"].as<String>();
      activeTrackName = t["name"].as<String>();
      bool dummy = false;
      loadActiveTrackRecordsFromJsonObj(t, dummy);
      doc["active"] = activeTrackId;
      saveTracksToFS();
      resetAllLaps(false);
      return true;
    }
  }
  return false;
}

bool addOrSelectTrack(const String& rawName) {
  String cleanName = rawName;
  cleanName.trim();
  if (cleanName.length() == 0) return false;
  if (cleanName.length() > 32) cleanName = cleanName.substring(0, 32);

  String slug = sanitizeTrackSlug(cleanName);

  JsonDocument doc;
  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
  }

  JsonArray arr = doc["tracks"].as<JsonArray>();
  if (!arr) arr = doc["tracks"].to<JsonArray>();

  startNewSessionFrame((uint8_t)currentMode);

  bool exists = false;
  for (JsonObject t : arr) {
    if (t["id"].as<String>() == slug) {
      t["name"] = cleanName;
      activeTrackId = slug;
      activeTrackName = cleanName;
      bool dummy = false;
      loadActiveTrackRecordsFromJsonObj(t, dummy);
      exists = true;
      break;
    }
  }

  if (!exists) {
    if (arr.size() >= 20) return false; // Schutz vor Überlauf
    String today = getCurrentLocalDateStr();
    activeTrackId = slug;
    activeTrackName = cleanName;
    clearRecordList(activeAllTimeRecords, activeAllTimeCount);
    clearRecordList(activeDailyRecords, activeDailyCount);
    dailyBestDate = today;
    syncTopRecordVars();
  }

  saveTracksToFS();
  resetAllLaps(false);
  return true;
}

static void removeDirectoryRecursive(const String& dirPath) {
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    String subPath = dirPath + "/" + dir.fileName();
    if (dir.isDirectory()) {
      removeDirectoryRecursive(subPath);
    } else {
      LittleFS.remove(subPath);
    }
  }
  LittleFS.rmdir(dirPath);
}

// Hilfsfunktion: Filtert /sessions.json im Streaming-Verfahren ohne RAM-Spitzen
static void filterSessionsIndexFile(const String& removeSessionId, const String& removeTrackId, const String& replaceSessionId, const String& replacementJson) {
  if (!LittleFS.exists("/sessions.json")) return;

  File fIn = LittleFS.open("/sessions.json", "r");
  if (!fIn) return;
  File fOut = LittleFS.open("/sessions.tmp", "w");
  if (!fOut) {
    fIn.close();
    return;
  }

  fOut.print("{\"sessions\":[");
  bool firstWritten = false;
  int braceDepth = 0;
  bool inQuotes = false;
  bool escaped = false;
  String currentObj = "";
  currentObj.reserve(512);

  while (fIn.available()) {
    char c = (char)fIn.read();
    if (braceDepth == 0) {
      if (c == '[') {
        braceDepth = 1;
      }
      continue;
    }

    if (braceDepth >= 1) {
      if (escaped) {
        escaped = false;
        if (braceDepth >= 2) currentObj += c;
        continue;
      }
      if (c == '\\' && inQuotes) {
        escaped = true;
        if (braceDepth >= 2) currentObj += c;
        continue;
      }
      if (c == '\"') {
        inQuotes = !inQuotes;
      }

      if (!inQuotes) {
        if (c == '{') {
          if (braceDepth == 1) {
            currentObj = "{";
          } else {
            currentObj += '{';
          }
          braceDepth++;
          continue;
        } else if (c == '}') {
          braceDepth--;
          if (braceDepth == 1) {
            currentObj += '}';
            // Vollständiges Session-Objekt gelesen
            bool shouldRemove = false;
            if (removeSessionId.length() > 0) {
              String needle = "\"id\":\"" + removeSessionId + "\"";
              if (currentObj.indexOf(needle) >= 0) {
                shouldRemove = true;
                // Zugehörige Session-Datei aus LittleFS entfernen
                int fIdx = currentObj.indexOf("\"file\":\"");
                if (fIdx >= 0) {
                  int startQ = fIdx + 8;
                  int endQ = currentObj.indexOf("\"", startQ);
                  if (endQ > startQ) {
                    String fPath = currentObj.substring(startQ, endQ);
                    if (LittleFS.exists(fPath)) LittleFS.remove(fPath);
                  }
                }
              }
            }
            if (!shouldRemove && removeTrackId.length() > 0) {
              String needle = "\"trackId\":\"" + removeTrackId + "\"";
              if (currentObj.indexOf(needle) >= 0) {
                shouldRemove = true;
              }
            }

            if (!shouldRemove) {
              if (!firstWritten) firstWritten = true;
              else fOut.print(",");

              if (replaceSessionId.length() > 0 && currentObj.indexOf("\"id\":\"" + replaceSessionId + "\"") >= 0) {
                fOut.print(replacementJson);
              } else {
                fOut.print(currentObj);
              }
            }
            currentObj = "";
            continue;
          } else if (braceDepth == 0) {
            break;
          }
        } else if (c == ']' && braceDepth == 1) {
          break;
        }
      }

      if (braceDepth >= 2) {
        currentObj += c;
      }
    }
  }

  fOut.print("]}                ");
  fIn.close();
  fOut.close();

  LittleFS.remove("/sessions.json");
  LittleFS.rename("/sessions.tmp", "/sessions.json");
  currentSessionIndexOffset = 0;
  currentSessionIndexLen = 0;
  updateFsUsageCache();
}

bool deleteTrackById(const String& trackId) {
  if (!LittleFS.exists("/tracks.json")) return false;
  File f = LittleFS.open("/tracks.json", "r");
  if (!f) return false;
  JsonDocument doc;
  if (deserializeJson(doc, f)) {
    f.close();
    return false;
  }
  f.close();

  JsonArray arr = doc["tracks"].as<JsonArray>();
  if (!arr) return false;

  startNewSessionFrame((uint8_t)currentMode);

  for (size_t i = 0; i < arr.size(); i++) {
    if (arr[i]["id"].as<String>() == trackId) {
      arr.remove(i);
      break;
    }
  }

  // Alle Dateien & Ordner dieser Strecke entfernen
  String trackDir = "/" + trackId;
  if (LittleFS.exists(trackDir)) {
    removeDirectoryRecursive(trackDir);
  }
  filterSessionsIndexFile("", trackId, "", "");

  if (arr.size() == 0) {
    JsonObject def = arr.add<JsonObject>();
    def["id"] = "standard";
    def["name"] = "Test-Strecke";
    def["recordLap"] = 0;
    def["bestLap"] = 0;
    def["allTimeRecords"].to<JsonArray>();
    def["dailyRecords"].to<JsonArray>();
  }

  JsonObject nextActive = arr[0];
  activeTrackId = nextActive["id"].as<String>();
  activeTrackName = nextActive["name"].as<String>();
  bool dummy = false;
  loadActiveTrackRecordsFromJsonObj(nextActive, dummy);
  doc["active"] = activeTrackId;

  File fWrite = LittleFS.open("/tracks.json", "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
  saveTracksToFS();
  resetAllLaps(false);
  return true;
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
        if (doc["learnedApIP"].is<String>()) learnedApIP = doc["learnedApIP"].as<String>();
        if (doc["mode"].is<uint8_t>()) activeWifiMode = doc["mode"].as<uint8_t>();
      }
    }
  }
  if (apSSID.length() == 0 || apSSID == AP_SSID) {
    apSSID = getDefaultApSSID();
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
  doc["ssid"] = hotspotSSID;
  doc["pass"] = hotspotPass;
  if (learnedApIP.length() > 0) doc["learnedApIP"] = learnedApIP;

  File f = LittleFS.open("/wifi.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void updateLearnedApIP(const IPAddress& ip) {
  if (ip[0] == 0 || ip[3] == 0) return;
  String ipStr = ip.toString();
  if (learnedApIP != ipStr) {
    learnedApIP = ipStr;
    saveWifiConfigToFS(activeWifiMode, homeSSID, homePass, hotspotSSID, hotspotPass, apSSID, apPass);
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
  loadTracksFromFS();

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

// --- SESSION & RUNDEN SPEICHERUNG ---

static String buildSessionEntryJson(const String& sId, const String& sFile, const String& trkId, const String& trkName, uint8_t mode, uint64_t startSec, uint64_t endSec, uint16_t carLaps[], uint32_t carBest[], uint32_t carAvg[] = nullptr) {
  uint8_t activeIds[MAX_CARS + 1];
  uint8_t count = 0;
  for (uint8_t c = 1; c <= MAX_CARS; c++) {
    if (carLaps[c] > 0 || carBest[c] > 0) {
      activeIds[count++] = c;
    }
  }

  // Sortieren nach Modus: Quali (1) & Drag (4) nach Bestzeit, sonst nach Runden & Bestzeit
  bool sortByBest = (mode == 1 || mode == 4);
  for (uint8_t i = 0; i < count; i++) {
    for (uint8_t j = i + 1; j < count; j++) {
      uint8_t a = activeIds[i];
      uint8_t b = activeIds[j];
      bool swap = false;
      if (sortByBest) {
        uint32_t ba = carBest[a] > 0 ? carBest[a] : 99999999UL;
        uint32_t bb = carBest[b] > 0 ? carBest[b] : 99999999UL;
        if (bb < ba) swap = true;
      } else {
        if (carLaps[b] > carLaps[a]) swap = true;
        else if (carLaps[b] == carLaps[a]) {
          uint32_t ba = carBest[a] > 0 ? carBest[a] : 99999999UL;
          uint32_t bb = carBest[b] > 0 ? carBest[b] : 99999999UL;
          if (bb < ba) swap = true;
        }
      }
      if (swap) {
        activeIds[i] = b;
        activeIds[j] = a;
      }
    }
  }

  String out;
  out.reserve(384);
  out = "{\"id\":\"" + sId + "\",\"file\":\"" + sFile + "\",\"trackId\":\"" + trkId + "\",\"trackName\":\"" + escapeJsonStr(trkName) + "\",\"mode\":" + String(mode) + ",\"modeName\":\"" + String(getModeDisplayName(mode)) + "\",\"startTs\":" + String((unsigned long)startSec) + ",\"endTs\":" + String((unsigned long)(endSec > 0 ? endSec : startSec)) + ",\"results\":[";

  for (uint8_t idx = 0; idx < count; idx++) {
    uint8_t c = activeIds[idx];
    if (idx > 0) out += ",";
    uint32_t aLap = (carAvg != nullptr) ? carAvg[c] : 0;
    out += "{\"pos\":" + String(idx + 1) + ",\"id\":" + String(c) + ",\"driver\":\"" + escapeJsonStr(driverNames[c]) + "\",\"car\":\"" + escapeJsonStr(carNames[c]) + "\",\"laps\":" + String(carLaps[c]) + ",\"bestLap\":" + String(carBest[c]) + ",\"avgLap\":" + String(aLap) + "}";
  }
  out += "]}";
  return out;
}

void flushCurrentSessionToIndex() {
  if (!currentSessionActive || currentSessionId.length() == 0) {
    sessionIndexDirty = false;
    return;
  }

  String entryJson = buildSessionEntryJson(
    currentSessionId,
    currentSessionFilePath,
    activeTrackId,
    activeTrackName,
    currentSessionMode,
    currentSessionStartSec,
    sessionLastEndSec,
    sessionCarLaps,
    sessionCarBest,
    sessionCarAvg
  );

  const char* indexPath = "/sessions.json";
  if (!LittleFS.exists(indexPath)) {
    File f = LittleFS.open(indexPath, "w");
    if (f) {
      f.print("{\"sessions\":[");
      currentSessionIndexOffset = f.position();
      f.print(entryJson);
      f.print("]}                ");
      currentSessionIndexLen = entryJson.length();
      f.close();
    }
    sessionIndexDirty = false;
    lastIndexFlushMs = millis();
    updateFsUsageCache();
    return;
  }

  File f = LittleFS.open(indexPath, "r+");
  if (!f) return;

  size_t fSize = f.size();
  if (fSize < 14) {
    f.close();
    LittleFS.remove(indexPath);
    flushCurrentSessionToIndex();
    return;
  }

  if (currentSessionIndexOffset > 0 && currentSessionIndexOffset < fSize) {
    f.seek(currentSessionIndexOffset, SeekSet);
    f.print(entryJson);
    f.print("]}");
    if (entryJson.length() < currentSessionIndexLen) {
      size_t diff = currentSessionIndexLen - entryJson.length() + 8;
      for (size_t i = 0; i < diff; i++) f.print(' ');
    } else {
      currentSessionIndexLen = entryJson.length();
      f.print("        ");
    }
    f.close();
  } else {
    // Ende des Arrays ']' von hinten suchen
    size_t searchStart = (fSize > 64) ? (fSize - 64) : 0;
    f.seek(searchStart, SeekSet);
    String tail = "";
    while (f.available()) {
      tail += (char)f.read();
    }
    int bracketPos = tail.lastIndexOf(']');
    if (bracketPos < 0) {
      f.close();
      return;
    }

    // Prüfen, ob vor ']' ein '[' steht (leeres Array)
    bool isEmptyArray = false;
    for (int k = bracketPos - 1; k >= 0; k--) {
      char ch = tail[k];
      if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') continue;
      if (ch == '[') isEmptyArray = true;
      break;
    }

    size_t writePos = searchStart + bracketPos;
    f.seek(writePos, SeekSet);
    if (!isEmptyArray) {
      f.print(",");
    }
    currentSessionIndexOffset = f.position();
    f.print(entryJson);
    f.print("]}                ");
    currentSessionIndexLen = entryJson.length();
    f.close();
  }

  sessionIndexDirty = false;
  lastIndexFlushMs = millis();
  updateFsUsageCache();
}

void startNewSessionFrame(uint8_t mode) {
  if (currentSessionActive && sessionIndexDirty) {
    flushCurrentSessionToIndex();
  }
  currentSessionActive = false;
  currentSessionId = "";
  currentSessionFilePath = "";
  currentSessionIndexOffset = 0;
  currentSessionIndexLen = 0;
  sessionIndexDirty = false;
  currentSessionMode = mode;
  currentSessionStartSec = (baseUnixTimestampSec > 0) ? (getCurrentTimestampMs() / 1000ULL) : 0;
  sessionLastEndSec = currentSessionStartSec;

  for (uint8_t i = 0; i <= MAX_CARS; i++) {
    sessionCarLaps[i] = 0;
    sessionCarBest[i] = 0;
    sessionCarValidLaps[i] = 0;
    sessionCarTimeSum[i] = 0;
    sessionCarAvg[i] = 0;
  }

  updateFsUsageCache();
  storageFullLocked = (cachedFsUsedPct >= 95);
}

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs, bool isOutLap) {
  // Regel: Keine Aufzeichnung bevor Time-Sync durch ein verbundenes Gerät erfolgt ist!
  if (baseUnixTimestampSec == 0) return;

  uint8_t nextHead = (queueHead + 1) % QUEUE_MAX;
  if (nextHead != queueTail) { 
    lapQueue[queueHead] = {carId, lapNum, lapTimeMs, sz1, sz2, sz3, sz4, position, mode, timestampMs, isOutLap};
    queueHead = nextHead;
  }
}

void processStorageQueue() {
  static uint32_t lastStorageWrite = 0;
  if (millis() - lastStorageWrite < 120) return;

  if (queueHead != queueTail) { 
    PendingLap lap = lapQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_MAX;
    saveLapToHistory(lap.carId, lap.lapNum, lap.lapTimeMs, lap.sz1, lap.sz2, lap.sz3, lap.sz4, lap.position, lap.mode, lap.timestampMs, lap.isOutLap);
    lastStorageWrite = millis();
  } else if (sessionIndexDirty && (millis() - lastIndexFlushMs > 2000)) {
    flushCurrentSessionToIndex();
    lastStorageWrite = millis();
  }
}

void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs, bool isOutLap) {
  // 1. Nicht speichern bevor Time-Sync erfolgt ist
  if (baseUnixTimestampSec == 0) return;

  // 2. Falls Modus gewechselt hat, sauberen neuen Session-Rahmen öffnen
  if (currentSessionActive && currentSessionMode != mode) {
    startNewSessionFrame(mode);
  }

  // 3. Neue Session-Datei initialisieren, falls noch keine aktiv ist
  if (!currentSessionActive) {
    updateFsUsageCache();
    // Harter Schnitt bei 95% Füllstand vor Beginn einer neuen Session!
    if (cachedFsUsedPct >= 95) {
      storageFullLocked = true;
      return;
    }
    storageFullLocked = false;

    uint64_t lapSec = timestampMs / 1000ULL;
    if (currentSessionStartSec == 0 || currentSessionStartSec < 1600000000ULL) {
      currentSessionStartSec = lapSec;
    }

    String trackDir = "/" + activeTrackId;
    String modeDir = trackDir + "/" + String(getModeFolderName(mode));
    if (!LittleFS.exists(trackDir)) LittleFS.mkdir(trackDir);
    if (!LittleFS.exists(modeDir)) LittleFS.mkdir(modeDir);

    currentSessionId = formatTimestampId(currentSessionStartSec);
    String candidatePath = modeDir + "/" + currentSessionId + ".json";
    uint8_t collisionTries = 0;
    while (LittleFS.exists(candidatePath) && collisionTries < 10) {
      currentSessionStartSec++;
      currentSessionId = formatTimestampId(currentSessionStartSec);
      candidatePath = modeDir + "/" + currentSessionId + ".json";
      collisionTries++;
    }

    currentSessionFilePath = candidatePath;
    currentSessionMode = mode;
    currentSessionActive = true;
    currentSessionIndexOffset = 0;
    currentSessionIndexLen = 0;
  } else {
    // Laufende Session darf bis max. 99% fertiggeschrieben werden
    if (cachedFsUsedPct >= 99) return;
  }

  // 4. Session-Statistik im RAM aktualisieren
  if (lapNum > sessionCarLaps[carId]) {
    sessionCarLaps[carId] = lapNum;
  } else {
    sessionCarLaps[carId]++;
  }
  if (!isOutLap && lapTimeMs > 0) {
    if (sessionCarBest[carId] == 0 || lapTimeMs < sessionCarBest[carId]) {
      sessionCarBest[carId] = lapTimeMs;
    }
    sessionCarValidLaps[carId]++;
    sessionCarTimeSum[carId] += lapTimeMs;
    sessionCarAvg[carId] = sessionCarTimeSum[carId] / sessionCarValidLaps[carId];
  }

  sessionLastEndSec = timestampMs / 1000ULL;

  // 5. Runden-JSON formatieren & stromausfallsicher in die Session-Datei schreiben
  String escDriver = escapeJsonStr(driverNames[carId]);
  String escCar = escapeJsonStr(carNames[carId]);

  char newLapJson[360];
  snprintf(newLapJson, sizeof(newLapJson),
           "{\"carId\":%u,\"driver\":\"%s\",\"car\":\"%s\",\"lap\":%u,\"time\":%lu,\"sz1\":%lu,\"sz2\":%lu,\"sz3\":%lu,\"sz4\":%lu,\"pos\":%u,\"ts\":%llu,\"outlap\":%u}",
           carId, escDriver.c_str(), escCar.c_str(), lapNum, lapTimeMs, sz1, sz2, sz3, sz4, position, timestampMs, isOutLap ? 1 : 0);

  if (!LittleFS.exists(currentSessionFilePath)) {
    File f = LittleFS.open(currentSessionFilePath, "w");
    if (f) {
      f.printf("{\"id\":\"%s\",\"trackId\":\"%s\",\"trackName\":\"%s\",\"mode\":%u,\"modeName\":\"%s\",\"startTs\":%lu,\"laps\":[%s]}",
               currentSessionId.c_str(),
               activeTrackId.c_str(),
               escapeJsonStr(activeTrackName).c_str(),
               mode,
               getModeDisplayName(mode),
               (unsigned long)currentSessionStartSec,
               newLapJson);
      f.close();
    }
  } else {
    File f = LittleFS.open(currentSessionFilePath, "r+");
    if (f) {
      size_t size = f.size();
      if (size >= 2) {
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
      }
      f.close();
    }
  }

  // 6. Beim ersten Eintrag sofort im Index registrieren, danach verzögert über Queue
  if (currentSessionIndexOffset == 0) {
    flushCurrentSessionToIndex();
  } else {
    sessionIndexDirty = true;
  }
}

void deleteSessionById(const String& sessionId) {
  if (sessionId.length() == 0) return;
  if (sessionIndexDirty) flushCurrentSessionToIndex();

  if (currentSessionActive && currentSessionId == sessionId) {
    currentSessionActive = false;
    currentSessionId = "";
    currentSessionFilePath = "";
    currentSessionIndexOffset = 0;
    currentSessionIndexLen = 0;
    sessionIndexDirty = false;
  }

  filterSessionsIndexFile(sessionId, "", "", "");
}

void deleteLapFromSession(const String& sessionId, uint8_t carId, uint16_t lapNum) {
  if (sessionId.length() == 0) return;
  if (sessionIndexDirty) flushCurrentSessionToIndex();

  // 1. Dateipfad der Session ermitteln
  String targetFile = "";
  if (currentSessionActive && currentSessionId == sessionId) {
    targetFile = currentSessionFilePath;
  } else if (LittleFS.exists("/sessions.json")) {
    File fIn = LittleFS.open("/sessions.json", "r");
    if (fIn) {
      String needle = "\"id\":\"" + sessionId + "\"";
      while (fIn.available()) {
        String line = fIn.readStringUntil('}');
        int idx = line.indexOf(needle);
        if (idx >= 0) {
          int fIdx = line.indexOf("\"file\":\"");
          if (fIdx >= 0) {
            int startQ = fIdx + 8;
            int endQ = line.indexOf("\"", startQ);
            if (endQ > startQ) targetFile = line.substring(startQ, endQ);
          }
          break;
        }
      }
      fIn.close();
    }
  }

  if (targetFile.length() == 0 || !LittleFS.exists(targetFile)) return;

  JsonDocument doc;
  File fRead = LittleFS.open(targetFile, "r");
  if (!fRead) return;
  DeserializationError err = deserializeJson(doc, fRead);
  fRead.close();
  if (err) return;

  JsonArray laps = doc["laps"].as<JsonArray>();
  if (!laps) return;

  uint32_t deletedLapTimeMs = 0;
  for (size_t i = 0; i < laps.size(); i++) {
    uint8_t cId = laps[i]["carId"] | 0;
    uint16_t lNum = laps[i]["lap"] | 0;
    if (cId == carId && lNum == lapNum) {
      deletedLapTimeMs = laps[i]["time"] | 0;
      laps.remove(i);
      break;
    }
  }

  String sessTrackId = doc["trackId"] | activeTrackId;

  // Falls die gelöschte Runde in der Top-5 Rekordliste stand, auch dort automatisch korrigieren
  if (deletedLapTimeMs > 0 && sessTrackId == activeTrackId) {
    for (uint8_t i = 0; i < activeAllTimeCount; i++) {
      if (activeAllTimeRecords[i].lapMs == deletedLapTimeMs && activeAllTimeRecords[i].carId == carId) {
        deleteTrackRecordEntry(activeTrackId, false, (int8_t)i);
        break;
      }
    }
    for (uint8_t i = 0; i < activeDailyCount; i++) {
      if (activeDailyRecords[i].lapMs == deletedLapTimeMs && activeDailyRecords[i].carId == carId) {
        deleteTrackRecordEntry(activeTrackId, true, (int8_t)i);
        break;
      }
    }
  }

  if (laps.size() == 0) {
    deleteSessionById(sessionId);
    return;
  }

  // Ergebnisse aus verbleibenden Runden neu berechnen
  uint16_t calcLaps[MAX_CARS + 1] = {0};
  uint32_t calcBest[MAX_CARS + 1] = {0};
  uint64_t maxTsSec = doc["startTs"] | 0;

  for (JsonObject l : laps) {
    uint8_t c = l["carId"] | 0;
    uint32_t tMs = l["time"] | 0;
    uint64_t tsMs = l["ts"] | 0;
    if (c >= 1 && c <= MAX_CARS) {
      calcLaps[c]++;
      if (tMs > 0 && (calcBest[c] == 0 || tMs < calcBest[c])) {
        calcBest[c] = tMs;
      }
      if (tsMs / 1000ULL > maxTsSec) maxTsSec = tsMs / 1000ULL;
    }
  }

  File fWrite = LittleFS.open(targetFile, "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }

  if (currentSessionActive && currentSessionId == sessionId) {
    for (uint8_t c = 1; c <= MAX_CARS; c++) {
      sessionCarLaps[c] = calcLaps[c];
      sessionCarBest[c] = calcBest[c];
    }
  }

  String updatedEntry = buildSessionEntryJson(
    sessionId,
    targetFile,
    sessTrackId,
    doc["trackName"] | activeTrackName,
    doc["mode"] | 0,
    doc["startTs"] | 0,
    maxTsSec,
    calcLaps,
    calcBest
  );

  filterSessionsIndexFile("", "", sessionId, updatedEntry);
}

void deleteDriverHistory(uint8_t carId) {
  if (carId < 1 || carId > MAX_CARS) return;
  if (sessionIndexDirty) flushCurrentSessionToIndex();

  if (!LittleFS.exists("/sessions.json")) return;
  File f = LittleFS.open("/sessions.json", "r");
  if (!f) return;

  std::vector<String> sessionFiles;
  std::vector<String> sessionIds;

  int braceDepth = 0;
  bool inQuotes = false;
  bool escaped = false;
  String currentObj = "";
  while (f.available()) {
    char c = (char)f.read();
    if (braceDepth == 0) {
      if (c == '[') braceDepth = 1;
      continue;
    }
    if (escaped) { escaped = false; if (braceDepth >= 2) currentObj += c; continue; }
    if (c == '\\' && inQuotes) { escaped = true; if (braceDepth >= 2) currentObj += c; continue; }
    if (c == '\"') inQuotes = !inQuotes;
    if (!inQuotes) {
      if (c == '{') {
        if (braceDepth == 1) currentObj = "{";
        else currentObj += '{';
        braceDepth++;
        continue;
      } else if (c == '}') {
        braceDepth--;
        if (braceDepth == 1) {
          currentObj += '}';
          String idNeedle = "\"id\":" + String(carId) + ",";
          if (currentObj.indexOf(idNeedle) >= 0) {
            int sIdx = currentObj.indexOf("\"id\":\"");
            int fIdx = currentObj.indexOf("\"file\":\"");
            if (sIdx >= 0 && fIdx >= 0) {
              String sId = currentObj.substring(sIdx + 6, currentObj.indexOf("\"", sIdx + 6));
              String sFile = currentObj.substring(fIdx + 8, currentObj.indexOf("\"", fIdx + 8));
              sessionIds.push_back(sId);
              sessionFiles.push_back(sFile);
            }
          }
          currentObj = "";
          continue;
        }
      } else if (c == ']' && braceDepth == 1) break;
    }
    if (braceDepth >= 2) currentObj += c;
  }
  f.close();

  for (size_t idx = 0; idx < sessionIds.size(); idx++) {
    String sId = sessionIds[idx];
    String sFile = sessionFiles[idx];
    if (!LittleFS.exists(sFile)) continue;

    JsonDocument doc;
    File fRead = LittleFS.open(sFile, "r");
    if (!fRead) continue;
    if (deserializeJson(doc, fRead)) { fRead.close(); continue; }
    fRead.close();

    JsonArray laps = doc["laps"].as<JsonArray>();
    if (!laps) continue;

    for (int i = (int)laps.size() - 1; i >= 0; i--) {
      if ((laps[i]["carId"] | 0) == carId) {
        laps.remove(i);
      }
    }

    if (laps.size() == 0) {
      deleteSessionById(sId);
    } else {
      uint16_t calcLaps[MAX_CARS + 1] = {0};
      uint32_t calcBest[MAX_CARS + 1] = {0};
      uint64_t maxTsSec = doc["startTs"] | 0;
      for (JsonObject l : laps) {
        uint8_t c = l["carId"] | 0;
        uint32_t tMs = l["time"] | 0;
        uint64_t tsMs = l["ts"] | 0;
        if (c >= 1 && c <= MAX_CARS) {
          calcLaps[c]++;
          if (tMs > 0 && (calcBest[c] == 0 || tMs < calcBest[c])) calcBest[c] = tMs;
          if (tsMs / 1000ULL > maxTsSec) maxTsSec = tsMs / 1000ULL;
        }
      }
      File fWrite = LittleFS.open(sFile, "w");
      if (fWrite) { serializeJson(doc, fWrite); fWrite.close(); }

      String updatedEntry = buildSessionEntryJson(
        sId, sFile, doc["trackId"] | activeTrackId, doc["trackName"] | activeTrackName,
        doc["mode"] | 0, doc["startTs"] | 0, maxTsSec, calcLaps, calcBest
      );
      filterSessionsIndexFile("", "", sId, updatedEntry);
    }
  }
}

void deleteAllHistory() {
  currentSessionActive = false;
  currentSessionId = "";
  currentSessionFilePath = "";
  currentSessionIndexOffset = 0;
  currentSessionIndexLen = 0;
  sessionIndexDirty = false;

  for (uint8_t i = 0; i <= MAX_CARS; i++) {
    sessionCarLaps[i] = 0;
    sessionCarBest[i] = 0;
    sessionCarValidLaps[i] = 0;
    sessionCarTimeSum[i] = 0;
    sessionCarAvg[i] = 0;
  }

  // Alle Unterordner im Root (Strecken-Ordner) leeren & entfernen
  Dir root = LittleFS.openDir("/");
  std::vector<String> dirsToRemove;
  while (root.next()) {
    if (root.isDirectory()) {
      dirsToRemove.push_back("/" + root.fileName());
    } else {
      String fn = root.fileName();
      if (fn.startsWith("stats_") && fn.endsWith(".json")) {
        LittleFS.remove("/" + fn);
      }
    }
  }
  for (const String& d : dirsToRemove) {
    removeDirectoryRecursive(d);
  }

  if (LittleFS.exists("/sessions.json")) {
    LittleFS.remove("/sessions.json");
  }
  if (LittleFS.exists("/daily_best.json")) {
    LittleFS.remove("/daily_best.json");
  }

  // Streckenrekorde & Tagesbestzeiten in /tracks.json zurücksetzen
  clearRecordList(activeAllTimeRecords, activeAllTimeCount);
  clearRecordList(activeDailyRecords, activeDailyCount);
  syncTopRecordVars();

  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        f.close();
        JsonArray arr = doc["tracks"].as<JsonArray>();
        for (JsonObject t : arr) {
          t["recordLap"] = 0;
          t["recordCarId"] = 0;
          t["recordDriver"] = "";
          t["recordCar"] = "";
          t["recordDate"] = "";
          t["bestLap"] = 0;
          t["bestCarId"] = 0;
          t["bestDriver"] = "";
          t["bestCar"] = "";
          t["allTimeRecords"].to<JsonArray>();
          t["dailyRecords"].to<JsonArray>();
        }
        File fW = LittleFS.open("/tracks.json", "w");
        if (fW) { serializeJson(doc, fW); fW.close(); }
      } else {
        f.close();
      }
    }
  }

  updateFsUsageCache();
}
