#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

static constexpr uint16_t kProtocolEepromSize = 0x0400U;
static constexpr uint16_t kProtocolHeaderSize = 0x0040U;
static constexpr uint16_t kProtocolProgramOffset = 0x0040U;
static constexpr uint16_t kProtocolProgramLimit = 0x03C0U;
static constexpr uint16_t kProtocolProgramMaxLength =
    kProtocolProgramLimit - kProtocolProgramOffset;

enum class ProtocolOpcode : uint8_t {
  kEndSection = 0x00U,
  kNop = 0x01U,
  kLiftDown = 0x10U,
  kLiftUp = 0x11U,
  kAutoZero = 0x12U,
  kGotoBge1 = 0x20U,
  kGotoBge2 = 0x21U,
  kGotoSampleCurrent = 0x22U,
  kGotoSlotBase = 0x30U,
  kGotoSlotMax = 0x3FU,
  kHvOn = 0x40U,
  kHvOff = 0x41U,
  kPumpOn = 0x42U,
  kPumpOff = 0x43U,
  kValve1On = 0x44U,
  kValve1Off = 0x45U,
  kValve2On = 0x46U,
  kValve2Off = 0x47U,
  kWaitBase = 0x50U,
  kWaitMax = 0x57U,
  kPressureSample = 0x60U,
  kEventRunStart = 0x61U,
  kEventRunStop = 0x62U,
  kEventSampleReady = 0x63U,
  kCollectSample = 0x70U,
  kInjectSample = 0x71U,
  kMakeBgeDroplet = 0x72U,
  kReplenish = 0x73U,
  kDrain = 0x74U,
};

enum class ProtocolStoreStatus : uint8_t {
  kOk = 0U,
  kInvalidArg,
  kBounds,
  kCrc,
  kInvalidImage,
  kIo,
};

class ProtocolStore {
 public:
  ProtocolStore();

  void Begin();
  ProtocolStoreStatus Clear();
  ProtocolStoreStatus BeginUpload(const ProtocolMetadata& metadata);
  ProtocolStoreStatus WriteChunk(uint16_t offset, const uint8_t* data,
                                 uint8_t length);
  ProtocolStoreStatus Commit(uint16_t prepare_length, uint16_t program_length,
                             uint16_t program_crc);
  ProtocolStoreStatus LoadInfo(ProtocolInfo* info) const;
  ProtocolStoreStatus ReadOpcode(uint16_t offset, uint8_t* opcode) const;
  ProtocolStoreStatus ReadWaitTimeMs(uint8_t index, uint32_t* wait_ms) const;
  ProtocolStoreStatus EnsureDefaultProtocol(const ProtocolMetadata& metadata,
                                            uint16_t prepare_length,
                                            const uint8_t* program,
                                            uint16_t program_length);

  static bool IsOpcodeValid(uint8_t opcode);
  static bool IsDirectSlotOpcode(uint8_t opcode);
  static uint8_t SlotFromOpcode(uint8_t opcode);

#if !defined(ARDUINO)
  static void ResetTestStorage();
#endif

 private:
  ProtocolStoreStatus ReadHeaderBytes(uint8_t* header_bytes,
                                      size_t header_size) const;
  ProtocolStoreStatus WriteHeaderBytes(const uint8_t* header_bytes,
                                       size_t header_size);
  ProtocolStoreStatus DecodeHeader(const uint8_t* header_bytes,
                                   ProtocolInfo* info) const;
  ProtocolStoreStatus EncodeHeader(const ProtocolInfo& info, uint8_t valid_flag,
                                   uint8_t* header_bytes,
                                   size_t header_size) const;
  ProtocolStoreStatus ReadByte(uint16_t address, uint8_t* value) const;
  ProtocolStoreStatus WriteByte(uint16_t address, uint8_t value);
  ProtocolStoreStatus ValidateMetadata(const ProtocolMetadata& metadata) const;
  ProtocolStoreStatus ValidateProgram(const ProtocolMetadata& metadata,
                                      uint16_t prepare_length,
                                      uint16_t program_length,
                                      uint16_t expected_crc) const;
};

ErrorCode ProtocolStoreStatusToErrorCode(ProtocolStoreStatus status);

}  // namespace ce_cube
