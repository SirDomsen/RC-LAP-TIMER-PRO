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

// Accolade Arcade Audio Jukebox: Spielt einen Track per ID ab
bool playBuzzerTrack(const String& trackId, bool loop = false);

// Accolade Grand Prix Circuit Themes (Kompatibilität)
void playBuzzerTitle();
void playBuzzerMenu();

// Sofortiger Not-Aus / Ton stoppen
void stopBuzzer();

// Statusabfragen
bool isBuzzerActive();
String getBuzzerCurrentTrack();

// Globaler Schalter (kann per API/Config aktiviert/deaktiviert werden)
extern bool buzzerEnabled;

#endif
