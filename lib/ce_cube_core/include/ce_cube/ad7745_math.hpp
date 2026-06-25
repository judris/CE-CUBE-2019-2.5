#pragma once

#include <stdint.h>

namespace ce_cube {

float CapacitanceFromRawBytes(uint8_t high, uint8_t mid, uint8_t low);
float TemperatureFromRawBytes(uint8_t high, uint8_t mid, uint8_t low);

}  // namespace ce_cube
