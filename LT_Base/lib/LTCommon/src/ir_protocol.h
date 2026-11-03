#ifndef LT_IR_PROTOCOL_H
#define LT_IR_PROTOCOL_H

#include <stdint.h>

namespace IrProtocol {

constexpr uint8_t kFrameBits = 12;
constexpr uint8_t kCarIdBits = 6;
constexpr uint8_t kCarIdMask = 0x3F;
constexpr uint16_t kFrameMask = 0x0FFF;

constexpr uint16_t kHeaderMinUs = 1800;
constexpr uint16_t kHeaderMaxUs = 4200;
constexpr uint16_t kBitMinUs = 400;
constexpr uint16_t kBitMaxUs = 1800;
constexpr uint16_t kBitThresholdUs = 950;

inline bool decodeFrame(uint32_t rawFrame, uint8_t& carId) {
  const uint16_t frame = static_cast<uint16_t>(rawFrame) & kFrameMask;
  const uint8_t decodedId = (frame >> kCarIdBits) & kCarIdMask;
  const uint8_t invertedId = frame & kCarIdMask;

  if (decodedId == 0 || (decodedId ^ invertedId) != kCarIdMask) {
    return false;
  }

  carId = decodedId;
  return true;
}

}  // namespace IrProtocol

#endif
