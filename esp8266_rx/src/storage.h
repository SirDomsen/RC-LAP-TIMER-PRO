#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

extern String driverNames[MAX_CARS + 1];
extern String carNames[MAX_CARS + 1];

// Puffer-Struktur für asynchrones Flash-Schreiben mit MS-Zeitstempel
struct PendingLap {
  uint8_t carId;
  uint16_t lapNum;
  uint32_t lapTimeMs;
  uint8_t position;
  uint8_t mode;
  uint64_t timestampMs;
};

void loadSettingsFromEEPROM();
void loadNamesFromFS(); // <--- Neu deklariert
void saveNamesToEEPROM(int id, const String& driver, const String& car);
void saveDeadtimeToEEPROM(uint16_t ms);
void saveFilterToEEPROM(bool enable);

// LittleFS Statistik-Funktionen
void initFS();
void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint8_t position, uint8_t mode, uint64_t timestampMs);

// Puffer-Queue für Flash-Schreibvorgänge
void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint8_t position, uint8_t mode, uint64_t timestampMs);
void processStorageQueue();

String getDriverHistoryJson(uint8_t carId);
void deleteLapFromHistory(uint8_t carId, uint16_t lapNum);
void deleteDriverHistory(uint8_t carId);
void deleteAllHistory();

// Session / Event Historie
void saveSessionToHistory(const String& sessionJsonStr);
String getSessionsJson();
void deleteSessionFromHistory(uint64_t sessionTs);

#endif