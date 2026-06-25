#pragma once

#include <Arduino.h>

#include "ce_cube/feature_flags.hpp"
#include "ce_cube/rf24_framing.hpp"
#include "ce_cube/types.hpp"
#include "firmware/usb_transport.hpp"

namespace ce_cube {

class Rf24Transport {
 public:
  Rf24Transport(uint8_t ce_pin, uint8_t csn_pin);

  bool Begin(uint64_t local_pipe, uint64_t remote_pipe);
  LineReceiveStatus PollReceive(char* message, size_t message_capacity);
 bool Send(MessageKind kind, uint16_t seq, const char* json);

 private:
  Rf24Reassembler reassembler_;
  uint8_t ce_pin_;
  uint8_t csn_pin_;
  bool initialized_;
};

}  // namespace ce_cube
