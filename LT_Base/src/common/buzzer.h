#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Initialisiert den Speaker-Pin und sorgt für sauberen Ruhezustand (LOW)
void initBuzzer();

// Non-blocking State-Machine für den loop()
void updateBuzzer();

// Startampel nach Grand Prix Circuit: 2x Rot (260 Hz) -> Grün (1050 Hz)
void playBuzzerStartLights();
void scheduleBuzzerStartLights(uint32_t delayMs = 6000);
void cancelBuzzerSchedule();

// Kurzer Bestätigungston bei Zieldurchfahrt
void playBuzzerLap();

// Retro-Arcade Fanfare bei neuem Streckenrekord / Bestzeit
void playBuzzerRecord();

// Geisterjagd Audio-Feedback (Modus 5)
void playBuzzerGhostVictory(); // Fanfare bei geschlagenem Geist ("da - da - da --- dadaaaaa!")
void playBuzzerGhostAhead();   // Grüner Sektor (Vorsprung) - Schneller Doppel-Beep hoch
void playBuzzerGhostBehind();  // Roter Sektor (Rückstand) - Trockener Bass-Beep tief

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
