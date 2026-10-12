#include "lap_logic.h"
#include "ir_decoder.h"
#include "storage.h"

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

uint32_t sz1TimeMs[MAX_CARS + 1] = {0};
uint32_t sz2TimeMs[MAX_CARS + 1] = {0};
uint32_t sz3TimeMs[MAX_CARS + 1] = {0};

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

void initLapLogic() {
  pinMode(LED_READY, OUTPUT);
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(LED_ONBOARD, OUTPUT);

  digitalWrite(LED_READY, LOW); 
  digitalWrite(LED_SIGNAL, LOW);
  digitalWrite(LED_ONBOARD, HIGH);
  
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
    currentExpectedSector[i] = 1;
    sz1TimeMs[i] = 0;
    sz2TimeMs[i] = 0;
    sz3TimeMs[i] = 0;
  }
  for (int i = 0; i < 5; i++) recentIDs[i] = 0;
  lastPendingId = 0;
  detectionCount = 0;
  readyLedBlinking = false;
}

uint8_t getCurrentPosition(uint8_t carId) {
  uint8_t pos = 1;
  for (uint8_t i = 1; i <= MAX_CARS; i++) {
    if (i == carId) continue;
    if (lapCounter[i] > lapCounter[carId]) {
      pos++;
    } else if (lapCounter[i] == lapCounter[carId] && lastCrossTime[i] < lastCrossTime[carId] && lapCounter[i] > 0) {
      pos++;
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
  lastSeenMs[carId] = millis();
  addRecentID(carId);

  // Im Dragrace-Modus fungiert der letzte aktive Sektor als Ziellinie
  if (currentMode == MODE_DRAGRACE && dragraceMode && stationId == getActiveTargetSector()) {
    processValidLap(carId, (uint64_t)timestampMs * 1000UL);
    return;
  }

  // Zwischenzeiten (Splits) relativ zum Startzeitpunkt der Runde berechnen
  if (lastCrossTime[carId] > 0) {
    uint32_t elapsedMs = (timestampMs >= (lastCrossTime[carId] / 1000UL)) ? (timestampMs - (lastCrossTime[carId] / 1000UL)) : 0;
    if (stationId == 1 && useSz1) sz1TimeMs[carId] = elapsedMs;
    if (stationId == 2 && useSz2) sz2TimeMs[carId] = elapsedMs;
    if (stationId == 3 && useSz3) sz3TimeMs[carId] = elapsedMs;
  }

  // Checkpoint-Validierung
  if (checkpointMode) {
    if (stationId == 1 && useSz1 && currentExpectedSector[carId] == 1) currentExpectedSector[carId] = 2;
    else if (stationId == 2 && useSz2 && currentExpectedSector[carId] == 2) currentExpectedSector[carId] = 3;
    else if (stationId == 3 && useSz3 && currentExpectedSector[carId] == 3) currentExpectedSector[carId] = 4;
  }
}

void processValidLap(uint8_t carId, uint64_t exactSignalTimeMicros) {
  if ((currentMode == MODE_QUALIFYING || currentMode == MODE_RACE_TIME) && modeEndTimeMs > 0 && millis() > modeEndTimeMs) return;
  if (currentMode == MODE_RACE_LAPS && targetRounds > 0 && lapCounter[carId] > targetRounds) return;

  if (checkpointMode && currentMode != MODE_DRAGRACE) {
    bool passedAll = true;
    if (useSz1 && currentExpectedSector[carId] < 2) passedAll = false;
    if (useSz2 && currentExpectedSector[carId] < 3) passedAll = false;
    if (useSz3 && currentExpectedSector[carId] < 4) passedAll = false;

    if (!passedAll) {
      currentExpectedSector[carId] = 1;
      return;
    }
    currentExpectedSector[carId] = 1;
  }

  uint32_t exactMicros = (uint32_t)exactSignalTimeMicros;
  bool lapCounted = false;

  if (lapCounter[carId] == 0) {
    lapCounter[carId] = 1;
    lastCrossTime[carId] = exactMicros;
    readyForNextLap[carId] = false;
    sz1TimeMs[carId] = 0;
    sz2TimeMs[carId] = 0;
    sz3TimeMs[carId] = 0;
    lapCounted = true;
  } 
  else if ((exactMicros - lastCrossTime[carId] > ((uint32_t)minLapTimeMs * 1000UL)) && readyForNextLap[carId]) {
    uint32_t currentLapTimeMs = (exactMicros - lastCrossTime[carId]) / 1000UL;
    
    lastLapDuration[carId] = currentLapTimeMs;
    if (bestLapDuration[carId] == 0 || currentLapTimeMs < bestLapDuration[carId]) {
      bestLapDuration[carId] = currentLapTimeMs;
    }
    
    uint16_t completedLap = lapCounter[carId];
    lapCounter[carId]++;
    lastCrossTime[carId] = exactMicros;
    readyForNextLap[carId] = false;
    sz1TimeMs[carId] = 0;
    sz2TimeMs[carId] = 0;
    sz3TimeMs[carId] = 0;
    lapCounted = true;

    uint64_t currentTsMs = getCurrentTimestampMs();
    uint8_t pos = getCurrentPosition(carId);
    
    if (currentMode == MODE_PRACTICE) {
      queuePracticeLapForStorage(carId, completedLap, currentLapTimeMs, pos, (uint8_t)currentMode, currentTsMs);
    } else {
      queueLapForStorage(carId, completedLap, currentLapTimeMs, pos, (uint8_t)currentMode, currentTsMs);
    }
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
      if (lapCounter[i] == 1) {
        lastCrossTime[i] = micros(); 
      }
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