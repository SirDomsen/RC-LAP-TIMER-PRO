#include "ir_decoder.h"

#include "ir_protocol.h"

#if defined(ESP32)
  #include <soc/gpio_struct.h>
#endif

volatile uint32_t irBuffer = 0;
volatile uint8_t bitIndex = 0;
volatile uint32_t lastEdgeTime = 0;
volatile bool newCodeAvailable = false;
volatile uint32_t receivedCode = 0;
volatile uint32_t irTriggerTimeMicros = 0;

namespace {
uint8_t signalLedPin = 0;

void IRAM_ATTR handleIRInterrupt() {
  if (!IrDecoder::processEdge(micros())) {
    return;
  }

#if defined(ESP8266)
  GPOS = (1 << signalLedPin);
#elif defined(ESP32)
  if (signalLedPin < 32) {
    GPIO.out_w1ts = (1UL << signalLedPin);
  } else {
    GPIO.out1_w1ts.val = (1UL << (signalLedPin - 32));
  }
#else
  digitalWrite(signalLedPin, HIGH);
#endif
}
}

void initIRDecoder(uint8_t irPin, uint8_t signalPin) {
  signalLedPin = signalPin;
  pinMode(irPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(irPin), handleIRInterrupt, FALLING);
}

bool IRAM_ATTR IrDecoder::processEdge(uint32_t nowMicros) {
  const uint32_t duration = nowMicros - lastEdgeTime;
  lastEdgeTime = nowMicros;

  if (duration >= IrProtocol::kHeaderMinUs && duration <= IrProtocol::kHeaderMaxUs) {
    bitIndex = 0;
    irBuffer = 0;
    irTriggerTimeMicros = nowMicros;
    return false;
  }

  if (duration >= IrProtocol::kBitMinUs && duration <= IrProtocol::kBitMaxUs) {
    irBuffer <<= 1;
    if (duration > IrProtocol::kBitThresholdUs) {
      irBuffer |= 1;
    }
    bitIndex++;

    if (bitIndex == IrProtocol::kFrameBits) {
      receivedCode = irBuffer;
      newCodeAvailable = true;
      bitIndex = 0;
      return true;
    }
    return false;
  }

  bitIndex = 0;
  irBuffer = 0;
  return false;
}
