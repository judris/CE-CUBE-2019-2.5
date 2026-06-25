#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

class Rf24Fragmenter {
 public:
  Rf24Fragmenter();

  bool Begin(MessageKind kind, uint16_t seq, const char* json);
  bool NextFrame(uint8_t* frame, size_t frame_capacity, size_t* frame_length);
  bool Done() const;

 private:
  MessageKind kind_;
  uint16_t seq_;
  const uint8_t* message_;
  size_t message_length_;
  uint16_t crc16_;
  uint8_t fragment_count_;
  uint8_t next_fragment_;
  bool active_;
};

class Rf24Reassembler {
 public:
  Rf24Reassembler();

  ErrorCode PushFrame(const uint8_t* frame, size_t frame_length,
                      char* message_buffer, size_t message_capacity);
  bool HasMessage() const;
  bool TakeMessage(MessageKind* kind, uint16_t* seq, size_t* message_length);
  void Reset();

 private:
  MessageKind kind_;
  uint16_t seq_;
  uint16_t expected_crc16_;
  uint8_t expected_fragments_;
  uint8_t next_fragment_;
  size_t message_length_;
  bool message_ready_;
};

}  // namespace ce_cube
