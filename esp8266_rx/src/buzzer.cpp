#include "buzzer.h"
#include "config.h"

// Aktivierungsstatus (Standard: aktiv)
bool buzzerEnabled = true;

struct ToneStep {
  uint16_t freq;      // Frequenz in Hz (0 = Pause)
  uint16_t duration;  // Dauer in Millisekunden
};

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

// 4. Grand Prix Circuit - Lead Synth Hook (Kris Hatlelid, A-Moll)
static const ToneStep gpcThemeSeq[] PROGMEM = {
  { 440,  95 }, { 0, 15 }, // A4
  { 440,  95 }, { 0, 15 }, // A4
  { 523,  95 }, { 0, 15 }, // C5
  { 587,  95 }, { 0, 15 }, // D5
  { 659, 190 }, { 0, 30 }, // E5
  { 587,  95 }, { 0, 15 }, // D5
  { 523,  95 }, { 0, 15 }, // C5
  { 440, 220 }, { 0, 50 }, // A4
  { 392,  95 }, { 0, 15 }, // G4
  { 440,  95 }, { 0, 15 }, // A4
  { 523, 190 }, { 0, 30 }, // C5
  { 587, 190 }, { 0, 30 }, // D5
  { 659, 350 }, { 0, 50 }, // E5
  // Schneller Arpeggio-Lauf (typischer Hatlelid-Synthlauf)
  { 440,  40 }, { 523, 40 }, { 659, 40 }, { 880, 120 },
  { 0,    50 }
};

// 5. Test Drive 1 - Blues Beeper Riff (Patrick Payne, E-Moll)
static const ToneStep tdThemeSeq[] PROGMEM = {
  { 165,  90 }, { 0, 20 }, // E3
  { 165,  90 }, { 0, 20 }, // E3
  { 196, 110 }, { 0, 20 }, // G3
  { 165,  90 }, { 0, 20 }, // E3
  { 220, 110 }, { 0, 20 }, // A3
  { 233,  90 }, { 0, 15 }, // Bb3
  { 247, 150 }, { 0, 30 }, // B3
  { 294, 110 }, { 0, 20 }, // D4
  { 330, 250 }, { 0, 60 }, // E4
  { 165,  90 }, { 0, 20 }, // E3
  { 294, 110 }, { 0, 20 }, // D4
  { 247, 180 }, { 0, 40 }, // B3
  { 196, 250 }, { 0, 50 }  // G3
};

// 6. Retro Victory Fanfare (Arcade Klassiker)
static const ToneStep kartThemeSeq[] PROGMEM = {
  { 523, 100 }, { 0, 20 },
  { 523, 100 }, { 0, 20 },
  { 523, 100 }, { 0, 20 },
  { 523, 300 }, { 0, 40 },
  { 415, 300 }, { 0, 30 }, // Ab4
  { 466, 300 }, { 0, 30 }, // Bb4
  { 523, 200 }, { 0, 20 }, // C5
  { 466, 100 }, { 0, 20 }, // Bb4
  { 523, 600 }, { 0, 50 }  // C5
};

// Status-Variablen der State-Machine (rein statischer RAM, 0 Byte Heap)
static const ToneStep* currentSeq = nullptr;
static uint8_t currentSeqLen = 0;
static uint8_t currentStepIdx = 0;
static uint32_t stepStartMs = 0;
static bool isBuzzerPlaying = false;

void initBuzzer() {
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
  noTone(SPEAKER_PIN);
  isBuzzerPlaying = false;
  currentSeq = nullptr;
}

static void startSequence(const ToneStep* seq, uint8_t length) {
  if (!buzzerEnabled || seq == nullptr || length == 0) return;

  currentSeq = seq;
  currentSeqLen = length;
  currentStepIdx = 0;
  isBuzzerPlaying = true;
  stepStartMs = millis();

  uint16_t freq = pgm_read_word(&(currentSeq[0].freq));
  if (freq > 0) {
    tone(SPEAKER_PIN, freq);
  } else {
    noTone(SPEAKER_PIN);
    digitalWrite(SPEAKER_PIN, LOW);
  }
}

void playBuzzerStartLights() {
  startSequence(startLightsSeq, sizeof(startLightsSeq) / sizeof(ToneStep));
}

void playBuzzerLap() {
  if (isBuzzerPlaying && currentSeq == startLightsSeq) return;
  startSequence(lapSeq, sizeof(lapSeq) / sizeof(ToneStep));
}

void playBuzzerRecord() {
  startSequence(recordSeq, sizeof(recordSeq) / sizeof(ToneStep));
}

void playBuzzerGrandPrix() {
  startSequence(gpcThemeSeq, sizeof(gpcThemeSeq) / sizeof(ToneStep));
}

void playBuzzerTestDrive() {
  startSequence(tdThemeSeq, sizeof(tdThemeSeq) / sizeof(ToneStep));
}

void playBuzzerKart() {
  startSequence(kartThemeSeq, sizeof(kartThemeSeq) / sizeof(ToneStep));
}

void stopBuzzer() {
  isBuzzerPlaying = false;
  currentSeq = nullptr;
  currentSeqLen = 0;
  currentStepIdx = 0;
  noTone(SPEAKER_PIN);
  digitalWrite(SPEAKER_PIN, LOW);
}

void updateBuzzer() {
  if (!isBuzzerPlaying || currentSeq == nullptr) return;

  uint32_t now = millis();
  uint16_t stepDur = pgm_read_word(&(currentSeq[currentStepIdx].duration));

  if (now - stepStartMs >= stepDur) {
    currentStepIdx++;
    if (currentStepIdx >= currentSeqLen) {
      stopBuzzer();
      return;
    }

    stepStartMs = now;
    uint16_t nextFreq = pgm_read_word(&(currentSeq[currentStepIdx].freq));
    if (buzzerEnabled && nextFreq > 0) {
      tone(SPEAKER_PIN, nextFreq);
    } else {
      noTone(SPEAKER_PIN);
      digitalWrite(SPEAKER_PIN, LOW);
    }
  }
}
