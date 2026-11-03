#include <Arduino.h>
#include "config.h"
#include <buzzer_output.h>

void BuzzerOutput::init() {
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
}

void BuzzerOutput::startTone(uint16_t frequency) {
  tone(SPEAKER_PIN, frequency);
}

void BuzzerOutput::stopTone(bool wasActive) {
  if (wasActive) {
    noTone(SPEAKER_PIN);
  }
  digitalWrite(SPEAKER_PIN, LOW);
}
