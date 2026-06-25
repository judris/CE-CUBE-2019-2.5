#include "firmware/usb_transport.hpp"

namespace ce_cube {

UsbTransport::UsbTransport() : index_(0U), overflow_(false) {}

void UsbTransport::Begin(unsigned long baud_rate, uint32_t startup_timeout_ms) {
  Serial.begin(baud_rate);
#if defined(USBCON)
  const uint32_t start_ms = millis();
  while (!Serial && ((millis() - start_ms) < startup_timeout_ms)) {
  }
#else
  (void)startup_timeout_ms;
#endif
}

LineReceiveStatus UsbTransport::PollReceive(char* message,
                                            size_t message_capacity) {
  if ((message == nullptr) || (message_capacity == 0U)) {
    return LineReceiveStatus::kNone;
  }

  if (index_ == 0U) {
    message[0] = '\0';
  }

  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      if (overflow_) {
        overflow_ = false;
        index_ = 0U;
        message[0] = '\0';
        return LineReceiveStatus::kOverflow;
      }
      if (index_ == 0U) {
        continue;
      }
      if (index_ >= message_capacity) {
        index_ = 0U;
        message[0] = '\0';
        return LineReceiveStatus::kOverflow;
      }
      message[index_] = '\0';
      index_ = 0U;
      return LineReceiveStatus::kMessage;
    }

    if ((index_ + 1U) >= message_capacity) {
      overflow_ = true;
      continue;
    }

    if (!overflow_) {
      message[index_] = incoming;
      ++index_;
      message[index_] = '\0';
    }
  }

  return LineReceiveStatus::kNone;
}

bool UsbTransport::Send(const char* json) {
  if (json == nullptr) {
    return false;
  }
  const size_t written = Serial.print(json);
  const size_t newline = Serial.print('\n');
  return (written > 0U) && (newline == 1U);
}

}  // namespace ce_cube
