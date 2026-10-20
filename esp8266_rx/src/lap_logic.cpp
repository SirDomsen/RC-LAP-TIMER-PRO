#include "lap_logic.h"
#include "ir_decoder.h"
#include "storage.h"
#include "web_server.h"
#include <sys/time.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

uint32_t lastCrossTime[MAX_CARS + 1]   = {0};
uint32_t lastLapDuration[MAX_CARS + 1] = {0};
uint32_t bestLapDuration[MAX_CARS + 1] = {0};
uint16_t lapCounter[MAX_CARS + 1]      = {0};
uint16_t targetRounds                  = 0;
uint16_t minLapTimeMs                  = DEFAULT_MIN_LAP_MS;

bool useSz1 = true;
bool useSz2 = true;
bool useSz3 = true;
bool checkpointMode = false;
bool dragraceMode = false;
bool sectorAudioFeedback = true;

bool awaitingBaseCross[MAX_CARS + 1] = {false};
bool finishedFinalLap[MAX_CARS + 1]  = {false};
bool dragRunActive[MAX_CARS + 1]     = {false};
bool dragTrackOccupied               = false;
uint8_t activeDragCarId              = 0;

bool inPitOrPause[MAX_CARS + 1]      = {false};
bool isOutLap[MAX_CARS + 1]          = {false};

// Ringpuffer für die letzten 3 gültigen Rundenzeiten pro Fahrzeug
static uint32_t recentValidLaps[MAX_CARS + 1][3] = {{0}};
static uint8_t  recentValidLapCount[MAX_CARS + 1] = {0};
static uint8_t  recentValidLapIdx[MAX_CARS + 1]   = {0};

// Bitmaske zum Schutz vor doppelter Sektorerfassung in derselben Runde (Bit 1=SZ1, Bit 2=SZ2, Bit 3=SZ3)
static uint8_t sectorPassedInLap[MAX_CARS + 1] = {0};

// Sektorzeiten
uint32_t sz1TimeMs[MAX_CARS + 1] = {0};
uint32_t sz2TimeMs[MAX_CARS + 1] = {0};
uint32_t sz3TimeMs[MAX_CARS + 1] = {0};
uint32_t sz4TimeMs[MAX_CARS + 1] = {0};

// Split-Zeiten
uint32_t split1TimeMs[MAX_CARS + 1] = {0};
uint32_t split2TimeMs[MAX_CARS + 1] = {0};
uint32_t split3TimeMs[MAX_CARS + 1] = {0};

uint32_t bestSz1TimeMs[MAX_CARS + 1] = {0};
uint32_t bestSz2TimeMs[MAX_CARS + 1] = {0};
uint32_t bestSz3TimeMs[MAX_CARS + 1] = {0};
uint32_t bestSz4TimeMs[MAX_CARS + 1] = {0};

uint32_t overallBestSz1Ms = 0;
uint32_t overallBestSz2Ms = 0;
uint32_t overallBestSz3Ms = 0;
uint32_t overallBestSz4Ms = 0;

uint32_t lastSectorTimeMs[MAX_CARS + 1] = {0};

static uint8_t currentExpectedSector[MAX_CARS + 1] = {0};

RaceMode currentMode = MODE_PRACTICE;
uint32_t modeEndTimeMs = 0;
uint16_t durationMinutes = 0;

uint8_t recentIDs[5] = {0, 0, 0, 0, 0};
bool requireTwoFrames = true;

uint32_t lastSeenMs[MAX_CARS + 1] = {0};
bool readyForNextLap[MAX_CARS + 1] = {false};

uint64_t baseUnixTimestampSec = 0;
uint32_t baseMillis = 0;

// Tagesbestzeit-Variablen (Track Record)
uint32_t dailyBestLapMs = 0;
uint8_t dailyBestCarId = 0;
String dailyBestDriver = "";
String dailyBestCar = "";

const uint16_t CLEARANCE_TIMEOUT_MS = 1000; 

static uint32_t readyLedOffTime = 0;
static bool readyLedBlinking = false;

static uint8_t  lastPendingId = 0;
static uint32_t firstDetectTimeMicros = 0;
static uint8_t  detectionCount = 0;

#define VALIDATION_WINDOW_MICROS 85000 

void loadDailyBest() {
  if (LittleFS.exists("/daily_best.json")) {
    File f = LittleFS.open("/daily_best.json", "r");
    if (f) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        dailyBestLapMs = doc["time"] | 0;
        dailyBestCarId = doc["id"] | 0;
        dailyBestDriver = doc["driver"] | "";
        dailyBestCar = doc["car"] | "";
      }
      f.close();
    }
  }
}

void saveDailyBest() {
  JsonDocument doc;
  doc["time"] = dailyBestLapMs;
  doc["id"] = dailyBestCarId;
  doc["driver"] = dailyBestDriver;
  doc["car"] = dailyBestCar;

  File f = LittleFS.open("/daily_best.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

uint32_t getAverageLapTimeMs(uint8_t carId) {
  if (carId < 1 || carId > MAX_CARS || recentValidLapCount[carId] == 0) return 0;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < recentValidLapCount[carId]; i++) {
    sum += recentValidLaps[carId][i];
  }
  return sum / recentValidLapCount[carId];
}

static void addValidLapToAverage(uint8_t carId, uint32_t lapTimeMs) {
  if (carId < 1 || carId > MAX_CARS || lapTimeMs == 0) return;
  recentValidLaps[carId][recentValidLapIdx[carId]] = lapTimeMs;
  recentValidLapIdx[carId] = (recentValidLapIdx[carId] + 1) % 3;
  if (recentValidLapCount[carId] < 3) {
    recentValidLapCount[carId]++;
  }
}

void broadcastSectorEvent(uint8_t carId, uint8_t sectorNum, uint32_t sectorTimeMs, uint32_t splitTimeMs, bool isPB, bool isOverallBest, int32_t deltaMs) {
  char buffer[192];
  snprintf(buffer, sizeof(buffer), 
    "{\"type\":\"sector\",\"carId\":%d,\"driver\":\"%s\",\"sector\":%d,\"time\":%lu,\"split\":%lu,\"isPB\":%s,\"isOverallBest\":%s,\"delta\":%ld}",
    carId, driverNames[carId].c_str(), sectorNum, sectorTimeMs, splitTimeMs,
    isPB ? "true" : "false", isOverallBest ? "true" : "false", (long)deltaMs);
  broadcastWebSocket(buffer);
}

void broadcastLapEvent(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, bool isPB, bool isOverallBest) {
  char buffer[160];
  snprintf(buffer, sizeof(buffer), 
    "{\"type\":\"lap\",\"carId\":%d,\"driver\":\"%s\",\"lap\":%d,\"lapTime\":%lu,\"isPB\":%s,\"isOverallBest\":%s}",
    carId, driverNames[carId].c_str(), lapNum, lapTimeMs, 
    isPB ? "true" : "false", isOverallBest ? "true" : "false");
  broadcastWebSocket(buffer);
}

void initLapLogic() {
  pinMode(LED_READY, OUTPUT);
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(LED_ONBOARD, OUTPUT);

  digitalWrite(LED_READY, LOW); 
  digitalWrite(LED_SIGNAL, LOW);
  digitalWrite(LED_ONBOARD, HIGH);
  
  currentMode = MODE_PRACTICE;
  modeEndTimeMs = 0;
  targetRounds = 0;
  
  loadDailyBest();
  resetAllLaps(false);
}

void syncClockOnce(uint64_t clientTsSec) {
  if (baseUnixTimestampSec == 0 || clientTsSec > baseUnixTimestampSec + 3600) {
    baseUnixTimestampSec = clientTsSec;
    baseMillis = millis();
    
    timeval tv = { (time_t)clientTsSec, 0 };
    settimeofday(&tv, nullptr);
  }
}

uint64_t getCurrentTimestampMs() {
  if (baseUnixTimestampSec == 0) {
    return (uint64_t)millis();
  }
  uint32_t elapsedMs = millis() - baseMillis;
  return (baseUnixTimestampSec * 1000ULL) + elapsedMs;
}

void addRecentID(uint8_t newId) {
  for (int i = 4; i > 0; i--) {
    recentIDs[i] = recentIDs[i - 1];
  }
  recentIDs[0] = newId;
}

void resetAllLaps(bool isHardReset) {
  for (uint8_t i = 0; i <= MAX_CARS; i++) {
    lastCrossTime[i] = 0;
    lastLapDuration[i] = 0;
    bestLapDuration[i] = 0;
    lapCounter[i] = 0;
    lastSeenMs[i] = 0;
    readyForNextLap[i] = true;
    awaitingBaseCross[i] = true;
    finishedFinalLap[i] = false;
    dragRunActive[i] = false;
    inPitOrPause[i] = false;
    isOutLap[i] = false;
    sectorPassedInLap[i] = 0;
    currentExpectedSector[i] = 1;
    sz1TimeMs[i] = 0;
    sz2TimeMs[i] = 0;
    sz3TimeMs[i] = 0;
    sz4TimeMs[i] = 0;
    split1TimeMs[i] = 0;
    split2TimeMs[i] = 0;
    split3TimeMs[i] = 0;
    bestSz1TimeMs[i] = 0;
    bestSz2TimeMs[i] = 0;
    bestSz3TimeMs[i] = 0;
    bestSz4TimeMs[i] = 0;
    lastSectorTimeMs[i] = 0;
    recentValidLapCount[i] = 0;
    recentValidLapIdx[i] = 0;
    for (uint8_t j = 0; j < 3; j++) recentValidLaps[i][j] = 0;
  }
  dragTrackOccupied = false;
  activeDragCarId = 0;

  overallBestSz1Ms = 0;
  overallBestSz2Ms = 0;
  overallBestSz3Ms = 0;
  overallBestSz4Ms = 0;
  
  if (isHardReset) {
    dailyBestLapMs = 0;
    dailyBestCarId = 0;
    dailyBestDriver = "";
    dailyBestCar = "";
    if (LittleFS.exists("/daily_best.json")) {
      LittleFS.remove("/daily_best.json");
    }
  }

  for (int i = 0; i < 5; i++) recentIDs[i] = 0;
  lastPendingId = 0;
  detectionCount = 0;
  readyLedBlinking = false;
  broadcastWebSocket("{\"type\":\"reset\"}");
}

uint8_t getCurrentPosition(uint8_t carId) {
  uint8_t pos = 1;
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (i == carId) continue;
    if (currentMode == MODE_DRAGRACE || currentMode == MODE_QUALIFYING) {
      if (bestLapDuration[i] > 0 && (bestLapDuration[carId] == 0 || bestLapDuration[i] < bestLapDuration[carId])) {
        pos++;
      }
    } else {
      if (lapCounter[i] > lapCounter[carId]) {
        pos++;
      } else if (lapCounter[i] == lapCounter[carId] && lastCrossTime[i] < lastCrossTime[carId] && lapCounter[i] > 0) {
        pos++;
      }
    }
  }
  return pos;
}

uint8_t getActiveTargetSector() {
  if (useSz3) return 3;
  if (useSz2) return 2;
  if (useSz1) return 1;
  return 0;
}

bool areAllActiveCarsFinished() {
  if (currentMode != MODE_QUALIFYING && currentMode != MODE_RACE_TIME) return false;
  if (modeEndTimeMs == 0 || millis() <= modeEndTimeMs) return false;

  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (lapCounter[i] > 0 && !finishedFinalLap[i] && !inPitOrPause[i]) {
      uint32_t lapStartMs = lastCrossTime[i] / 1000UL;
      if (lapStartMs <= modeEndTimeMs) {
        if (millis() - lastSeenMs[i] < 60000UL) {
          return false;
        }
      }
    }
  }
  return true;
}

void processSectorPacket(uint8_t stationId, uint8_t carId, uint32_t timestampMs) {
  if (carId < 1 || carId > MAX_CARS) return;
  uint32_t now = millis();
  lastSeenMs[carId] = now;
  addRecentID(carId);

  if (currentMode == MODE_DRAGRACE) {
    if (!dragRunActive[carId] || lastCrossTime[carId] == 0) return;
  }

  if ((currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) && finishedFinalLap[carId]) {
    return;
  }

  if (timestampMs == 0 || (timestampMs > now && timestampMs - now > 5000) || (now > timestampMs && now - timestampMs > 300000)) {
    timestampMs = now;
  }

  // Wenn das Auto in Box oder Pause war, hebt das Überfahren IRGENDEINES Sektors
  // den Status auf und setzt das Fahrzeug auf "Outlap"!
  if (currentMode != MODE_DRAGRACE && inPitOrPause[carId]) {
    inPitOrPause[carId] = false;
    isOutLap[carId] = true;
    readyForNextLap[carId] = true;
    char outlapJson[96];
    snprintf(outlapJson, sizeof(outlapJson), "{\"type\":\"outlap\",\"carId\":%d,\"station\":%d}", carId, stationId);
    broadcastWebSocket(outlapJson);
  }

  if (currentMode != MODE_DRAGRACE && (lastCrossTime[carId] == 0 || lapCounter[carId] == 0)) {
    readyForNextLap[carId] = true;
    if (awaitingBaseCross[carId]) {
      awaitingBaseCross[carId] = false;
      isOutLap[carId] = true;
      char trackJson[64];
      snprintf(trackJson, sizeof(trackJson), "{\"type\":\"track_seen\",\"carId\":%d,\"station\":%d}", carId, stationId);
      broadcastWebSocket(trackJson);
    }
    return;
  }

  uint32_t lapStartMs = lastCrossTime[carId] / 1000UL;
  if (timestampMs < lapStartMs) {
    if (lapStartMs - timestampMs < 150) timestampMs = lapStartMs + 1;
    else return;
  }

  uint32_t elapsedMs = timestampMs - lapStartMs;
  if (elapsedMs < 100 || elapsedMs > 600000) return;

  // Prüfen, ob die aktuelle Zeit bereits über 150% des Durchschnitts liegt -> ebenfalls Outlap
  uint32_t avgLap = getAverageLapTimeMs(carId);
  if (currentMode != MODE_DRAGRACE && avgLap > 0 && elapsedMs > (avgLap + (avgLap >> 1))) {
    isOutLap[carId] = true;
  }
  bool isOutSector = isOutLap[carId];

  // --- DRAGRACE LOGIK ---
  if (currentMode == MODE_DRAGRACE) {
    uint8_t targetSector = getActiveTargetSector();
    if (targetSector == 0) return;

    if (stationId == targetSector) {
      if (sectorPassedInLap[carId] & (1 << targetSector)) return;
      sectorPassedInLap[carId] |= (1 << targetSector);

      dragRunActive[carId] = false;
      dragTrackOccupied = false;
      activeDragCarId = 0;
      readyForNextLap[carId] = false;

      if (targetSector == 3) {
        split3TimeMs[carId] = elapsedMs;
        uint32_t prevSplit = (useSz2 && split2TimeMs[carId] > 0) ? split2TimeMs[carId] : ((useSz1 && split1TimeMs[carId] > 0) ? split1TimeMs[carId] : 0);
        sz3TimeMs[carId] = (elapsedMs > prevSplit) ? (elapsedMs - prevSplit) : elapsedMs;
      } else if (targetSector == 2) {
        split2TimeMs[carId] = elapsedMs;
        uint32_t prevSplit = (useSz1 && split1TimeMs[carId] > 0) ? split1TimeMs[carId] : 0;
        sz2TimeMs[carId] = (elapsedMs > prevSplit) ? (elapsedMs - prevSplit) : elapsedMs;
      } else if (targetSector == 1) {
        split1TimeMs[carId] = elapsedMs;
        sz1TimeMs[carId] = elapsedMs;
      }

      uint32_t finishSecTime = (targetSector == 3) ? sz3TimeMs[carId] : ((targetSector == 2) ? sz2TimeMs[carId] : sz1TimeMs[carId]);
      bool isSecPB = false;
      bool isSecOverall = false;
      int32_t secDeltaMs = 0;

      if (targetSector == 3) {
        if (bestSz3TimeMs[carId] == 0 || finishSecTime < bestSz3TimeMs[carId]) {
          if (bestSz3TimeMs[carId] > 0) secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz3TimeMs[carId];
          bestSz3TimeMs[carId] = finishSecTime;
          isSecPB = true;
        } else {
          secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz3TimeMs[carId];
        }
        if (overallBestSz3Ms == 0 || finishSecTime < overallBestSz3Ms) {
          overallBestSz3Ms = finishSecTime;
          isSecOverall = true;
        }
      } else if (targetSector == 2) {
        if (bestSz2TimeMs[carId] == 0 || finishSecTime < bestSz2TimeMs[carId]) {
          if (bestSz2TimeMs[carId] > 0) secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz2TimeMs[carId];
          bestSz2TimeMs[carId] = finishSecTime;
          isSecPB = true;
        } else {
          secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz2TimeMs[carId];
        }
        if (overallBestSz2Ms == 0 || finishSecTime < overallBestSz2Ms) {
          overallBestSz2Ms = finishSecTime;
          isSecOverall = true;
        }
      } else if (targetSector == 1) {
        if (bestSz1TimeMs[carId] == 0 || finishSecTime < bestSz1TimeMs[carId]) {
          if (bestSz1TimeMs[carId] > 0) secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz1TimeMs[carId];
          bestSz1TimeMs[carId] = finishSecTime;
          isSecPB = true;
        } else {
          secDeltaMs = (int32_t)finishSecTime - (int32_t)bestSz1TimeMs[carId];
        }
        if (overallBestSz1Ms == 0 || finishSecTime < overallBestSz1Ms) {
          overallBestSz1Ms = finishSecTime;
          isSecOverall = true;
        }
      }

      lastLapDuration[carId] = elapsedMs;
      bool isPB = false;
      bool isOverall = false;

      if (bestLapDuration[carId] == 0 || elapsedMs < bestLapDuration[carId]) {
        bestLapDuration[carId] = elapsedMs;
        isPB = true;
      }

      uint32_t currentOverall = 0;
      for (uint8_t c = 1; c <= MAX_CARS; c++) {
        if (c != carId && bestLapDuration[c] > 0 && (currentOverall == 0 || bestLapDuration[c] < currentOverall)) {
          currentOverall = bestLapDuration[c];
        }
      }
      if (currentOverall == 0 || elapsedMs < currentOverall) {
        isOverall = true;
      }

      uint16_t completedRun = lapCounter[carId];
      lapCounter[carId]++;

      uint64_t currentTsMs = getCurrentTimestampMs();
      uint8_t pos = getCurrentPosition(carId);

      queueLapForStorage(carId, completedRun, elapsedMs, sz1TimeMs[carId], sz2TimeMs[carId], sz3TimeMs[carId], 0, pos, (uint8_t)currentMode, currentTsMs);

      digitalWrite(LED_READY, LOW);
      readyLedBlinking = true;
      readyLedOffTime = millis() + 200;

      broadcastSectorEvent(carId, targetSector, finishSecTime, elapsedMs, isSecPB, isSecOverall, secDeltaMs);

      char finishJson[192];
      snprintf(finishJson, sizeof(finishJson),
        "{\"type\":\"drag_finish\",\"carId\":%d,\"driver\":\"%s\",\"time\":%lu,\"isPB\":%s,\"isOverallBest\":%s}",
        carId, driverNames[carId].c_str(), elapsedMs, isPB ? "true" : "false", isOverall ? "true" : "false");
      broadcastWebSocket(finishJson);

      broadcastLapEvent(carId, completedRun, elapsedMs, isPB, isOverall);
      return;
    }

    if (stationId < targetSector) {
      if (stationId == 1 && useSz1) {
        if (sectorPassedInLap[carId] & (1 << 1)) return;
        sectorPassedInLap[carId] |= (1 << 1);

        uint32_t s1 = elapsedMs;
        split1TimeMs[carId] = s1;
        sz1TimeMs[carId] = s1;
        lastSectorTimeMs[carId] = timestampMs;

        bool isPB = false;
        bool isOverall = false;
        int32_t deltaMs = 0;
        
        if (bestSz1TimeMs[carId] > 0) {
          deltaMs = (int32_t)s1 - (int32_t)bestSz1TimeMs[carId];
          if (s1 < bestSz1TimeMs[carId]) { bestSz1TimeMs[carId] = s1; isPB = true; }
        } else {
          bestSz1TimeMs[carId] = s1;
          isPB = true;
        }

        if (overallBestSz1Ms == 0 || s1 < overallBestSz1Ms) {
          overallBestSz1Ms = s1;
          isOverall = true;
        }
        broadcastSectorEvent(carId, 1, s1, s1, isPB, isOverall, deltaMs);
      } 
      else if (stationId == 2 && useSz2) {
        if (sectorPassedInLap[carId] & (1 << 2)) return;
        sectorPassedInLap[carId] |= (1 << 2);

        uint32_t split2 = elapsedMs;
        uint32_t prevSplit = (useSz1 && split1TimeMs[carId] > 0) ? split1TimeMs[carId] : 0;
        uint32_t s2 = (split2 > prevSplit) ? (split2 - prevSplit) : split2;

        split2TimeMs[carId] = split2;
        sz2TimeMs[carId] = s2;
        lastSectorTimeMs[carId] = timestampMs;

        bool isPB = false;
        bool isOverall = false;
        int32_t deltaMs = 0;

        if (bestSz2TimeMs[carId] > 0) {
          deltaMs = (int32_t)s2 - (int32_t)bestSz2TimeMs[carId];
          if (s2 < bestSz2TimeMs[carId]) { bestSz2TimeMs[carId] = s2; isPB = true; }
        } else {
          bestSz2TimeMs[carId] = s2;
          isPB = true;
        }

        if (overallBestSz2Ms == 0 || s2 < overallBestSz2Ms) {
          overallBestSz2Ms = s2;
          isOverall = true;
        }
        broadcastSectorEvent(carId, 2, s2, split2, isPB, isOverall, deltaMs);
      }
    }
    return;
  }

  // --- NORMALE RUNDENRENNEN-LOGIK ---
  if (stationId == 1 && useSz1) {
    if (checkpointMode && currentExpectedSector[carId] != 1) return;
    if (sectorPassedInLap[carId] & (1 << 1)) return;
    sectorPassedInLap[carId] |= (1 << 1);

    uint32_t split1 = elapsedMs;
    split1TimeMs[carId] = split1;
    sz1TimeMs[carId] = split1;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 2;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (!isOutSector) {
      if (bestSz1TimeMs[carId] > 0) {
        deltaMs = (int32_t)split1 - (int32_t)bestSz1TimeMs[carId];
        if (split1 < bestSz1TimeMs[carId]) { bestSz1TimeMs[carId] = split1; isPB = true; }
      } else {
        bestSz1TimeMs[carId] = split1;
        isPB = true;
      }

      if (overallBestSz1Ms == 0 || split1 < overallBestSz1Ms) {
        overallBestSz1Ms = split1;
        isOverall = true;
      }
    }
    broadcastSectorEvent(carId, 1, split1, split1, isPB, isOverall, deltaMs);
  } 
  else if (stationId == 2 && useSz2) {
    if (checkpointMode) {
      uint8_t expected = useSz1 ? 2 : 1;
      if (currentExpectedSector[carId] != expected) return;
    }
    if (sectorPassedInLap[carId] & (1 << 2)) return;
    sectorPassedInLap[carId] |= (1 << 2);

    uint32_t split2 = elapsedMs;
    uint32_t prevSplit = (useSz1 && (sectorPassedInLap[carId] & (1 << 1)) && split1TimeMs[carId] > 0) ? split1TimeMs[carId] : 0;
    uint32_t s2 = (split2 > prevSplit) ? (split2 - prevSplit) : split2;

    split2TimeMs[carId] = split2;
    sz2TimeMs[carId] = s2;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 3;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (!isOutSector) {
      if (bestSz2TimeMs[carId] > 0) {
        deltaMs = (int32_t)s2 - (int32_t)bestSz2TimeMs[carId];
        if (s2 < bestSz2TimeMs[carId]) { bestSz2TimeMs[carId] = s2; isPB = true; }
      } else {
        bestSz2TimeMs[carId] = s2;
        isPB = true;
      }

      if (overallBestSz2Ms == 0 || s2 < overallBestSz2Ms) {
        overallBestSz2Ms = s2;
        isOverall = true;
      }
    }
    broadcastSectorEvent(carId, 2, s2, split2, isPB, isOverall, deltaMs);
  } 
  else if (stationId == 3 && useSz3) {
    if (checkpointMode) {
      uint8_t expected = useSz2 ? 3 : (useSz1 ? 2 : 1);
      if (currentExpectedSector[carId] != expected) return;
    }
    if (sectorPassedInLap[carId] & (1 << 3)) return;
    sectorPassedInLap[carId] |= (1 << 3);

    uint32_t split3 = elapsedMs;
    uint32_t prevSplit = 0;
    if (useSz2 && (sectorPassedInLap[carId] & (1 << 2)) && split2TimeMs[carId] > 0) {
      prevSplit = split2TimeMs[carId];
    } else if (useSz1 && (sectorPassedInLap[carId] & (1 << 1)) && split1TimeMs[carId] > 0) {
      prevSplit = split1TimeMs[carId];
    }
    uint32_t s3 = (split3 > prevSplit) ? (split3 - prevSplit) : split3;

    split3TimeMs[carId] = split3;
    sz3TimeMs[carId] = s3;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 4;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (!isOutSector) {
      if (bestSz3TimeMs[carId] > 0) {
        deltaMs = (int32_t)s3 - (int32_t)bestSz3TimeMs[carId];
        if (s3 < bestSz3TimeMs[carId]) { bestSz3TimeMs[carId] = s3; isPB = true; }
      } else {
        bestSz3TimeMs[carId] = s3;
        isPB = true;
      }

      if (overallBestSz3Ms == 0 || s3 < overallBestSz3Ms) {
        overallBestSz3Ms = s3;
        isOverall = true;
      }
    }
    broadcastSectorEvent(carId, 3, s3, split3, isPB, isOverall, deltaMs);
  }
}

void processValidLap(uint8_t carId, uint64_t exactSignalTimeMicros) {
  uint32_t exactMicros = (uint32_t)exactSignalTimeMicros;
  uint32_t nowMs = exactMicros / 1000UL;

  if (currentMode == MODE_DRAGRACE) {
    // Streckensperre: Start im Dragrace nur erlaubt, wenn kein Fahrzeug auf der Strecke ist!
    if (dragTrackOccupied) return;
    if (dragRunActive[carId]) return;
    if (lastCrossTime[carId] > 0 && (exactMicros - lastCrossTime[carId] < ((uint32_t)minLapTimeMs * 1000UL))) return;

    dragTrackOccupied = true;
    activeDragCarId = carId;
    dragRunActive[carId] = true;
    readyForNextLap[carId] = false;
    sectorPassedInLap[carId] = 0;

    lapCounter[carId] = (lapCounter[carId] == 0) ? 1 : lapCounter[carId];
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    currentExpectedSector[carId] = 1;
    sz1TimeMs[carId] = 0; sz2TimeMs[carId] = 0; sz3TimeMs[carId] = 0; sz4TimeMs[carId] = 0;
    split1TimeMs[carId] = 0; split2TimeMs[carId] = 0; split3TimeMs[carId] = 0;

    digitalWrite(LED_READY, LOW);
    readyLedBlinking = true;
    readyLedOffTime = millis() + 200;

    char startJson[128];
    snprintf(startJson, sizeof(startJson), "{\"type\":\"drag_start\",\"carId\":%d,\"driver\":\"%s\"}", carId, driverNames[carId].c_str());
    broadcastWebSocket(startJson);
    return;
  }

  if (lapCounter[carId] == 0) {
    if ((currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) && modeEndTimeMs > 0 && millis() > modeEndTimeMs) return;

    awaitingBaseCross[carId] = false;
    inPitOrPause[carId] = false;
    isOutLap[carId] = false;
    sectorPassedInLap[carId] = 0;
    lapCounter[carId] = 1;
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    readyForNextLap[carId] = false;
    currentExpectedSector[carId] = 1;
    sz1TimeMs[carId] = 0; sz2TimeMs[carId] = 0; sz3TimeMs[carId] = 0; sz4TimeMs[carId] = 0;
    split1TimeMs[carId] = 0; split2TimeMs[carId] = 0; split3TimeMs[carId] = 0;

    digitalWrite(LED_READY, LOW);
    readyLedBlinking = true;
    readyLedOffTime = millis() + 200;

    char baseJson[128];
    snprintf(baseJson, sizeof(baseJson), "{\"type\":\"base_cross\",\"carId\":%d,\"driver\":\"%s\"}", carId, driverNames[carId].c_str());
    broadcastWebSocket(baseJson);
    return;
  }

  if (currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) {
    if (modeEndTimeMs > 0 && millis() > modeEndTimeMs) {
      if (finishedFinalLap[carId]) return;
      if (lastCrossTime[carId] == 0) return;
      uint32_t lapStartMs = lastCrossTime[carId] / 1000UL;
      if (lapStartMs > modeEndTimeMs) return;
    }
  }

  if (currentMode == MODE_RACE_LAPS && targetRounds > 0 && lapCounter[carId] > targetRounds) return;

  if (checkpointMode && currentMode != MODE_DRAGRACE && lapCounter[carId] > 0) {
    bool passedAll = true;
    if (useSz3 && currentExpectedSector[carId] < 4) passedAll = false;
    else if (useSz2 && !useSz3 && currentExpectedSector[carId] < 3) passedAll = false;
    else if (useSz1 && !useSz2 && !useSz3 && currentExpectedSector[carId] < 2) passedAll = false;

    if (!passedAll) {
      currentExpectedSector[carId] = 1;
      sectorPassedInLap[carId] = 0;
      return;
    }
    currentExpectedSector[carId] = 1;
  }

  bool lapCounted = false;

  if ((exactMicros - lastCrossTime[carId] > ((uint32_t)minLapTimeMs * 1000UL)) && readyForNextLap[carId]) {
    uint32_t currentLapTimeMs = (exactMicros - lastCrossTime[carId]) / 1000UL;
    
    // --- DYNAMISCHE OUT-LAP / BOXEN-PRÜFUNG (50% über Durchschnitt der letzten 3 Runden) ---
    uint32_t avgLap = getAverageLapTimeMs(carId);
    bool currentLapIsOutLap = isOutLap[carId] || inPitOrPause[carId];
    if (avgLap > 0 && currentLapTimeMs > (avgLap + (avgLap >> 1))) {
      currentLapIsOutLap = true;
    }

    uint8_t finalSectorNum = useSz3 ? 4 : (useSz2 ? 3 : (useSz1 ? 2 : 0));

    if (finalSectorNum > 0 && lastSectorTimeMs[carId] > (lastCrossTime[carId] / 1000UL)) {
      uint32_t lastSplit = 0;
      if (finalSectorNum == 4 && (sectorPassedInLap[carId] & (1 << 3))) lastSplit = split3TimeMs[carId];
      else if (finalSectorNum == 3 && (sectorPassedInLap[carId] & (1 << 2))) lastSplit = split2TimeMs[carId];
      else if (finalSectorNum == 2 && (sectorPassedInLap[carId] & (1 << 1))) lastSplit = split1TimeMs[carId];

      if (lastSplit > 0 && currentLapTimeMs > lastSplit) {
        uint32_t sFinal = currentLapTimeMs - lastSplit;

        if (sFinal >= 100 && sFinal < 600000) {
          bool isPB = false;
          bool isOverall = false;
          int32_t deltaMs = 0;

          if (!currentLapIsOutLap) {
            if (finalSectorNum == 4) {
              sz4TimeMs[carId] = sFinal;
              if (bestSz4TimeMs[carId] > 0) {
                deltaMs = (int32_t)sFinal - (int32_t)bestSz4TimeMs[carId];
                if (sFinal < bestSz4TimeMs[carId]) { bestSz4TimeMs[carId] = sFinal; isPB = true; }
              } else { bestSz4TimeMs[carId] = sFinal; isPB = true; }
              if (overallBestSz4Ms == 0 || sFinal < overallBestSz4Ms) { overallBestSz4Ms = sFinal; isOverall = true; }
            } 
            else if (finalSectorNum == 3) {
              sz3TimeMs[carId] = sFinal;
              split3TimeMs[carId] = currentLapTimeMs;
              if (bestSz3TimeMs[carId] > 0) {
                deltaMs = (int32_t)sFinal - (int32_t)bestSz3TimeMs[carId];
                if (sFinal < bestSz3TimeMs[carId]) { bestSz3TimeMs[carId] = sFinal; isPB = true; }
              } else { bestSz3TimeMs[carId] = sFinal; isPB = true; }
              if (overallBestSz3Ms == 0 || sFinal < overallBestSz3Ms) { overallBestSz3Ms = sFinal; isOverall = true; }
            } 
            else if (finalSectorNum == 2) {
              sz2TimeMs[carId] = sFinal;
              split2TimeMs[carId] = currentLapTimeMs;
              if (bestSz2TimeMs[carId] > 0) {
                deltaMs = (int32_t)sFinal - (int32_t)bestSz2TimeMs[carId];
                if (sFinal < bestSz2TimeMs[carId]) { bestSz2TimeMs[carId] = sFinal; isPB = true; }
              } else { bestSz2TimeMs[carId] = sFinal; isPB = true; }
              if (overallBestSz2Ms == 0 || sFinal < overallBestSz2Ms) { overallBestSz2Ms = sFinal; isOverall = true; }
            }
          } else {
            if (finalSectorNum == 4) sz4TimeMs[carId] = sFinal;
            else if (finalSectorNum == 3) { sz3TimeMs[carId] = sFinal; split3TimeMs[carId] = currentLapTimeMs; }
            else if (finalSectorNum == 2) { sz2TimeMs[carId] = sFinal; split2TimeMs[carId] = currentLapTimeMs; }
          }
          broadcastSectorEvent(carId, finalSectorNum, sFinal, currentLapTimeMs, isPB, isOverall, deltaMs);
        }
      }
    }

    lastLapDuration[carId] = currentLapTimeMs;
    bool isLapPB = false;
    bool isLapOverall = false;

    // Bestzeiten & Durchschnitt NUR aktualisieren, wenn es keine Outlap / Boxenrunde war!
    if (!currentLapIsOutLap) {
      addValidLapToAverage(carId, currentLapTimeMs);

      if (bestLapDuration[carId] > 0 && currentLapTimeMs < bestLapDuration[carId]) {
        bestLapDuration[carId] = currentLapTimeMs;
        isLapPB = true;
      } else if (bestLapDuration[carId] == 0) {
        bestLapDuration[carId] = currentLapTimeMs;
        isLapPB = true;
      }

      uint32_t currentOverallBestLap = 0;
      for (uint8_t c = 1; c <= MAX_CARS; c++) {
        if (c != carId && bestLapDuration[c] > 0 && (currentOverallBestLap == 0 || bestLapDuration[c] < currentOverallBestLap)) {
          currentOverallBestLap = bestLapDuration[c];
        }
      }
      if (currentOverallBestLap == 0 || currentLapTimeMs < currentOverallBestLap) {
        isLapOverall = true;
      }

      // --- TAGESBESTZEIT CHECK (Nur für rundenbasierte Events!) ---
      if (currentMode != MODE_DRAGRACE && currentLapTimeMs > 100) {
        if (dailyBestLapMs == 0 || currentLapTimeMs < dailyBestLapMs) {
          dailyBestLapMs = currentLapTimeMs;
          dailyBestCarId = carId;
          dailyBestDriver = driverNames[carId];
          dailyBestCar = carNames[carId];
          saveDailyBest(); 
        }
      }
    }
    
    // Gültige Sektorzeiten dieser Runde für die Statistik sichern
    uint32_t savedSz1 = (sectorPassedInLap[carId] & (1 << 1)) ? sz1TimeMs[carId] : 0;
    uint32_t savedSz2 = ((sectorPassedInLap[carId] & (1 << 2)) || finalSectorNum == 2) ? sz2TimeMs[carId] : 0;
    uint32_t savedSz3 = ((sectorPassedInLap[carId] & (1 << 3)) || finalSectorNum == 3) ? sz3TimeMs[carId] : 0;
    uint32_t savedSz4 = (finalSectorNum == 4) ? sz4TimeMs[carId] : 0;

    uint16_t completedLap = lapCounter[carId];
    lapCounter[carId]++;
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    readyForNextLap[carId] = false;
    currentExpectedSector[carId] = 1;
    
    // Nach der Basis-Überfahrt ist das Fahrzeug wieder auf einer regulären fliegenden Runde
    inPitOrPause[carId] = false;
    isOutLap[carId] = false;
    sectorPassedInLap[carId] = 0;
    split1TimeMs[carId] = 0; split2TimeMs[carId] = 0; split3TimeMs[carId] = 0;
    // HINWEIS: sz1TimeMs .. sz4TimeMs bleiben absichtlich stehen, bis der jeweilige Sektor in der neuen Runde überfahren wird!
    
    lapCounted = true;

    if ((currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) && modeEndTimeMs > 0 && millis() > modeEndTimeMs) {
      finishedFinalLap[carId] = true;
    }

    uint64_t currentTsMs = getCurrentTimestampMs();
    uint8_t pos = getCurrentPosition(carId);
    
    queueLapForStorage(carId, completedLap, currentLapTimeMs, savedSz1, savedSz2, savedSz3, savedSz4, pos, (uint8_t)currentMode, currentTsMs);

    broadcastLapEvent(carId, completedLap, currentLapTimeMs, isLapPB, isLapOverall);
  }

  if (lapCounted) {
    digitalWrite(LED_READY, LOW);
    readyLedBlinking = true;
    readyLedOffTime = millis() + 200;
  }
}

void processLapLogic() {
  if (!newCodeAvailable) return;

  uint16_t code = receivedCode & 0xFFF;
  uint32_t exactSignalTime = irTriggerTimeMicros;
  newCodeAvailable = false;

  uint8_t carId    = (code >> 6) & 0x3F;
  uint8_t carIdInv = code & 0x3F;

  bool isValidSignal = ((carId ^ carIdInv) == 0x3F);

  if (isValidSignal && carId >= 1 && carId <= MAX_CARS) {
    lastSeenMs[carId] = millis();
    bool confirmPassed = false;

    if (requireTwoFrames) {
      if (carId == lastPendingId && (exactSignalTime - firstDetectTimeMicros < VALIDATION_WINDOW_MICROS)) {
        detectionCount++;
      } else {
        lastPendingId = carId;
        firstDetectTimeMicros = exactSignalTime;
        detectionCount = 1;
      }

      if (detectionCount >= 2) {
        detectionCount = 0;
        confirmPassed = true;
      }
    } else {
      confirmPassed = true;
    }

    if (confirmPassed) {
      addRecentID(carId);
      processValidLap(carId, exactSignalTime);
    }
  }
}

void updateLapLogicTasks() {
  uint32_t now = millis();
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (!readyForNextLap[i] && (now - lastSeenMs[i] > CLEARANCE_TIMEOUT_MS)) {
      readyForNextLap[i] = true; 
    }

    // Prüfen, ob Durchschnittszeit der letzten 3 Runden um 50% überschritten wurde -> Box / Pause
    if (currentMode != MODE_DRAGRACE && lapCounter[i] > 0 && !finishedFinalLap[i] && !inPitOrPause[i] && !isOutLap[i]) {
      uint32_t avgMs = getAverageLapTimeMs(i);
      if (avgMs > 0 && lastCrossTime[i] > 0) {
        uint32_t lapStartMs = lastCrossTime[i] / 1000UL;
        if (now > lapStartMs) {
          uint32_t currentElapsedMs = now - lapStartMs;
          uint32_t thresholdMs = avgMs + (avgMs >> 1); // 1.5x Durchschnitt (+50%)
          if (currentElapsedMs > thresholdMs) {
            inPitOrPause[i] = true;
          }
        }
      }
    }
  }

  if (currentMode == MODE_DRAGRACE) {
    for (uint8_t i = 1; i <= MAX_CARS; i++) {
      if (dragRunActive[i] && (now - lastSectorTimeMs[i] > 45000UL)) {
        dragRunActive[i] = false;
        readyForNextLap[i] = true;
        if (activeDragCarId == i) {
          dragTrackOccupied = false;
          activeDragCarId = 0;
        }
      }
    }
  }
  
  // --- FREIES TRAINING AUTO-SAVE ---
  static uint32_t lastSessionSave = 0;
  if (currentMode == MODE_PRACTICE && (now - lastSessionSave > 60000)) {
    lastSessionSave = now;
    broadcastWebSocket("{\"type\":\"request_session_save\"}");
  }

  static bool sessionEndBroadcasted = false;
  if (currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) {
    if (modeEndTimeMs > 0 && now > modeEndTimeMs) {
      if (!sessionEndBroadcasted && areAllActiveCarsFinished()) {
        sessionEndBroadcasted = true;
        broadcastWebSocket("{\"type\":\"session_completed\"}");
      }
    } else {
      sessionEndBroadcasted = false;
    }
  } else {
    sessionEndBroadcasted = false;
  }
}

void updateLEDs() {
  static uint32_t lastYellowFlash = 0;
  if (digitalRead(LED_SIGNAL) == HIGH) {
    if (millis() - lastYellowFlash > 12) {
      digitalWrite(LED_SIGNAL, LOW);
      lastYellowFlash = millis();
    }
  } else {
    lastYellowFlash = millis();
  }

  if (readyLedBlinking && millis() >= readyLedOffTime) {
    digitalWrite(LED_READY, HIGH);
    readyLedBlinking = false;
  }
}