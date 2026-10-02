#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <ESPAsyncWebServer.h>

void handleGetLaps(AsyncWebServerRequest *request);
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

// Zeit-Synchronisierung, Session-Historie & Statistik
void handlePostSyncTime(AsyncWebServerRequest *request);
void handleGetStats(AsyncWebServerRequest *request);
void handlePostDeleteLap(AsyncWebServerRequest *request);
void handlePostDeleteDriverStats(AsyncWebServerRequest *request);
void handlePostDeleteAllStats(AsyncWebServerRequest *request);
void handleGetSessions(AsyncWebServerRequest *request);
void handlePostSaveSessionResult(AsyncWebServerRequest *request);
void handlePostDeleteSession(AsyncWebServerRequest *request);
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

#endif