#include "ce_cube/rf24_framing.hpp"

#include <string.h>

#include "ce_cube/crc16.hpp"
#include "ce_cube/feature_flags.hpp"

namespace ce_cube {
namespace {

uint16_t ReadBigEndian16(const uint8_t* bytes) {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) |
                               static_cast<uint16_t>(bytes[1]));
}

void WriteBigEndian16(uint16_t value, uint8_t* bytes) {
  bytes[0] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  bytes[1] = static_cast<uint8_t>(value & 0xFFU);
}

bool IsSupportedFrameVersion(uint8_t version) {
  if (version == kProtocolVersion) {
    return true;
  }
#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
  return version == kLegacyProtocolVersion;
#else
  return false;
#endif
}

}  // namespace

Rf24Fragmenter::Rf24Fragmenter()
    : kind_(MessageKind::kInvalid),
      seq_(0U),
      message_(nullptr),
      message_length_(0U),
      crc16_(0U),
      fragment_count_(0U),
      next_fragment_(0U),
      active_(false) {}

bool Rf24Fragmenter::Begin(MessageKind kind, uint16_t seq, const char* json) {
  if ((json == nullptr) || (kind == MessageKind::kInvalid)) {
    return false;
  }

  const size_t length = strlen(json);
  if ((length == 0U) || (length > kMaxTelemetryJsonLength)) {
    return false;
  }

  const size_t fragments = (length + kRf24PayloadSize - 1U) / kRf24PayloadSize;
  if ((fragments == 0U) || (fragments > 255U)) {
    return false;
  }

  kind_ = kind;
  seq_ = seq;
  message_ = reinterpret_cast<const uint8_t*>(json);
  message_length_ = length;
  crc16_ = ComputeCrc16Ccitt(message_, message_length_);
  fragment_count_ = static_cast<uint8_t>(fragments);
  next_fragment_ = 0U;
  active_ = true;
  return true;
}

bool Rf24Fragmenter::NextFrame(uint8_t* frame, size_t frame_capacity,
                               size_t* frame_length) {
  if (!active_ || (frame == nullptr) || (frame_capacity < kMaxRf24FrameSize) ||
      (frame_length == nullptr)) {
    return false;
  }

  const size_t offset = static_cast<size_t>(next_fragment_) * kRf24PayloadSize;
  const size_t remaining = message_length_ - offset;
  const size_t payload_length =
      (remaining > kRf24PayloadSize) ? kRf24PayloadSize : remaining;

  frame[0] = kProtocolVersion;
  frame[1] = static_cast<uint8_t>(kind_);
  WriteBigEndian16(seq_, &frame[2]);
  frame[4] = next_fragment_;
  frame[5] = fragment_count_;
  frame[6] = static_cast<uint8_t>(payload_length);
  WriteBigEndian16(crc16_, &frame[7]);
  memcpy(&frame[kRf24HeaderSize], &message_[offset], payload_length);

  *frame_length = kRf24HeaderSize + payload_length;
  ++next_fragment_;
  if (next_fragment_ >= fragment_count_) {
    active_ = false;
  }
  return true;
}

bool Rf24Fragmenter::Done() const { return !active_; }

Rf24Reassembler::Rf24Reassembler()
    : kind_(MessageKind::kInvalid),
      seq_(0U),
      expected_crc16_(0U),
      expected_fragments_(0U),
      next_fragment_(0U),
      message_length_(0U),
      message_ready_(false) {}

ErrorCode Rf24Reassembler::PushFrame(const uint8_t* frame, size_t frame_length,
                                     char* message_buffer,
                                     size_t message_capacity) {
  if ((frame == nullptr) || (message_buffer == nullptr) ||
      (message_capacity == 0U) || (frame_length < kRf24HeaderSize) ||
      (frame_length > kMaxRf24FrameSize)) {
    return ErrorCode::kRfFragment;
  }
  if (!IsSupportedFrameVersion(frame[0])) {
    return ErrorCode::kRfFragment;
  }

  const uint8_t fragment_index = frame[4];
  const uint8_t fragment_count = frame[5];
  const uint8_t payload_length = frame[6];
  if ((fragment_count == 0U) || (payload_length == 0U) ||
      (frame_length != (kRf24HeaderSize + payload_length))) {
    return ErrorCode::kRfFragment;
  }

  if (fragment_index == 0U) {
    kind_ = static_cast<MessageKind>(frame[1]);
    seq_ = ReadBigEndian16(&frame[2]);
    expected_crc16_ = ReadBigEndian16(&frame[7]);
    expected_fragments_ = fragment_count;
    next_fragment_ = 0U;
    message_length_ = 0U;
    message_ready_ = false;
    message_buffer[0] = '\0';
  }

  if ((fragment_index != next_fragment_) || (fragment_count != expected_fragments_)) {
    return ErrorCode::kRfFragment;
  }
  if ((message_length_ + payload_length) >= message_capacity) {
    return ErrorCode::kRfFragment;
  }

  memcpy(&message_buffer[message_length_], &frame[kRf24HeaderSize], payload_length);
  message_length_ += payload_length;
  ++next_fragment_;

  if (next_fragment_ == expected_fragments_) {
    message_buffer[message_length_] = '\0';
    const uint16_t actual_crc = ComputeCrc16Ccitt(
        reinterpret_cast<const uint8_t*>(message_buffer), message_length_);
    if (actual_crc != expected_crc16_) {
      Reset();
      return ErrorCode::kRfCrc;
    }
    message_ready_ = true;
  }

  return ErrorCode::kNone;
}

bool Rf24Reassembler::HasMessage() const { return message_ready_; }

bool Rf24Reassembler::TakeMessage(MessageKind* kind, uint16_t* seq,
                                  size_t* message_length) {
  if (!message_ready_ || (kind == nullptr) || (seq == nullptr) ||
      (message_length == nullptr)) {
    return false;
  }
  *kind = kind_;
  *seq = seq_;
  *message_length = message_length_;
  Reset();
  return true;
}

void Rf24Reassembler::Reset() {
  kind_ = MessageKind::kInvalid;
  seq_ = 0U;
  expected_crc16_ = 0U;
  expected_fragments_ = 0U;
  next_fragment_ = 0U;
  message_length_ = 0U;
  message_ready_ = false;
}

}  // namespace ce_cube
