#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Initialisiert den Speaker-Pin und sorgt für sauberen Ruhezustand (LOW)
void initBuzzer();

// Non-blocking State-Machine für den loop()
void updateBuzzer();

// Startampel nach Grand Prix Circuit: 2x Rot (260 Hz) -> Grün (1050 Hz)
void playBuzzerStartLights();

// Kurzer Bestätigungston bei Zieldurchfahrt
void playBuzzerLap();

// Retro-Arcade Fanfare bei neuem Streckenrekord / Bestzeit
void playBuzzerRecord();

// Melodien-Tests
void playBuzzerGrandPrix();
void playBuzzerTestDrive();
void playBuzzerKart();

// Sofortiger Not-Aus / Ton stoppen
void stopBuzzer();

// Globaler Schalter (kann per API/Config aktiviert/deaktiviert werden)
extern bool buzzerEnabled;

#endif
