#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

extern String driverNames[MAX_CARS + 1];
extern String carNames[MAX_CARS + 1];
extern bool webCacheEnabled;

// NEU: Hotspot-Variablen
extern String hotspotSSID;
extern String hotspotPass;

// Puffer-Struktur mit vollständigen Sektorzeiten
struct PendingLap {
  uint8_t carId;
  uint16_t lapNum;
  uint32_t lapTimeMs;
  uint32_t sz1;
  uint32_t sz2;
  uint32_t sz3;
  uint32_t sz4;
  uint8_t position;
  uint8_t mode;
  uint64_t timestampMs;
};

void loadSettingsFromEEPROM();
void loadNamesFromFS();
void saveNamesToEEPROM(int id, const String& driver, const String& car);
void saveDeadtimeToEEPROM(uint16_t ms);
void saveFilterToEEPROM(bool enable);
void saveCacheConfigToEEPROM(bool enable);

// NEU: Hotspot-Ladefunktionen
void loadHotspotConfigFromFS();
void saveHotspotConfigToFS(const String& ssid, const String& pass);

void initFS();
void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs);
void saveSectorConfigToEEPROM(bool s1, bool s2, bool s3, bool cp, bool dr, bool audio);

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs);
void processStorageQueue();

void deleteLapFromHistory(uint8_t carId, uint16_t lapNum);
void deleteDriverHistory(uint8_t carId);
void deleteAllHistory();

void saveSessionToHistory(const String& sessionJsonStr);
void deleteSessionFromHistory(uint64_t sessionTs);

#endif