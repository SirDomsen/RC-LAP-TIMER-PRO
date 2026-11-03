#include "lap_ir_input.h"

#include <ir_decoder.h>

bool LapIrInput::consumeFrame(
    uint8_t maxCarId,
    uint8_t& carId,
    uint32_t& signalTimeMicros,
    bool& confirmed) {
  return IrDecoder::consumeFrame(maxCarId, carId, signalTimeMicros, confirmed);
}

void LapIrInput::resetFrameFilter() {
  IrDecoder::resetFrameFilter();
}
