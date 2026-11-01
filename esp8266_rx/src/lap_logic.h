#ifndef LAP_LOGIC_H
#define LAP_LOGIC_H

#include "config.h"
#include <Arduino.h>

enum RaceMode {
  MODE_PRACTICE,   // Freies Training
  MODE_QUALIFYING, // Qualifying (Zeitbasiert)
  MODE_RACE_LAPS,  // Rennen (Rundenbasiert)
  MODE_RACE_TIME,  // Rennen (Zeitbasiert)
  MODE_DRAGRACE,   // Dragrace-Modus (Modus 4)
  MODE_GHOST_HUNT  // Geisterjagd / Ghost Hunt (Modus 5)
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
extern bool ghostAudioEnabled;

// Status-Flags
extern bool awaitingBaseCross[MAX_CARS + 1];
extern bool finishedFinalLap[MAX_CARS + 1];
extern bool dragRunActive[MAX_CARS + 1];
extern bool dragTrackOccupied;
extern uint8_t activeDragCarId;

// Dynamische Boxen- / Pause- / Outlap-Flags
extern bool inPitOrPause[MAX_CARS + 1];
extern uint32_t pitEnteredMs[MAX_CARS + 1];
extern bool isOutLap[MAX_CARS + 1];
extern bool lastLapIsOutLap[MAX_CARS + 1];

// Aktuelle Sektorzeiten pro Auto in Millisekunden
extern uint32_t sz1TimeMs[MAX_CARS + 1];
extern uint32_t sz2TimeMs[MAX_CARS + 1];
extern uint32_t sz3TimeMs[MAX_CARS + 1];
extern uint32_t sz4TimeMs[MAX_CARS + 1];

// Aktuelle Split-Zeiten pro Auto in Millisekunden (kumuliert ab Start)
extern uint32_t split1TimeMs[MAX_CARS + 1];
extern uint32_t split2TimeMs[MAX_CARS + 1];
extern uint32_t split3TimeMs[MAX_CARS + 1];

// Persönliche Bestzeiten pro Sektor
extern uint32_t bestSz1TimeMs[MAX_CARS + 1];
extern uint32_t bestSz2TimeMs[MAX_CARS + 1];
extern uint32_t bestSz3TimeMs[MAX_CARS + 1];
extern uint32_t bestSz4TimeMs[MAX_CARS + 1];

// Absolute Bestzeiten (Purple Sectors) der aktuellen Session
extern uint32_t overallBestSz1Ms;
extern uint32_t overallBestSz2Ms;
extern uint32_t overallBestSz3Ms;
extern uint32_t overallBestSz4Ms;

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

// ALL-TIME STRECKENREKORD (P1 der aktiven Strecke)
extern uint32_t allTimeBestLapMs;
extern uint8_t allTimeBestCarId;
extern String allTimeBestDriver;
extern String allTimeBestCar;
extern String allTimeBestDate;
extern uint32_t allTimeBestS1Ms;
extern uint32_t allTimeBestS2Ms;
extern uint32_t allTimeBestS3Ms;
extern uint32_t allTimeBestS4Ms;
extern uint8_t allTimeBestSzMask;

// TAGESBESTZEIT (P1 heute der aktiven Strecke)
extern uint32_t dailyBestLapMs;
extern uint8_t dailyBestCarId;
extern String dailyBestDriver;
extern String dailyBestCar;
extern String dailyBestDate;
extern uint32_t dailyBestS1Ms;
extern uint32_t dailyBestS2Ms;
extern uint32_t dailyBestS3Ms;
extern uint32_t dailyBestS4Ms;
extern uint8_t dailyBestSzMask;

// GEISTERJAGD (MODUS 5)
extern uint8_t ghostTargetType;
extern uint8_t ghostCarId;
extern uint32_t ghostLapMs;
extern uint32_t ghostS1Ms;
extern uint32_t ghostS2Ms;
extern uint32_t ghostS3Ms;
extern uint32_t ghostS4Ms;
extern uint32_t ghostSplit1Ms;
extern uint32_t ghostSplit2Ms;
extern uint32_t ghostSplit3Ms;
extern uint8_t ghostSzMask;
extern String ghostDriverName;
extern String ghostCarName;
void setGhostTarget(uint8_t targetType, uint32_t lapMs, uint32_t s1, uint32_t s2, uint32_t s3, uint32_t s4, uint8_t szMask, const String& driver, const String& car, uint8_t carId = 0);

void initLapLogic();
void loadDailyBest();
void saveDailyBest();

void processLapLogic();
void processSectorPacket(uint8_t stationId, uint8_t carId, uint32_t timestampMs);
void processValidLap(uint8_t carId, uint64_t exactSignalTimeMicros);
void updateLapLogicTasks(); 
void updateLEDs();
void broadcastLapEvent(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint8_t pos = 1, bool isPB = false, bool isOverallBest = false, bool isTrackRecord = false, bool isDailyRecord = false, bool isOutLap = false);

// resetAllLaps erlaubt einen "Hard Reset", der auch die Tagesbestzeit löscht
void resetAllLaps(bool isHardReset = false);

void addRecentID(uint8_t newId);
uint8_t getCurrentPosition(uint8_t carId);
bool areAllActiveCarsFinished();
uint8_t getActiveCarsCount(uint32_t timeoutMs = 300000UL);
uint32_t getAverageLapTimeMs(uint8_t carId);

void syncClockOnce(uint64_t clientTsSec);
uint64_t getCurrentTimestampMs();

#endif
