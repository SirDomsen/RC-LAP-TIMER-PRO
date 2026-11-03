#include "race_rules.h"

bool RaceRules::ranksByBestLap(RaceMode mode) {
  return mode == MODE_DRAGRACE ||
         mode == MODE_QUALIFYING ||
         mode == MODE_GHOST_HUNT;
}

bool RaceRules::isTimedMode(RaceMode mode) {
  return mode == MODE_QUALIFYING || mode == MODE_RACE_TIME;
}

bool RaceRules::hasPassedAllCheckpoints(
    bool useSz1,
    bool useSz2,
    bool useSz3,
    uint8_t expectedSector) {
  const uint8_t finalSector = getFinalSectorNumber(useSz1, useSz2, useSz3);
  return finalSector == 0 || expectedSector >= finalSector;
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
