#include "firmware/current_device.hpp"

namespace ce_cube {

CurrentDevice::CurrentDevice(uint8_t analog_pin)
    : analog_pin_(analog_pin),
      sample_count_(0U),
      next_poll_ms_(0U),
      sample_sum_(0U) {}

bool CurrentDevice::Begin() {
  sample_count_ = 0U;
  next_poll_ms_ = 0U;
  sample_sum_ = 0U;
  return true;
}

bool CurrentDevice::Poll(uint32_t now_ms, CurrentMeasurement* measurement) {
  if (measurement == nullptr) {
    return false;
  }
  if (now_ms < next_poll_ms_) {
    return false;
  }

  next_poll_ms_ = now_ms + kPollIntervalMs;
  sample_sum_ += static_cast<uint16_t>(analogRead(analog_pin_));
  ++sample_count_;
  if (sample_count_ < kAverageSamples) {
    return false;
  }

  const uint32_t average_counts = sample_sum_ / kAverageSamples;
  const int32_t scaled =
      static_cast<int32_t>((static_cast<int32_t>(average_counts) *
                            kCurrentScaleNumerator) /
                           kCurrentScaleDenominator);
  measurement->current_ua = scaled;
  measurement->valid = true;

  sample_sum_ = 0U;
  sample_count_ = 0U;
  return true;
}

}  // namespace ce_cube
