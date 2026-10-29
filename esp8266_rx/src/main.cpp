#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "ir_decoder.h"
#include "lap_logic.h"
#include "web_server.h"
#include "esp_now_base.h"
#include "buzzer.h"
#include <ESP8266mDNS.h>
#include <espnow.h>

// Globale Performance-Metriken
uint32_t perfLoopCount = 0;
uint32_t perfMaxLoopUs = 0;
uint32_t perfAvgLoopUs = 0;

void setup() {
  // CPU-Takt auf 160 MHz setzen für maximale Rechenleistung
  system_update_cpu_freq(160);

  Serial.begin(115200);
  delay(100);

  initFS();
  loadSettingsFromEEPROM();
  initIRDecoder();
  initLapLogic();
  initNetworkAndServer();
  initESPNowBase();
  initBuzzer();

  Serial.println();
  Serial.println(F("========================================="));
  Serial.println(F("   RC LAP TIMER PRO - v4.4.0"));
  Serial.println(F("   Arcade Sound Test-Modus aktiv!"));
  Serial.println(F("   Tasten im Terminal:"));
  Serial.println(F("   [1] Startampel (2-1-GO)"));
  Serial.println(F("   [2] Zieldurchfahrt (Lap-Tick)"));
  Serial.println(F("   [3] Streckenrekord Fanfare"));
  Serial.println(F("   [4] Grand Prix Circuit (1988)"));
  Serial.println(F("   [5] Test Drive 1 (1987)"));
  Serial.println(F("   [6] Retro Victory Fanfare"));
  Serial.println(F("   [s] Sound Stoppen"));
  Serial.println(F("========================================="));
  Serial.println();
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

  // Serielles Test-Interface für D1-Mini / Desktop-Testing
  if (Serial.available()) {
    char c = Serial.read();
    if (c != '\r' && c != '\n') {
      if (c == '1') {
        Serial.println(F("[BUZZER] ▶ Startampel (2-1-GO)"));
        playBuzzerStartLights();
      } else if (c == '2') {
        Serial.println(F("[BUZZER] ▶ Lap Tick"));
        playBuzzerLap();
      } else if (c == '3') {
        Serial.println(F("[BUZZER] ▶ Bestzeit Fanfare"));
        playBuzzerRecord();
      } else if (c == '4') {
        Serial.println(F("[BUZZER] ▶ Grand Prix Circuit Theme"));
        playBuzzerGrandPrix();
      } else if (c == '5') {
        Serial.println(F("[BUZZER] ▶ Test Drive 1 Theme"));
        playBuzzerTestDrive();
      } else if (c == '6') {
        Serial.println(F("[BUZZER] ▶ Retro Victory Fanfare"));
        playBuzzerKart();
      } else if (c == 's') {
        Serial.println(F("[BUZZER] ⏹ Sound gestoppt"));
        stopBuzzer();
      }
    }
  }

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
    Serial.printf("[SYSTEM] Freier Heap: %u Bytes | Fragmentierung: %u%% | Max Loop: %u us\n", 
                  ESP.getFreeHeap(), ESP.getHeapFragmentation(), perfMaxLoopUs);
  }
}
