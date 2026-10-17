#include "web_handlers.h"
#include "storage.h"
#include "lap_logic.h"
#include "esp_now_base.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include <utime.h>

void handleGetLaps(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->print("[");
  bool first = true;
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (lapCounter[i] > 0 || sz1TimeMs[i] > 0 || sz2TimeMs[i] > 0 || sz3TimeMs[i] > 0 || sz4TimeMs[i] > 0 || bestSz1TimeMs[i] > 0 || bestSz2TimeMs[i] > 0 || bestSz3TimeMs[i] > 0 || bestSz4TimeMs[i] > 0) {
      if (!first) response->print(",");
      response->printf("{\"id\":%d,\"driver\":\"%s\",\"car\":\"%s\",\"completedLaps\":%d,",
                       i, driverNames[i].c_str(), carNames[i].c_str(), (lapCounter[i] > 0 ? (lapCounter[i] - 1) : 0));
      response->printf("\"sz1\":%lu,\"sz2\":%lu,\"sz3\":%lu,\"sz4\":%lu,", sz1TimeMs[i], sz2TimeMs[i], sz3TimeMs[i], sz4TimeMs[i]);
      response->printf("\"bestSz1\":%lu,\"bestSz2\":%lu,\"bestSz3\":%lu,\"bestSz4\":%lu,", bestSz1TimeMs[i], bestSz2TimeMs[i], bestSz3TimeMs[i], bestSz4TimeMs[i]);
      response->printf("\"split1\":%lu,\"split2\":%lu,\"split3\":%lu,", split1TimeMs[i], split2TimeMs[i], split3TimeMs[i]);
      response->printf("\"lastLap\":%lu,\"bestLap\":%lu,\"lastSeenMs\":%lu}", lastLapDuration[i], bestLapDuration[i], lastSeenMs[i]);
      first = false;
    }
  }
  response->print("]");
  request->send(response);
}

void handleGetStationsStatus(AsyncWebServerRequest *request) {
  String json;
  json.reserve(160);
  json = "{";
  json += "\"sz1\":" + String(isStationOnline(1) ? "true" : "false") + ",";
  json += "\"sz2\":" + String(isStationOnline(2) ? "true" : "false") + ",";
  json += "\"sz3\":" + String(isStationOnline(3) ? "true" : "false") + ",";
  json += "\"sz1Bat\":" + String(isStationBatteryLow(1) ? "true" : "false") + ",";
  json += "\"sz2Bat\":" + String(isStationBatteryLow(2) ? "true" : "false") + ",";
  json += "\"sz3Bat\":" + String(isStationBatteryLow(3) ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

void handlePostSession(AsyncWebServerRequest *request) {
  if (request->hasParam("mode")) {
    int mode = request->getParam("mode")->value().toInt();
    RaceMode newMode = (RaceMode)mode;

    if (newMode != currentMode) {
      resetAllLaps();
    }

    currentMode = newMode;
    
    uint16_t duration = request->hasParam("duration") ? request->getParam("duration")->value().toInt() : 0;
    targetRounds = request->hasParam("laps") ? request->getParam("laps")->value().toInt() : 0;

    if (duration > 0) {
      modeEndTimeMs = millis() + ((uint32_t)duration * 60000UL);
    } else {
      modeEndTimeMs = 0;
    }

    for (uint8_t i = 0; i <= MAX_CARS; i++) {
      finishedFinalLap[i] = false;
    }
  }
  request->send(200, "text/plain", "OK");
}

void handleGetTimeLeft(AsyncWebServerRequest *request) {
  int32_t secondsLeft = -1;
  if (modeEndTimeMs > 0) {
    if (millis() < modeEndTimeMs) {
      secondsLeft = (modeEndTimeMs - millis()) / 1000;
    } else {
      secondsLeft = 0;
    }
  }
  String json;
  json.reserve(160);
  json = "{\"mode\":";
  json += (int)currentMode;
  json += ",\"secondsLeft\":";
  json += secondsLeft;
  json += ",\"serverMillis\":";
  json += millis();
  json += ",\"allFinished\":";
  json += (areAllActiveCarsFinished() ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

void handleGetRecent(AsyncWebServerRequest *request) {
  String json;
  json.reserve(64);
  json = "[";
  for (int i = 0; i < 5; i++) {
    json += recentIDs[i];
    if (i < 4) json += ",";
  }
  json += "]";
  request->send(200, "application/json", json);
}

void handleGetDeadtime(AsyncWebServerRequest *request) {
  String json;
  json.reserve(32);
  json = "{\"ms\":";
  json += minLapTimeMs;
  json += "}";
  request->send(200, "application/json", json);
}

void handleGetFilter(AsyncWebServerRequest *request) {
  String json;
  json.reserve(32);
  json = "{\"enabled\":";
  json += (requireTwoFrames ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

void handleGetNames(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->print("{");
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (i > 1) response->print(",");
    response->printf("\"%d\":{\"driver\":\"%s\",\"car\":\"%s\"}", i, driverNames[i].c_str(), carNames[i].c_str());
  }
  response->print("}");
  request->send(response);
}

void handleGetVersion(AsyncWebServerRequest *request) {
  String json;
  json.reserve(64);
  json = "{\"firmware\":\"" FIRMWARE_VERSION "\"}";
  request->send(200, "application/json", json);
}

void handlePostSetFilter(AsyncWebServerRequest *request) {
  if (request->hasParam("enable")) {
    bool enable = (request->getParam("enable")->value() == "1");
    requireTwoFrames = enable;
    saveFilterToEEPROM(enable);
  }
  request->send(200, "text/plain", "OK");
}

void handlePostSetDeadtime(AsyncWebServerRequest *request) {
  if (request->hasParam("ms")) {
    uint16_t ms = request->getParam("ms")->value().toInt();
    if (ms >= 500 && ms <= 30000) {
      minLapTimeMs = ms;
      saveDeadtimeToEEPROM(ms);
    }
  }
  request->send(200, "text/plain", "OK");
}

void handlePostRename(AsyncWebServerRequest *request) {
  if (request->hasParam("id")) {
    int id = request->getParam("id")->value().toInt();
    String driver = request->hasParam("driver") ? request->getParam("driver")->value() : driverNames[id];
    String car = request->hasParam("car") ? request->getParam("car")->value() : carNames[id];
    
    if (id >= 1 && id <= MAX_CARS) {
      driverNames[id] = driver;
      carNames[id] = car;
      saveNamesToEEPROM(id, driver, car);
    }
  }
  request->send(200, "text/plain", "OK");
}

void handlePostReset(AsyncWebServerRequest *request) {
  resetAllLaps();
  request->send(200, "text/plain", "OK");
}

void handlePostSyncTime(AsyncWebServerRequest *request) {
  if (request->hasParam("ts")) {
    uint64_t clientTsSec = strtoull(request->getParam("ts")->value().c_str(), NULL, 10);
    syncClockOnce(clientTsSec);
  }
  request->send(200, "text/plain", "OK");
}

void handleGetStats(AsyncWebServerRequest *request) {
  if (request->hasParam("id")) {
    uint8_t id = request->getParam("id")->value().toInt();
    String filepath = "/stats_" + String(id) + ".json";
    
    if (LittleFS.exists(filepath)) {
      request->send(LittleFS, filepath, "application/json");
    } else {
      request->send(200, "application/json", "{\"laps\":[]}");
    }
  } else {
    request->send(400, "text/plain", "Fehlende ID");
  }
}

void handlePostDeleteLap(AsyncWebServerRequest *request) {
  if (request->hasParam("id") && request->hasParam("lap")) {
    uint8_t id = request->getParam("id")->value().toInt();
    uint16_t lap = request->getParam("lap")->value().toInt();
    deleteLapFromHistory(id, lap);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
}

void handlePostDeleteDriverStats(AsyncWebServerRequest *request) {
  if (request->hasParam("id")) {
    uint8_t id = request->getParam("id")->value().toInt();
    deleteDriverHistory(id);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
}

void handlePostDeleteAllStats(AsyncWebServerRequest *request) {
  deleteAllHistory();
  request->send(200, "text/plain", "OK");
}

void handleGetSessions(AsyncWebServerRequest *request) {
  String filepath = "/sessions.json";
  if (LittleFS.exists(filepath)) {
    request->send(LittleFS, filepath, "application/json");
  } else {
    request->send(200, "application/json", "{\"sessions\":[]}");
  }
}

void handlePostSaveSessionResult(AsyncWebServerRequest *request) {
  if (request->hasParam("data", true)) {
    String jsonStr = request->getParam("data", true)->value();
    saveSessionToHistory(jsonStr);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
}

void handleGetSectorConfig(AsyncWebServerRequest *request) {
  String json;
  json.reserve(160);
  json = "{";
  json += "\"useSz1\":" + String(useSz1 ? "true" : "false") + ",";
  json += "\"useSz2\":" + String(useSz2 ? "true" : "false") + ",";
  json += "\"useSz3\":" + String(useSz3 ? "true" : "false") + ",";
  json += "\"checkpointMode\":" + String(checkpointMode ? "true" : "false") + ",";
  json += "\"dragraceMode\":" + String(dragraceMode ? "true" : "false") + ",";
  json += "\"sectorAudio\":" + String(sectorAudioFeedback ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

void handlePostSetSectorConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("sz1"))   useSz1 = (request->getParam("sz1")->value() == "1");
  if (request->hasParam("sz2"))   useSz2 = (request->getParam("sz2")->value() == "1");
  if (request->hasParam("sz3"))   useSz3 = (request->getParam("sz3")->value() == "1");
  if (request->hasParam("cp"))    checkpointMode = (request->getParam("cp")->value() == "1");
  if (request->hasParam("dr"))    dragraceMode = (request->getParam("dr")->value() == "1");
  if (request->hasParam("audio")) sectorAudioFeedback = (request->getParam("audio")->value() == "1");

  saveSectorConfigToEEPROM(useSz1, useSz2, useSz3, checkpointMode, dragraceMode, sectorAudioFeedback);
  request->send(200, "text/plain", "OK");
}

void handlePostDeleteSession(AsyncWebServerRequest *request) {
  if (request->hasParam("ts")) {
    uint64_t ts = strtoull(request->getParam("ts")->value().c_str(), NULL, 10);
    deleteSessionFromHistory(ts);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
}

static File uploadFile;

void handleGetDebugPackets(AsyncWebServerRequest *request) {
  request->send(200, "application/json", getDebugPacketsJSON());
}

void handleGetDebugStatus(AsyncWebServerRequest *request) {
  request->send(200, "application/json", getDebugMode() ? "{\"debug\":true}" : "{\"debug\":false}");
}

void handlePostSetDebugMode(AsyncWebServerRequest *request) {
  if (request->hasParam("active")) {
    bool active = (request->getParam("active")->value() == "1");
    setDebugMode(active);
  }
  request->send(200, "text/plain", "OK");
}

// Heap-schonender Upload-Handler mit PC-Zeitstempel (mtime)
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  static char cleanPath[64];
  static uint32_t targetMtime = 0;

  if (!index) {
    targetMtime = 0;
    if (request->hasParam("mtime")) {
      targetMtime = strtoul(request->getParam("mtime")->value().c_str(), NULL, 10);
    }

    if (!filename.startsWith("/")) {
      snprintf(cleanPath, sizeof(cleanPath), "/%s", filename.c_str());
    } else {
      snprintf(cleanPath, sizeof(cleanPath), "%s", filename.c_str());
    }
    
    if (strstr(cleanPath, ".gz") != NULL) {
      char rawPath[64];
      strncpy(rawPath, cleanPath, sizeof(rawPath));
      char *gzExt = strstr(rawPath, ".gz");
      if (gzExt) *gzExt = '\0';
      if (LittleFS.exists(rawPath)) {
        LittleFS.remove(rawPath);
      }
    }

    Serial.printf("[Upload] Start: %s (PC-mtime: %u)\n", cleanPath, targetMtime);
    uploadFile = LittleFS.open(cleanPath, "w");
  }
  
  if (uploadFile) {
    uploadFile.write(data, len);
  }
  
  if (final) {
    if (uploadFile) {
      timeval oldTv;
      bool timeSwapped = false;
      if (targetMtime > 0) {
        gettimeofday(&oldTv, nullptr);
        timeval newTv = { (time_t)targetMtime, 0 };
        settimeofday(&newTv, nullptr);
        timeSwapped = true;
      }

      uploadFile.close();

      if (timeSwapped) {
        settimeofday(&oldTv, nullptr);
      }

      Serial.printf("[Upload] Fertig: %s (%u Bytes, PC-mtime: %u)\n", filename.c_str(), index + len, targetMtime);

      if (filename.indexOf("names.json") >= 0) {
        loadNamesFromFS();
      } 
      else if (filename.indexOf("backup.json") >= 0) {
        File bFile = LittleFS.open("/" + filename, "r");
        if (bFile) {
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, bFile);
          bFile.close();

          if (!err) {
            if (doc["sessions"].is<JsonArray>()) {
              for (JsonObject sess : doc["sessions"].as<JsonArray>()) {
                String sessStr;
                serializeJson(sess, sessStr);
                saveSessionToHistory(sessStr);
              }
            }

            JsonObject statsObj;
            if (doc["driverStats"].is<JsonObject>()) {
              statsObj = doc["driverStats"].as<JsonObject>();
            } else if (doc["stats"].is<JsonObject>()) {
              statsObj = doc["stats"].as<JsonObject>();
            }

            if (!statsObj.isNull()) {
              for (JsonPair kv : statsObj) {
                uint8_t carId = String(kv.key().c_str()).toInt();
                if (carId >= 1 && carId <= MAX_CARS && kv.value()["laps"].is<JsonArray>()) {
                  for (JsonObject lapObj : kv.value()["laps"].as<JsonArray>()) {
                    uint16_t lap = lapObj["lap"] | 0;
                    uint32_t timeMs = lapObj["time"] | 0;
                    uint32_t sz1 = lapObj["sz1"] | 0;
                    uint32_t sz2 = lapObj["sz2"] | 0;
                    uint32_t sz3 = lapObj["sz3"] | 0;
                    uint32_t sz4 = lapObj["sz4"] | 0;
                    uint8_t mode = lapObj["mode"] | 0;
                    uint64_t ts = lapObj["ts"] | 0;
                    uint8_t pos = lapObj["pos"] | 1;
                    
                    if (lap > 0 && timeMs > 0) {
                      saveLapToHistory(carId, lap, timeMs, sz1, sz2, sz3, sz4, pos, mode, ts);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}


void handleGetCacheConfig(AsyncWebServerRequest *request) {
  String json = "{\"cache\":" + String(webCacheEnabled ? "true" : "false") + "}";
  request->send(200, "application/json", json);
}

void handlePostSetCacheConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("cache")) {
    webCacheEnabled = (request->getParam("cache")->value() == "1");
    saveCacheConfigToEEPROM(webCacheEnabled);
  }
  request->send(200, "text/plain", "OK");
}