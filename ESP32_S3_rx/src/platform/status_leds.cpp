#include <Arduino.h>
#include "../../../esp8266_rx/src/config.h"
#include <status_leds.h>

namespace {
uint32_t readyLedOffTime = 0;
uint32_t lastYellowFlash = 0;
bool readyLedBlinking = false;
}

void LapStatusLeds::init() {
  pinMode(LED_READY, OUTPUT);
  pinMode(LED_SIGNAL, OUTPUT);
  pinMode(LED_ONBOARD, OUTPUT);

  digitalWrite(LED_READY, LOW);
  digitalWrite(LED_SIGNAL, LOW);
  digitalWrite(LED_ONBOARD, HIGH);
}

void LapStatusLeds::pulseReady() {
  digitalWrite(LED_READY, LOW);
  readyLedBlinking = true;
  readyLedOffTime = millis() + 200;
}

void LapStatusLeds::cancelReadyPulse() {
  readyLedBlinking = false;
}

void LapStatusLeds::setNetworkReady(bool ready) {
  digitalWrite(LED_READY, ready ? HIGH : LOW);
}

void LapStatusLeds::update() {
  if (digitalRead(LED_SIGNAL) == HIGH) {
    if (millis() - lastYellowFlash > 12) {
      digitalWrite(LED_SIGNAL, LOW);
      lastYellowFlash = millis();
    }
  } else {
    lastYellowFlash = millis();
  }

  if (readyLedBlinking && millis() >= readyLedOffTime) {
    digitalWrite(LED_READY, HIGH);
    readyLedBlinking = false;
  }
}
