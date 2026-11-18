#include <Arduino.h>
#include "config.h"
#include <buzzer_output.h>

void BuzzerOutput::init() {
#ifdef SPEAKER_PIN
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
#endif
}

void BuzzerOutput::startTone(uint16_t frequency) {
#ifdef SPEAKER_PIN
  tone(SPEAKER_PIN, frequency);
#else
  (void)frequency;
#endif
}

void BuzzerOutput::stopTone(bool wasActive) {
#ifdef SPEAKER_PIN
  if (wasActive) {
    noTone(SPEAKER_PIN);
  }
  digitalWrite(SPEAKER_PIN, LOW);
#else
  (void)wasActive;
#endif
}
