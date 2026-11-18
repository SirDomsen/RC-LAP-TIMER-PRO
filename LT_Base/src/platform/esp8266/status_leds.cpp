#include <Arduino.h>
#include "config.h"
#include "status_leds.h"

namespace {
uint32_t readyLedOffTime = 0;
uint32_t lastYellowFlash = 0;
bool readyLedBlinking = false;

void setReadyLeds(bool on) {
#ifdef LED_READY
  digitalWrite(LED_READY, on ? HIGH : LOW);
#endif
  digitalWrite(LED_ONBOARD, on ? LOW : HIGH);  // Active LOW
}
}

void LapStatusLeds::init() {
#ifdef LED_READY
  pinMode(LED_READY, OUTPUT);
#endif
#if defined(LED_SIGNAL) && (LED_SIGNAL != 0xFF)
  pinMode(LED_SIGNAL, OUTPUT);
  digitalWrite(LED_SIGNAL, LOW);
#endif
  pinMode(LED_ONBOARD, OUTPUT);

  setReadyLeds(false);
}

void LapStatusLeds::pulseReady() {
  setReadyLeds(false);
  readyLedBlinking = true;
  readyLedOffTime = millis() + 200;
}

void LapStatusLeds::cancelReadyPulse() {
  readyLedBlinking = false;
}

void LapStatusLeds::setNetworkReady(bool ready) {
  setReadyLeds(ready);
}

void LapStatusLeds::update() {
#if defined(LED_SIGNAL) && (LED_SIGNAL != 0xFF)
  if (digitalRead(LED_SIGNAL) == HIGH) {
    if (millis() - lastYellowFlash > 12) {
      digitalWrite(LED_SIGNAL, LOW);
      lastYellowFlash = millis();
    }
  } else {
    lastYellowFlash = millis();
  }
#endif

  if (readyLedBlinking && millis() >= readyLedOffTime) {
    setReadyLeds(true);
    readyLedBlinking = false;
  }
}

bool LapStatusLeds::hasRgbLed() {
  return false;
}

LapStatusLeds::RgbLedConfig LapStatusLeds::getRgbConfig() {
  return RgbLedConfig{};
}

bool LapStatusLeds::setRgbConfig(const RgbLedConfig&, bool) {
  return false;
}

void LapStatusLeds::previewRgbColor(uint32_t, uint8_t, uint16_t) {}

void LapStatusLeds::triggerSignalFlash() {}
