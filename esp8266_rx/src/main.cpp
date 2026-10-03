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

  // Periodische Kontrolle des freien Arbeitsspeichers (Heap) zur Diagnose von Speicherlecks
  static uint32_t lastHeapCheck = 0;
  if (millis() - lastHeapCheck > 30000) { // Alle 30 Sekunden
    lastHeapCheck = millis();
    Serial.printf("[SYSTEM] Freier Heap: %u Bytes | Fragmentierung: %u%%\n", 
                  ESP.getFreeHeap(), ESP.getHeapFragmentation());
  }
}