#include "race_rules.h"

bool RaceRules::ranksByBestLap(RaceMode mode) {
  return mode == MODE_DRAGRACE ||
         mode == MODE_QUALIFYING ||
         mode == MODE_GHOST_HUNT;
}

bool RaceRules::isTimedMode(RaceMode mode) {
  return mode == MODE_QUALIFYING || mode == MODE_RACE_TIME;
}

bool RaceRules::isLapLimitExceeded(
    RaceMode mode,
    uint16_t lapCount,
    uint16_t targetRounds) {
  return mode == MODE_RACE_LAPS && targetRounds > 0 && lapCount > targetRounds;
}

bool RaceRules::isLapTimeAboveMinimum(
    uint64_t elapsedMicros,
    uint16_t minimumLapTimeMs) {
  return elapsedMicros > ((uint32_t)minimumLapTimeMs * 1000UL);
}

bool RaceRules::isValidSectorDuration(uint32_t durationMs) {
  return durationMs >= 100 && durationMs < 600000;
}

bool RaceRules::isValidIntermediateSectorDuration(uint32_t durationMs) {
  return durationMs >= 100 && durationMs <= 600000;
}

bool RaceRules::hasFinalSectorRemainder(
    uint32_t lapDurationMs,
    uint32_t lastSplitMs) {
  return lastSplitMs > 0 && lapDurationMs > lastSplitMs;
}

bool RaceRules::canAcceptTimedEventCross(
    uint32_t modeEndTimeMs,
    uint32_t nowMs,
    uint16_t lapCount,
    uint32_t lapStartMs,
    bool finishedFinalLap) {
  if (modeEndTimeMs == 0 || nowMs <= modeEndTimeMs) return true;
  if (lapCount == 0 || lapStartMs == 0 || finishedFinalLap) return false;
  return lapStartMs <= modeEndTimeMs;
}

bool RaceRules::isTimedEventCarPendingFinish(
    uint32_t modeEndTimeMs,
    uint32_t nowMs,
    uint16_t lapCount,
    uint32_t lapStartMs,
    uint32_t lastSeenMs,
    bool finishedFinalLap,
    bool inPitOrPause,
    uint32_t inactivityTimeoutMs) {
  if (modeEndTimeMs == 0 || nowMs <= modeEndTimeMs) return false;
  if (lapCount == 0 || finishedFinalLap || inPitOrPause) return false;
  if (lapStartMs > modeEndTimeMs) return false;
  return nowMs - lastSeenMs < inactivityTimeoutMs;
}

bool RaceRules::exceedsOutLapThreshold(
    uint32_t averageLapMs,
    uint32_t elapsedMs,
    uint16_t factorPercent) {
  if (averageLapMs == 0) return false;
  const uint32_t thresholdMs = (averageLapMs * factorPercent) / 100;
  return elapsedMs > thresholdMs;
}

bool RaceRules::hasPassedAllCheckpoints(
    bool useSz1,
    bool useSz2,
    bool useSz3,
    uint8_t expectedSector) {
  const uint8_t finalSector = getFinalSectorNumber(useSz1, useSz2, useSz3);
  return finalSector == 0 || expectedSector >= finalSector;
}

uint8_t RaceRules::getExpectedSectorForStation(
    uint8_t stationId,
    bool useSz1,
    bool useSz2,
    bool useSz3) {
  if (stationId == 1 && useSz1) return 1;
  if (stationId == 2 && useSz2) return useSz1 ? 2 : 1;
  if (stationId == 3 && useSz3) {
    if (useSz2) return 3;
    return useSz1 ? 2 : 1;
  }
  return 0;
}

uint8_t RaceRules::getNextExpectedSector(uint8_t stationId) {
  if (stationId < 1 || stationId > 3) return 0;
  return stationId + 1;
}

uint8_t RaceRules::getActiveTargetSector(bool useSz1, bool useSz2, bool useSz3) {
  if (useSz3) return 3;
  if (useSz2) return 2;
  if (useSz1) return 1;
  return 0;
}

uint8_t RaceRules::getFinalSectorNumber(bool useSz1, bool useSz2, bool useSz3) {
  if (useSz3) return 4;
  if (useSz2) return 3;
  if (useSz1) return 2;
  return 0;
}

uint8_t RaceRules::getPosition(
    uint8_t carId,
    uint8_t maxCars,
    bool rankByBestLap,
    const uint32_t* bestLapDuration,
    const uint16_t* lapCounter,
    const uint32_t* lastCrossTime) {
  uint8_t position = 1;
  for (uint8_t otherCarId = 1; otherCarId <= maxCars; otherCarId++) {
    if (otherCarId == carId) continue;

    if (rankByBestLap) {
      if (bestLapDuration[otherCarId] > 0 &&
          (bestLapDuration[carId] == 0 ||
           bestLapDuration[otherCarId] < bestLapDuration[carId])) {
        position++;
      }
    } else if (lapCounter[otherCarId] > lapCounter[carId]) {
      position++;
    } else if (lapCounter[otherCarId] == lapCounter[carId] &&
               lastCrossTime[otherCarId] < lastCrossTime[carId] &&
               lapCounter[otherCarId] > 0) {
      position++;
    }
  }
  return position;
}
