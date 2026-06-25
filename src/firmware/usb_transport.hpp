#pragma once

#include <Arduino.h>

namespace ce_cube {

enum class LineReceiveStatus : uint8_t {
  kNone = 0U,
  kMessage = 1U,
  kOverflow = 2U,
};

class UsbTransport {
 public:
  UsbTransport();

  void Begin(unsigned long baud_rate, uint32_t startup_timeout_ms);
  LineReceiveStatus PollReceive(char* message, size_t message_capacity);
  bool Send(const char* json);

 private:
  size_t index_;
  bool overflow_;
};

}  // namespace ce_cube
