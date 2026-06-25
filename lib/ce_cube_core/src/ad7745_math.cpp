#include "ce_cube/ad7745_math.hpp"

namespace ce_cube {

float CapacitanceFromRawBytes(uint8_t high, uint8_t mid, uint8_t low) {
  const uint32_t raw_value = (static_cast<uint32_t>(high) << 16U) |
                             (static_cast<uint32_t>(mid) << 8U) |
                             static_cast<uint32_t>(low);
  const float scaled = (static_cast<float>(raw_value) * 8.192F) / 16777216.0F;
  return scaled - 4.096F;
}

float TemperatureFromRawBytes(uint8_t high, uint8_t mid, uint8_t low) {
  const uint32_t raw_value = (static_cast<uint32_t>(high) << 16U) |
                             (static_cast<uint32_t>(mid) << 8U) |
                             static_cast<uint32_t>(low);
  return (static_cast<float>(raw_value) / 2048.0F) - 4096.0F;
}

}  // namespace ce_cube
