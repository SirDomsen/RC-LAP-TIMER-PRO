#include "ir_decoder.h"

volatile uint32_t irBuffer = 0;
volatile uint8_t  bitIndex = 0;
volatile uint32_t lastEdgeTime = 0;
volatile bool     newCodeAvailable = false;
volatile uint32_t receivedCode = 0;
volatile uint32_t irTriggerTimeMicros = 0;

void initIRDecoder() {
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_PIN), handleIRInterrupt, FALLING);
}

void IRAM_ATTR handleIRInterrupt() {
  uint32_t now = micros();
  uint32_t duration = now - lastEdgeTime;
  lastEdgeTime = now;

  // 1. Leader Erkennung (~3000 µs: 2000 µs Puls + 1000 µs Pause)
  // Breites Fenster für den ATtiny, aber strikter Schnitt
  if (duration >= 1800 && duration <= 4200) {
    bitIndex = 0;
    irBuffer = 0;
    irTriggerTimeMicros = now;
    return;
  }

  // 2. Bit 0 (~700 µs) und Bit 1 (~1400 µs) Erkennung
  if (duration >= 400 && duration <= 1800) {
    irBuffer <<= 1;
    if (duration > 950) {
      irBuffer |= 1;
    }
    bitIndex++;

    if (bitIndex == 12) {
      receivedCode = irBuffer;
      newCodeAvailable = true;
      bitIndex = 0;

      // Gelbe LED sofort schalten
      GPOS = (1 << LED_SIGNAL);
    }
    return;
  }

  // 3. Wenn die Flanke WEDER ein Header NOCH ein gültiges Bit war:
  // Sofortiger, kompromissloser Hard Reset!
  bitIndex = 0;
  irBuffer = 0;
}