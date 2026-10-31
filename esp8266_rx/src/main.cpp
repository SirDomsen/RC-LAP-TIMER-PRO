#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "ir_decoder.h"
#include "lap_logic.h"
#include "web_server.h"
#include "esp_now_base.h"
#include "buzzer.h"

#if defined(ESP8266)
#include <ESP8266mDNS.h>
#include <espnow.h>
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <esp_now.h>
#endif

// Globale Performance-Metriken
uint32_t perfLoopCount = 0;
uint32_t perfMaxLoopUs = 0;
uint32_t perfAvgLoopUs = 0;

void setup() {
#if defined(ESP8266)
  // CPU-Takt auf 160 MHz setzen für maximale Rechenleistung
  system_update_cpu_freq(160);
#elif defined(ESP32)
  setCpuFrequencyMhz(240);
#endif

  Serial.begin(115200);
#if defined(ESP32)
  uint32_t cdcWait = millis();
  while (!Serial && (millis() - cdcWait < 1500)) {
    delay(10);
  }
#else
  delay(100);
#endif

  initFS();
  loadSettingsFromEEPROM();
  initIRDecoder();
  initLapLogic();
  initNetworkAndServer();
  initESPNowBase();
  initBuzzer();
}

void loop() {
  uint32_t loopStart = micros();

  processLapLogic();
  updateLapLogicTasks();
  processStorageQueue();
  updateLEDs();
  updateBuzzer();
  handleNetworkTasks();
  handleTimeSyncBroadcast();

  uint32_t loopDuration = micros() - loopStart;
  if (loopDuration > perfMaxLoopUs) {
    perfMaxLoopUs = loopDuration;
  }
  perfLoopCount++;

  // Jede Sekunde Performance-Durchschnitt berechnen & zurücksetzen
  static uint32_t lastPerfCalc = 0;
  if (millis() - lastPerfCalc >= 1000) {
    perfAvgLoopUs = (perfLoopCount > 0) ? (1000000UL / perfLoopCount) : 0;
    lastPerfCalc = millis();
    perfLoopCount = 0;
    // Max-Wert nach dem Auslesen sanft abklingen lassen
    static uint8_t decay = 0;
    if (++decay >= 3) {
      perfMaxLoopUs = loopDuration;
      decay = 0;
    }
  }

  static uint32_t lastHeapCheck = 0;
  if (millis() - lastHeapCheck > 30000) {
    lastHeapCheck = millis();
#if defined(ESP8266)
    Serial.printf("[SYSTEM] Freier Heap: %u Bytes | Fragmentierung: %u%% | Max Loop: %u us\n", 
                  ESP.getFreeHeap(), ESP.getHeapFragmentation(), perfMaxLoopUs);
#elif defined(ESP32)
    Serial.printf("[SYSTEM] Freier Heap: %u Bytes | PSRAM: %u Bytes | Max Loop: %u us\n", 
                  ESP.getFreeHeap(), ESP.getFreePsram(), perfMaxLoopUs);
#endif
  }
}
