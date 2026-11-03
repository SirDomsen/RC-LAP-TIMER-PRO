#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include <ir_decoder.h>
#include "lap_logic.h"
#include "web_server.h"
#include "esp_now_base.h"
#include "buzzer.h"
#include <status_leds.h>
#include <lap_ir_input.h>
#include <platform_runtime.h>

// Globale Performance-Metriken
uint32_t perfLoopCount = 0;
uint32_t perfMaxLoopUs = 0;
uint32_t perfAvgLoopUs = 0;

void setup() {
  PlatformRuntime::init();

  initFS();
  loadSettingsFromEEPROM();
  initIRDecoder(IR_PIN, LED_SIGNAL);
  initLapLogic();
  initNetworkAndServer();
  initESPNowBase();
  initBuzzer();
}

void loop() {
  uint32_t loopStart = micros();

  uint8_t carId = 0;
  uint32_t exactSignalTime = 0;
  bool confirmed = false;
  if (LapIrInput::consumeFrame(MAX_CARS, carId, exactSignalTime, confirmed)) {
    processLapInputFrame(carId, exactSignalTime, confirmed);
  }
  updateLapLogicTasks();
  processStorageQueue();
  LapStatusLeds::update();
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
    PlatformRuntime::logSystemMetrics(perfMaxLoopUs);
  }
}
