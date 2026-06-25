#include "ce_cube/protocol.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ce_cube/feature_flags.hpp"
#include "ce_cube/protocol_store.hpp"

#if defined(ARDUINO)
#include <avr/pgmspace.h>
#endif

namespace ce_cube {
namespace {

#if defined(ARDUINO)
using FlashString = const char*;
#define CE_FLASH_LITERAL(text) (PSTR(text))

size_t FlashLength(FlashString text) { return strlen_P(text); }

char FlashCharAt(FlashString text, size_t index) {
  return static_cast<char>(pgm_read_byte(text + index));
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
int CompareRamToFlash(const char* ram_text, FlashString flash_text) {
  return strcmp_P(ram_text, flash_text);
}
#endif

const char* FindFlashSubstring(const char* haystack, FlashString needle) {
  return strstr_P(haystack, needle);
}
#else
using FlashString = const char*;
#define CE_FLASH_LITERAL(text) (text)

size_t FlashLength(FlashString text) { return strlen(text); }

char FlashCharAt(FlashString text, size_t index) { return text[index]; }

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
int CompareRamToFlash(const char* ram_text, FlashString flash_text) {
  return strcmp(ram_text, flash_text);
}
#endif

const char* FindFlashSubstring(const char* haystack, FlashString needle) {
  return strstr(haystack, needle);
}
#endif

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
FlashString WaitPattern(uint8_t wait_index) {
  switch (wait_index) {
    case 0U:
      return CE_FLASH_LITERAL("\"wait_0_ms\":");
    case 1U:
      return CE_FLASH_LITERAL("\"wait_1_ms\":");
    case 2U:
      return CE_FLASH_LITERAL("\"wait_2_ms\":");
    case 3U:
      return CE_FLASH_LITERAL("\"wait_3_ms\":");
    case 4U:
      return CE_FLASH_LITERAL("\"wait_4_ms\":");
    case 5U:
      return CE_FLASH_LITERAL("\"wait_5_ms\":");
    case 6U:
      return CE_FLASH_LITERAL("\"wait_6_ms\":");
    case 7U:
      return CE_FLASH_LITERAL("\"wait_7_ms\":");
    default:
      return CE_FLASH_LITERAL("");
  }
}
#endif

FlashString CompactWaitPattern(uint8_t wait_index) {
  switch (wait_index) {
    case 0U:
      return CE_FLASH_LITERAL("\"w0\":");
    case 1U:
      return CE_FLASH_LITERAL("\"w1\":");
    case 2U:
      return CE_FLASH_LITERAL("\"w2\":");
    case 3U:
      return CE_FLASH_LITERAL("\"w3\":");
    case 4U:
      return CE_FLASH_LITERAL("\"w4\":");
    case 5U:
      return CE_FLASH_LITERAL("\"w5\":");
    case 6U:
      return CE_FLASH_LITERAL("\"w6\":");
    case 7U:
      return CE_FLASH_LITERAL("\"w7\":");
    default:
      return CE_FLASH_LITERAL("");
  }
}

#if CE_CUBE_ENABLE_PROTOCOL_INFO
FlashString EventWaitPrefix(uint8_t wait_index) {
  switch (wait_index) {
    case 0U:
      return CE_FLASH_LITERAL(",\"wait_0_ms\":");
    case 1U:
      return CE_FLASH_LITERAL(",\"wait_1_ms\":");
    case 2U:
      return CE_FLASH_LITERAL(",\"wait_2_ms\":");
    case 3U:
      return CE_FLASH_LITERAL(",\"wait_3_ms\":");
    case 4U:
      return CE_FLASH_LITERAL(",\"wait_4_ms\":");
    case 5U:
      return CE_FLASH_LITERAL(",\"wait_5_ms\":");
    case 6U:
      return CE_FLASH_LITERAL(",\"wait_6_ms\":");
    case 7U:
      return CE_FLASH_LITERAL(",\"wait_7_ms\":");
    default:
      return CE_FLASH_LITERAL("");
  }
}

FlashString CompactEventWaitPrefix(uint8_t wait_index) {
  switch (wait_index) {
    case 0U:
      return CE_FLASH_LITERAL(",\"w0\":");
    case 1U:
      return CE_FLASH_LITERAL(",\"w1\":");
    case 2U:
      return CE_FLASH_LITERAL(",\"w2\":");
    case 3U:
      return CE_FLASH_LITERAL(",\"w3\":");
    case 4U:
      return CE_FLASH_LITERAL(",\"w4\":");
    case 5U:
      return CE_FLASH_LITERAL(",\"w5\":");
    case 6U:
      return CE_FLASH_LITERAL(",\"w6\":");
    case 7U:
      return CE_FLASH_LITERAL(",\"w7\":");
    default:
      return CE_FLASH_LITERAL("");
  }
}
#endif

bool AppendChar(char character, char* buffer, size_t buffer_size,
                size_t* index) {
  if ((buffer == nullptr) || (index == nullptr) ||
      ((*index + 1U) >= buffer_size)) {
    return false;
  }

  buffer[*index] = character;
  ++(*index);
  buffer[*index] = '\0';
  return true;
}

bool AppendFlashLiteral(FlashString text, char* buffer, size_t buffer_size,
                        size_t* index) {
  if ((text == nullptr) || (buffer == nullptr) || (index == nullptr)) {
    return false;
  }

  const size_t length = FlashLength(text);
  if ((*index + length) >= buffer_size) {
    return false;
  }

  for (size_t char_index = 0U; char_index < length; ++char_index) {
    buffer[*index + char_index] = FlashCharAt(text, char_index);
  }
  *index += length;
  buffer[*index] = '\0';
  return true;
}

bool AppendUInt(uint32_t value, char* buffer, size_t buffer_size, size_t* index) {
  char digits[10] = {0};
  size_t digit_count = 0U;
  uint32_t working = value;

  do {
    digits[digit_count] = static_cast<char>('0' + (working % 10U));
    working /= 10U;
    ++digit_count;
  } while ((working > 0U) && (digit_count < sizeof(digits)));

  if (((*index) + digit_count) >= buffer_size) {
    return false;
  }

  while (digit_count > 0U) {
    --digit_count;
    if (!AppendChar(digits[digit_count], buffer, buffer_size, index)) {
      return false;
    }
  }
  return true;
}

bool AppendInt(int32_t value, char* buffer, size_t buffer_size, size_t* index) {
  if ((buffer == nullptr) || (index == nullptr)) {
    return false;
  }

  if (value < 0) {
    const uint32_t magnitude =
        static_cast<uint32_t>(-(value + 1L)) + 1U;
    if (!AppendChar('-', buffer, buffer_size, index)) {
      return false;
    }
    return AppendUInt(magnitude, buffer, buffer_size, index);
  }

  return AppendUInt(static_cast<uint32_t>(value), buffer, buffer_size, index);
}

bool AppendUptimeField(uint32_t uptime_ms, char* buffer, size_t buffer_size,
                       size_t* index) {
  return AppendFlashLiteral(CE_FLASH_LITERAL(",\"gt\":"), buffer, buffer_size,
                            index) &&
         AppendUInt(uptime_ms, buffer, buffer_size, index);
}

bool AppendAnalysisTimeField(uint32_t analysis_time_ms, bool valid, char* buffer,
                             size_t buffer_size, size_t* index) {
  if (!valid) {
    return true;
  }

  return AppendFlashLiteral(CE_FLASH_LITERAL(",\"at\":"), buffer, buffer_size,
                            index) &&
         AppendUInt(analysis_time_ms, buffer, buffer_size, index);
}

bool AppendTimingFields(uint32_t uptime_ms, uint32_t analysis_time_ms,
                        bool analysis_time_valid, char* buffer,
                        size_t buffer_size, size_t* index) {
  return AppendUptimeField(uptime_ms, buffer, buffer_size, index) &&
         AppendAnalysisTimeField(analysis_time_ms, analysis_time_valid, buffer,
                                 buffer_size, index);
}

bool AppendFixed(float value, uint8_t decimals, char* buffer, size_t buffer_size,
                 size_t* index) {
  int32_t scale = 1;
  for (uint8_t count = 0U; count < decimals; ++count) {
    scale *= 10;
  }

  const float scaled_value = value * static_cast<float>(scale);
  const float rounding = (scaled_value >= 0.0F) ? 0.5F : -0.5F;
  const int32_t scaled = static_cast<int32_t>(scaled_value + rounding);
  const bool negative = scaled < 0;
  const uint32_t magnitude =
      negative ? static_cast<uint32_t>(-scaled)
               : static_cast<uint32_t>(scaled);
  const uint32_t integer_part = magnitude / static_cast<uint32_t>(scale);
  uint32_t fraction_part = magnitude % static_cast<uint32_t>(scale);

  if (negative && !AppendChar('-', buffer, buffer_size, index)) {
    return false;
  }
  if (!AppendUInt(integer_part, buffer, buffer_size, index) ||
      !AppendChar('.', buffer, buffer_size, index)) {
    return false;
  }

  char fraction_digits[6] = {0};
  if (decimals > sizeof(fraction_digits)) {
    return false;
  }
  for (uint8_t digit = 0U; digit < decimals; ++digit) {
    const uint8_t reversed_index =
        static_cast<uint8_t>((static_cast<uint16_t>(decimals) -
                              static_cast<uint16_t>(digit)) -
                             1U);
    fraction_digits[reversed_index] =
        static_cast<char>('0' + (fraction_part % 10U));
    fraction_part /= 10U;
  }
  for (uint8_t digit = 0U; digit < decimals; ++digit) {
    if (!AppendChar(fraction_digits[digit], buffer, buffer_size, index)) {
      return false;
    }
  }
  return true;
}

bool FindValueStart(const char* json, FlashString pattern,
                    const char** value_start) {
  if ((json == nullptr) || (pattern == nullptr) || (value_start == nullptr)) {
    return false;
  }

  const char* location = FindFlashSubstring(json, pattern);
  if (location == nullptr) {
    return false;
  }

  location += FlashLength(pattern);
  while ((*location == ' ') || (*location == '\t') || (*location == '\r') ||
         (*location == '\n')) {
    ++location;
  }

  *value_start = location;
  return true;
}

bool ParseUnsignedAt(const char* start, uint32_t* value) {
  if ((start == nullptr) || (value == nullptr) || (*start < '0') ||
      (*start > '9')) {
    return false;
  }

  uint32_t parsed = 0U;
  const char* cursor = start;
  while ((*cursor >= '0') && (*cursor <= '9')) {
    parsed = (parsed * 10U) + static_cast<uint32_t>(*cursor - '0');
    ++cursor;
  }
  *value = parsed;
  return true;
}

bool ParseSignedAt(const char* start, int32_t* value) {
  if ((start == nullptr) || (value == nullptr)) {
    return false;
  }

  bool negative = false;
  const char* cursor = start;
  if (*cursor == '-') {
    negative = true;
    ++cursor;
  }

  uint32_t magnitude = 0U;
  if (!ParseUnsignedAt(cursor, &magnitude)) {
    return false;
  }

  *value = negative ? -static_cast<int32_t>(magnitude)
                    : static_cast<int32_t>(magnitude);
  return true;
}

bool ParseUintField(const char* json, FlashString pattern, uint32_t* value) {
  const char* start = nullptr;
  return FindValueStart(json, pattern, &start) && ParseUnsignedAt(start, value);
}

bool ParseIntField(const char* json, FlashString pattern, int32_t* value) {
  const char* start = nullptr;
  return FindValueStart(json, pattern, &start) && ParseSignedAt(start, value);
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ParseBoolField(const char* json, FlashString pattern, bool* value) {
  const char* start = nullptr;
  if (!FindValueStart(json, pattern, &start) || (value == nullptr)) {
    return false;
  }

  if ((*start == '1') || ((*start == 't') && (start[1] == 'r'))) {
    *value = true;
    return true;
  }
  if ((*start == '0') || ((*start == 'f') && (start[1] == 'a'))) {
    *value = false;
    return true;
  }
  return false;
}
#endif

bool ParseStringField(const char* json, FlashString pattern, char* value,
                      size_t value_size) {
  const char* start = nullptr;
  if (!FindValueStart(json, pattern, &start) || (value == nullptr) ||
      (value_size < 2U) || (*start != '"')) {
    return false;
  }

  ++start;
  size_t index = 0U;
  while ((*start != '\0') && (*start != '"')) {
    if ((index + 1U) >= value_size) {
      return false;
    }
    value[index] = *start;
    ++index;
    ++start;
  }
  if (*start != '"') {
    return false;
  }

  value[index] = '\0';
  return true;
}

bool MapUpdateRate(uint32_t rate_hz, UpdateRate* rate) {
  if (rate == nullptr) {
    return false;
  }

  switch (rate_hz) {
    case 9U:
      *rate = UpdateRate::k9Hz;
      return true;
    case 11U:
      *rate = UpdateRate::k11Hz;
      return true;
    case 13U:
      *rate = UpdateRate::k13Hz;
      return true;
    case 16U:
      *rate = UpdateRate::k16Hz;
      return true;
    case 26U:
      *rate = UpdateRate::k26Hz;
      return true;
    case 50U:
      *rate = UpdateRate::k50Hz;
      return true;
    case 84U:
      *rate = UpdateRate::k84Hz;
      return true;
    case 91U:
      *rate = UpdateRate::k91Hz;
      return true;
    default:
      return false;
  }
}

bool MapExcitationFrequency(uint32_t freq_hz, ExcitationFrequency* frequency) {
  if (frequency == nullptr) {
    return false;
  }
  if (freq_hz == 16000U) {
    *frequency = ExcitationFrequency::k16kHz;
    return true;
  }
  if (freq_hz == 32000U) {
    *frequency = ExcitationFrequency::k32kHz;
    return true;
  }
  return false;
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool MapExcitationLevel(const char* value, ExcitationLevel* level) {
  if ((value == nullptr) || (level == nullptr)) {
    return false;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("vdd_8")) == 0) {
    *level = ExcitationLevel::kVddDiv8;
    return true;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("vdd_4")) == 0) {
    *level = ExcitationLevel::kVddDiv4;
    return true;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("vdd_3_8")) == 0) {
    *level = ExcitationLevel::kVddTimes3Div8;
    return true;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("vdd_2")) == 0) {
    *level = ExcitationLevel::kVddDiv2;
    return true;
  }
  return false;
}

bool MapLiftPosition(const char* value, LiftPosition* position) {
  if ((value == nullptr) || (position == nullptr)) {
    return false;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("up")) == 0) {
    *position = LiftPosition::kUp;
    return true;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("down")) == 0) {
    *position = LiftPosition::kDown;
    return true;
  }
  return false;
}

bool MapInjectionMode(const char* value, InjectionMode* mode) {
  if ((value == nullptr) || (mode == nullptr)) {
    return false;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("hydro")) == 0) {
    *mode = InjectionMode::kHydrodynamic;
    return true;
  }
  if (CompareRamToFlash(value, CE_FLASH_LITERAL("electro")) == 0) {
    *mode = InjectionMode::kElectrokinetic;
    return true;
  }
  return false;
}

bool MapAdjustDirection(const char* value, int8_t* direction) {
  if ((value == nullptr) || (direction == nullptr)) {
    return false;
  }
  if ((CompareRamToFlash(value, CE_FLASH_LITERAL("cw")) == 0) ||
      (CompareRamToFlash(value, CE_FLASH_LITERAL("clock")) == 0)) {
    *direction = 1;
    return true;
  }
  if ((CompareRamToFlash(value, CE_FLASH_LITERAL("ccw")) == 0) ||
      (CompareRamToFlash(value, CE_FLASH_LITERAL("counter")) == 0)) {
    *direction = -1;
    return true;
  }
  return false;
}

bool MapCommandType(const char* value, CommandType* type) {
  if ((value == nullptr) || (type == nullptr)) {
    return false;
  }

  if (CompareRamToFlash(value, CE_FLASH_LITERAL("sensor.set_rate")) == 0) {
    *type = CommandType::kSensorSetRate;
  } else if (CompareRamToFlash(value,
                               CE_FLASH_LITERAL("sensor.set_excitation")) == 0) {
    *type = CommandType::kSensorSetExcitation;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("sensor.temp_comp")) ==
             0) {
    *type = CommandType::kSensorTempComp;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("sensor.auto_zero")) ==
             0) {
    *type = CommandType::kSensorAutoZero;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("lift.move")) == 0) {
    *type = CommandType::kLiftMove;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("carousel.step")) == 0) {
    *type = CommandType::kCarouselStep;
  } else if (CompareRamToFlash(value,
                               CE_FLASH_LITERAL("carousel.goto_slot")) == 0) {
    *type = CommandType::kCarouselGotoSlot;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("carousel.adjust")) ==
             0) {
    *type = CommandType::kCarouselAdjust;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("hv.set")) == 0) {
    *type = CommandType::kHvSet;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("pump.set")) == 0) {
    *type = CommandType::kPumpSet;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("valve.set")) == 0) {
    *type = CommandType::kValveSet;
  } else if (CompareRamToFlash(value,
                               CE_FLASH_LITERAL("injection.configure")) == 0) {
    *type = CommandType::kInjectionConfigure;
  } else if (CompareRamToFlash(value,
                               CE_FLASH_LITERAL("collection.configure")) == 0) {
    *type = CommandType::kCollectionConfigure;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("sample.collect")) == 0) {
    *type = CommandType::kSampleCollect;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("sample.stop")) == 0) {
    *type = CommandType::kSampleStop;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("injection.run")) == 0) {
    *type = CommandType::kInjectionRun;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("droplet.make")) == 0) {
    *type = CommandType::kDropletMake;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("replenish.run")) == 0) {
    *type = CommandType::kReplenishRun;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("drain.run")) == 0) {
    *type = CommandType::kDrainRun;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("carousel.home")) == 0) {
    *type = CommandType::kCarouselHome;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("run.start")) == 0) {
    *type = CommandType::kRunStart;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("run.stop")) == 0) {
    *type = CommandType::kRunStop;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("run.status")) == 0) {
    *type = CommandType::kRunStatus;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("protocol.begin")) == 0) {
    *type = CommandType::kProtocolBegin;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("protocol.chunk")) == 0) {
    *type = CommandType::kProtocolChunk;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("protocol.commit")) ==
             0) {
    *type = CommandType::kProtocolCommit;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("protocol.info")) == 0) {
    *type = CommandType::kProtocolInfo;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("protocol.clear")) == 0) {
    *type = CommandType::kProtocolClear;
  } else if (CompareRamToFlash(value, CE_FLASH_LITERAL("status.get")) == 0) {
    *type = CommandType::kStatusGet;
  } else {
    *type = CommandType::kInvalid;
    return false;
  }

  return true;
}
#endif

bool ParseHexNibble(char character, uint8_t* nibble) {
  if (nibble == nullptr) {
    return false;
  }
  if ((character >= '0') && (character <= '9')) {
    *nibble = static_cast<uint8_t>(character - '0');
    return true;
  }
  if ((character >= 'A') && (character <= 'F')) {
    *nibble = static_cast<uint8_t>(10 + character - 'A');
    return true;
  }
  if ((character >= 'a') && (character <= 'f')) {
    *nibble = static_cast<uint8_t>(10 + character - 'a');
    return true;
  }
  return false;
}

bool ParseHexDataField(const char* json, FlashString pattern, uint8_t* data,
                       size_t data_length) {
  char value[2U * kProtocolChunkDataBytes + 1U] = "";
  if (!ParseStringField(json, pattern, value, sizeof(value)) || (data == nullptr) ||
      (data_length != kProtocolChunkDataBytes) ||
      (strlen(value) != (2U * data_length))) {
    return false;
  }

  for (size_t index = 0U; index < data_length; ++index) {
    uint8_t high = 0U;
    uint8_t low = 0U;
    if (!ParseHexNibble(value[index * 2U], &high) ||
        !ParseHexNibble(value[index * 2U + 1U], &low)) {
      return false;
    }
    data[index] = static_cast<uint8_t>((high << 4U) | low);
  }
  return true;
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ParseWaitPresetMs(const char* json, uint8_t wait_index, uint16_t* wait_units) {
  if ((wait_units == nullptr) || (wait_index >= kProtocolWaitPresetCount)) {
    return false;
  }

  uint32_t wait_ms = 0U;
  if (!ParseUintField(json, WaitPattern(wait_index), &wait_ms)) {
    *wait_units = 0U;
    return true;
  }
  if (wait_ms > kMaxStoredWaitMs) {
    return false;
  }

  *wait_units = static_cast<uint16_t>((wait_ms + 50U) / 100U);
  return true;
}
#endif

bool ParseCompactWaitPresetMs(const char* json, uint8_t wait_index,
                              uint16_t* wait_units) {
  if ((wait_units == nullptr) || (wait_index >= kProtocolWaitPresetCount)) {
    return false;
  }

  uint32_t wait_ms = 0U;
  if (!ParseUintField(json, CompactWaitPattern(wait_index), &wait_ms)) {
    *wait_units = 0U;
    return true;
  }
  if (wait_ms > kMaxStoredWaitMs) {
    return false;
  }

  *wait_units = static_cast<uint16_t>((wait_ms + 50U) / 100U);
  return true;
}

void InitializeParsedCommandDefaults(ParsedCommand* command) {
  if (command == nullptr) {
    return;
  }

  *command = {};
  command->type = CommandType::kInvalid;
  command->lift_position = LiftPosition::kDown;
  command->injection_mode = InjectionMode::kElectrokinetic;
  command->update_rate = UpdateRate::k9Hz;
  command->excitation_frequency = ExcitationFrequency::k32kHz;
  command->excitation_level = ExcitationLevel::kVddDiv2;
  command->protocol_metadata.slot_count = kDefaultSlotCount;
  command->protocol_metadata.bge1_slot = 0U;
  command->protocol_metadata.bge2_slot = 1U;
  command->protocol_metadata.sample_1_slot = 5U;
  command->protocol_metadata.sample_count = 1U;
  command->protocol_metadata.repetitions = 1U;
  command->protocol_metadata.injection_mode = InjectionMode::kElectrokinetic;
  command->protocol_metadata.collection_duration_ms = 10000UL;
  command->protocol_metadata.injection_duration_ms = 10000UL;
  command->protocol_metadata.droplet_duration_ms = 5000UL;
  command->collection_duration_ms = 10000UL;
  command->chunk_length = static_cast<uint8_t>(kProtocolChunkDataBytes);
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ContainsPattern(const char* json, FlashString pattern) {
  return (json != nullptr) && (pattern != nullptr) &&
         (FindFlashSubstring(json, pattern) != nullptr);
}
#endif

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ContainsLegacyWaitFields(const char* json) {
  for (uint8_t wait_index = 0U; wait_index < kProtocolWaitPresetCount;
       ++wait_index) {
    if (ContainsPattern(json, WaitPattern(wait_index))) {
      return true;
    }
  }
  return false;
}
#endif

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ContainsCompactWaitFields(const char* json) {
  for (uint8_t wait_index = 0U; wait_index < kProtocolWaitPresetCount;
       ++wait_index) {
    if (ContainsPattern(json, CompactWaitPattern(wait_index))) {
      return true;
    }
  }
  return false;
}
#endif

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ContainsLegacyCommandFields(const char* json) {
  return ContainsPattern(json, CE_FLASH_LITERAL("\"kind\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"seq\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"cmd\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"rate_hz\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"freq_hz\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"level\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"enable\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"clear\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"position\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"steps\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"slot\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"dir\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"id\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"mode\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"duration_ms\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"collection_duration_ms\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"injection_duration_ms\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"droplet_duration_ms\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"injection_mode\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"slot_count\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"bge1_slot\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"bge2_slot\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"sample_1_slot\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"sample_count\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"repetitions\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"off\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"data\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"prepare_length\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"len\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"crc\":")) ||
         ContainsLegacyWaitFields(json);
}
#endif

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
bool ContainsCompactCommandFields(const char* json) {
  return ContainsPattern(json, CE_FLASH_LITERAL("\"k\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"s\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"c\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"rh\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"fh\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"xl\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"en\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"cl\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"lp\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"st\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"sl\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"ad\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"vi\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"im\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"cd\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"jd\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"dd\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"du\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"sc\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"b1\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"b2\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"s1\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"sn\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"rr\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"of\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"dt\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"pl\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"ln\":")) ||
         ContainsPattern(json, CE_FLASH_LITERAL("\"cr\":")) ||
         ContainsCompactWaitFields(json);
}
#endif

bool ParseBinaryField(const char* json, FlashString pattern, bool* value) {
  uint32_t raw = 0U;
  if (!ParseUintField(json, pattern, &raw) || (raw > 1U) || (value == nullptr)) {
    return false;
  }

  *value = (raw == 1U);
  return true;
}

bool MapCommandTypeId(uint32_t value, CommandType* type) {
  if ((type == nullptr) || (value == 0U) ||
      (value > static_cast<uint32_t>(CommandType::kCarouselHome))) {
    return false;
  }

  *type = static_cast<CommandType>(value);
  return true;
}

bool MapExcitationLevelCode(uint32_t value, ExcitationLevel* level) {
  if (level == nullptr) {
    return false;
  }

  switch (value) {
    case 0U:
      *level = ExcitationLevel::kVddDiv8;
      return true;
    case 1U:
      *level = ExcitationLevel::kVddDiv4;
      return true;
    case 2U:
      *level = ExcitationLevel::kVddTimes3Div8;
      return true;
    case 3U:
      *level = ExcitationLevel::kVddDiv2;
      return true;
    default:
      return false;
  }
}

bool MapLiftPositionCode(uint32_t value, LiftPosition* position) {
  if (position == nullptr) {
    return false;
  }

  if (value == 0U) {
    *position = LiftPosition::kDown;
    return true;
  }
  if (value == 1U) {
    *position = LiftPosition::kUp;
    return true;
  }
  return false;
}

bool MapInjectionModeCode(uint32_t value, InjectionMode* mode) {
  if (mode == nullptr) {
    return false;
  }

  if (value == 0U) {
    *mode = InjectionMode::kHydrodynamic;
    return true;
  }
  if (value == 1U) {
    *mode = InjectionMode::kElectrokinetic;
    return true;
  }
  return false;
}

bool EncodeMessageBase(MessageKind kind, uint16_t seq, char* buffer,
                       size_t buffer_size, size_t* index) {
  return AppendFlashLiteral(CE_FLASH_LITERAL("{\"v\":"), buffer, buffer_size,
                            index) &&
         AppendUInt(kProtocolVersion, buffer, buffer_size, index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"k\":"), buffer, buffer_size,
                            index) &&
         AppendUInt(static_cast<uint8_t>(kind), buffer, buffer_size, index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"s\":"), buffer, buffer_size,
                            index) &&
         AppendUInt(seq, buffer, buffer_size, index);
}

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
__attribute__((noinline)) bool ParseCommandJsonV1(const char* json,
                                                  ParsedCommand* command,
                                                  ParseResult* result) {
  char kind[16] = "";
  if (!ParseStringField(json, CE_FLASH_LITERAL("\"kind\":"), kind,
                        sizeof(kind)) ||
      (CompareRamToFlash(kind, CE_FLASH_LITERAL("command")) != 0)) {
    result->error = ErrorCode::kInvalidField;
    return false;
  }

  uint32_t seq = 0U;
  if (!ParseUintField(json, CE_FLASH_LITERAL("\"seq\":"), &seq) ||
      (seq > 65535U)) {
    result->error = ErrorCode::kInvalidField;
    return false;
  }
  command->seq = static_cast<uint16_t>(seq);
  result->seq = command->seq;

  char cmd[32] = "";
  if (!ParseStringField(json, CE_FLASH_LITERAL("\"cmd\":"), cmd, sizeof(cmd)) ||
      !MapCommandType(cmd, &command->type)) {
    result->error = ErrorCode::kInvalidCommand;
    return false;
  }

  switch (command->type) {
    case CommandType::kSensorSetRate: {
      uint32_t rate_hz = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"rate_hz\":"), &rate_hz) ||
          !MapUpdateRate(rate_hz, &command->update_rate)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kSensorSetExcitation: {
      uint32_t freq_hz = 0U;
      char level[16] = "";
      if (ParseUintField(json, CE_FLASH_LITERAL("\"freq_hz\":"), &freq_hz)) {
        if (!MapExcitationFrequency(freq_hz, &command->excitation_frequency)) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
        command->has_excitation_frequency = true;
      }
      if (ParseStringField(json, CE_FLASH_LITERAL("\"level\":"), level,
                           sizeof(level))) {
        if (!MapExcitationLevel(level, &command->excitation_level)) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
        command->has_excitation_level = true;
      }
      if (!command->has_excitation_frequency && !command->has_excitation_level) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kSensorTempComp:
    case CommandType::kHvSet:
    case CommandType::kPumpSet:
      if (!ParseBoolField(json, CE_FLASH_LITERAL("\"enable\":"),
                          &command->enable)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    case CommandType::kSensorAutoZero:
      if (!ParseBoolField(json, CE_FLASH_LITERAL("\"clear\":"),
                          &command->clear_zero)) {
        command->clear_zero = false;
      }
      return true;
    case CommandType::kLiftMove: {
      char position[8] = "";
      if (!ParseStringField(json, CE_FLASH_LITERAL("\"position\":"),
                            position, sizeof(position)) ||
          !MapLiftPosition(position, &command->lift_position)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kCarouselStep: {
      int32_t steps = 0;
      if (!ParseIntField(json, CE_FLASH_LITERAL("\"steps\":"), &steps) ||
          (steps < -11L) || (steps > 11L) || (steps == 0L)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->carousel_steps = static_cast<int8_t>(steps);
      return true;
    }
    case CommandType::kCarouselGotoSlot: {
      uint32_t slot = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"slot\":"), &slot) ||
          (slot >= kMaxSupportedSlots)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->slot = static_cast<uint8_t>(slot);
      return true;
    }
    case CommandType::kCarouselAdjust: {
      char dir[8] = "";
      if (!ParseStringField(json, CE_FLASH_LITERAL("\"dir\":"), dir,
                            sizeof(dir)) ||
          !MapAdjustDirection(dir, &command->adjust_direction)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kValveSet: {
      uint32_t valve_id = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"id\":"), &valve_id) ||
          (valve_id < 1U) || (valve_id > 2U) ||
          !ParseBoolField(json, CE_FLASH_LITERAL("\"enable\":"),
                          &command->enable)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->valve_id = static_cast<uint8_t>(valve_id);
      return true;
    }
    case CommandType::kCollectionConfigure: {
      uint32_t duration_ms = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"duration_ms\":"),
                          &duration_ms) ||
          (duration_ms == 0U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->collection_duration_ms = duration_ms;
      return true;
    }
    case CommandType::kInjectionConfigure: {
      char mode[12] = "";
      uint32_t duration_ms = 0U;
      if (!ParseStringField(json, CE_FLASH_LITERAL("\"mode\":"), mode,
                            sizeof(mode)) ||
          !MapInjectionMode(mode, &command->injection_mode) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"duration_ms\":"),
                          &duration_ms)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->duration_ms = duration_ms;
      return true;
    }
    case CommandType::kSampleCollect:
    case CommandType::kSampleStop:
    case CommandType::kInjectionRun:
    case CommandType::kDropletMake:
    case CommandType::kReplenishRun:
    case CommandType::kDrainRun:
    case CommandType::kCarouselHome:
      return true;
    case CommandType::kProtocolBegin: {
      uint32_t slot_count = 0U;
      uint32_t bge1_slot = 0U;
      uint32_t bge2_slot = 0U;
      uint32_t sample_1_slot = 0U;
      uint32_t sample_count = 0U;
      uint32_t repetitions = 0U;
      uint32_t collection_duration_ms = 0U;
      uint32_t injection_duration_ms = 0U;
      uint32_t droplet_duration_ms = 0U;
      char injection_mode[16] = "";
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"slot_count\":"),
                          &slot_count) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"bge1_slot\":"),
                          &bge1_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"bge2_slot\":"),
                          &bge2_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"sample_1_slot\":"),
                          &sample_1_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"sample_count\":"),
                          &sample_count) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"repetitions\":"),
                          &repetitions) ||
          !ParseStringField(json, CE_FLASH_LITERAL("\"injection_mode\":"),
                            injection_mode, sizeof(injection_mode)) ||
          !MapInjectionMode(injection_mode, &command->protocol_metadata.injection_mode) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"collection_duration_ms\":"),
                          &collection_duration_ms) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"injection_duration_ms\":"),
                          &injection_duration_ms) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"droplet_duration_ms\":"),
                          &droplet_duration_ms) ||
          (slot_count == 0U) || (slot_count > kMaxSupportedSlots) ||
          (bge1_slot >= slot_count) || (bge2_slot >= slot_count) ||
          (sample_1_slot >= slot_count) || (sample_count == 0U) ||
          (repetitions == 0U) || (collection_duration_ms == 0U) ||
          (injection_duration_ms == 0U) || (droplet_duration_ms == 0U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }

      command->protocol_metadata.slot_count = static_cast<uint8_t>(slot_count);
      command->protocol_metadata.bge1_slot = static_cast<uint8_t>(bge1_slot);
      command->protocol_metadata.bge2_slot = static_cast<uint8_t>(bge2_slot);
      command->protocol_metadata.sample_1_slot =
          static_cast<uint8_t>(sample_1_slot);
      command->protocol_metadata.sample_count =
          static_cast<uint8_t>(sample_count);
      command->protocol_metadata.repetitions =
          static_cast<uint8_t>(repetitions);
      command->protocol_metadata.collection_duration_ms = collection_duration_ms;
      command->protocol_metadata.injection_duration_ms = injection_duration_ms;
      command->protocol_metadata.droplet_duration_ms = droplet_duration_ms;

      for (uint8_t index = 0U; index < kProtocolWaitPresetCount; ++index) {
        if (!ParseWaitPresetMs(
                json, index,
                &command->protocol_metadata.wait_times_100ms[index])) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
      }
      return true;
    }
    case CommandType::kProtocolChunk: {
      uint32_t offset = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"off\":"), &offset) ||
          (offset > kProtocolProgramMaxLength) ||
          !ParseHexDataField(json, CE_FLASH_LITERAL("\"data\":"),
                             command->chunk_data, kProtocolChunkDataBytes)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->offset = static_cast<uint16_t>(offset);
      command->chunk_length = static_cast<uint8_t>(kProtocolChunkDataBytes);
      return true;
    }
    case CommandType::kProtocolCommit: {
      uint32_t prepare_length = 0U;
      uint32_t length = 0U;
      uint32_t crc = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"prepare_length\":"),
                          &prepare_length) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"len\":"), &length) ||
          (prepare_length == 0U) || (prepare_length >= length) ||
          (length == 0U) || (length > kProtocolProgramMaxLength) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"crc\":"), &crc) ||
          (crc > 65535U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->prepare_length = static_cast<uint16_t>(prepare_length);
      command->program_length = static_cast<uint16_t>(length);
      command->crc16 = static_cast<uint16_t>(crc);
      return true;
    }
    case CommandType::kRunStart:
    case CommandType::kRunStop:
    case CommandType::kRunStatus:
    case CommandType::kProtocolInfo:
    case CommandType::kProtocolClear:
    case CommandType::kStatusGet:
      return true;
    default:
      result->error = ErrorCode::kInvalidCommand;
      return false;
  }
}
#endif

__attribute__((noinline)) bool ParseCommandJsonV2(const char* json,
                                                  ParsedCommand* command,
                                                  ParseResult* result) {
  uint32_t kind = 0U;
  if (!ParseUintField(json, CE_FLASH_LITERAL("\"k\":"), &kind) ||
      (kind != static_cast<uint32_t>(MessageKind::kCommand))) {
    result->error = ErrorCode::kInvalidField;
    return false;
  }

  uint32_t seq = 0U;
  if (!ParseUintField(json, CE_FLASH_LITERAL("\"s\":"), &seq) ||
      (seq > 65535U)) {
    result->error = ErrorCode::kInvalidField;
    return false;
  }
  command->seq = static_cast<uint16_t>(seq);
  result->seq = command->seq;

  uint32_t command_id = 0U;
  if (!ParseUintField(json, CE_FLASH_LITERAL("\"c\":"), &command_id) ||
      !MapCommandTypeId(command_id, &command->type)) {
    result->error = ErrorCode::kInvalidCommand;
    return false;
  }

  switch (command->type) {
    case CommandType::kSensorSetRate: {
      uint32_t rate_hz = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"rh\":"), &rate_hz) ||
          !MapUpdateRate(rate_hz, &command->update_rate)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kSensorSetExcitation: {
      uint32_t freq_hz = 0U;
      uint32_t level = 0U;
      if (ParseUintField(json, CE_FLASH_LITERAL("\"fh\":"), &freq_hz)) {
        if (!MapExcitationFrequency(freq_hz, &command->excitation_frequency)) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
        command->has_excitation_frequency = true;
      }
      if (ParseUintField(json, CE_FLASH_LITERAL("\"xl\":"), &level)) {
        if (!MapExcitationLevelCode(level, &command->excitation_level)) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
        command->has_excitation_level = true;
      }
      if (!command->has_excitation_frequency && !command->has_excitation_level) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kSensorTempComp:
    case CommandType::kHvSet:
    case CommandType::kPumpSet:
      if (!ParseBinaryField(json, CE_FLASH_LITERAL("\"en\":"),
                            &command->enable)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    case CommandType::kSensorAutoZero:
      if (!ParseBinaryField(json, CE_FLASH_LITERAL("\"cl\":"),
                            &command->clear_zero)) {
        command->clear_zero = false;
      }
      return true;
    case CommandType::kLiftMove: {
      uint32_t position = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"lp\":"), &position) ||
          !MapLiftPositionCode(position, &command->lift_position)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      return true;
    }
    case CommandType::kCarouselStep: {
      int32_t steps = 0;
      if (!ParseIntField(json, CE_FLASH_LITERAL("\"st\":"), &steps) ||
          (steps < -11L) || (steps > 11L) || (steps == 0L)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->carousel_steps = static_cast<int8_t>(steps);
      return true;
    }
    case CommandType::kCarouselGotoSlot: {
      uint32_t slot = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"sl\":"), &slot) ||
          (slot >= kMaxSupportedSlots)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->slot = static_cast<uint8_t>(slot);
      return true;
    }
    case CommandType::kCarouselAdjust: {
      int32_t direction = 0;
      if (!ParseIntField(json, CE_FLASH_LITERAL("\"ad\":"), &direction) ||
          ((direction != -1L) && (direction != 1L))) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->adjust_direction = static_cast<int8_t>(direction);
      return true;
    }
    case CommandType::kValveSet: {
      uint32_t valve_id = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"vi\":"), &valve_id) ||
          (valve_id < 1U) || (valve_id > 2U) ||
          !ParseBinaryField(json, CE_FLASH_LITERAL("\"en\":"),
                            &command->enable)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->valve_id = static_cast<uint8_t>(valve_id);
      return true;
    }
    case CommandType::kCollectionConfigure: {
      uint32_t duration_ms = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"du\":"), &duration_ms) ||
          (duration_ms == 0U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->collection_duration_ms = duration_ms;
      return true;
    }
    case CommandType::kInjectionConfigure: {
      uint32_t mode = 0U;
      uint32_t duration_ms = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"im\":"), &mode) ||
          !MapInjectionModeCode(mode, &command->injection_mode) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"du\":"), &duration_ms)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->duration_ms = duration_ms;
      return true;
    }
    case CommandType::kSampleCollect:
    case CommandType::kSampleStop:
    case CommandType::kInjectionRun:
    case CommandType::kDropletMake:
    case CommandType::kReplenishRun:
    case CommandType::kDrainRun:
    case CommandType::kCarouselHome:
      return true;
    case CommandType::kProtocolBegin: {
      uint32_t slot_count = 0U;
      uint32_t bge1_slot = 0U;
      uint32_t bge2_slot = 0U;
      uint32_t sample_1_slot = 0U;
      uint32_t sample_count = 0U;
      uint32_t repetitions = 0U;
      uint32_t mode = 0U;
      uint32_t collection_duration_ms = 0U;
      uint32_t injection_duration_ms = 0U;
      uint32_t droplet_duration_ms = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"sc\":"), &slot_count) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"b1\":"), &bge1_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"b2\":"), &bge2_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"s1\":"), &sample_1_slot) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"sn\":"), &sample_count) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"rr\":"), &repetitions) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"im\":"), &mode) ||
          !MapInjectionModeCode(mode, &command->protocol_metadata.injection_mode) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"cd\":"), &collection_duration_ms) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"jd\":"), &injection_duration_ms) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"dd\":"), &droplet_duration_ms) ||
          (slot_count == 0U) || (slot_count > kMaxSupportedSlots) ||
          (bge1_slot >= slot_count) || (bge2_slot >= slot_count) ||
          (sample_1_slot >= slot_count) || (sample_count == 0U) ||
          (repetitions == 0U) || (collection_duration_ms == 0U) ||
          (injection_duration_ms == 0U) || (droplet_duration_ms == 0U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }

      command->protocol_metadata.slot_count = static_cast<uint8_t>(slot_count);
      command->protocol_metadata.bge1_slot = static_cast<uint8_t>(bge1_slot);
      command->protocol_metadata.bge2_slot = static_cast<uint8_t>(bge2_slot);
      command->protocol_metadata.sample_1_slot =
          static_cast<uint8_t>(sample_1_slot);
      command->protocol_metadata.sample_count =
          static_cast<uint8_t>(sample_count);
      command->protocol_metadata.repetitions =
          static_cast<uint8_t>(repetitions);
      command->protocol_metadata.collection_duration_ms = collection_duration_ms;
      command->protocol_metadata.injection_duration_ms = injection_duration_ms;
      command->protocol_metadata.droplet_duration_ms = droplet_duration_ms;

      for (uint8_t index = 0U; index < kProtocolWaitPresetCount; ++index) {
        if (!ParseCompactWaitPresetMs(
                json, index,
                &command->protocol_metadata.wait_times_100ms[index])) {
          result->error = ErrorCode::kInvalidField;
          return false;
        }
      }
      return true;
    }
    case CommandType::kProtocolChunk: {
      uint32_t offset = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"of\":"), &offset) ||
          (offset > kProtocolProgramMaxLength) ||
          !ParseHexDataField(json, CE_FLASH_LITERAL("\"dt\":"),
                             command->chunk_data, kProtocolChunkDataBytes)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->offset = static_cast<uint16_t>(offset);
      command->chunk_length = static_cast<uint8_t>(kProtocolChunkDataBytes);
      return true;
    }
    case CommandType::kProtocolCommit: {
      uint32_t prepare_length = 0U;
      uint32_t length = 0U;
      uint32_t crc = 0U;
      if (!ParseUintField(json, CE_FLASH_LITERAL("\"pl\":"), &prepare_length) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"ln\":"), &length) ||
          (prepare_length == 0U) || (prepare_length >= length) ||
          (length == 0U) || (length > kProtocolProgramMaxLength) ||
          !ParseUintField(json, CE_FLASH_LITERAL("\"cr\":"), &crc) ||
          (crc > 65535U)) {
        result->error = ErrorCode::kInvalidField;
        return false;
      }
      command->prepare_length = static_cast<uint16_t>(prepare_length);
      command->program_length = static_cast<uint16_t>(length);
      command->crc16 = static_cast<uint16_t>(crc);
      return true;
    }
    case CommandType::kRunStart:
    case CommandType::kRunStop:
    case CommandType::kRunStatus:
    case CommandType::kProtocolInfo:
    case CommandType::kProtocolClear:
    case CommandType::kStatusGet:
      return true;
    default:
      result->error = ErrorCode::kInvalidCommand;
      return false;
  }
}

}  // namespace

__attribute__((noinline)) ParseResult ParseCommandJson(const char* json,
                                                       ParsedCommand* command) {
  ParseResult result = {false, ErrorCode::kInvalidJson, 0U};
  if ((json == nullptr) || (command == nullptr)) {
    return result;
  }

  InitializeParsedCommandDefaults(command);

  uint32_t version = 0U;
  if (!ParseUintField(json, CE_FLASH_LITERAL("\"v\":"), &version)) {
    result.error = ErrorCode::kInvalidField;
    return result;
  }

#if CE_CUBE_ENABLE_LEGACY_V1_COMMANDS
  if (version == kLegacyProtocolVersion) {
    if (ContainsCompactCommandFields(json)) {
      result.error = ErrorCode::kInvalidField;
      return result;
    }
    if (!ParseCommandJsonV1(json, command, &result)) {
      if (result.error == ErrorCode::kNone) {
        result.error = ErrorCode::kInvalidField;
      }
      return result;
    }
  } else if (version == kProtocolVersion) {
    if (ContainsLegacyCommandFields(json)) {
      result.error = ErrorCode::kInvalidField;
      return result;
    }
    if (!ParseCommandJsonV2(json, command, &result)) {
      if (result.error == ErrorCode::kNone) {
        result.error = ErrorCode::kInvalidField;
      }
      return result;
    }
  } else {
    result.error = ErrorCode::kInvalidField;
    return result;
  }
#else
  if (version != kProtocolVersion) {
    result.error = ErrorCode::kInvalidField;
    return result;
  }
  if (!ParseCommandJsonV2(json, command, &result)) {
    if (result.error == ErrorCode::kNone) {
      result.error = ErrorCode::kInvalidField;
    }
    return result;
  }
#endif

  result.ok = true;
  result.error = ErrorCode::kNone;
  return result;
}

bool EncodeTelemetryJson(const TelemetrySnapshot& snapshot, char* buffer,
                         size_t buffer_size, size_t* out_length) {
  if ((buffer == nullptr) || (buffer_size == 0U)) {
    return false;
  }

  size_t index = 0U;
  buffer[0] = '\0';
  const bool ok =
      EncodeMessageBase(MessageKind::kTelemetry, snapshot.seq, buffer,
                        buffer_size, &index) &&
      AppendTimingFields(snapshot.uptime_ms, snapshot.analysis_time_ms,
                         snapshot.analysis_time_valid, buffer, buffer_size,
                         &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"cp\":"), buffer, buffer_size,
                         &index) &&
      AppendFixed(snapshot.cap_pf, 6U, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"tc\":"), buffer, buffer_size,
                         &index) &&
      AppendFixed(snapshot.temp_c, 3U, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"ss\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(snapshot.sensor_status), buffer,
                 buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"pp\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.pressure_pa, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"ps\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(snapshot.pressure_status), buffer,
                 buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"cu\":"), buffer, buffer_size,
                         &index) &&
      AppendInt(snapshot.current_ua, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"hv\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.hv_enabled ? 1U : 0U, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"pm\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.pump_enabled ? 1U : 0U, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"v1\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.valve1_enabled ? 1U : 0U, buffer, buffer_size,
                 &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"v2\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.valve2_enabled ? 1U : 0U, buffer, buffer_size,
                 &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"pv\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.protocol_valid ? 1U : 0U, buffer, buffer_size,
                 &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"lf\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(snapshot.lift_state), buffer, buffer_size,
                 &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"cs\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.carousel_position, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"rs\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(snapshot.run_state), buffer, buffer_size,
                 &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"pc\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.run_pc, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"si\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.sample_index, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"sl\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.sample_slot, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"ri\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.repetition, buffer, buffer_size, &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"ff\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(snapshot.faults, buffer, buffer_size, &index) &&
      AppendChar('}', buffer, buffer_size, &index);

  if (!ok) {
    return false;
  }
  if (out_length != nullptr) {
    *out_length = index;
  }
  return true;
}

bool EncodeAckJson(const ReplyMessage& reply, char* buffer, size_t buffer_size,
                   size_t* out_length) {
  if ((reply.kind != MessageKind::kAck) || (buffer == nullptr) ||
      (buffer_size == 0U)) {
    return false;
  }

  size_t index = 0U;
  buffer[0] = '\0';
  const bool ok =
      EncodeMessageBase(MessageKind::kAck, reply.seq, buffer, buffer_size,
                        &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"a\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(reply.ack_code), buffer, buffer_size,
                 &index) &&
      AppendUptimeField(reply.uptime_ms, buffer, buffer_size, &index) &&
      AppendChar('}', buffer, buffer_size, &index);

  if (!ok) {
    return false;
  }
  if (out_length != nullptr) {
    *out_length = index;
  }
  return true;
}

bool EncodeErrorJson(const ReplyMessage& reply, char* buffer, size_t buffer_size,
                     size_t* out_length) {
  if ((reply.kind != MessageKind::kError) || (buffer == nullptr) ||
      (buffer_size == 0U)) {
    return false;
  }

  size_t index = 0U;
  buffer[0] = '\0';
  const bool ok =
      EncodeMessageBase(MessageKind::kError, reply.seq, buffer, buffer_size,
                        &index) &&
      AppendFlashLiteral(CE_FLASH_LITERAL(",\"e\":"), buffer, buffer_size,
                         &index) &&
      AppendUInt(static_cast<uint8_t>(reply.error_code), buffer, buffer_size,
                 &index) &&
      AppendUptimeField(reply.uptime_ms, buffer, buffer_size, &index) &&
      AppendChar('}', buffer, buffer_size, &index);

  if (!ok) {
    return false;
  }
  if (out_length != nullptr) {
    *out_length = index;
  }
  return true;
}

bool EncodeEventJson(const EventSnapshot& event, uint16_t seq, char* buffer,
                     size_t buffer_size, size_t* out_length) {
  if ((buffer == nullptr) || (buffer_size == 0U) ||
      (event.code == EventCode::kNone)) {
    return false;
  }

  size_t index = 0U;
  buffer[0] = '\0';
  bool ok = EncodeMessageBase(MessageKind::kEvent, seq, buffer, buffer_size,
                              &index) &&
            AppendFlashLiteral(CE_FLASH_LITERAL(",\"ev\":"), buffer,
                               buffer_size, &index) &&
            AppendUInt(static_cast<uint8_t>(event.code), buffer, buffer_size,
                       &index) &&
            AppendTimingFields(event.uptime_ms, event.analysis_time_ms,
                               event.analysis_time_valid, buffer, buffer_size,
                               &index);
  if (!ok) {
    return false;
  }

  if (event.code == EventCode::kProtocolInfo) {
#if CE_CUBE_ENABLE_PROTOCOL_INFO
    ok = ok && AppendFlashLiteral(CE_FLASH_LITERAL(",\"pv\":"), buffer,
                                  buffer_size, &index) &&
         AppendUInt(event.protocol_info.valid ? 1U : 0U, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"sc\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.slot_count, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"b1\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.bge1_slot, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"b2\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.bge2_slot, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"s1\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.sample_1_slot, buffer,
                    buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"sn\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.sample_count, buffer,
                    buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"rr\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.repetitions, buffer,
                    buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"im\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(static_cast<uint8_t>(event.protocol_info.metadata.injection_mode),
                    buffer, buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"cd\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.collection_duration_ms, buffer,
                    buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"jd\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.injection_duration_ms, buffer,
                    buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"dd\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.metadata.droplet_duration_ms, buffer,
                    buffer_size, &index);
    for (uint8_t wait_index = 0U; wait_index < kProtocolWaitPresetCount;
         ++wait_index) {
      ok = ok &&
           AppendFlashLiteral(CompactEventWaitPrefix(wait_index), buffer,
                              buffer_size, &index) &&
           AppendUInt(static_cast<uint32_t>(
                          event.protocol_info.metadata.wait_times_100ms[wait_index]) *
                          100U,
                      buffer, buffer_size, &index);
    }
    ok = ok && AppendFlashLiteral(CE_FLASH_LITERAL(",\"pl\":"), buffer,
                                  buffer_size, &index) &&
         AppendUInt(event.protocol_info.prepare_length, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"ln\":"), buffer,
                                  buffer_size, &index) &&
         AppendUInt(event.protocol_info.program_length, buffer, buffer_size,
                    &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"cr\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.protocol_info.program_crc16, buffer, buffer_size,
                    &index);
#else
    return false;
#endif
  } else {
    ok = ok && AppendFlashLiteral(CE_FLASH_LITERAL(",\"si\":"), buffer,
                                  buffer_size, &index) &&
         AppendUInt(event.sample_index, buffer, buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"sl\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.sample_slot, buffer, buffer_size, &index) &&
         AppendFlashLiteral(CE_FLASH_LITERAL(",\"ri\":"), buffer, buffer_size,
                            &index) &&
         AppendUInt(event.repetition, buffer, buffer_size, &index);
  }

  ok = ok && AppendChar('}', buffer, buffer_size, &index);
  if (!ok) {
    return false;
  }
  if (out_length != nullptr) {
    *out_length = index;
  }
  return true;
}

}  // namespace ce_cube
