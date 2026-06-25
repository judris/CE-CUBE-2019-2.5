#pragma once

#include <Arduino.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

class Ad7745Device {
 public:
  Ad7745Device();

  bool Begin();
  bool ApplyConfig(const SensorDesiredConfig& config);
  bool Poll(uint32_t now_ms, SensorMeasurement* measurement);

 private:
  static uint32_t PollIntervalMs(UpdateRate rate);
  static uint8_t ConfigByte(UpdateRate rate, bool temperature_compensation);
  static uint8_t ExcitationByte(ExcitationFrequency frequency,
                                ExcitationLevel level);
  bool WriteRegister(uint8_t address, uint8_t value);
  bool WriteDefaultTrim();
  bool ReadBlock(uint8_t start_address, uint8_t* data, size_t data_length);

  SensorDesiredConfig config_;
  uint32_t next_poll_ms_;
};

}  // namespace ce_cube
