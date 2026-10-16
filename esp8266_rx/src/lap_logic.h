#ifndef LAP_LOGIC_H
#define LAP_LOGIC_H

#include "config.h"

enum RaceMode {
  MODE_PRACTICE,   // Freies Training
  MODE_QUALIFYING, // Qualifying (Zeitbasiert)
  MODE_RACE_LAPS,  // Rennen (Rundenbasiert)
  MODE_RACE_TIME,  // Rennen (Zeitbasiert)
  MODE_DRAGRACE    // Dragrace-Modus (Modus 4)
};

extern RaceMode currentMode;
extern uint32_t modeEndTimeMs;   
extern uint16_t durationMinutes; 

// Konfigurations-Flags für Sektoren und Modi
extern bool useSz1;
extern bool useSz2;
extern bool useSz3;
extern bool checkpointMode;
extern bool dragraceMode;
extern bool sectorAudioFeedback;

// Aktuelle Sektorzeiten pro Auto in Millisekunden
extern uint32_t sz1TimeMs[MAX_CARS + 1];
extern uint32_t sz2TimeMs[MAX_CARS + 1];
extern uint32_t sz3TimeMs[MAX_CARS + 1];

// Aktuelle Split-Zeiten pro Auto in Millisekunden (kumuliert ab Start)
extern uint32_t split1TimeMs[MAX_CARS + 1];
extern uint32_t split2TimeMs[MAX_CARS + 1];
extern uint32_t split3TimeMs[MAX_CARS + 1];

// Persönliche Bestzeiten pro Sektor
extern uint32_t bestSz1TimeMs[MAX_CARS + 1];
extern uint32_t bestSz2TimeMs[MAX_CARS + 1];
extern uint32_t bestSz3TimeMs[MAX_CARS + 1];

// Absolute Bestzeiten (Purple Sectors) der aktuellen Session
extern uint32_t overallBestSz1Ms;
extern uint32_t overallBestSz2Ms;
extern uint32_t overallBestSz3Ms;

// Letzter Durchgangszeitstempel (in ms) für Sektorintervall-Berechnung
extern uint32_t lastSectorTimeMs[MAX_CARS + 1];

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
void processSectorPacket(uint8_t stationId, uint8_t carId, uint32_t timestampMs);
void processValidLap(uint8_t carId, uint64_t exactSignalTimeMicros);
void updateLapLogicTasks(); 
void updateLEDs();
void resetAllLaps();
void addRecentID(uint8_t newId);
uint8_t getCurrentPosition(uint8_t carId);

void syncClockOnce(uint64_t clientTsSec);
uint64_t getCurrentTimestampMs();

#endif