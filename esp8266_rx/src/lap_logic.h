#ifndef LAP_LOGIC_H
#define LAP_LOGIC_H

#include "config.h"

enum RaceMode {
  MODE_PRACTICE,   // Freies Training
  MODE_QUALIFYING, // Qualifying (Zeitbasiert)
  MODE_RACE_LAPS,  // Rennen (Rundenbasiert)
  MODE_RACE_TIME   // Rennen (Zeitbasiert)
};

extern RaceMode currentMode;
extern uint32_t modeEndTimeMs;   
extern uint16_t durationMinutes; 

extern uint32_t lastCrossTime[MAX_CARS + 1];
extern uint32_t lastLapDuration[MAX_CARS + 1];
extern uint32_t bestLapDuration[MAX_CARS + 1];
extern uint16_t lapCounter[MAX_CARS + 1];
extern uint16_t targetRounds;
extern uint8_t recentIDs[5];
extern uint16_t minLapTimeMs; 
extern bool requireTwoFrames;

extern uint32_t lastSeenMs[MAX_CARS + 1];
extern bool readyForNextLap[MAX_CARS + 1];

extern uint64_t baseUnixTimestampSec;
extern uint32_t baseMillis;

void initLapLogic();
void processLapLogic();
void updateLapLogicTasks(); 
void updateLEDs();
void resetAllLaps();
void addRecentID(uint8_t newId);
uint8_t getCurrentPosition(uint8_t carId);

void syncClockOnce(uint64_t clientTsSec);
uint64_t getCurrentTimestampMs();

#endif