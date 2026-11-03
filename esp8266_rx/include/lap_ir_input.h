#ifndef LAP_IR_INPUT_H
#define LAP_IR_INPUT_H

#include <Arduino.h>

namespace LapIrInput {

bool consumeFrame(uint8_t maxCarId, uint8_t& carId, uint32_t& signalTimeMicros, bool& confirmed);
void resetFrameFilter();

}  // namespace LapIrInput

#endif
