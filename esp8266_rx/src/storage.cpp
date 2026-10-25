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
uint8_t activeWifiMode = 1; // Standard: 1 = Heim-WLAN

// Aktive Strecke & Speicherstatus
String activeTrackId = "standard";
String activeTrackName = "Standard-Strecke";
uint8_t cachedFsUsedPct = 0;
uint32_t cachedFsUsedBytes = 0;
uint32_t cachedFsTotalBytes = 0;
bool storageFullLocked = false;
int32_t clientTzOffsetSec = 3600; // Default UTC+1 (MEZ), wird vom Browser exakt synchronisiert

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

void checkDailyBestDateRollover() {
  static uint32_t lastCheckMs = 0;
  if (millis() - lastCheckMs < 5000) return;
  lastCheckMs = millis();
  if (baseUnixTimestampSec == 0) return;

  String today = getCurrentLocalDateStr();
  if (today.length() == 0) return;

  if (dailyBestLapMs > 0 && dailyBestDate.length() > 0 && dailyBestDate != today) {
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    dailyBestDate = today;
    saveActiveTrackBestToFS();

    char buf[160];
    snprintf(buf, sizeof(buf), "{\"type\":\"track_record\",\"trackId\":\"%s\",\"trackName\":\"%s\",\"bestLap\":0}",
             activeTrackId.c_str(), activeTrackName.c_str());
    broadcastWebSocket(buf);
  }
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
          activeTrackName = foundObj["name"] | "Standard-Strecke";
          dailyBestLapMs = foundObj["bestLap"] | 0;
          dailyBestCarId = foundObj["bestCarId"] | 0;
          dailyBestDriver = foundObj["bestDriver"] | "";
          dailyBestCar = foundObj["bestCar"] | "";
          dailyBestDate = foundObj["bestDate"] | "";
          String today = getCurrentLocalDateStr();
          if (today.length() > 0 && dailyBestDate.length() > 0 && dailyBestDate != today && dailyBestLapMs > 0) {
            dailyBestLapMs = 0;
            dailyBestCarId = 0;
            dailyBestDriver = "";
            dailyBestCar = "";
            dailyBestDate = today;
            foundObj["bestLap"] = 0;
            foundObj["bestCarId"] = 0;
            foundObj["bestDriver"] = "";
            foundObj["bestCar"] = "";
            foundObj["bestDate"] = today;
            File fFix = LittleFS.open("/tracks.json", "w");
            if (fFix) { serializeJson(doc, fFix); fFix.close(); }
          }
          loaded = true;
        }
      }
    }
  }

  if (!loaded) {
    activeTrackId = "standard";
    activeTrackName = "Standard-Strecke";
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    dailyBestDate = getCurrentLocalDateStr();
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

  bool found = false;
  for (JsonObject t : arr) {
    if (t["id"].as<String>() == activeTrackId) {
      t["name"] = activeTrackName;
      t["bestLap"] = dailyBestLapMs;
      t["bestCarId"] = dailyBestCarId;
      t["bestDriver"] = dailyBestDriver;
      t["bestCar"] = dailyBestCar;
      t["bestDate"] = dailyBestDate;
      found = true;
      break;
    }
  }

  if (!found) {
    JsonObject t = arr.add<JsonObject>();
    t["id"] = activeTrackId;
    t["name"] = activeTrackName;
    t["bestLap"] = dailyBestLapMs;
    t["bestCarId"] = dailyBestCarId;
    t["bestDriver"] = dailyBestDriver;
    t["bestCar"] = dailyBestCar;
    t["bestDate"] = dailyBestDate;
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
      uint32_t bLap = t["bestLap"] | 0;
      String bDate = t["bestDate"] | "";
      String today = getCurrentLocalDateStr();
      if (today.length() > 0 && bDate.length() > 0 && bDate != today && bLap > 0) {
        dailyBestLapMs = 0;
        dailyBestCarId = 0;
        dailyBestDriver = "";
        dailyBestCar = "";
        dailyBestDate = today;
        t["bestLap"] = 0;
        t["bestCarId"] = 0;
        t["bestDriver"] = "";
        t["bestCar"] = "";
        t["bestDate"] = today;
      } else {
        dailyBestLapMs = bLap;
        dailyBestCarId = t["bestCarId"] | 0;
        dailyBestDriver = t["bestDriver"] | "";
        dailyBestCar = t["bestCar"] | "";
        dailyBestDate = bDate;
      }
      doc["active"] = activeTrackId;
      File fWrite = LittleFS.open("/tracks.json", "w");
      if (fWrite) {
        serializeJson(doc, fWrite);
        fWrite.close();
      }
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
      uint32_t bLap = t["bestLap"] | 0;
      String bDate = t["bestDate"] | "";
      String today = getCurrentLocalDateStr();
      if (today.length() > 0 && bDate.length() > 0 && bDate != today && bLap > 0) {
        dailyBestLapMs = 0;
        dailyBestCarId = 0;
        dailyBestDriver = "";
        dailyBestCar = "";
        dailyBestDate = today;
        t["bestLap"] = 0;
        t["bestCarId"] = 0;
        t["bestDriver"] = "";
        t["bestCar"] = "";
        t["bestDate"] = today;
      } else {
        dailyBestLapMs = bLap;
        dailyBestCarId = t["bestCarId"] | 0;
        dailyBestDriver = t["bestDriver"] | "";
        dailyBestCar = t["bestCar"] | "";
        dailyBestDate = bDate;
      }
      exists = true;
      break;
    }
  }

  if (!exists) {
    if (arr.size() >= 20) return false; // Schutz vor Überlauf
    String today = getCurrentLocalDateStr();
    JsonObject t = arr.add<JsonObject>();
    t["id"] = slug;
    t["name"] = cleanName;
    t["bestLap"] = 0;
    t["bestCarId"] = 0;
    t["bestDriver"] = "";
    t["bestCar"] = "";
    t["bestDate"] = today;
    activeTrackId = slug;
    activeTrackName = cleanName;
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    dailyBestDate = today;
  }

  doc["active"] = activeTrackId;
  File fWrite = LittleFS.open("/tracks.json", "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
  resetAllLaps(false);
  updateFsUsageCache();
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
    def["name"] = "Standard-Strecke";
    def["bestLap"] = 0;
    def["bestCarId"] = 0;
    def["bestDriver"] = "";
    def["bestCar"] = "";
  }

  JsonObject nextActive = arr[0];
  activeTrackId = nextActive["id"].as<String>();
  activeTrackName = nextActive["name"].as<String>();
  dailyBestLapMs = nextActive["bestLap"] | 0;
  dailyBestCarId = nextActive["bestCarId"] | 0;
  dailyBestDriver = nextActive["bestDriver"] | "";
  dailyBestCar = nextActive["bestCar"] | "";
  doc["active"] = activeTrackId;

  File fWrite = LittleFS.open("/tracks.json", "w");
  if (fWrite) {
    serializeJson(doc, fWrite);
    fWrite.close();
  }
  resetAllLaps(false);
  updateFsUsageCache();
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

static String buildSessionEntryJson(const String& sId, const String& sFile, const String& trkId, const String& trkName, uint8_t mode, uint64_t startSec, uint64_t endSec, uint16_t carLaps[], uint32_t carBest[]) {
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
    out += "{\"pos\":" + String(idx + 1) + ",\"id\":" + String(c) + ",\"driver\":\"" + escapeJsonStr(driverNames[c]) + "\",\"car\":\"" + escapeJsonStr(carNames[c]) + "\",\"laps\":" + String(carLaps[c]) + ",\"bestLap\":" + String(carBest[c]) + "}";
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
    sessionCarBest
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
  }

  updateFsUsageCache();
  storageFullLocked = (cachedFsUsedPct >= 95);
}

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs) {
  // Regel: Keine Aufzeichnung bevor Time-Sync durch ein verbundenes Gerät erfolgt ist!
  if (baseUnixTimestampSec == 0) return;

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
  } else if (sessionIndexDirty && (millis() - lastIndexFlushMs > 2000)) {
    flushCurrentSessionToIndex();
    lastStorageWrite = millis();
  }
}

void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs) {
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
  if (lapTimeMs > 0 && (sessionCarBest[carId] == 0 || lapTimeMs < sessionCarBest[carId])) {
    sessionCarBest[carId] = lapTimeMs;
  }
  sessionLastEndSec = timestampMs / 1000ULL;

  // 5. Runden-JSON formatieren & stromausfallsicher in die Session-Datei schreiben
  String escDriver = escapeJsonStr(driverNames[carId]);
  String escCar = escapeJsonStr(carNames[carId]);

  char newLapJson[340];
  snprintf(newLapJson, sizeof(newLapJson),
           "{\"carId\":%u,\"driver\":\"%s\",\"car\":\"%s\",\"lap\":%u,\"time\":%lu,\"sz1\":%lu,\"sz2\":%lu,\"sz3\":%lu,\"sz4\":%lu,\"pos\":%u,\"ts\":%llu}",
           carId, escDriver.c_str(), escCar.c_str(), lapNum, lapTimeMs, sz1, sz2, sz3, sz4, position, timestampMs);

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

  for (size_t i = 0; i < laps.size(); i++) {
    uint8_t cId = laps[i]["carId"] | 0;
    uint16_t lNum = laps[i]["lap"] | 0;
    if (cId == carId && lNum == lapNum) {
      laps.remove(i);
      break;
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
    doc["trackId"] | activeTrackId,
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

  // Alle Session-IDs sammeln, an denen dieser Fahrer beteiligt war
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

  // Streckenrekorde in /tracks.json zurücksetzen
  if (LittleFS.exists("/tracks.json")) {
    File f = LittleFS.open("/tracks.json", "r");
    if (f) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        f.close();
        JsonArray arr = doc["tracks"].as<JsonArray>();
        for (JsonObject t : arr) {
          t["bestLap"] = 0;
          t["bestCarId"] = 0;
          t["bestDriver"] = "";
          t["bestCar"] = "";
        }
        File fW = LittleFS.open("/tracks.json", "w");
        if (fW) { serializeJson(doc, fW); fW.close(); }
      } else {
        f.close();
      }
    }
  }

  dailyBestLapMs = 0;
  dailyBestCarId = 0;
  dailyBestDriver = "";
  dailyBestCar = "";

  updateFsUsageCache();
}
