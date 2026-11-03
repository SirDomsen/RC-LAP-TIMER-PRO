#ifndef LAP_BUZZER_OUTPUT_H
#define LAP_BUZZER_OUTPUT_H

#include <Arduino.h>

namespace BuzzerOutput {

void init();
void startTone(uint16_t frequency);
void stopTone(bool wasActive);

}  // namespace BuzzerOutput

#endif
