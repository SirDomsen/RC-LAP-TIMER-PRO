#include "web_handlers.h"
#include "storage.h"
#include "lap_logic.h"
#include <LittleFS.h>

void handleGetLaps(AsyncWebServerRequest *request) {
  String json;
  json.reserve(1024);
  json = "[";
  bool first = true;
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (lapCounter[i] > 0) {
      if (!first) json += ",";
      json += "{\"id\":";
      json += i;
      json += ",\"driver\":\"";
      json += driverNames[i];
      json += "\",\"car\":\"";
      json += carNames[i];
      json += "\",\"completedLaps\":";
      json += (lapCounter[i] - 1);
      json += ",\"lastLap\":";
      json += lastLapDuration[i];
      json += ",\"bestLap\":";
      json += bestLapDuration[i];
      json += ",\"lastSeenMs\":";
      json += lastSeenMs[i];
      json += "}";
      first = false;
    }
  }
  json += "]";
  request->send(200, "application/json", json);
}

void handlePostSession(AsyncWebServerRequest *request) {
  if (request->hasParam("mode")) {
    int mode = request->getParam("mode")->value().toInt();
    currentMode = (RaceMode)mode;
    
    uint16_t duration = request->getParam("duration")->value().toInt();
    targetRounds = request->getParam("laps")->value().toInt();

    if (duration > 0) {
      modeEndTimeMs = millis() + ((uint32_t)duration * 60000UL);
    } else {
      modeEndTimeMs = 0;
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
  json.reserve(128);
  json = "{\"mode\":";
  json += (int)currentMode;
  json += ",\"secondsLeft\":";
  json += secondsLeft;
  json += ",\"serverMillis\":";
  json += millis();
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
  String json = "{\"ms\":" + String(minLapTimeMs) + "}";
  request->send(200, "application/json", json);
}

void handleGetFilter(AsyncWebServerRequest *request) {
  String json = "{\"enabled\":";
  json += (requireTwoFrames ? "true" : "false");
  json += "}";
  request->send(200, "application/json", json);
}

void handleGetNames(AsyncWebServerRequest *request) {
  String json;
  json.reserve(1024);
  json = "{";
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (i > 1) json += ",";
    json += "\"";
    json += i;
    json += "\":{\"driver\":\"";
    json += driverNames[i];
    json += "\",\"car\":\"";
    json += carNames[i];
    json += "\"}";
  }
  json += "}";
  request->send(200, "application/json", json);
}

void handleGetVersion(AsyncWebServerRequest *request) {
  String json = "{\"firmware\":\"" FIRMWARE_VERSION "\",\"ui\":\"" UI_VERSION "\"}";
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
    uint64_t clientTs = strtoull(request->getParam("ts")->value().c_str(), NULL, 10);
    latestClientTimestampMs = clientTs;
    lastSyncMillis = millis();
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

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    Serial.printf("[Update] Upload Start: %s\n", filename.c_str());
    uploadFile = LittleFS.open(filename, "w");
  }
  
  if (uploadFile) {
    uploadFile.write(data, len);
  }
  
  if (final) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[Update] Upload Fertig: %s (%u Bytes)\n", filename.c_str(), index + len);
    }
    request->send(200, "text/plain", "Datei erfolgreich aktualisiert!");
  }
}