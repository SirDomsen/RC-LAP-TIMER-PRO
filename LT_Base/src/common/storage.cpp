#include "storage.h"
#include "lap_logic.h"
#include "web_server.h"
#include "platform_runtime.h"
#include <ir_decoder.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

String driverNames[MAX_CARS + 1];
String carNames[MAX_CARS + 1];


// WLAN Variablen mit Default-Werten aus config.h
String homeSSID = HOME_SSID;
String homePass = HOME_PASS;
String hotspotSSID = "";
String hotspotPass = "";
String apSSID = "";
String apPass = AP_PASS;
String learnedApIP = "";
uint8_t activeWifiMode = 1; // Standard: 1 = Heim-WLAN

// Eindeutige Hardware-Identifikatoren
String getChipIdSuffix() {
  char buf[8];
#if defined(ESP8266)
  snprintf(buf, sizeof(buf), "%04X", (uint16_t)(ESP.getChipId() & 0xFFFF));
#elif defined(ESP32)
  snprintf(buf, sizeof(buf), "%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
#endif
  return String(buf);
}

String getDefaultApSSID() {
  return String(AP_SSID) + "-" + getChipIdSuffix();
}

String getDeviceHostname() {
  return "laptimer";
}

String getOtaHostname() {
  return "laptimer";
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
static char sessionDriverNames[MAX_CARS + 1][61] = {};
static char sessionCarNames[MAX_CARS + 1][61] = {};
static uint16_t sessionNamesWrittenMask = 0;

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
    case 5: return "ghost";
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
    case 5: return "Ghost Battle";
    default: return "Freies Training";
  }
}

static String formatTimestampId(uint64_t tsSec) {
  time_t localT = (time_t)(tsSec + clientTzOffsetSec);
  struct tm *tm_info = gmtime(&localT);
  char buf[32];
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
  char buf[32];
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
    list[i].s1 = 0;
    list[i].s2 = 0;
    list[i].s3 = 0;
    list[i].s4 = 0;
    list[i].szMask = 0;
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
    allTimeBestS1Ms = activeAllTimeRecords[0].s1;
    allTimeBestS2Ms = activeAllTimeRecords[0].s2;
    allTimeBestS3Ms = activeAllTimeRecords[0].s3;
    allTimeBestS4Ms = activeAllTimeRecords[0].s4;
    allTimeBestSzMask = activeAllTimeRecords[0].szMask;
  } else {
    allTimeBestLapMs = 0;
    allTimeBestCarId = 0;
    allTimeBestDriver = "";
    allTimeBestCar = "";
    allTimeBestDate = "";
    allTimeBestS1Ms = 0;
    allTimeBestS2Ms = 0;
    allTimeBestS3Ms = 0;
    allTimeBestS4Ms = 0;
    allTimeBestSzMask = 0;
  }

  if (activeDailyCount > 0 && activeDailyRecords[0].lapMs > 0) {
    dailyBestLapMs = activeDailyRecords[0].lapMs;
    dailyBestCarId = activeDailyRecords[0].carId;
    dailyBestDriver = activeDailyRecords[0].driver;
    dailyBestCar = activeDailyRecords[0].car;
    dailyBestDate = activeDailyRecords[0].date;
    dailyBestS1Ms = activeDailyRecords[0].s1;
    dailyBestS2Ms = activeDailyRecords[0].s2;
    dailyBestS3Ms = activeDailyRecords[0].s3;
    dailyBestS4Ms = activeDailyRecords[0].s4;
    dailyBestSzMask = activeDailyRecords[0].szMask;
  } else {
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    dailyBestS1Ms = 0;
    dailyBestS2Ms = 0;
    dailyBestS3Ms = 0;
    dailyBestS4Ms = 0;
    dailyBestSzMask = 0;
    String today = getCurrentLocalDateStr();
    if (today.length() > 0) dailyBestDate = today;
  }
}

static bool updateRecordList(RecordEntry* list, uint8_t& count, uint32_t lapMs, uint8_t carId, const String& driver, const String& car, const String& date, uint32_t s1 = 0, uint32_t s2 = 0, uint32_t s3 = 0, uint32_t s4 = 0, uint8_t szMask = 0) {
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
      list[existingIdx].s1 = s1;
      list[existingIdx].s2 = s2;
      list[existingIdx].s3 = s3;
      list[existingIdx].s4 = s4;
      list[existingIdx].szMask = szMask;
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
    list[count].s1 = s1;
    list[count].s2 = s2;
    list[count].s3 = s3;
    list[count].s4 = s4;
    list[count].szMask = szMask;
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
    list[MAX_TRACK_RECORDS - 1].s1 = s1;
    list[MAX_TRACK_RECORDS - 1].s2 = s2;
    list[MAX_TRACK_RECORDS - 1].s3 = s3;
    list[MAX_TRACK_RECORDS - 1].s4 = s4;
    list[MAX_TRACK_RECORDS - 1].szMask = szMask;
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
      list[count].s1 = item["s1"] | 0;
      list[count].s2 = item["s2"] | 0;
      list[count].s3 = item["s3"] | 0;
      list[count].s4 = item["s4"] | 0;
      list[count].szMask = item["szMask"] | 0;
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
    if (list[i].szMask > 0 || list[i].s1 > 0) {
      item["s1"] = list[i].s1;
      item["s2"] = list[i].s2;
      item["s3"] = list[i].s3;
      item["s4"] = list[i].s4;
      item["szMask"] = list[i].szMask;
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

void checkAndUpdateTrackRecords(uint8_t carId, uint32_t lapTimeMs, bool& isNewAllTimeBest, bool& isNewDailyBest, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint8_t szMask) {
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

  bool changedAllTime = updateRecordList(activeAllTimeRecords, activeAllTimeCount, lapTimeMs, carId, dName, cName, today, s1, s2, s3, s4, szMask);
  bool changedDaily = updateRecordList(activeDailyRecords, activeDailyCount, lapTimeMs, carId, dName, cName, today, s1, s2, s3, s4, szMask);

  if (changedAllTime || changedDaily) {
    if (changedAllTime && wasNewP1AllTime) isNewAllTimeBest = true;
    if (changedDaily && wasNewP1Daily) isNewDailyBest = true;
    syncTopRecordVars();
    saveTracksToFS();

    char buf[160];
    snprintf(buf, sizeof(buf), "{\"type\":\"track_record\",\"trackId\":\"%s\",\"recordLap\":%lu,\"bestLap\":%lu}",
             activeTrackId.c_str(), (unsigned long)allTimeBestLapMs, (unsigned long)dailyBestLapMs);
    broadcastWebSocket(buf);
  }
}

static bool removeEntryFromList(RecordEntry* list, uint8_t& count, int8_t index, 
                                uint32_t& deletedLapOut, uint8_t& deletedCarIdOut, uint32_t& restoredLapOut,
                                String* deletedDriverOut = nullptr, String* deletedCarOut = nullptr, String* deletedDateOut = nullptr) {
  deletedLapOut = 0;
  deletedCarIdOut = 0;
  restoredLapOut = 0;
  if (deletedDriverOut) *deletedDriverOut = "";
  if (deletedCarOut) *deletedCarOut = "";
  if (deletedDateOut) *deletedDateOut = "";

  if (index < 0) {
    clearRecordList(list, count);
    return true;
  }
  if ((uint8_t)index >= count) return false;

  deletedLapOut = list[index].lapMs;
  deletedCarIdOut = list[index].carId;
  restoredLapOut = list[index].prevLapMs;
  if (deletedDriverOut) *deletedDriverOut = list[index].driver;
  if (deletedCarOut) *deletedCarOut = list[index].car;
  if (deletedDateOut) *deletedDateOut = list[index].date;

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

static int8_t findMatchingRecordIndex(const RecordEntry* list, uint8_t count, uint32_t lapMs, uint8_t carId, const String& driver, const String& expectedDate) {
  if (lapMs == 0 || count == 0) return -1;
  for (uint8_t i = 0; i < count; i++) {
    if (list[i].lapMs == lapMs) {
      bool driverMatch = false;
      if (driver.length() > 0 && list[i].driver.length() > 0) {
        if (list[i].driver.equalsIgnoreCase(driver)) driverMatch = true;
      } else if (carId > 0 && list[i].carId == carId) {
        driverMatch = true;
      }
      if (driverMatch) {
        if (expectedDate.length() == 0 || list[i].date.length() == 0 || list[i].date == expectedDate) {
          return (int8_t)i;
        }
      }
    }
  }
  return -1;
}

bool deleteTrackRecordEntry(const String& trackId, bool isDaily, int8_t index) {
  String targetId = trackId.length() > 0 ? trackId : activeTrackId;
  uint32_t deletedLap = 0;
  uint8_t deletedCarId = 0;
  uint32_t restoredLap = 0;
  String deletedDriver = "";
  String deletedCar = "";
  String deletedDate = "";
  String today = getCurrentLocalDateStr();

  if (targetId == activeTrackId) {
    bool ok = false;
    if (isDaily) {
      ok = removeEntryFromList(activeDailyRecords, activeDailyCount, index, deletedLap, deletedCarId, restoredLap, &deletedDriver, &deletedCar, &deletedDate);
      if (!ok) return false;

      // Kaskadierung zum All-Time Streckenrekord:
      // Eine Tagesbestzeit stammt von heute. Falls derselbe Eintrag (gleiche Rundenzeit + Fahrer von heute)
      // auch in der All-Time Liste steht, war diese Tagesbestzeit gleichzeitig der Streckenrekord!
      if (index >= 0 && deletedLap > 0) {
        String matchDate = deletedDate.length() > 0 ? deletedDate : today;
        int8_t allTimeIdx = findMatchingRecordIndex(activeAllTimeRecords, activeAllTimeCount, deletedLap, deletedCarId, deletedDriver, matchDate);
        if (allTimeIdx >= 0) {
          uint32_t dLap = 0, rLap = 0;
          uint8_t dCar = 0;
          removeEntryFromList(activeAllTimeRecords, activeAllTimeCount, allTimeIdx, dLap, dCar, rLap);
        }
      }
    } else {
      ok = removeEntryFromList(activeAllTimeRecords, activeAllTimeCount, index, deletedLap, deletedCarId, restoredLap, &deletedDriver, &deletedCar, &deletedDate);
      if (!ok) return false;

      // Kaskadierung zur Tagesbestzeit:
      // Ein Streckenrekord kann älter sein als heute (Streckenrekord != Tagesbestzeit falls älter!).
      // Nur wenn der gelöschte Streckenrekord von HEUTE stammt, existiert er auch in der Tagesbestzeit.
      bool isFromToday = (today.length() > 0 && deletedDate.length() > 0 && deletedDate == today);
      if (index >= 0 && deletedLap > 0 && (isFromToday || deletedDate.length() == 0)) {
        int8_t dailyIdx = findMatchingRecordIndex(activeDailyRecords, activeDailyCount, deletedLap, deletedCarId, deletedDriver, "");
        if (dailyIdx >= 0) {
          uint32_t dLap = 0, rLap = 0;
          uint8_t dCar = 0;
          removeEntryFromList(activeDailyRecords, activeDailyCount, dailyIdx, dLap, dCar, rLap);
        }
      }
    }

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
        RecordEntry allTimeList[MAX_TRACK_RECORDS];
        uint8_t allTimeCount = 0;
        loadRecordArrayFromJson(t, "allTimeRecords", allTimeList, allTimeCount);

        RecordEntry dailyList[MAX_TRACK_RECORDS];
        uint8_t dailyCount = 0;
        loadRecordArrayFromJson(t, "dailyRecords", dailyList, dailyCount);

        bool ok = false;
        if (isDaily) {
          ok = removeEntryFromList(dailyList, dailyCount, index, deletedLap, deletedCarId, restoredLap, &deletedDriver, &deletedCar, &deletedDate);
          if (!ok) return false;

          if (index >= 0 && deletedLap > 0) {
            String matchDate = deletedDate.length() > 0 ? deletedDate : today;
            int8_t allTimeIdx = findMatchingRecordIndex(allTimeList, allTimeCount, deletedLap, deletedCarId, deletedDriver, matchDate);
            if (allTimeIdx >= 0) {
              uint32_t dLap = 0, rLap = 0;
              uint8_t dCar = 0;
              removeEntryFromList(allTimeList, allTimeCount, allTimeIdx, dLap, dCar, rLap);
            }
          }
        } else {
          ok = removeEntryFromList(allTimeList, allTimeCount, index, deletedLap, deletedCarId, restoredLap, &deletedDriver, &deletedCar, &deletedDate);
          if (!ok) return false;

          bool isFromToday = (today.length() > 0 && deletedDate.length() > 0 && deletedDate == today);
          if (index >= 0 && deletedLap > 0 && (isFromToday || deletedDate.length() == 0)) {
            int8_t dailyIdx = findMatchingRecordIndex(dailyList, dailyCount, deletedLap, deletedCarId, deletedDriver, "");
            if (dailyIdx >= 0) {
              uint32_t dLap = 0, rLap = 0;
              uint8_t dCar = 0;
              removeEntryFromList(dailyList, dailyCount, dailyIdx, dLap, dCar, rLap);
            }
          }
        }

        saveRecordArrayToJson(t, "allTimeRecords", allTimeList, allTimeCount);
        saveRecordArrayToJson(t, "dailyRecords", dailyList, dailyCount);

        // Header-Felder synchronisieren
        if (dailyCount > 0 && dailyList[0].lapMs > 0) {
          t["bestLap"] = dailyList[0].lapMs;
          t["bestCarId"] = dailyList[0].carId;
          t["bestDriver"] = dailyList[0].driver;
          t["bestCar"] = dailyList[0].car;
          t["bestDate"] = dailyList[0].date;
        } else {
          t["bestLap"] = 0;
          t["bestCarId"] = 0;
          t["bestDriver"] = "";
          t["bestCar"] = "";
        }

        if (allTimeCount > 0 && allTimeList[0].lapMs > 0) {
          t["recordLap"] = allTimeList[0].lapMs;
          t["recordCarId"] = allTimeList[0].carId;
          t["recordDriver"] = allTimeList[0].driver;
          t["recordCar"] = allTimeList[0].car;
          t["recordDate"] = allTimeList[0].date;
        } else {
          t["recordLap"] = 0;
          t["recordCarId"] = 0;
          t["recordDriver"] = "";
          t["recordCar"] = "";
          t["recordDate"] = "";
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
           activeTrackId.c_str(), (unsigned long)allTimeBestLapMs, (unsigned long)dailyBestLapMs);
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

static bool appendJsonStringField(char* output, size_t capacity, size_t& length, const char* key, const String& value) {
  int added = snprintf(output + length, capacity - length, ",\"%s\":\"", key);
  if (added < 0 || (size_t)added >= capacity - length) return false;
  length += (size_t)added;

  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    if (c == '"' || c == '\\') {
      if (length + 2 >= capacity) return false;
      output[length++] = '\\';
      output[length++] = (char)c;
    } else if (c < 0x20) {
      if (length + 6 >= capacity) return false;
      int escaped = snprintf(output + length, capacity - length, "\\u%04x", c);
      if (escaped != 6) return false;
      length += 6;
    } else {
      if (length + 1 >= capacity) return false;
      output[length++] = (char)c;
    }
  }
  if (length + 2 > capacity) return false;
  output[length++] = '"';
  output[length] = '\0';
  return true;
}

static void copySessionName(char* output, size_t capacity, const String& value) {
  size_t copyLength = value.length();
  if (copyLength >= capacity) copyLength = capacity - 1;
  memcpy(output, value.c_str(), copyLength);
  output[copyLength] = '\0';
}

void updateFsUsageCache() {
#if defined(ESP8266)
  FSInfo fs_info;
  if (LittleFS.info(fs_info) && fs_info.totalBytes > 0) {
    cachedFsTotalBytes = fs_info.totalBytes;
    cachedFsUsedBytes = fs_info.usedBytes;
    cachedFsUsedPct = (uint8_t)((fs_info.usedBytes * 100ULL) / fs_info.totalBytes);
    if (cachedFsUsedPct < 95) {
      storageFullLocked = false;
    }
  }
#elif defined(ESP32)
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  if (total > 0) {
    cachedFsTotalBytes = total;
    cachedFsUsedBytes = used;
    cachedFsUsedPct = (uint8_t)((used * 100ULL) / total);
    if (cachedFsUsedPct < 95) {
      storageFullLocked = false;
    }
  }
#endif
}

void initFS() {
#if defined(ESP8266)
  if (LittleFS.begin()) {
    updateFsUsageCache();
    recordBootResetReason();
  }
#elif defined(ESP32)
  if (LittleFS.begin(true)) {
    updateFsUsageCache();
    recordBootResetReason();
  }
#endif
}

String lastResetReason = "Unbekannt";
String lastResetInfo = "";
bool lastResetIsCrash = false;

#if defined(ESP8266)
// Letzte Browser-Uhrzeit im RTC-Speicher (ueberlebt Software-Resets/Crashes, nicht Power-On)
struct RtcClockBlock {
  uint32_t magic;
  uint32_t epochSec;
  int32_t tzSec;
  uint32_t check;
};
static const uint32_t RTC_CLOCK_MAGIC = 0x4C54434B;
static const uint32_t RTC_CLOCK_SLOT = 64;

static uint32_t rtcClockCheck(const RtcClockBlock& b) {
  return b.magic ^ b.epochSec ^ (uint32_t)b.tzSec ^ 0xA5A5A5A5UL;
}

static String formatClockText(uint32_t epochSec, int32_t tzSec) {
  time_t localT = (time_t)((int64_t)epochSec + tzSec);
  struct tm *tm_info = gmtime(&localT);
  if (!tm_info) return "?";
  char buf[24];
  snprintf(buf, sizeof(buf), "%02d.%02d. %02d:%02d", tm_info->tm_mday, tm_info->tm_mon + 1,
           tm_info->tm_hour, tm_info->tm_min);
  return String(buf);
}

static void rtcClockSave() {
  if (baseUnixTimestampSec == 0) return;
  RtcClockBlock b;
  b.magic = RTC_CLOCK_MAGIC;
  b.epochSec = (uint32_t)(baseUnixTimestampSec + ((millis() - baseMillis) / 1000UL));
  b.tzSec = clientTzOffsetSec;
  b.check = rtcClockCheck(b);
  ESP.rtcUserMemoryWrite(RTC_CLOCK_SLOT, (uint32_t*)&b, sizeof(b));
}

static bool rtcClockLoad(RtcClockBlock& b) {
  if (!ESP.rtcUserMemoryRead(RTC_CLOCK_SLOT, (uint32_t*)&b, sizeof(b))) return false;
  return b.magic == RTC_CLOCK_MAGIC && b.check == rtcClockCheck(b);
}

// Diagnose: letzter Schritt des Rundenloeschens, bleibt nach einem Crash im RTC-Speicher erhalten
static const uint32_t RTC_STEP_MAGIC = 0x4C445354;
static const uint32_t RTC_STEP_SLOT = 72;
#endif

static bool lapStepActive = false;

static void markLapDeleteStep(uint32_t step) {
  lapStepActive = (step != 0);
#if defined(ESP8266)
  uint32_t blk[3] = { RTC_STEP_MAGIC, step, (uint32_t)ESP.getFreeHeap() };
  ESP.rtcUserMemoryWrite(RTC_STEP_SLOT, blk, sizeof(blk));
#endif
}

// Feinschritte nur waehrend eines laufenden Rundenloeschens protokollieren
static void markLapDeleteSubStep(uint32_t step) {
  if (lapStepActive) markLapDeleteStep(step);
}

// Alle 30 s die Uhrzeit sichern und nach dem ersten Zeitsync eine Zeile im Reset-Log ablegen
static void updateResetLogClock() {
#if defined(ESP8266)
  static uint32_t lastTickMs = 0;
  static bool syncLogged = false;
  if (baseUnixTimestampSec == 0) return;
  if (millis() - lastTickMs < 30000UL && syncLogged) return;
  lastTickMs = millis();
  rtcClockSave();
  if (!syncLogged) {
    syncLogged = true;
    File f = LittleFS.open("/crash.log", "a");
    if (f) {
      uint32_t nowSec = (uint32_t)(baseUnixTimestampSec + ((millis() - baseMillis) / 1000UL));
      f.print("[ZEITSYNC] ");
      f.print(formatClockText(nowSec, clientTzOffsetSec));
      f.print(" | Uptime ");
      f.print(millis() / 1000UL);
      f.println(" s");
      f.close();
    }
  }
#endif
}

void recordBootResetReason() {
#if defined(ESP8266)
  lastResetReason = ESP.getResetReason();
  lastResetInfo = ESP.getResetInfo();

  // Ist es ein Crash / WDT / Exception?
  String rLower = lastResetReason;
  rLower.toLowerCase();
  if (rLower.indexOf("watchdog") >= 0 || rLower.indexOf("exception") >= 0 || rLower.indexOf("fatal") >= 0) {
    lastResetIsCrash = true;
  } else {
    lastResetIsCrash = false;
  }

  // Protokolliert jeden Neustart in /crash.log im LittleFS (max. 6 KB halten)
  File f = LittleFS.open("/crash.log", "a");
  if (f) {
    if (f.size() > 6144) {
      f.close();
      LittleFS.remove("/crash.log");
      f = LittleFS.open("/crash.log", "w");
    }
    if (f) {
      const char* tag = "[START]";
      if (lastResetIsCrash) {
        tag = "[CRASH ⚠️]";
      } else if (rLower.indexOf("power") >= 0) {
        tag = "[POWER-ON]";
      } else if (rLower.indexOf("restart") >= 0) {
        tag = "[RESTART]";
      } else if (rLower.indexOf("external") >= 0) {
        tag = "[EXT-RESET]";
      }

      f.print(tag);
      RtcClockBlock rtcBlock;
      if (rtcClockLoad(rtcBlock)) {
        f.print(" zuletzt aktiv ca. ");
        f.print(formatClockText(rtcBlock.epochSec, rtcBlock.tzSec));
      } else {
        f.print(" Zeit unbekannt");
      }
      f.print(" |");
      f.print(" Grund: ");
      f.print(lastResetReason);
      f.print(" | Details: ");
      f.print(lastResetInfo);
      uint32_t stepBlk[3];
      if (ESP.rtcUserMemoryRead(RTC_STEP_SLOT, stepBlk, sizeof(stepBlk)) &&
          stepBlk[0] == RTC_STEP_MAGIC && stepBlk[1] != 0) {
        f.print(" | Loeschschritt ");
        f.print(stepBlk[1]);
        f.print(" Heap ");
        f.print(stepBlk[2]);
      }
      f.println();
      f.close();
      markLapDeleteStep(0);
    }
  }
#elif defined(ESP32)
  lastResetReason = "Normal Boot (ESP32)";
  lastResetInfo = "";
  lastResetIsCrash = false;
#endif
}

String getCrashLog() {
  if (!LittleFS.exists("/crash.log")) {
    return "Keine Reset-Eintraege vorhanden.";
  }
  File f = LittleFS.open("/crash.log", "r");
  if (!f) return "Fehler beim Lesen des Reset-Logs.";
  String log = f.readString();
  f.close();
  return log.length() > 0 ? log : "Log ist leer.";
}

bool clearCrashLog() {
  return !LittleFS.exists("/crash.log") || LittleFS.remove("/crash.log");
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
  markLapDeleteSubStep(10);
  JsonDocument doc;
  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      deserializeJson(doc, f);
      f.close();
    }
  }
  markLapDeleteSubStep(11);

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

  markLapDeleteSubStep(12);
  File fWrite = LittleFS.open("/tracks.json", "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
  markLapDeleteSubStep(13);
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
#if defined(ESP8266)
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
#elif defined(ESP32)
  File dir = LittleFS.open(dirPath);
  if (!dir || !dir.isDirectory()) return;
  File file = dir.openNextFile();
  while (file) {
    String subPath = file.path();
    if (file.isDirectory()) {
      removeDirectoryRecursive(subPath);
    } else {
      LittleFS.remove(subPath);
    }
    file = dir.openNextFile();
  }
  LittleFS.rmdir(dirPath);
#endif
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
  currentObj.reserve(1024);
  size_t bytesProcessed = 0;

  while (fIn.available()) {
    char c = (char)fIn.read();
    // Kein yield(): Aufrufer laufen teils im Async-Webserver-Kontext (yield dort = Panic)
    ++bytesProcessed;
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
  if (apSSID.length() == 0) {
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
    IrDecoder::setTwoFrameFilterEnabled(true);
    useSz1 = true;
    useSz2 = true;
    useSz3 = true;
    checkpointMode = false;
    dragraceMode = false;
    sectorAudioFeedback = true;
  } else {
    uint16_t low = EEPROM.read(301);
    uint16_t high = EEPROM.read(302);
    minLapTimeMs = low | (high << 8);
    if (minLapTimeMs < 500 || minLapTimeMs > 30000) {
      minLapTimeMs = DEFAULT_MIN_LAP_MS;
    }

    IrDecoder::setTwoFrameFilterEnabled(EEPROM.read(303) != 0);
    useSz1 = (EEPROM.read(304) != 0);
    useSz2 = (EEPROM.read(305) != 0);
    useSz3 = (EEPROM.read(306) != 0);
    checkpointMode = (EEPROM.read(307) != 0);
    dragraceMode = (EEPROM.read(308) != 0);
    sectorAudioFeedback = (EEPROM.read(309) != 0);
  }
}

bool saveNamesToEEPROM(const String& namesJson) {
  JsonDocument names;
  if (deserializeJson(names, namesJson) || !names.is<JsonObject>()) return false;

  JsonObject root = names.as<JsonObject>();
  for (uint8_t id = 1; id <= MAX_CARS; id++) {
    String idKey = String(id);
    JsonVariant entry = root[idKey];
    if (!entry.is<JsonObject>() ||
        !entry["driver"].is<String>() ||
        !entry["car"].is<String>()) return false;

    const String driver = entry["driver"].as<String>();
    const String car = entry["car"].as<String>();
    if (driver.length() > 60 || car.length() > 60) return false;
  }

  File f = LittleFS.open("/names.json", "w");
  if (!f) return false;
  size_t written = serializeJson(names, f);
  f.close();
  if (written == 0) return false;

  for (uint8_t id = 1; id <= MAX_CARS; id++) {
    String idKey = String(id);
    driverNames[id] = names[idKey]["driver"].as<String>();
    carNames[id] = names[idKey]["car"].as<String>();
  }
  return true;
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

static String buildSessionEntryJson(const String& sId, const String& sFile, const String& trkId, const String& trkName, uint8_t mode, uint64_t startSec, uint64_t endSec, uint16_t carLaps[], uint32_t carBest[], uint32_t carAvg[] = nullptr, const String* driverOverrides = nullptr, const String* carOverrides = nullptr) {
  uint8_t activeIds[MAX_CARS + 1];
  uint8_t count = 0;
  for (uint8_t c = 1; c <= MAX_CARS; c++) {
    if (carLaps[c] > 0 || carBest[c] > 0) {
      activeIds[count++] = c;
    }
  }

  // Sortieren nach Modus: Quali (1), Drag (4) & Ghost Battle (5) nach Bestzeit, sonst nach Runden & Bestzeit
  bool sortByBest = (mode == 1 || mode == 4 || mode == 5);
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
    String driver = (driverOverrides != nullptr && driverOverrides[c].length() > 0)
      ? driverOverrides[c]
      : (sessionDriverNames[c][0] ? String(sessionDriverNames[c]) : driverNames[c]);
    String car = (carOverrides != nullptr && carOverrides[c].length() > 0)
      ? carOverrides[c]
      : (sessionCarNames[c][0] ? String(sessionCarNames[c]) : carNames[c]);
    out += "{\"pos\":" + String(idx + 1) + ",\"id\":" + String(c) + ",\"driver\":\"" + escapeJsonStr(driver) + "\",\"car\":\"" + escapeJsonStr(car) + "\",\"laps\":" + String(carLaps[c]) + ",\"bestLap\":" + String(carBest[c]) + ",\"avgLap\":" + String(aLap) + "}";
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
      f.print("]}                                                                ");
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
      size_t diff = currentSessionIndexLen - entryJson.length() + 32;
      for (size_t i = 0; i < diff; i++) f.print(' ');
    } else {
      currentSessionIndexLen = entryJson.length();
      f.print("                                ");
    }
    f.close();
  } else {
    // Ende des Arrays ']' von hinten suchen
    size_t searchStart = fSize - ((fSize > 512) ? 512 : fSize);
    f.seek(searchStart, SeekSet);
    String tail = "";
    tail.reserve(520);
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
    f.print("]}                                                                ");
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
    sessionDriverNames[i][0] = '\0';
    sessionCarNames[i][0] = '\0';
  }
  sessionNamesWrittenMask = 0;

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

// Loeschauftraege aus dem Webserver-Callback; Abarbeitung im loop()
#define DELETE_QUEUE_MAX 4
#define DELETE_PATH_MAX 64
static char deleteQueue[DELETE_QUEUE_MAX][DELETE_PATH_MAX];
static volatile uint8_t deleteQueueHead = 0;
static volatile uint8_t deleteQueueTail = 0;

bool queueFileDeletion(const String& path) {
  if (path.length() == 0 || path.length() >= DELETE_PATH_MAX) return false;
  uint8_t nextHead = (deleteQueueHead + 1) % DELETE_QUEUE_MAX;
  if (nextHead == deleteQueueTail) return false;
  strlcpy(deleteQueue[deleteQueueHead], path.c_str(), DELETE_PATH_MAX);
  deleteQueueHead = nextHead;
  return true;
}

static void processFileDeletion(const String& filename) {
  int lastSlash = filename.lastIndexOf('/');
  if (lastSlash > 0 && filename.endsWith(".json")) {
    String baseName = filename.substring(lastSlash + 1, filename.length() - 5);
    deleteSessionById(baseName);
  }
  if (LittleFS.exists(filename)) {
    LittleFS.remove(filename);
  }
  updateFsUsageCache();
}

static struct {
  char sessionId[40];
  char file[DELETE_PATH_MAX];
  uint8_t carId;
  uint16_t lapNum;
  uint32_t lapTimeMs;
} lapDeleteJob;
static volatile LapDeleteState lapDeleteState = LAP_DELETE_IDLE;

bool queueLapDeletion(const String& sessionId, const String& filePath, uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs) {
  if (lapDeleteState == LAP_DELETE_PENDING) return false;
  if (sessionId.length() >= sizeof(lapDeleteJob.sessionId) || filePath.length() >= DELETE_PATH_MAX) return false;
  strlcpy(lapDeleteJob.sessionId, sessionId.c_str(), sizeof(lapDeleteJob.sessionId));
  strlcpy(lapDeleteJob.file, filePath.c_str(), DELETE_PATH_MAX);
  lapDeleteJob.carId = carId;
  lapDeleteJob.lapNum = lapNum;
  lapDeleteJob.lapTimeMs = lapTimeMs;
  lapDeleteState = LAP_DELETE_PENDING;
  return true;
}

LapDeleteState getLapDeletionState() {
  return lapDeleteState;
}

void processStorageQueue() {
  static uint32_t lastStorageWrite = 0;

  updateResetLogClock();

  if (lapDeleteState == LAP_DELETE_PENDING) {
    LapDeleteState result = deleteLapFromSession(String(lapDeleteJob.sessionId), String(lapDeleteJob.file),
                                                 lapDeleteJob.carId, lapDeleteJob.lapNum, lapDeleteJob.lapTimeMs);
    updateFsUsageCache();
    lapDeleteState = result;
    lastStorageWrite = millis();
    return;
  }

  if (deleteQueueHead != deleteQueueTail) {
    String path = String(deleteQueue[deleteQueueTail]);
    deleteQueueTail = (deleteQueueTail + 1) % DELETE_QUEUE_MAX;
    processFileDeletion(path);
    lastStorageWrite = millis();
    return;
  }

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

  // 5. Runden-JSON formatieren & stromausfallsicher in die Session-Datei schreiben (Speicheroptimiert)
  char newLapJson[512];
  int len = snprintf(newLapJson, sizeof(newLapJson),
                     "{\"c\":%u,\"l\":%u,\"t\":%lu,\"p\":%u,\"s\":%llu",
                     carId, lapNum, (unsigned long)lapTimeMs, position, timestampMs);
  bool lapJsonValid = len > 0 && (size_t)len < sizeof(newLapJson);
  size_t jsonLength = lapJsonValid ? (size_t)len : 0;
  bool writeNames = carId > 0 && carId <= MAX_CARS &&
                    (sessionNamesWrittenMask & ((uint16_t)1 << carId)) == 0;
  if (lapJsonValid && writeNames) {
    lapJsonValid = appendJsonStringField(newLapJson, sizeof(newLapJson), jsonLength, "d", driverNames[carId]) &&
                   appendJsonStringField(newLapJson, sizeof(newLapJson), jsonLength, "v", carNames[carId]);
  }
  if (lapJsonValid && sz1 > 0) {
    int added = snprintf(newLapJson + jsonLength, sizeof(newLapJson) - jsonLength, ",\"1\":%lu", (unsigned long)sz1);
    lapJsonValid = added > 0 && (size_t)added < sizeof(newLapJson) - jsonLength;
    if (lapJsonValid) jsonLength += (size_t)added;
  }
  if (lapJsonValid && sz2 > 0) {
    int added = snprintf(newLapJson + jsonLength, sizeof(newLapJson) - jsonLength, ",\"2\":%lu", (unsigned long)sz2);
    lapJsonValid = added > 0 && (size_t)added < sizeof(newLapJson) - jsonLength;
    if (lapJsonValid) jsonLength += (size_t)added;
  }
  if (lapJsonValid && sz3 > 0) {
    int added = snprintf(newLapJson + jsonLength, sizeof(newLapJson) - jsonLength, ",\"3\":%lu", (unsigned long)sz3);
    lapJsonValid = added > 0 && (size_t)added < sizeof(newLapJson) - jsonLength;
    if (lapJsonValid) jsonLength += (size_t)added;
  }
  if (lapJsonValid && sz4 > 0) {
    int added = snprintf(newLapJson + jsonLength, sizeof(newLapJson) - jsonLength, ",\"4\":%lu", (unsigned long)sz4);
    lapJsonValid = added > 0 && (size_t)added < sizeof(newLapJson) - jsonLength;
    if (lapJsonValid) jsonLength += (size_t)added;
  }
  if (lapJsonValid && isOutLap) {
    int added = snprintf(newLapJson + jsonLength, sizeof(newLapJson) - jsonLength, ",\"o\":1");
    lapJsonValid = added > 0 && (size_t)added < sizeof(newLapJson) - jsonLength;
    if (lapJsonValid) jsonLength += (size_t)added;
  }
  if (lapJsonValid && jsonLength + 2 <= sizeof(newLapJson)) {
    newLapJson[jsonLength++] = '}';
    newLapJson[jsonLength] = '\0';
  } else {
    lapJsonValid = false;
  }

  bool lapSaved = false;
  if (lapJsonValid && !LittleFS.exists(currentSessionFilePath)) {
    File f = LittleFS.open(currentSessionFilePath, "w");
    if (f) {
      int written = f.printf("{\"i\":\"%s\",\"t\":\"%s\",\"n\":\"%s\",\"m\":%u,\"s\":%lu,\"l\":[%s]}",
               currentSessionId.c_str(),
               activeTrackId.c_str(),
               escapeJsonStr(activeTrackName).c_str(),
               mode,
               (unsigned long)currentSessionStartSec,
               newLapJson);
      lapSaved = written > 0;
      f.close();
    }
  } else if (lapJsonValid) {
    File f = LittleFS.open(currentSessionFilePath, "r+");
    if (f) {
      size_t size = f.size();
      if (size >= 2) {
        f.seek(size - 2, SeekSet);
        char lastChars[2];
        f.readBytes(lastChars, 2);
        if (lastChars[0] == '[' && lastChars[1] == ']') {
          f.seek(size - 1, SeekSet);
          lapSaved = f.print(newLapJson) > 0 && f.print("]}") > 0;
        } else {
          f.seek(size - 2, SeekSet);
          lapSaved = f.print(",") > 0 && f.print(newLapJson) > 0 && f.print("]}") > 0;
        }
      }
      f.close();
    }
  }

  if (lapSaved && writeNames) {
    copySessionName(sessionDriverNames[carId], sizeof(sessionDriverNames[carId]), driverNames[carId]);
    copySessionName(sessionCarNames[carId], sizeof(sessionCarNames[carId]), carNames[carId]);
    sessionNamesWrittenMask |= ((uint16_t)1 << carId);
  }

  // 6. Beim ersten Eintrag sofort im Index registrieren, danach verzögert über Queue
  if (currentSessionIndexOffset == 0) {
    flushCurrentSessionToIndex();
  } else {
    sessionIndexDirty = true;
  }
}

static void readSessionNames(JsonArrayConst laps, String* drivers, String* cars) {
  for (uint8_t id = 1; id <= MAX_CARS; id++) {
    drivers[id] = "";
    cars[id] = "";
  }
  for (JsonObjectConst lap : laps) {
    uint8_t id = lap["c"] | (lap["carId"] | 0);
    if (id < 1 || id > MAX_CARS) continue;
    if (drivers[id].length() == 0 && lap["d"].is<const char*>()) {
      drivers[id] = lap["d"].as<String>();
    }
    if (cars[id].length() == 0 && lap["v"].is<const char*>()) {
      cars[id] = lap["v"].as<String>();
    }
  }
}

static void preserveSessionNamesOnFirstLaps(JsonArray laps, const String* drivers, const String* cars) {
  bool seen[MAX_CARS + 1] = {false};
  for (JsonObject lap : laps) {
    uint8_t id = lap["c"] | (lap["carId"] | 0);
    if (id < 1 || id > MAX_CARS || seen[id]) continue;
    seen[id] = true;
    if (drivers[id].length() > 0) lap["d"] = drivers[id];
    if (cars[id].length() > 0) lap["v"] = cars[id];
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

#define LAP_OBJ_MAX 640

// Liest das naechste Runden-Objekt "{...}" aus dem Stream; false bei Arrayende, Dateiende oder Ueberlauf
static bool readNextLapObject(File& f, char* buf, size_t cap, bool& overflow) {
  size_t len = 0;
  int depth = 0;
  bool inQuotes = false;
  bool escaped = false;
  while (f.available()) {
    char c = (char)f.read();
    if (depth == 0) {
      if (c == '{') { depth = 1; buf[len++] = c; }
      else if (c == ']') return false;
      continue;
    }
    if (len + 1 >= cap) { overflow = true; return false; }
    buf[len++] = c;
    if (escaped) { escaped = false; continue; }
    if (inQuotes) {
      if (c == '\\') escaped = true;
      else if (c == '"') inQuotes = false;
      continue;
    }
    if (c == '"') inQuotes = true;
    else if (c == '{') depth++;
    else if (c == '}' && --depth == 0) { buf[len] = '\0'; return true; }
  }
  return false;
}

struct LapRewriteResult {
  String trackId;
  String trackName;
  uint8_t mode = 0;
  uint32_t startTs = 0;
  uint32_t deletedLapTimeMs = 0;
  uint16_t remaining = 0;
  uint16_t calcLaps[MAX_CARS + 1] = {0};
  uint32_t calcBest[MAX_CARS + 1] = {0};
  uint64_t maxTsSec = 0;
  String drivers[MAX_CARS + 1];
  String cars[MAX_CARS + 1];
};

// Schreibt die Session ohne die Zielrunde in eine Temp-Datei und ersetzt dann atomar das Original.
// Es liegt immer nur ein Runden-Objekt im RAM; bei jedem Fehler bleibt das Original unveraendert.
static LapDeleteState rewriteSessionWithoutLap(const String& file, uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, LapRewriteResult& r) {
  char* buf = (char*)malloc(LAP_OBJ_MAX * 2);
  if (!buf) return LAP_DELETE_NO_HEAP;
  char* outBuf = buf + LAP_OBJ_MAX;

  File in = LittleFS.open(file, "r");
  if (!in) { free(buf); return LAP_DELETE_NOT_FOUND; }

  // Header bis einschliesslich "l":[ einlesen
  String head;
  head.reserve(160);
  bool headFound = false;
  while (in.available() && head.length() < 600) {
    head += (char)in.read();
    if (head.endsWith("\"l\":[")) { headFound = true; break; }
  }
  if (!headFound) { in.close(); free(buf); return LAP_DELETE_NOT_FOUND; }
  {
    String hdrJson = head.substring(0, head.length() - 5);
    if (hdrJson.endsWith(",")) hdrJson.remove(hdrJson.length() - 1);
    hdrJson += "}";
    JsonDocument hdr;
    if (deserializeJson(hdr, hdrJson)) { in.close(); free(buf); return LAP_DELETE_NOT_FOUND; }
    r.trackId = hdr["t"] | (hdr["trackId"] | "");
    r.trackName = hdr["n"] | (hdr["trackName"] | "");
    r.mode = hdr["m"] | (hdr["mode"] | 0);
    r.startTs = hdr["s"] | (hdr["startTs"] | 0);
    r.maxTsSec = r.startTs;
  }

  // Durchlauf 1: Zielrunde finden, Ergebnisse und Namen der verbleibenden Runden sammeln
  bool overflow = false;
  bool parseError = false;
  int idx = 0;
  int targetIdx = -1;
  while (readNextLapObject(in, buf, LAP_OBJ_MAX, overflow)) {
    JsonDocument d;
    if (deserializeJson(d, buf)) { parseError = true; break; }
    uint8_t c = d["c"] | (d["carId"] | 0);
    uint16_t lNum = d["l"] | (d["lap"] | 0);
    uint32_t tMs = d["t"] | (d["time"] | 0);
    if (c >= 1 && c <= MAX_CARS) {
      if (r.drivers[c].length() == 0 && d["d"].is<const char*>()) r.drivers[c] = d["d"].as<String>();
      if (r.cars[c].length() == 0 && d["v"].is<const char*>()) r.cars[c] = d["v"].as<String>();
    }
    bool isTarget = targetIdx < 0 && (carId == 0 || c == carId) &&
                    ((lapNum > 0 && lNum == lapNum) || (lapTimeMs > 0 && tMs == lapTimeMs));
    if (isTarget) {
      targetIdx = idx;
      r.deletedLapTimeMs = tMs;
    } else {
      r.remaining++;
      uint64_t tsMs = d["s"] | (d["ts"] | 0);
      if (c >= 1 && c <= MAX_CARS) {
        r.calcLaps[c]++;
        if (tMs > 0 && (r.calcBest[c] == 0 || tMs < r.calcBest[c])) r.calcBest[c] = tMs;
        if (tsMs / 1000ULL > r.maxTsSec) r.maxTsSec = tsMs / 1000ULL;
      }
    }
    idx++;
    yield(); // nur aus loop() aufgerufen
  }
  if (overflow || parseError || targetIdx < 0) { in.close(); free(buf); return LAP_DELETE_NOT_FOUND; }

  // Letzte Runde der Session: es gibt nichts umzuschreiben, der Aufrufer loescht die ganze Session
  if (r.remaining == 0) { in.close(); free(buf); return LAP_DELETE_OK; }

  // Durchlauf 2: Temp-Datei ohne Zielrunde schreiben
  String tmpPath = file + ".tmp";
  if (LittleFS.exists(tmpPath)) LittleFS.remove(tmpPath);
  File out = LittleFS.open(tmpPath, "w");
  if (!out) { in.close(); free(buf); return LAP_DELETE_NO_SPACE; }

  bool ok = out.print(head) == head.length();
  in.seek(head.length(), SeekSet);
  bool seen[MAX_CARS + 1] = {false};
  bool firstOut = true;
  idx = 0;
  while (ok && readNextLapObject(in, buf, LAP_OBJ_MAX, overflow)) {
    if (idx++ == targetIdx) continue;
    JsonDocument d;
    if (deserializeJson(d, buf)) { ok = false; break; }
    uint8_t c = d["c"] | (d["carId"] | 0);
    if (c >= 1 && c <= MAX_CARS && !seen[c]) {
      seen[c] = true;
      if (r.drivers[c].length() > 0) d["d"] = r.drivers[c];
      if (r.cars[c].length() > 0) d["v"] = r.cars[c];
    }
    size_t n = serializeJson(d, outBuf, LAP_OBJ_MAX);
    if (n == 0 || n >= LAP_OBJ_MAX) { ok = false; break; }
    if (!firstOut && out.write((const uint8_t*)",", 1) != 1) { ok = false; break; }
    if (out.write((const uint8_t*)outBuf, n) != n) { ok = false; break; }
    firstOut = false;
    yield(); // nur aus loop() aufgerufen
  }
  if (ok && out.print("]}") != 2) ok = false;
  if (overflow) ok = false;
  out.close();
  in.close();
  free(buf);

  if (!ok) {
    LittleFS.remove(tmpPath);
    return LAP_DELETE_NO_SPACE;
  }
  // rename ersetzt das Original atomar (LittleFS)
  if (!LittleFS.rename(tmpPath, file)) {
    LittleFS.remove(tmpPath);
    return LAP_DELETE_NO_SPACE;
  }
  return LAP_DELETE_OK;
}

LapDeleteState deleteLapFromSession(const String& sessionId, const String& filePath, uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs) {
  if (sessionId.length() == 0 && filePath.length() == 0) return LAP_DELETE_NOT_FOUND;
  if (sessionIndexDirty) flushCurrentSessionToIndex();

  // 1. Dateipfad der Session ermitteln
  String targetFile = filePath;
  if (targetFile.length() > 0 && !targetFile.startsWith("/")) {
    targetFile = "/" + targetFile;
  }

  if (targetFile.length() == 0 || !LittleFS.exists(targetFile)) {
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
  }

  if (targetFile.length() == 0 || !LittleFS.exists(targetFile)) return LAP_DELETE_NOT_FOUND;

  if (PlatformRuntime::isHeapCritical(4000, 100, 0)) return LAP_DELETE_NO_HEAP;
  markLapDeleteStep(1);
  LapRewriteResult r;
  LapDeleteState rewriteState = rewriteSessionWithoutLap(targetFile, carId, lapNum, lapTimeMs, r);
  if (rewriteState != LAP_DELETE_OK) { markLapDeleteStep(0); return rewriteState; }
  markLapDeleteStep(2);

  uint32_t deletedLapTimeMs = r.deletedLapTimeMs;
  String sessTrackId = r.trackId.length() > 0 ? r.trackId : activeTrackId;
  // Falls die gelöschte Runde in der Top-5 Rekordliste stand, auch dort automatisch korrigieren.
  // deleteTrackRecordEntry übernimmt die automatische Kaskadierung (Daily <-> All-Time falls Datum heute).
  if (deletedLapTimeMs > 0) {
    bool handled = false;
    if (sessTrackId == activeTrackId) {
      for (uint8_t i = 0; i < activeDailyCount; i++) {
        if (activeDailyRecords[i].lapMs == deletedLapTimeMs && activeDailyRecords[i].carId == carId) {
          deleteTrackRecordEntry(activeTrackId, true, (int8_t)i);
          handled = true;
          break;
        }
      }
      if (!handled) {
        for (uint8_t i = 0; i < activeAllTimeCount; i++) {
          if (activeAllTimeRecords[i].lapMs == deletedLapTimeMs && activeAllTimeRecords[i].carId == carId) {
            deleteTrackRecordEntry(activeTrackId, false, (int8_t)i);
            break;
          }
        }
      }
    } else {
      // Session auf einer anderen (nicht aktiven) Strecke: passenden Eintrag in tracks.json suchen
      if (LittleFS.exists("/tracks.json")) {
        File f = LittleFS.open("/tracks.json", "r");
        if (f) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, f);
          f.close();
          if (!err) {
            JsonArray arr = doc["tracks"].as<JsonArray>();
            for (JsonObject t : arr) {
              if (t["id"].as<String>() == sessTrackId) {
                RecordEntry dList[MAX_TRACK_RECORDS];
                uint8_t dCount = 0;
                loadRecordArrayFromJson(t, "dailyRecords", dList, dCount);
                int8_t dIdx = findMatchingRecordIndex(dList, dCount, deletedLapTimeMs, carId, "", "");
                if (dIdx >= 0) {
                  deleteTrackRecordEntry(sessTrackId, true, dIdx);
                } else {
                  RecordEntry aList[MAX_TRACK_RECORDS];
                  uint8_t aCount = 0;
                  loadRecordArrayFromJson(t, "allTimeRecords", aList, aCount);
                  int8_t aIdx = findMatchingRecordIndex(aList, aCount, deletedLapTimeMs, carId, "", "");
                  if (aIdx >= 0) {
                    deleteTrackRecordEntry(sessTrackId, false, aIdx);
                  }
                }
                break;
              }
            }
          }
        }
      }
    }
  }

  markLapDeleteStep(3);
  if (r.remaining == 0) {
    deleteSessionById(sessionId);
    markLapDeleteStep(0);
    return LAP_DELETE_OK;
  }

  uint16_t* calcLaps = r.calcLaps;
  uint32_t* calcBest = r.calcBest;
  String* sessionDrivers = r.drivers;
  String* sessionCars = r.cars;
  uint64_t maxTsSec = r.maxTsSec;

  if (currentSessionActive && currentSessionId == sessionId) {
    for (uint8_t c = 1; c <= MAX_CARS; c++) {
      sessionCarLaps[c] = calcLaps[c];
      sessionCarBest[c] = calcBest[c];
      copySessionName(sessionDriverNames[c], sizeof(sessionDriverNames[c]), sessionDrivers[c]);
      copySessionName(sessionCarNames[c], sizeof(sessionCarNames[c]), sessionCars[c]);
      if (sessionDrivers[c].length() > 0) sessionNamesWrittenMask |= ((uint16_t)1 << c);
    }
  }

  String updatedEntry = buildSessionEntryJson(
    sessionId,
    targetFile,
    sessTrackId,
    r.trackName.length() > 0 ? r.trackName : activeTrackName,
    r.mode,
    r.startTs,
    maxTsSec,
    calcLaps,
    calcBest,
    nullptr,
    sessionDrivers,
    sessionCars
  );

  markLapDeleteStep(4);
  filterSessionsIndexFile("", "", sessionId, updatedEntry);
  markLapDeleteStep(0);
  return LAP_DELETE_OK;
}

void deleteDriverHistory(const String& driverName) {
  if (driverName.length() == 0) return;
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
          int sIdx = currentObj.indexOf("\"id\":\"");
          int fIdx = currentObj.indexOf("\"file\":\"");
          if (sIdx >= 0 && fIdx >= 0) {
            String sId = currentObj.substring(sIdx + 6, currentObj.indexOf("\"", sIdx + 6));
            String sFile = currentObj.substring(fIdx + 8, currentObj.indexOf("\"", fIdx + 8));
            sessionIds.push_back(sId);
            sessionFiles.push_back(sFile);
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

    JsonArray laps = doc["l"].as<JsonArray>();
    if (!laps) continue;

    String sessionDrivers[MAX_CARS + 1];
    String sessionCars[MAX_CARS + 1];
    readSessionNames(laps, sessionDrivers, sessionCars);
    for (int i = (int)laps.size() - 1; i >= 0; i--) {
      uint8_t c = laps[i]["c"] | (laps[i]["carId"] | 0);
      if (c >= 1 && c <= MAX_CARS && sessionDrivers[c].equalsIgnoreCase(driverName)) {
        laps.remove(i);
      }
    }

    if (laps.size() == 0) {
      deleteSessionById(sId);
    } else {
      preserveSessionNamesOnFirstLaps(laps, sessionDrivers, sessionCars);
      uint16_t calcLaps[MAX_CARS + 1] = {0};
      uint32_t calcBest[MAX_CARS + 1] = {0};
      uint64_t maxTsSec = doc["s"] | (doc["startTs"] | 0);
      for (JsonObject l : laps) {
        uint8_t c = l["c"] | (l["carId"] | 0);
        uint32_t tMs = l["t"] | (l["time"] | 0);
        uint64_t tsMs = l["s"] | (l["ts"] | 0);
        if (c >= 1 && c <= MAX_CARS) {
          calcLaps[c]++;
          if (tMs > 0 && (calcBest[c] == 0 || tMs < calcBest[c])) calcBest[c] = tMs;
          if (tsMs / 1000ULL > maxTsSec) maxTsSec = tsMs / 1000ULL;
        }
      }
      File fWrite = LittleFS.open(sFile, "w");
      if (fWrite) { serializeJson(doc, fWrite); fWrite.close(); }

      if (currentSessionActive && currentSessionId == sId) {
        for (uint8_t c = 1; c <= MAX_CARS; c++) {
          sessionCarLaps[c] = calcLaps[c];
          sessionCarBest[c] = calcBest[c];
          copySessionName(sessionDriverNames[c], sizeof(sessionDriverNames[c]), sessionDrivers[c]);
          copySessionName(sessionCarNames[c], sizeof(sessionCarNames[c]), sessionCars[c]);
          if (sessionDrivers[c].length() > 0) sessionNamesWrittenMask |= ((uint16_t)1 << c);
        }
      }

      String updatedEntry = buildSessionEntryJson(
        sId, sFile,
        doc["t"] | (doc["trackId"] | activeTrackId),
        doc["n"] | (doc["trackName"] | activeTrackName),
        doc["m"] | (doc["mode"] | 0),
        doc["s"] | (doc["startTs"] | 0),
        maxTsSec, calcLaps, calcBest, nullptr, sessionDrivers, sessionCars
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
    sessionDriverNames[i][0] = '\0';
    sessionCarNames[i][0] = '\0';
  }
  sessionNamesWrittenMask = 0;

  // Alle Unterordner im Root (Strecken-Ordner) leeren & entfernen
#if defined(ESP8266)
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
#elif defined(ESP32)
  File root = LittleFS.open("/");
  std::vector<String> dirsToRemove;
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) {
      if (f.isDirectory()) {
        dirsToRemove.push_back(f.path());
      } else {
        String fn = f.name();
        if (fn.startsWith("stats_") && fn.endsWith(".json")) {
          LittleFS.remove(f.path());
        }
      }
      f = root.openNextFile();
    }
  }
#endif
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
