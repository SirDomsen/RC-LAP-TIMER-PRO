#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <ESPAsyncWebServer.h>

void handleGetLaps(AsyncWebServerRequest *request);
void handleGetStationsStatus(AsyncWebServerRequest *request);
void handlePostSession(AsyncWebServerRequest *request);
void handleGetTimeLeft(AsyncWebServerRequest *request);
void handleGetRecent(AsyncWebServerRequest *request);
void handleGetDeadtime(AsyncWebServerRequest *request);
void handleGetFilter(AsyncWebServerRequest *request);
void handleGetNames(AsyncWebServerRequest *request);
void handleGetVersion(AsyncWebServerRequest *request);
void handlePostSetFilter(AsyncWebServerRequest *request);
void handlePostSetDeadtime(AsyncWebServerRequest *request);
void handlePostRename(AsyncWebServerRequest *request);
void handlePostReset(AsyncWebServerRequest *request);

void handlePostSyncTime(AsyncWebServerRequest *request);
void handleGetSessions(AsyncWebServerRequest *request);
void handleGetSessionFile(AsyncWebServerRequest *request);
void handlePostDeleteLap(AsyncWebServerRequest *request);
void handlePostDeleteDriverStats(AsyncWebServerRequest *request);
void handlePostDeleteAllStats(AsyncWebServerRequest *request);
void handlePostDeleteSession(AsyncWebServerRequest *request);

// Strecken-Verwaltung & Top-5 Rekord-Management
void handleGetTracks(AsyncWebServerRequest *request);
void handlePostSelectTrack(AsyncWebServerRequest *request);
void handlePostAddTrack(AsyncWebServerRequest *request);
void handlePostDeleteTrack(AsyncWebServerRequest *request);
void handlePostDeleteTrackRecord(AsyncWebServerRequest *request);

void handleGetSectorConfig(AsyncWebServerRequest *request);
void handlePostSetSectorConfig(AsyncWebServerRequest *request);

void handleGetDebugPackets(AsyncWebServerRequest *request);
void handleGetDebugStatus(AsyncWebServerRequest *request);
void handlePostSetDebugMode(AsyncWebServerRequest *request);

void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

void handleGetCacheConfig(AsyncWebServerRequest *request);
void handlePostSetCacheConfig(AsyncWebServerRequest *request);

// Konsolidierter Handler für alle Einstellungen (1 TCP-Verbindung)
void handleGetAllConfig(AsyncWebServerRequest *request);

// Handlers für voll konfigurierbares WLAN & SZ-OTA
void handleGetWifiConfig(AsyncWebServerRequest *request);
void handlePostSetWifiConfig(AsyncWebServerRequest *request);
void handlePostSwitchWifi(AsyncWebServerRequest *request);
void handlePostTriggerSzOta(AsyncWebServerRequest *request);

#endif
