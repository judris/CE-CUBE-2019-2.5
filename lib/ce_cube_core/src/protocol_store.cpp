#include "ce_cube/protocol_store.hpp"

#include <string.h>

#include "ce_cube/crc16.hpp"

#if defined(ARDUINO)
#include <EEPROM.h>
#endif

namespace ce_cube {
namespace {

static constexpr uint8_t kHeaderMagic0 = static_cast<uint8_t>('C');
static constexpr uint8_t kHeaderMagic1 = static_cast<uint8_t>('E');
static constexpr uint8_t kHeaderMagic2 = static_cast<uint8_t>('C');
static constexpr uint8_t kHeaderMagic3 = static_cast<uint8_t>('2');
static constexpr uint8_t kHeaderValidFlag = 0xA5U;
static constexpr uint8_t kHeaderVersion = 3U;
static constexpr size_t kHeaderCrcOffset = 48U;
static constexpr size_t kHeaderReservedOffset = 50U;
static constexpr size_t kPrepareLengthOffset = 14U;
static constexpr size_t kProgramLengthOffset = 16U;
static constexpr size_t kProgramCrcOffset = 18U;
static constexpr size_t kCollectionDurationOffset = 20U;
static constexpr size_t kInjectionDurationOffset = 24U;
static constexpr size_t kDropletDurationOffset = 28U;
static constexpr size_t kWaitTimesOffset = 32U;
static constexpr uint32_t kDefaultCollectionDurationMs = 10000UL;
static constexpr uint32_t kDefaultInjectionDurationMs = 10000UL;
static constexpr uint32_t kDefaultDropletDurationMs = 5000UL;

#if !defined(ARDUINO)
static uint8_t g_test_storage[kProtocolEepromSize] = {0U};
#endif

uint16_t ReadLittleEndian16(const uint8_t* data, size_t offset) {
  return static_cast<uint16_t>(
      static_cast<uint16_t>(data[offset]) |
      static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1U]) << 8U));
}

uint32_t ReadLittleEndian32(const uint8_t* data, size_t offset) {
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1U]) << 8U) |
         (static_cast<uint32_t>(data[offset + 2U]) << 16U) |
         (static_cast<uint32_t>(data[offset + 3U]) << 24U);
}

void WriteLittleEndian16(uint16_t value, uint8_t* data, size_t offset) {
  data[offset] = static_cast<uint8_t>(value & 0xFFU);
  data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void WriteLittleEndian32(uint32_t value, uint8_t* data, size_t offset) {
  data[offset] = static_cast<uint8_t>(value & 0xFFU);
  data[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  data[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  data[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

bool IsWaitOpcode(uint8_t opcode) {
  return (opcode >= static_cast<uint8_t>(ProtocolOpcode::kWaitBase)) &&
         (opcode <= static_cast<uint8_t>(ProtocolOpcode::kWaitMax));
}

bool IsStructuralOpcodeValid(uint8_t opcode) {
  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kEndSection:
    case ProtocolOpcode::kNop:
    case ProtocolOpcode::kLiftDown:
    case ProtocolOpcode::kLiftUp:
    case ProtocolOpcode::kAutoZero:
    case ProtocolOpcode::kGotoBge1:
    case ProtocolOpcode::kGotoBge2:
    case ProtocolOpcode::kGotoSampleCurrent:
    case ProtocolOpcode::kHvOn:
    case ProtocolOpcode::kHvOff:
    case ProtocolOpcode::kPumpOn:
    case ProtocolOpcode::kPumpOff:
    case ProtocolOpcode::kValve1On:
    case ProtocolOpcode::kValve1Off:
    case ProtocolOpcode::kValve2On:
    case ProtocolOpcode::kValve2Off:
    case ProtocolOpcode::kPressureSample:
    case ProtocolOpcode::kEventRunStart:
    case ProtocolOpcode::kEventRunStop:
    case ProtocolOpcode::kEventSampleReady:
    case ProtocolOpcode::kCollectSample:
    case ProtocolOpcode::kInjectSample:
    case ProtocolOpcode::kMakeBgeDroplet:
    case ProtocolOpcode::kReplenish:
    case ProtocolOpcode::kDrain:
    case ProtocolOpcode::kWaitBase:
    case ProtocolOpcode::kWaitMax:
    case ProtocolOpcode::kGotoSlotBase:
    case ProtocolOpcode::kGotoSlotMax:
      return true;
    default:
      break;
  }

  if (IsWaitOpcode(opcode)) {
    return true;
  }

  return (opcode >= static_cast<uint8_t>(ProtocolOpcode::kGotoSlotBase)) &&
         (opcode <= static_cast<uint8_t>(ProtocolOpcode::kGotoSlotMax));
}

ProtocolInfo InvalidProtocolInfo() {
  ProtocolInfo info = {};
  info.valid = false;
  info.metadata.slot_count = kDefaultSlotCount;
  info.metadata.bge1_slot = 0U;
  info.metadata.bge2_slot = 1U;
  info.metadata.sample_1_slot = 5U;
  info.metadata.sample_count = 1U;
  info.metadata.repetitions = 1U;
  info.metadata.injection_mode = InjectionMode::kElectrokinetic;
  info.metadata.collection_duration_ms = kDefaultCollectionDurationMs;
  info.metadata.injection_duration_ms = kDefaultInjectionDurationMs;
  info.metadata.droplet_duration_ms = kDefaultDropletDurationMs;
  for (uint8_t index = 0U; index < kProtocolWaitPresetCount; ++index) {
    info.metadata.wait_times_100ms[index] = 0U;
  }
  info.prepare_length = 0U;
  info.program_length = 0U;
  info.program_crc16 = 0U;
  return info;
}

}  // namespace

ProtocolStore::ProtocolStore() {}

void ProtocolStore::Begin() {}

ProtocolStoreStatus ProtocolStore::Clear() {
  uint8_t header_bytes[kProtocolHeaderSize] = {0U};
  return WriteHeaderBytes(header_bytes, sizeof(header_bytes));
}

ProtocolStoreStatus ProtocolStore::BeginUpload(const ProtocolMetadata& metadata) {
  const ProtocolStoreStatus metadata_status = ValidateMetadata(metadata);
  if (metadata_status != ProtocolStoreStatus::kOk) {
    return metadata_status;
  }

  ProtocolInfo info = {};
  info.valid = false;
  info.metadata = metadata;
  info.prepare_length = 0U;
  info.program_length = 0U;
  info.program_crc16 = 0U;

  uint8_t header_bytes[kProtocolHeaderSize] = {0U};
  const ProtocolStoreStatus encode_status =
      EncodeHeader(info, 0U, header_bytes, sizeof(header_bytes));
  if (encode_status != ProtocolStoreStatus::kOk) {
    return encode_status;
  }

  return WriteHeaderBytes(header_bytes, sizeof(header_bytes));
}

ProtocolStoreStatus ProtocolStore::WriteChunk(uint16_t offset, const uint8_t* data,
                                              uint8_t length) {
  if ((data == nullptr) || (length == 0U)) {
    return ProtocolStoreStatus::kInvalidArg;
  }
  if ((offset >= kProtocolProgramMaxLength) ||
      (static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
       static_cast<uint32_t>(kProtocolProgramMaxLength))) {
    return ProtocolStoreStatus::kBounds;
  }

  for (uint8_t index = 0U; index < length; ++index) {
    const ProtocolStoreStatus write_status =
        WriteByte(static_cast<uint16_t>(kProtocolProgramOffset + offset + index),
                  data[index]);
    if (write_status != ProtocolStoreStatus::kOk) {
      return write_status;
    }
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::Commit(uint16_t prepare_length,
                                          uint16_t program_length,
                                          uint16_t program_crc) {
  uint8_t header_bytes[kProtocolHeaderSize] = {0U};
  ProtocolStoreStatus header_status =
      ReadHeaderBytes(header_bytes, sizeof(header_bytes));
  if (header_status != ProtocolStoreStatus::kOk) {
    return header_status;
  }

  ProtocolInfo info = InvalidProtocolInfo();
  header_status = DecodeHeader(header_bytes, &info);
  if (header_status != ProtocolStoreStatus::kOk) {
    return header_status;
  }

  info.prepare_length = prepare_length;
  info.program_length = program_length;
  info.program_crc16 = program_crc;

  const ProtocolStoreStatus validate_status = ValidateProgram(
      info.metadata, prepare_length, program_length, program_crc);
  if (validate_status != ProtocolStoreStatus::kOk) {
    return validate_status;
  }

  header_status =
      EncodeHeader(info, kHeaderValidFlag, header_bytes, sizeof(header_bytes));
  if (header_status != ProtocolStoreStatus::kOk) {
    return header_status;
  }

  return WriteHeaderBytes(header_bytes, sizeof(header_bytes));
}

ProtocolStoreStatus ProtocolStore::LoadInfo(ProtocolInfo* info) const {
  if (info == nullptr) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  uint8_t header_bytes[kProtocolHeaderSize] = {0U};
  ProtocolStoreStatus status = ReadHeaderBytes(header_bytes, sizeof(header_bytes));
  if (status != ProtocolStoreStatus::kOk) {
    return status;
  }

  status = DecodeHeader(header_bytes, info);
  if (status != ProtocolStoreStatus::kOk) {
    return status;
  }
  if (!info->valid) {
    return ProtocolStoreStatus::kInvalidImage;
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::ReadOpcode(uint16_t offset, uint8_t* opcode) const {
  if (opcode == nullptr) {
    return ProtocolStoreStatus::kInvalidArg;
  }
  if (offset >= kProtocolProgramMaxLength) {
    return ProtocolStoreStatus::kBounds;
  }
  return ReadByte(static_cast<uint16_t>(kProtocolProgramOffset + offset), opcode);
}

ProtocolStoreStatus ProtocolStore::ReadWaitTimeMs(uint8_t index,
                                                  uint32_t* wait_ms) const {
  if ((wait_ms == nullptr) || (index >= kProtocolWaitPresetCount)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  ProtocolInfo info = InvalidProtocolInfo();
  const ProtocolStoreStatus status = LoadInfo(&info);
  if (status != ProtocolStoreStatus::kOk) {
    return status;
  }

  *wait_ms = static_cast<uint32_t>(info.metadata.wait_times_100ms[index]) * 100U;
  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::EnsureDefaultProtocol(
    const ProtocolMetadata& metadata, uint16_t prepare_length,
    const uint8_t* program, uint16_t program_length) {
  if ((program == nullptr) || (program_length == 0U)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  ProtocolInfo info = InvalidProtocolInfo();
  if (LoadInfo(&info) == ProtocolStoreStatus::kOk) {
    return ProtocolStoreStatus::kOk;
  }

  ProtocolStoreStatus status = BeginUpload(metadata);
  if (status != ProtocolStoreStatus::kOk) {
    return status;
  }

  status = WriteChunk(0U, program, static_cast<uint8_t>(program_length));
  if (status != ProtocolStoreStatus::kOk) {
    return status;
  }

  const uint16_t crc = ComputeCrc16Ccitt(program, program_length);
  return Commit(prepare_length, program_length, crc);
}

bool ProtocolStore::IsOpcodeValid(uint8_t opcode) {
  return IsStructuralOpcodeValid(opcode);
}

bool ProtocolStore::IsDirectSlotOpcode(uint8_t opcode) {
  return (opcode >= static_cast<uint8_t>(ProtocolOpcode::kGotoSlotBase)) &&
         (opcode <= static_cast<uint8_t>(ProtocolOpcode::kGotoSlotMax));
}

uint8_t ProtocolStore::SlotFromOpcode(uint8_t opcode) {
  return static_cast<uint8_t>(opcode -
                              static_cast<uint8_t>(ProtocolOpcode::kGotoSlotBase));
}

#if !defined(ARDUINO)
void ProtocolStore::ResetTestStorage() {
  for (uint16_t index = 0U; index < kProtocolEepromSize; ++index) {
    g_test_storage[index] = 0U;
  }
}
#endif

ProtocolStoreStatus ProtocolStore::ReadHeaderBytes(uint8_t* header_bytes,
                                                   size_t header_size) const {
  if ((header_bytes == nullptr) || (header_size < kProtocolHeaderSize)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  for (uint16_t index = 0U; index < kProtocolHeaderSize; ++index) {
    const ProtocolStoreStatus status = ReadByte(index, &header_bytes[index]);
    if (status != ProtocolStoreStatus::kOk) {
      return status;
    }
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::WriteHeaderBytes(const uint8_t* header_bytes,
                                                    size_t header_size) {
  if ((header_bytes == nullptr) || (header_size < kProtocolHeaderSize)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  for (uint16_t index = 0U; index < kProtocolHeaderSize; ++index) {
    const ProtocolStoreStatus status = WriteByte(index, header_bytes[index]);
    if (status != ProtocolStoreStatus::kOk) {
      return status;
    }
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::DecodeHeader(const uint8_t* header_bytes,
                                                ProtocolInfo* info) const {
  if ((header_bytes == nullptr) || (info == nullptr)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  *info = InvalidProtocolInfo();
  if ((header_bytes[0] != kHeaderMagic0) || (header_bytes[1] != kHeaderMagic1) ||
      (header_bytes[2] != kHeaderMagic2) || (header_bytes[3] != kHeaderMagic3) ||
      (header_bytes[4] != kHeaderVersion)) {
    return ProtocolStoreStatus::kInvalidImage;
  }

  uint8_t crc_buffer[kProtocolHeaderSize] = {0U};
  memcpy(crc_buffer, header_bytes, sizeof(crc_buffer));
  crc_buffer[kHeaderCrcOffset] = 0U;
  crc_buffer[kHeaderCrcOffset + 1U] = 0U;
  const uint16_t expected_crc = ReadLittleEndian16(header_bytes, kHeaderCrcOffset);
  const uint16_t actual_crc = ComputeCrc16Ccitt(crc_buffer, sizeof(crc_buffer));
  if (expected_crc != actual_crc) {
    return ProtocolStoreStatus::kCrc;
  }

  info->valid = (header_bytes[5] == kHeaderValidFlag);
  info->metadata.slot_count = header_bytes[6];
  info->metadata.bge1_slot = header_bytes[7];
  info->metadata.bge2_slot = header_bytes[8];
  info->metadata.sample_1_slot = header_bytes[9];
  info->metadata.sample_count = header_bytes[10];
  info->metadata.repetitions = header_bytes[11];
  info->metadata.injection_mode =
      static_cast<InjectionMode>(header_bytes[12]);
  info->prepare_length = ReadLittleEndian16(header_bytes, kPrepareLengthOffset);
  info->program_length = ReadLittleEndian16(header_bytes, kProgramLengthOffset);
  info->program_crc16 = ReadLittleEndian16(header_bytes, kProgramCrcOffset);
  info->metadata.collection_duration_ms =
      ReadLittleEndian32(header_bytes, kCollectionDurationOffset);
  info->metadata.injection_duration_ms =
      ReadLittleEndian32(header_bytes, kInjectionDurationOffset);
  info->metadata.droplet_duration_ms =
      ReadLittleEndian32(header_bytes, kDropletDurationOffset);
  for (uint8_t index = 0U; index < kProtocolWaitPresetCount; ++index) {
    const size_t wait_offset = kWaitTimesOffset + static_cast<size_t>(index) * 2U;
    info->metadata.wait_times_100ms[index] =
        ReadLittleEndian16(header_bytes, wait_offset);
  }

  if (ValidateMetadata(info->metadata) != ProtocolStoreStatus::kOk) {
    return ProtocolStoreStatus::kInvalidImage;
  }
  if (!((info->metadata.injection_mode == InjectionMode::kHydrodynamic) ||
        (info->metadata.injection_mode == InjectionMode::kElectrokinetic))) {
    return ProtocolStoreStatus::kInvalidImage;
  }
  if (info->program_length > kProtocolProgramMaxLength) {
    return ProtocolStoreStatus::kInvalidImage;
  }
  if ((info->prepare_length > 0U) && (info->prepare_length >= info->program_length) &&
      (info->program_length != 0U)) {
    return ProtocolStoreStatus::kInvalidImage;
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::EncodeHeader(const ProtocolInfo& info,
                                                uint8_t valid_flag,
                                                uint8_t* header_bytes,
                                                size_t header_size) const {
  if ((header_bytes == nullptr) || (header_size < kProtocolHeaderSize)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

  const ProtocolStoreStatus metadata_status = ValidateMetadata(info.metadata);
  if (metadata_status != ProtocolStoreStatus::kOk) {
    return metadata_status;
  }
  if (info.program_length > kProtocolProgramMaxLength) {
    return ProtocolStoreStatus::kBounds;
  }
  if ((info.prepare_length >= info.program_length) && (info.program_length != 0U)) {
    return ProtocolStoreStatus::kBounds;
  }

  memset(header_bytes, 0, header_size);
  header_bytes[0] = kHeaderMagic0;
  header_bytes[1] = kHeaderMagic1;
  header_bytes[2] = kHeaderMagic2;
  header_bytes[3] = kHeaderMagic3;
  header_bytes[4] = kHeaderVersion;
  header_bytes[5] = valid_flag;
  header_bytes[6] = info.metadata.slot_count;
  header_bytes[7] = info.metadata.bge1_slot;
  header_bytes[8] = info.metadata.bge2_slot;
  header_bytes[9] = info.metadata.sample_1_slot;
  header_bytes[10] = info.metadata.sample_count;
  header_bytes[11] = info.metadata.repetitions;
  header_bytes[12] = static_cast<uint8_t>(info.metadata.injection_mode);
  header_bytes[13] = 0U;
  WriteLittleEndian16(info.prepare_length, header_bytes, kPrepareLengthOffset);
  WriteLittleEndian16(info.program_length, header_bytes, kProgramLengthOffset);
  WriteLittleEndian16(info.program_crc16, header_bytes, kProgramCrcOffset);
  WriteLittleEndian32(info.metadata.collection_duration_ms, header_bytes,
                      kCollectionDurationOffset);
  WriteLittleEndian32(info.metadata.injection_duration_ms, header_bytes,
                      kInjectionDurationOffset);
  WriteLittleEndian32(info.metadata.droplet_duration_ms, header_bytes,
                      kDropletDurationOffset);
  for (uint8_t index = 0U; index < kProtocolWaitPresetCount; ++index) {
    const size_t wait_offset = kWaitTimesOffset + static_cast<size_t>(index) * 2U;
    WriteLittleEndian16(info.metadata.wait_times_100ms[index], header_bytes,
                        wait_offset);
  }
  WriteLittleEndian16(0U, header_bytes, kHeaderCrcOffset);
  for (size_t index = kHeaderReservedOffset; index < kProtocolHeaderSize; ++index) {
    header_bytes[index] = 0U;
  }

  const uint16_t header_crc = ComputeCrc16Ccitt(header_bytes, kProtocolHeaderSize);
  WriteLittleEndian16(header_crc, header_bytes, kHeaderCrcOffset);
  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::ReadByte(uint16_t address, uint8_t* value) const {
  if ((value == nullptr) || (address >= kProtocolEepromSize)) {
    return ProtocolStoreStatus::kInvalidArg;
  }

#if defined(ARDUINO)
  *value = EEPROM.read(address);
#else
  *value = g_test_storage[address];
#endif
  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::WriteByte(uint16_t address, uint8_t value) {
  if (address >= kProtocolEepromSize) {
    return ProtocolStoreStatus::kInvalidArg;
  }

#if defined(ARDUINO)
  EEPROM.update(address, value);
#else
  g_test_storage[address] = value;
#endif
  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::ValidateMetadata(
    const ProtocolMetadata& metadata) const {
  if ((metadata.slot_count == 0U) || (metadata.slot_count > kMaxSupportedSlots)) {
    return ProtocolStoreStatus::kBounds;
  }
  if ((metadata.sample_count == 0U) || (metadata.repetitions == 0U)) {
    return ProtocolStoreStatus::kBounds;
  }
  if ((metadata.bge1_slot >= metadata.slot_count) ||
      (metadata.bge2_slot >= metadata.slot_count) ||
      (metadata.sample_1_slot >= metadata.slot_count)) {
    return ProtocolStoreStatus::kBounds;
  }
  if ((metadata.collection_duration_ms == 0U) ||
      (metadata.injection_duration_ms == 0U) ||
      (metadata.droplet_duration_ms == 0U)) {
    return ProtocolStoreStatus::kBounds;
  }
  if (!((metadata.injection_mode == InjectionMode::kHydrodynamic) ||
        (metadata.injection_mode == InjectionMode::kElectrokinetic))) {
    return ProtocolStoreStatus::kBounds;
  }

  const uint32_t last_sample =
      static_cast<uint16_t>(metadata.sample_1_slot) +
      static_cast<uint16_t>(metadata.sample_count) - 1U;
  if (last_sample >= metadata.slot_count) {
    return ProtocolStoreStatus::kBounds;
  }

  return ProtocolStoreStatus::kOk;
}

ProtocolStoreStatus ProtocolStore::ValidateProgram(
    const ProtocolMetadata& metadata, uint16_t prepare_length,
    uint16_t program_length, uint16_t expected_crc) const {
  if ((prepare_length == 0U) || (prepare_length >= program_length) ||
      (program_length > kProtocolProgramMaxLength)) {
    return ProtocolStoreStatus::kBounds;
  }

  uint8_t opcode = 0U;
  uint16_t running_crc = 0xFFFFU;
  for (uint16_t offset = 0U; offset < program_length; ++offset) {
    const ProtocolStoreStatus status = ReadOpcode(offset, &opcode);
    if (status != ProtocolStoreStatus::kOk) {
      return status;
    }
    running_crc = UpdateCrc16Ccitt(running_crc, opcode);

    if (!IsOpcodeValid(opcode)) {
      return ProtocolStoreStatus::kInvalidImage;
    }
    if (IsDirectSlotOpcode(opcode) &&
        (SlotFromOpcode(opcode) >= metadata.slot_count)) {
      return ProtocolStoreStatus::kBounds;
    }
    if (opcode == static_cast<uint8_t>(ProtocolOpcode::kEndSection)) {
      const bool valid_prepare_end =
          (offset == static_cast<uint16_t>(prepare_length - 1U));
      const bool valid_cycle_end =
          (offset == static_cast<uint16_t>(program_length - 1U));
      if (!valid_prepare_end && !valid_cycle_end) {
        return ProtocolStoreStatus::kInvalidImage;
      }
    }
  }

  if (running_crc != expected_crc) {
    return ProtocolStoreStatus::kCrc;
  }

  if (ReadOpcode(static_cast<uint16_t>(prepare_length - 1U), &opcode) !=
      ProtocolStoreStatus::kOk) {
    return ProtocolStoreStatus::kIo;
  }
  if (opcode != static_cast<uint8_t>(ProtocolOpcode::kEndSection)) {
    return ProtocolStoreStatus::kInvalidImage;
  }
  if (ReadOpcode(static_cast<uint16_t>(program_length - 1U), &opcode) !=
      ProtocolStoreStatus::kOk) {
    return ProtocolStoreStatus::kIo;
  }
  if (opcode != static_cast<uint8_t>(ProtocolOpcode::kEndSection)) {
    return ProtocolStoreStatus::kInvalidImage;
  }

  return ProtocolStoreStatus::kOk;
}

ErrorCode ProtocolStoreStatusToErrorCode(ProtocolStoreStatus status) {
  switch (status) {
    case ProtocolStoreStatus::kOk:
      return ErrorCode::kNone;
    case ProtocolStoreStatus::kBounds:
      return ErrorCode::kProtocolBounds;
    case ProtocolStoreStatus::kCrc:
      return ErrorCode::kProtocolCrc;
    case ProtocolStoreStatus::kInvalidArg:
    case ProtocolStoreStatus::kInvalidImage:
      return ErrorCode::kProtocolInvalid;
    case ProtocolStoreStatus::kIo:
      return ErrorCode::kStorage;
    default:
      return ErrorCode::kStorage;
  }
}

}  // namespace ce_cube
