#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"

#define MAX_TRACK_RECORDS 5

struct RecordEntry {
  uint32_t lapMs;
  uint8_t carId;
  String driver;
  String car;
  String date;
  uint32_t prevLapMs;
  String prevDate;
};

extern String driverNames[MAX_CARS + 1];
extern String carNames[MAX_CARS + 1];
extern bool webCacheEnabled;

// Vollständig konfigurierbares WLAN
extern String homeSSID;
extern String homePass;
extern String hotspotSSID;
extern String hotspotPass;
extern String apSSID;
extern String apPass;
extern uint8_t activeWifiMode; // 0 = AP, 1 = Heim-WLAN, 2 = Hotspot

// Aktive Strecke & Flash-Speicherstatus
extern String activeTrackId;
extern String activeTrackName;
extern uint8_t cachedFsUsedPct;
extern uint32_t cachedFsUsedBytes;
extern uint32_t cachedFsTotalBytes;
extern bool storageFullLocked;
extern int32_t clientTzOffsetSec;

// Top-5 Rekordlisten der aktiven Strecke (All-Time & Heute)
extern RecordEntry activeAllTimeRecords[MAX_TRACK_RECORDS];
extern uint8_t activeAllTimeCount;
extern RecordEntry activeDailyRecords[MAX_TRACK_RECORDS];
extern uint8_t activeDailyCount;

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

// WLAN-Ladefunktionen
void loadWifiConfigFromFS();
void saveWifiConfigToFS(uint8_t mode, const String& hSsid, const String& hPass, const String& hsSsid, const String& hsPass, const String& aSsid, const String& aPass);
void saveHotspotConfigToFS(const String& ssid, const String& pass);

void initFS();
void updateFsUsageCache();

// Streckenverwaltung (/tracks.json), Streckenrekord (Top 5) & Tagesbestzeit (Top 5)
void loadTracksFromFS();
void saveTracksToFS();
String sanitizeTrackSlug(const String& rawName);
bool selectTrackById(const String& trackId);
bool addOrSelectTrack(const String& rawName);
bool deleteTrackById(const String& trackId);
void saveActiveTrackBestToFS();
String getCurrentLocalDateStr();
void checkDailyBestDateRollover();
void syncTopRecordVars();
void checkAndUpdateTrackRecords(uint8_t carId, uint32_t lapTimeMs, bool& isNewAllTimeBest, bool& isNewDailyBest);
bool deleteTrackRecordEntry(const String& trackId, bool isDaily, int8_t index);

// Session-basierte Statistik-Speicherung (/<track>/<mode>/<YY-MM-DD_HH-MM-SS>.json)
void startNewSessionFrame(uint8_t mode);
void flushCurrentSessionToIndex();
void saveLapToHistory(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs);
void saveSectorConfigToEEPROM(bool s1, bool s2, bool s3, bool cp, bool dr, bool audio);

void queueLapForStorage(uint8_t carId, uint16_t lapNum, uint32_t lapTimeMs, uint32_t sz1, uint32_t sz2, uint32_t sz3, uint32_t sz4, uint8_t position, uint8_t mode, uint64_t timestampMs);
void processStorageQueue();

void deleteLapFromSession(const String& sessionId, uint8_t carId, uint16_t lapNum);
void deleteDriverHistory(uint8_t carId);
void deleteAllHistory();

void deleteSessionById(const String& sessionId);

#endif
