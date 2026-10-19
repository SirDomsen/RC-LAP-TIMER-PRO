#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
extern AsyncWebSocket ws;

void initNetworkAndServer();
void handleNetworkTasks();
void broadcastWebSocket(const String& msg);
void scheduleESPReset(uint16_t delayMs = 500);

// Netzwerk-Wechsel auslösen
void triggerWifiSwitch(uint8_t mode);

#endif