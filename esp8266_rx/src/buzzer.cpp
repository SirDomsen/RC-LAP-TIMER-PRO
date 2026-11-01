#include "buzzer.h"
#include "config.h"
#include "../Sound/soundtracks.h"

// Aktivierungsstatus (Standard: aktiv)
bool buzzerEnabled = true;

// 1. Startampel (Grand Prix Circuit: 2-1-GO!)
static const ToneStep startLightsSeq[] PROGMEM = {
  { 260,  300 }, // Rot 1 (2)
  { 0,    650 }, // Pause
  { 260,  300 }, // Rot 2 (1)
  { 0,    650 }, // Pause
  { 1050, 450 }, // GRÜN (GO!)
  { 0,     50 }  // Ausklang
};

// 2. Zieldurchfahrt (Knackiger, kurzer Impuls)
static const ToneStep lapSeq[] PROGMEM = {
  { 1760, 35 },
  { 0,    10 }
};

// 3. Neuer Streckenrekord / Bestzeit (Aufsteigende Arcade-Fanfare)
static const ToneStep recordSeq[] PROGMEM = {
  { 523,   65 }, // C5
  { 0,     15 },
  { 659,   65 }, // E5
  { 0,     15 },
  { 784,   65 }, // G5
  { 0,     15 },
  { 1046, 180 }, // C6
  { 0,     50 }
};

// 4. Geisterjagd: Siegesfanfare bei geschlagenem Geist ("da - da - da --- dadaaaaa!")
static const ToneStep ghostVictorySeq[] PROGMEM = {
  { 523,   65 }, // C5 (kurz stakkato)
  { 0,     30 },
  { 659,   65 }, // E5 (kurz stakkato)
  { 0,     30 },
  { 784,   65 }, // G5 (kurz stakkato)
  { 0,     90 }, // dramatische Pause ("da - da - da ---")
  { 784,   75 }, // G5 (Auftakt "da-")
  { 0,     20 },
  { 1046, 480 }, // C6 (langer Schlusston "daaaaa!")
  { 0,     50 }
};

// 5. Geisterjagd: Sektor Grün (Vorsprung - schneller Doppel-Beep hoch)
static const ToneStep ghostAheadSeq[] PROGMEM = {
  { 1760,  40 }, // A6
  { 0,     25 },
  { 2349,  50 }, // D7
  { 0,     10 }
};

// 6. Geisterjagd: Sektor Rot (Rückstand - trockener Einzel-Beep tief)
static const ToneStep ghostBehindSeq[] PROGMEM = {
  { 440,   60 }, // A4
  { 0,     10 }
};

// Status-Variablen für Ton-Sequenzen
static const ToneStep* currentSeq = nullptr;
static uint16_t currentSeqLen = 0;
static uint16_t currentStepIdx = 0;
static uint32_t stepStartMs = 0;
static bool isBuzzerPlaying = false;
static bool isLooping = false;
static bool seqInGap = false;
static bool currentIsExtracted = false;
static bool hwToneActive = false;
static uint32_t scheduledStartLightsAtMs = 0;
static String currentTrackName = "";

static inline void soundOn(uint16_t freq) {
  if (freq > 0) {
    tone(SPEAKER_PIN, freq);
    hwToneActive = true;
  }
}

static inline void soundOff() {
  if (hwToneActive) {
    noTone(SPEAKER_PIN);
    hwToneActive = false;
  }
  digitalWrite(SPEAKER_PIN, LOW);
}

void initBuzzer() {
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
  hwToneActive = false;
  isBuzzerPlaying = false;
  isLooping = false;
  seqInGap = false;
  currentIsExtracted = false;
  currentSeq = nullptr;
  currentSeqLen = 0;
  currentStepIdx = 0;
  scheduledStartLightsAtMs = 0;
  currentTrackName = "";
}

static void startSequence(const ToneStep* seq, uint16_t length, bool loop = false, bool isExtracted = false, const String& name = "") {
  if (!buzzerEnabled || seq == nullptr || length == 0) return;

  stopBuzzer();
  currentSeq = seq;
  currentSeqLen = length;
  currentStepIdx = 0;
  isBuzzerPlaying = true;
  isLooping = loop;
  seqInGap = false;
  currentIsExtracted = isExtracted;
  currentTrackName = name;
  stepStartMs = millis();

  uint16_t freq = pgm_read_word(&(currentSeq[0].freq));
  if (freq > 0) {
    soundOn(freq);
  } else {
    soundOff();
  }
}

void playBuzzerStartLights() {
  startSequence(startLightsSeq, sizeof(startLightsSeq) / sizeof(ToneStep), false, false, "start_lights");
}

void scheduleBuzzerStartLights(uint32_t delayMs) {
  scheduledStartLightsAtMs = millis() + delayMs;
}

void cancelBuzzerSchedule() {
  scheduledStartLightsAtMs = 0;
}

void playBuzzerLap() {
  if (isBuzzerPlaying && currentSeq == startLightsSeq) return;
  startSequence(lapSeq, sizeof(lapSeq) / sizeof(ToneStep), false, false, "lap");
}

void playBuzzerRecord() {
  startSequence(recordSeq, sizeof(recordSeq) / sizeof(ToneStep), false, false, "record");
}

void playBuzzerGhostVictory() {
  startSequence(ghostVictorySeq, sizeof(ghostVictorySeq) / sizeof(ToneStep), false, false, "ghost_win");
}

void playBuzzerGhostAhead() {
  if (isBuzzerPlaying && (currentSeq == startLightsSeq || currentSeq == ghostVictorySeq)) return;
  startSequence(ghostAheadSeq, sizeof(ghostAheadSeq) / sizeof(ToneStep), false, false, "ghost_ahead");
}

void playBuzzerGhostBehind() {
  if (isBuzzerPlaying && (currentSeq == startLightsSeq || currentSeq == ghostVictorySeq)) return;
  startSequence(ghostBehindSeq, sizeof(ghostBehindSeq) / sizeof(ToneStep), false, false, "ghost_behind");
}

bool playBuzzerTrack(const String& trackId, bool loop) {
  const ArcadeTrack* t = findArcadeTrack(trackId);
  if (!t) return false;
  startSequence(t->seq, t->len, loop, t->isExtracted, t->id);
  return true;
}

void playBuzzerTitle() {
  playBuzzerTrack("gpc_title", true);
}

void playBuzzerMenu() {
  playBuzzerTrack("gpc_menu", true);
}

void stopBuzzer() {
  scheduledStartLightsAtMs = 0;
  isBuzzerPlaying = false;
  isLooping = false;
  seqInGap = false;
  currentIsExtracted = false;
  currentSeq = nullptr;
  currentSeqLen = 0;
  currentStepIdx = 0;
  currentTrackName = "";
  soundOff();
}

bool isBuzzerActive() {
  return isBuzzerPlaying || (scheduledStartLightsAtMs > 0);
}

String getBuzzerCurrentTrack() {
  return currentTrackName;
}

void updateBuzzer() {
  if (scheduledStartLightsAtMs > 0 && millis() >= scheduledStartLightsAtMs) {
    scheduledStartLightsAtMs = 0;
    playBuzzerStartLights();
  }

  if (!isBuzzerPlaying || currentSeq == nullptr) return;

  uint32_t now = millis();
  uint16_t stepDur = pgm_read_word(&(currentSeq[currentStepIdx].duration));
  uint16_t stepFreq = pgm_read_word(&(currentSeq[currentStepIdx].freq));

  // Artikulation: Stakkato-Trennlücke vor dem nächsten Schritt
  if (!seqInGap) {
    uint32_t toneMs = stepDur;
    if (stepFreq > 0) {
      if (currentIsExtracted) {
        // wav2c-extrahierte Tracks haben bereits explizite Pausen ({0, ms})
        // Nur bei längeren Einzeltönen (>=60ms) einen minimalen Schnitt von 6ms für sauberen Attack lassen
        if (stepDur >= 60) {
          toneMs = stepDur - 6;
        }
      } else {
        // Klassische Sequenzen ohne explizite Ruhetakte
        if (stepDur >= 50) {
          toneMs = (stepDur * 85) / 100;
        } else if (stepDur > 12) {
          toneMs = stepDur - 8;
        }
      }
    }
    if (now - stepStartMs >= toneMs) {
      soundOff();
      seqInGap = true;
    }
  }

  if (now - stepStartMs >= stepDur) {
    currentStepIdx++;
    seqInGap = false;
    if (currentStepIdx >= currentSeqLen) {
      if (isLooping) {
        currentStepIdx = 0; // Nahtloser Dauerloop
      } else {
        stopBuzzer();
        return;
      }
    }

    stepStartMs = now;
    uint16_t nextFreq = pgm_read_word(&(currentSeq[currentStepIdx].freq));
    if (buzzerEnabled && nextFreq > 0) {
      soundOn(nextFreq);
    } else {
      soundOff();
    }
  }
}
