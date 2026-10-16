#include "lap_logic.h"
#include "ir_decoder.h"
#include "storage.h"
#include "web_server.h"

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

const uint16_t CLEARANCE_TIMEOUT_MS = 1000; 

static uint32_t readyLedOffTime = 0;
static bool readyLedBlinking = false;

static uint8_t  lastPendingId = 0;
static uint32_t firstDetectTimeMicros = 0;
static uint8_t  detectionCount = 0;

#define VALIDATION_WINDOW_MICROS 85000 

void broadcastSectorEvent(uint8_t carId, uint8_t sectorNum, uint32_t sectorTimeMs, uint32_t splitTimeMs, bool isPB, bool isOverallBest, int32_t deltaMs) {
  String json;
  json.reserve(192);
  json = "{\"type\":\"sector\",\"carId\":";
  json += carId;
  json += ",\"driver\":\"";
  json += driverNames[carId];
  json += "\",\"sector\":";
  json += sectorNum;
  json += ",\"time\":";
  json += sectorTimeMs;
  json += ",\"split\":";
  json += splitTimeMs;
  json += ",\"isPB\":";
  json += (isPB ? "true" : "false");
  json += ",\"isOverallBest\":";
  json += (isOverallBest ? "true" : "false");
  json += ",\"delta\":";
  json += deltaMs;
  json += "}";
  broadcastWebSocket(json);
}

void broadcastLapEvent(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, bool isPB, bool isOverallBest) {
  String json;
  json.reserve(160);
  json = "{\"type\":\"lap\",\"carId\":";
  json += carId;
  json += ",\"driver\":\"";
  json += driverNames[carId];
  json += "\",\"lap\":";
  json += lapNum;
  json += ",\"lapTime\":";
  json += lapTimeMs;
  json += ",\"isPB\":";
  json += (isPB ? "true" : "false");
  json += ",\"isOverallBest\":";
  json += (isOverallBest ? "true" : "false");
  json += "}";
  broadcastWebSocket(json);
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
  
  resetAllLaps();
}

void syncClockOnce(uint64_t clientTsSec) {
  if (baseUnixTimestampSec == 0 || clientTsSec > baseUnixTimestampSec + 3600) {
    baseUnixTimestampSec = clientTsSec;
    baseMillis = millis();
    Serial.printf("[ClockSync] Basis-Uhrzeit gesetzt auf: %llu\n", clientTsSec);
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

void resetAllLaps() {
  for (uint8_t i = 0; i <= MAX_CARS; i++) {
    lastCrossTime[i] = 0;
    lastLapDuration[i] = 0;
    bestLapDuration[i] = 0;
    lapCounter[i] = 0;
    lastSeenMs[i] = 0;
    readyForNextLap[i] = true;
    awaitingBaseCross[i] = true;
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
  }
  overallBestSz1Ms = 0;
  overallBestSz2Ms = 0;
  overallBestSz3Ms = 0;
  overallBestSz4Ms = 0;
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

void processSectorPacket(uint8_t stationId, uint8_t carId, uint32_t timestampMs) {
  if (carId < 1 || carId > MAX_CARS) return;
  uint32_t now = millis();
  lastSeenMs[carId] = now;
  addRecentID(carId);

  // Plausibilitäts-Check: Falls SZ-Uhr desynchronisiert ist, nutze Empfangszeit
  if (timestampMs == 0 || (timestampMs > now && timestampMs - now > 5000) || (now > timestampMs && now - timestampMs > 300000)) {
    timestampMs = now;
  }

  // Wenn das Auto noch NIE gestartet ist:
  if (lastCrossTime[carId] == 0 || lapCounter[carId] == 0) {
    readyForNextLap[carId] = true;
    if (awaitingBaseCross[carId]) {
      awaitingBaseCross[carId] = false;
      String trackJson = "{\"type\":\"track_seen\",\"carId\":";
      trackJson += carId;
      trackJson += ",\"station\":";
      trackJson += stationId;
      trackJson += "}";
      broadcastWebSocket(trackJson);
    }
    return;
  }

  uint32_t lapStartMs = lastCrossTime[carId] / 1000UL;

  // Toleranz gegen minimale Sync-Verschiebungen:
  if (timestampMs < lapStartMs) {
    if (lapStartMs - timestampMs < 150) {
      timestampMs = lapStartMs + 1;
    } else {
      return;
    }
  }

  uint32_t elapsedMs = timestampMs - lapStartMs;
  if (elapsedMs < 100 || elapsedMs > 600000) return;

  // --- DRAGRACE LOGIK ---
  if (currentMode == MODE_DRAGRACE) {
    uint8_t targetSector = getActiveTargetSector();
    if (targetSector == 0) return;

    // Wenn Zielstation erreicht wurde:
    if (stationId == targetSector) {
      if (targetSector == 3) {
        split3TimeMs[carId] = elapsedMs;
        sz3TimeMs[carId] = (split2TimeMs[carId] > 0) ? (elapsedMs - split2TimeMs[carId]) : ((split1TimeMs[carId] > 0) ? (elapsedMs - split1TimeMs[carId]) : elapsedMs);
      } else if (targetSector == 2) {
        split2TimeMs[carId] = elapsedMs;
        sz2TimeMs[carId] = (split1TimeMs[carId] > 0) ? (elapsedMs - split1TimeMs[carId]) : elapsedMs;
      } else if (targetSector == 1) {
        split1TimeMs[carId] = elapsedMs;
        sz1TimeMs[carId] = elapsedMs;
      }

      lastLapDuration[carId] = elapsedMs;
      bool isPB = false;
      bool isOverall = false;

      if (bestLapDuration[carId] > 0 && elapsedMs < bestLapDuration[carId]) {
        bestLapDuration[carId] = elapsedMs;
        isPB = true;
      } else if (bestLapDuration[carId] == 0) {
        bestLapDuration[carId] = elapsedMs;
      }

      uint32_t currentOverall = 0;
      for (uint8_t c = 1; c <= MAX_CARS; c++) {
        if (c != carId && bestLapDuration[c] > 0 && (currentOverall == 0 || bestLapDuration[c] < currentOverall)) {
          currentOverall = bestLapDuration[c];
        }
      }
      if (currentOverall > 0 && elapsedMs < currentOverall) {
        isOverall = true;
      }

      uint16_t completedRun = lapCounter[carId];
      lapCounter[carId]++;
      readyForNextLap[carId] = false;

      uint64_t currentTsMs = getCurrentTimestampMs();
      uint8_t pos = getCurrentPosition(carId);

      // In Flash-Speicher puffern mit allen Sektoren
      queueLapForStorage(carId, completedRun, elapsedMs, sz1TimeMs[carId], sz2TimeMs[carId], sz3TimeMs[carId], 0, pos, (uint8_t)currentMode, currentTsMs);

      digitalWrite(LED_READY, LOW);
      readyLedBlinking = true;
      readyLedOffTime = millis() + 200;

      uint32_t finishSecTime = (targetSector == 3) ? sz3TimeMs[carId] : ((targetSector == 2) ? sz2TimeMs[carId] : sz1TimeMs[carId]);
      broadcastSectorEvent(carId, targetSector, finishSecTime, elapsedMs, isPB, isOverall, 0);

      String finishJson = "{\"type\":\"drag_finish\",\"carId\":";
      finishJson += carId;
      finishJson += ",\"driver\":\"" + driverNames[carId] + "\"";
      finishJson += ",\"time\":" + String(elapsedMs);
      finishJson += ",\"isPB\":" + String(isPB ? "true" : "false");
      finishJson += ",\"isOverallBest\":" + String(isOverall ? "true" : "false");
      finishJson += "}";
      broadcastWebSocket(finishJson);
      broadcastLapEvent(carId, completedRun, elapsedMs, isPB, isOverall);
      return;
    }

    // Zwischenzeiten vor dem Ziel
    if (stationId < targetSector) {
      if (stationId == 1 && useSz1) {
        uint32_t s1 = elapsedMs;
        split1TimeMs[carId] = s1;
        sz1TimeMs[carId] = s1;
        lastSectorTimeMs[carId] = timestampMs;

        bool isPB = false;
        bool isOverall = false;
        int32_t deltaMs = 0;
        if (bestSz1TimeMs[carId] > 0) {
          deltaMs = (int32_t)s1 - (int32_t)bestSz1TimeMs[carId];
          if (s1 < bestSz1TimeMs[carId]) {
            bestSz1TimeMs[carId] = s1;
            isPB = true;
          }
        } else {
          bestSz1TimeMs[carId] = s1;
        }

        if (overallBestSz1Ms > 0 && s1 < overallBestSz1Ms) {
          overallBestSz1Ms = s1;
          isOverall = true;
        } else if (overallBestSz1Ms == 0) {
          overallBestSz1Ms = s1;
        }

        broadcastSectorEvent(carId, 1, s1, s1, isPB, isOverall, deltaMs);
      } 
      else if (stationId == 2 && useSz2) {
        uint32_t split2 = elapsedMs;
        uint32_t prevPoint = (useSz1 && split1TimeMs[carId] > 0) ? (lapStartMs + split1TimeMs[carId]) : lapStartMs;
        uint32_t s2 = (timestampMs > prevPoint) ? (timestampMs - prevPoint) : split2;

        split2TimeMs[carId] = split2;
        sz2TimeMs[carId] = s2;
        lastSectorTimeMs[carId] = timestampMs;

        bool isPB = false;
        bool isOverall = false;
        int32_t deltaMs = 0;
        if (bestSz2TimeMs[carId] > 0) {
          deltaMs = (int32_t)s2 - (int32_t)bestSz2TimeMs[carId];
          if (s2 < bestSz2TimeMs[carId]) {
            bestSz2TimeMs[carId] = s2;
            isPB = true;
          }
        } else {
          bestSz2TimeMs[carId] = s2;
        }

        if (overallBestSz2Ms > 0 && s2 < overallBestSz2Ms) {
          overallBestSz2Ms = s2;
          isOverall = true;
        } else if (overallBestSz2Ms == 0) {
          overallBestSz2Ms = s2;
        }

        broadcastSectorEvent(carId, 2, s2, split2, isPB, isOverall, deltaMs);
      }
    }
    return;
  }

  // --- NORMALE RUNDENRENNEN-LOGIK ---
  if (stationId == 1 && useSz1) {
    if (checkpointMode && currentExpectedSector[carId] != 1) return;

    uint32_t split1 = elapsedMs;
    split1TimeMs[carId] = split1;
    sz1TimeMs[carId] = split1;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 2;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (bestSz1TimeMs[carId] > 0) {
      deltaMs = (int32_t)split1 - (int32_t)bestSz1TimeMs[carId];
      if (split1 < bestSz1TimeMs[carId]) {
        bestSz1TimeMs[carId] = split1;
        isPB = true;
      }
    } else {
      bestSz1TimeMs[carId] = split1;
    }

    if (overallBestSz1Ms > 0 && split1 < overallBestSz1Ms) {
      overallBestSz1Ms = split1;
      isOverall = true;
    } else if (overallBestSz1Ms == 0) {
      overallBestSz1Ms = split1;
    }

    broadcastSectorEvent(carId, 1, split1, split1, isPB, isOverall, deltaMs);
  } 
  else if (stationId == 2 && useSz2) {
    if (checkpointMode) {
      uint8_t expected = useSz1 ? 2 : 1;
      if (currentExpectedSector[carId] != expected) return;
    }

    uint32_t split2 = elapsedMs;
    uint32_t prevPoint = (useSz1 && lastSectorTimeMs[carId] > lapStartMs) ? lastSectorTimeMs[carId] : lapStartMs;
    uint32_t s2 = (timestampMs > prevPoint) ? (timestampMs - prevPoint) : split2;

    split2TimeMs[carId] = split2;
    sz2TimeMs[carId] = s2;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 3;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (bestSz2TimeMs[carId] > 0) {
      deltaMs = (int32_t)s2 - (int32_t)bestSz2TimeMs[carId];
      if (s2 < bestSz2TimeMs[carId]) {
        bestSz2TimeMs[carId] = s2;
        isPB = true;
      }
    } else {
      bestSz2TimeMs[carId] = s2;
    }

    if (overallBestSz2Ms > 0 && s2 < overallBestSz2Ms) {
      overallBestSz2Ms = s2;
      isOverall = true;
    } else if (overallBestSz2Ms == 0) {
      overallBestSz2Ms = s2;
    }

    broadcastSectorEvent(carId, 2, s2, split2, isPB, isOverall, deltaMs);
  } 
  else if (stationId == 3 && useSz3) {
    if (checkpointMode) {
      uint8_t expected = useSz2 ? 3 : (useSz1 ? 2 : 1);
      if (currentExpectedSector[carId] != expected) return;
    }

    uint32_t split3 = elapsedMs;
    uint32_t prevPoint = (lastSectorTimeMs[carId] > lapStartMs) ? lastSectorTimeMs[carId] : lapStartMs;
    uint32_t s3 = (timestampMs > prevPoint) ? (timestampMs - prevPoint) : split3;

    split3TimeMs[carId] = split3;
    sz3TimeMs[carId] = s3;
    lastSectorTimeMs[carId] = timestampMs;
    if (checkpointMode) currentExpectedSector[carId] = 4;

    bool isPB = false;
    bool isOverall = false;
    int32_t deltaMs = 0;

    if (bestSz3TimeMs[carId] > 0) {
      deltaMs = (int32_t)s3 - (int32_t)bestSz3TimeMs[carId];
      if (s3 < bestSz3TimeMs[carId]) {
        bestSz3TimeMs[carId] = s3;
        isPB = true;
      }
    } else {
      bestSz3TimeMs[carId] = s3;
    }

    if (overallBestSz3Ms > 0 && s3 < overallBestSz3Ms) {
      overallBestSz3Ms = s3;
      isOverall = true;
    } else if (overallBestSz3Ms == 0) {
      overallBestSz3Ms = s3;
    }

    broadcastSectorEvent(carId, 3, s3, split3, isPB, isOverall, deltaMs);
  }
}

void processValidLap(uint8_t carId, uint64_t exactSignalTimeMicros) {
  uint32_t exactMicros = (uint32_t)exactSignalTimeMicros;
  uint32_t nowMs = exactMicros / 1000UL;

  // --- DRAGRACE START AM ZIEL / BASIS ---
  if (currentMode == MODE_DRAGRACE) {
    lapCounter[carId] = 1;
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    readyForNextLap[carId] = true;
    currentExpectedSector[carId] = 1;
    sz1TimeMs[carId] = 0;
    sz2TimeMs[carId] = 0;
    sz3TimeMs[carId] = 0;
    sz4TimeMs[carId] = 0;
    split1TimeMs[carId] = 0;
    split2TimeMs[carId] = 0;
    split3TimeMs[carId] = 0;

    digitalWrite(LED_READY, LOW);
    readyLedBlinking = true;
    readyLedOffTime = millis() + 200;

    String startJson = "{\"type\":\"drag_start\",\"carId\":";
    startJson += carId;
    startJson += ",\"driver\":\"" + driverNames[carId] + "\"}";
    broadcastWebSocket(startJson);
    return;
  }

  if ((currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) && modeEndTimeMs > 0 && millis() > modeEndTimeMs) return;
  if (currentMode == MODE_RACE_LAPS && targetRounds > 0 && lapCounter[carId] > targetRounds) return;

  // INITIALER START ODER NEUER START NACH RESET
  if (lapCounter[carId] == 0) {
    awaitingBaseCross[carId] = false;
    lapCounter[carId] = 1;
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    readyForNextLap[carId] = false;
    currentExpectedSector[carId] = 1;
    sz1TimeMs[carId] = 0;
    sz2TimeMs[carId] = 0;
    sz3TimeMs[carId] = 0;
    sz4TimeMs[carId] = 0;
    split1TimeMs[carId] = 0;
    split2TimeMs[carId] = 0;
    split3TimeMs[carId] = 0;

    digitalWrite(LED_READY, LOW);
    readyLedBlinking = true;
    readyLedOffTime = millis() + 200;

    String baseJson = "{\"type\":\"base_cross\",\"carId\":";
    baseJson += carId;
    baseJson += ",\"driver\":\"" + driverNames[carId] + "\"}";
    broadcastWebSocket(baseJson);
    return;
  }

  // CHECKPOINT-MODUS PRÜFUNG (erst ab gewerteter Runde relevant)
  if (checkpointMode && currentMode != MODE_DRAGRACE && lapCounter[carId] > 0) {
    bool passedAll = true;
    if (useSz3 && currentExpectedSector[carId] < 4) passedAll = false;
    else if (useSz2 && !useSz3 && currentExpectedSector[carId] < 3) passedAll = false;
    else if (useSz1 && !useSz2 && !useSz3 && currentExpectedSector[carId] < 2) passedAll = false;

    if (!passedAll) {
      currentExpectedSector[carId] = 1;
      return;
    }
    currentExpectedSector[carId] = 1;
  }

  bool lapCounted = false;

  if ((exactMicros - lastCrossTime[carId] > ((uint32_t)minLapTimeMs * 1000UL)) && readyForNextLap[carId]) {
    uint32_t currentLapTimeMs = (exactMicros - lastCrossTime[carId]) / 1000UL;
    
    uint8_t finalSectorNum = useSz3 ? 4 : (useSz2 ? 3 : (useSz1 ? 2 : 0));

    if (finalSectorNum > 0 && lastSectorTimeMs[carId] > (lastCrossTime[carId] / 1000UL)) {
      uint32_t lastSplit = (finalSectorNum == 4) ? split3TimeMs[carId] : 
                           ((finalSectorNum == 3) ? split2TimeMs[carId] : split1TimeMs[carId]);

      if (currentLapTimeMs > lastSplit) {
        uint32_t sFinal = currentLapTimeMs - lastSplit;

        if (sFinal >= 100 && sFinal < 600000) {
          bool isPB = false;
          bool isOverall = false;
          int32_t deltaMs = 0;

          if (finalSectorNum == 4) {
            sz4TimeMs[carId] = sFinal;
            if (bestSz4TimeMs[carId] > 0) {
              deltaMs = (int32_t)sFinal - (int32_t)bestSz4TimeMs[carId];
              if (sFinal < bestSz4TimeMs[carId]) {
                bestSz4TimeMs[carId] = sFinal;
                isPB = true;
              }
            } else {
              bestSz4TimeMs[carId] = sFinal;
            }

            if (overallBestSz4Ms > 0 && sFinal < overallBestSz4Ms) {
              overallBestSz4Ms = sFinal;
              isOverall = true;
            } else if (overallBestSz4Ms == 0) {
              overallBestSz4Ms = sFinal;
            }
          } 
          else if (finalSectorNum == 3) {
            sz3TimeMs[carId] = sFinal;
            split3TimeMs[carId] = currentLapTimeMs;
            if (bestSz3TimeMs[carId] > 0) {
              deltaMs = (int32_t)sFinal - (int32_t)bestSz3TimeMs[carId];
              if (sFinal < bestSz3TimeMs[carId]) {
                bestSz3TimeMs[carId] = sFinal;
                isPB = true;
              }
            } else {
              bestSz3TimeMs[carId] = sFinal;
            }

            if (overallBestSz3Ms > 0 && sFinal < overallBestSz3Ms) {
              overallBestSz3Ms = sFinal;
              isOverall = true;
            } else if (overallBestSz3Ms == 0) {
              overallBestSz3Ms = sFinal;
            }
          } 
          else if (finalSectorNum == 2) {
            sz2TimeMs[carId] = sFinal;
            split2TimeMs[carId] = currentLapTimeMs;
            if (bestSz2TimeMs[carId] > 0) {
              deltaMs = (int32_t)sFinal - (int32_t)bestSz2TimeMs[carId];
              if (sFinal < bestSz2TimeMs[carId]) {
                bestSz2TimeMs[carId] = sFinal;
                isPB = true;
              }
            } else {
              bestSz2TimeMs[carId] = sFinal;
            }

            if (overallBestSz2Ms > 0 && sFinal < overallBestSz2Ms) {
              overallBestSz2Ms = sFinal;
              isOverall = true;
            } else if (overallBestSz2Ms == 0) {
              overallBestSz2Ms = sFinal;
            }
          }

          broadcastSectorEvent(carId, finalSectorNum, sFinal, currentLapTimeMs, isPB, isOverall, deltaMs);
        }
      }
    }

    lastLapDuration[carId] = currentLapTimeMs;
    bool isLapPB = false;
    bool isLapOverall = false;

    if (lapCounter[carId] > 1 && bestLapDuration[carId] > 0 && currentLapTimeMs < bestLapDuration[carId]) {
      bestLapDuration[carId] = currentLapTimeMs;
      isLapPB = true;
    } else if (bestLapDuration[carId] == 0) {
      bestLapDuration[carId] = currentLapTimeMs;
    }

    uint32_t currentOverallBestLap = 0;
    for (uint8_t c = 1; c <= MAX_CARS; c++) {
      if (c != carId && bestLapDuration[c] > 0 && (currentOverallBestLap == 0 || bestLapDuration[c] < currentOverallBestLap)) {
        currentOverallBestLap = bestLapDuration[c];
      }
    }
    if (currentOverallBestLap > 0 && currentLapTimeMs < currentOverallBestLap) {
      isLapOverall = true;
    }
    
    uint16_t completedLap = lapCounter[carId];
    lapCounter[carId]++;
    lastCrossTime[carId] = exactMicros;
    lastSectorTimeMs[carId] = nowMs;
    readyForNextLap[carId] = false;
    currentExpectedSector[carId] = 1;
    lapCounted = true;

    uint64_t currentTsMs = getCurrentTimestampMs();
    uint8_t pos = getCurrentPosition(carId);
    
    // In Flash-Queue einreihen (inklusive aller Sektorzeiten)
    queueLapForStorage(carId, completedLap, currentLapTimeMs, sz1TimeMs[carId], sz2TimeMs[carId], sz3TimeMs[carId], sz4TimeMs[carId], pos, (uint8_t)currentMode, currentTsMs);

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