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

// Accolade Grand Prix Circuit - Title Theme (Original MIDI)
static const ToneStep gpcTitleSeq[172] PROGMEM = {
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 185, 200 },
    { 277, 200 },
    { 370, 200 },
    { 185, 200 },
    { 330, 200 },
    { 185, 100 },
    { 311, 200 },
    { 185, 100 },
    { 277, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 494, 200 },
    { 740, 200 },
    { 988, 200 },
    { 494, 200 },
    { 880, 200 },
    { 494, 100 },
    { 831, 200 },
    { 494, 100 },
    { 740, 200 },
    { 494, 200 },
    { 740, 200 },
    { 988, 200 },
    { 494, 200 },
    { 880, 200 },
    { 494, 100 },
    { 831, 200 },
    { 494, 100 },
    { 740, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 277, 200 },
    { 415, 200 },
    { 554, 200 },
    { 277, 200 },
    { 494, 200 },
    { 277, 100 },
    { 466, 200 },
    { 277, 100 },
    { 415, 200 },
    { 494, 200 },
    { 740, 200 },
    { 988, 200 },
    { 494, 200 },
    { 880, 200 },
    { 494, 100 },
    { 831, 200 },
    { 494, 100 },
    { 740, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 },
    { 554, 200 },
    { 740, 200 },
    { 370, 200 },
    { 659, 200 },
    { 370, 100 },
    { 622, 200 },
    { 370, 100 },
    { 554, 200 },
    { 370, 200 }
};

// Accolade Grand Prix Circuit - Menu Theme (Original MIDI)
static const ToneStep gpcMenuSeq[191] PROGMEM = {
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 },
    { 87, 120 },
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 },
    { 87, 120 },
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 220, 120 },
    { 110, 120 },
    { 262, 120 },
    { 110, 120 },
    { 330, 120 },
    { 110, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 294, 120 },
    { 147, 120 },
    { 349, 120 },
    { 147, 120 },
    { 440, 120 },
    { 147, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 196, 120 },
    { 98, 120 },
    { 262, 120 },
    { 98, 120 },
    { 294, 120 },
    { 98, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 175, 120 },
    { 87, 120 },
    { 233, 120 },
    { 87, 120 },
    { 262, 120 }
};


// Status-Variablen für Ton-Sequenzen
static const ToneStep* currentSeq = nullptr;
static uint16_t currentSeqLen = 0;
static uint16_t currentStepIdx = 0;
static uint32_t stepStartMs = 0;
static bool isBuzzerPlaying = false;
static bool isLooping = false;
static bool seqInGap = false;

void initBuzzer() {
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);
  noTone(SPEAKER_PIN);
  isBuzzerPlaying = false;
  isLooping = false;
  seqInGap = false;
  currentSeq = nullptr;
  currentSeqLen = 0;
  currentStepIdx = 0;
}

static void startSequence(const ToneStep* seq, uint16_t length, bool loop = false) {
  if (!buzzerEnabled || seq == nullptr || length == 0) return;

  stopBuzzer();
  currentSeq = seq;
  currentSeqLen = length;
  currentStepIdx = 0;
  isBuzzerPlaying = true;
  isLooping = loop;
  seqInGap = false;
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
  startSequence(startLightsSeq, sizeof(startLightsSeq) / sizeof(ToneStep), false);
}

void playBuzzerLap() {
  if (isBuzzerPlaying && currentSeq == startLightsSeq) return;
  startSequence(lapSeq, sizeof(lapSeq) / sizeof(ToneStep), false);
}

void playBuzzerRecord() {
  startSequence(recordSeq, sizeof(recordSeq) / sizeof(ToneStep), false);
}

void playBuzzerTitle() {
  startSequence(gpcTitleSeq, sizeof(gpcTitleSeq) / sizeof(ToneStep), true);
}

void playBuzzerMenu() {
  startSequence(gpcMenuSeq, sizeof(gpcMenuSeq) / sizeof(ToneStep), true);
}

void stopBuzzer() {
  isBuzzerPlaying = false;
  isLooping = false;
  seqInGap = false;
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
  uint16_t stepFreq = pgm_read_word(&(currentSeq[currentStepIdx].freq));

  // Artikulation: Stakkato-Trennlücke vor dem nächsten Schritt
  if (!seqInGap) {
    uint32_t toneMs = stepDur;
    if (stepFreq > 0) {
      if (stepDur >= 50) {
        toneMs = (stepDur * 85) / 100;
      } else if (stepDur > 12) {
        toneMs = stepDur - 8;
      }
    }
    if (now - stepStartMs >= toneMs) {
      noTone(SPEAKER_PIN);
      digitalWrite(SPEAKER_PIN, LOW);
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
      tone(SPEAKER_PIN, nextFreq);
    } else {
      noTone(SPEAKER_PIN);
      digitalWrite(SPEAKER_PIN, LOW);
    }
  }
}
