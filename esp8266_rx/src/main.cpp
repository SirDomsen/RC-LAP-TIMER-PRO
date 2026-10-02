#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "ir_decoder.h"
#include "lap_logic.h"
#include "web_server.h"
#include <ESP8266mDNS.h>

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.print("[CPU] Taktfrequenz: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");

  initFS();
  loadSettingsFromEEPROM();
  initIRDecoder();
  initLapLogic();
  initNetworkAndServer();

  
}

void loop() {
  processLapLogic();
  updateLapLogicTasks();
  processStorageQueue(); // Verarbeitet gepufferte Runden für die Flash-Speicherung
  updateLEDs();
  handleNetworkTasks();
}