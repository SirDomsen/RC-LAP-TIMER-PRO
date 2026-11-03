#ifndef LT_RACE_RULES_H
#define LT_RACE_RULES_H

#include <stdint.h>
#include "race_mode.h"

namespace RaceRules {

bool ranksByBestLap(RaceMode mode);
bool isTimedMode(RaceMode mode);
bool canAcceptTimedEventCross(
    uint32_t modeEndTimeMs,
    uint32_t nowMs,
    uint16_t lapCount,
    uint32_t lapStartMs,
    bool finishedFinalLap);
bool isTimedEventCarPendingFinish(
    uint32_t modeEndTimeMs,
    uint32_t nowMs,
    uint16_t lapCount,
    uint32_t lapStartMs,
    uint32_t lastSeenMs,
    bool finishedFinalLap,
    bool inPitOrPause,
    uint32_t inactivityTimeoutMs);
bool exceedsOutLapThreshold(uint32_t averageLapMs, uint32_t elapsedMs, uint16_t factorPercent);
bool hasPassedAllCheckpoints(bool useSz1, bool useSz2, bool useSz3, uint8_t expectedSector);
uint8_t getExpectedSectorForStation(uint8_t stationId, bool useSz1, bool useSz2, bool useSz3);
uint8_t getNextExpectedSector(uint8_t stationId);
uint8_t getActiveTargetSector(bool useSz1, bool useSz2, bool useSz3);
uint8_t getFinalSectorNumber(bool useSz1, bool useSz2, bool useSz3);

uint8_t getPosition(
    uint8_t carId,
    uint8_t maxCars,
    bool rankByBestLap,
    const uint32_t* bestLapDuration,
    const uint16_t* lapCounter,
    const uint32_t* lastCrossTime);

}  // namespace RaceRules

#endif
