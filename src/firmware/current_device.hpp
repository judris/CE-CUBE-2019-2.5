#pragma once

#include <Arduino.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

class CurrentDevice {
 public:
  explicit CurrentDevice(uint8_t analog_pin);

  bool Begin();
  bool Poll(uint32_t now_ms, CurrentMeasurement* measurement);

 private:
  static constexpr uint8_t kAverageSamples = 16U;
  static constexpr uint32_t kPollIntervalMs = 2U;
  static constexpr int32_t kCurrentScaleNumerator = 22155L;
  static constexpr int32_t kCurrentScaleDenominator = 1000000L;

  uint8_t analog_pin_;
  uint8_t sample_count_;
  uint32_t next_poll_ms_;
  uint32_t sample_sum_;
};

}  // namespace ce_cube
