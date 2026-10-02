#ifndef IR_DECODER_H
#define IR_DECODER_H

#include "config.h"

extern volatile bool newCodeAvailable;
extern volatile uint32_t receivedCode;
extern volatile uint32_t irTriggerTimeMicros;

void initIRDecoder();
void IRAM_ATTR handleIRInterrupt();

#endif