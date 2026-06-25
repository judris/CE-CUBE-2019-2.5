#include "ce_cube/crc16.hpp"

namespace ce_cube {

uint16_t UpdateCrc16Ccitt(uint16_t crc, uint8_t value) {
  crc ^= static_cast<uint16_t>(value) << 8U;
  for (uint8_t bit = 0U; bit < 8U; ++bit) {
    if ((crc & 0x8000U) != 0U) {
      crc = static_cast<uint16_t>((crc << 1U) ^ 0x1021U);
    } else {
      crc = static_cast<uint16_t>(crc << 1U);
    }
  }
  return crc;
}

uint16_t ComputeCrc16Ccitt(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFFU;
  if (data == nullptr) {
    return crc;
  }

  for (size_t index = 0U; index < length; ++index) {
    crc = UpdateCrc16Ccitt(crc, data[index]);
  }

  return crc;
}

}  // namespace ce_cube
