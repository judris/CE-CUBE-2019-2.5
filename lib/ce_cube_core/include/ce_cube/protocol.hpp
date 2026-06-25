#pragma once

#include <stddef.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

ParseResult ParseCommandJson(const char* json, ParsedCommand* command);
bool EncodeTelemetryJson(const TelemetrySnapshot& snapshot, char* buffer,
                         size_t buffer_size, size_t* out_length);
bool EncodeAckJson(const ReplyMessage& reply, char* buffer, size_t buffer_size,
                   size_t* out_length);
bool EncodeErrorJson(const ReplyMessage& reply, char* buffer, size_t buffer_size,
                     size_t* out_length);
bool EncodeEventJson(const EventSnapshot& event, uint16_t seq, char* buffer,
                     size_t buffer_size, size_t* out_length);

}  // namespace ce_cube
