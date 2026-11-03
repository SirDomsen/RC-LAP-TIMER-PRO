#ifndef LT_IR_DECODER_H
#define LT_IR_DECODER_H

#include <Arduino.h>

extern volatile bool newCodeAvailable;
extern volatile uint32_t receivedCode;
extern volatile uint32_t irTriggerTimeMicros;

void initIRDecoder(uint8_t irPin, uint8_t signalLedPin);

namespace IrDecoder {
bool IRAM_ATTR processEdge(uint32_t nowMicros);
}

#endif
