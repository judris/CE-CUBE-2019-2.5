#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ce_cube {

uint16_t UpdateCrc16Ccitt(uint16_t crc, uint8_t value);
uint16_t ComputeCrc16Ccitt(const uint8_t* data, size_t length);

}  // namespace ce_cube
