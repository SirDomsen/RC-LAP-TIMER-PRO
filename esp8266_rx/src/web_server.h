#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"
#include <DNSServer.h>

extern DNSServer dnsServer;

void initNetworkAndServer();
void handleNetworkTasks();
void broadcastWebSocket(const String& msg);
void scheduleESPReset(uint16_t delayMs = 500);

#endif