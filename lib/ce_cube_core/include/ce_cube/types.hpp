#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ce_cube {

static constexpr uint8_t kLegacyProtocolVersion = 1U;
static constexpr uint8_t kProtocolVersion = 2U;
static constexpr size_t kMaxCommandJsonLength = 240U;
static constexpr size_t kMaxTelemetryJsonLength = 272U;
static constexpr size_t kMaxEventJsonLength = 240U;
static constexpr size_t kMaxRf24FrameSize = 32U;
static constexpr size_t kRf24HeaderSize = 9U;
static constexpr size_t kRf24PayloadSize = kMaxRf24FrameSize - kRf24HeaderSize;
static constexpr size_t kProtocolChunkDataBytes = 8U;
static constexpr size_t kProtocolWaitPresetCount = 8U;
static constexpr uint8_t kDefaultSlotCount = 12U;
static constexpr uint8_t kMaxSupportedSlots = 16U;
static constexpr uint32_t kMaxStoredWaitMs = 6553500U;

enum class MessageKind : uint8_t {
  kInvalid = 0U,
  kTelemetry = 1U,
  kCommand = 2U,
  kAck = 3U,
  kError = 4U,
  kEvent = 5U,
};

enum class MessageSource : uint8_t {
  kUsb = 0U,
  kRf24 = 1U,
};

enum class CommandType : uint8_t {
  kInvalid = 0U,
  kSensorSetRate,
  kSensorSetExcitation,
  kSensorTempComp,
  kSensorAutoZero,
  kLiftMove,
  kCarouselStep,
  kCarouselGotoSlot,
  kCarouselAdjust,
  kHvSet,
  kPumpSet,
  kValveSet,
  kInjectionConfigure,
  kRunStart,
  kRunStop,
  kRunStatus,
  kProtocolBegin,
  kProtocolChunk,
  kProtocolCommit,
  kProtocolInfo,
  kProtocolClear,
  kStatusGet,
  kCollectionConfigure,
  kSampleCollect,
  kSampleStop,
  kInjectionRun,
  kDropletMake,
  kReplenishRun,
  kDrainRun,
  kCarouselHome,
};

enum class SensorStatusCode : uint8_t {
  kOk = 0U,
  kNotReady = 1U,
  kBusError = 2U,
  kConfigError = 3U,
  kInvalidSample = 4U,
};

enum class PressureStatusCode : uint8_t {
  kOk = 0U,
  kUnavailable = 1U,
  kBusError = 2U,
  kConfigError = 3U,
};

enum class LiftPosition : uint8_t {
  kDown = 0U,
  kUp = 1U,
};

enum class LiftStateCode : uint8_t {
  kDown = 0U,
  kMovingUp = 1U,
  kUp = 2U,
  kMovingDown = 3U,
};

enum class RunStateCode : uint8_t {
  kIdle = 0U,
  kRunning = 1U,
  kComplete = 2U,
  kStopped = 3U,
  kFault = 4U,
};

enum class EventCode : uint8_t {
  kNone = 0U,
  kRunStart = 1U,
  kRunStop = 2U,
  kSampleReady = 3U,
  kProtocolInfo = 4U,
};

enum class UpdateRate : uint8_t {
  k9Hz = 0U,
  k11Hz = 1U,
  k13Hz = 2U,
  k16Hz = 3U,
  k26Hz = 4U,
  k50Hz = 5U,
  k84Hz = 6U,
  k91Hz = 7U,
};

enum class ExcitationFrequency : uint8_t {
  k16kHz = 0U,
  k32kHz = 1U,
};

enum class ExcitationLevel : uint8_t {
  kVddDiv8 = 0U,
  kVddDiv4 = 1U,
  kVddTimes3Div8 = 2U,
  kVddDiv2 = 3U,
};

enum class InjectionMode : uint8_t {
  kHydrodynamic = 0U,
  kElectrokinetic = 1U,
};

enum class ErrorCode : uint8_t {
  kNone = 0U,
  kInvalidJson,
  kInvalidField,
  kInvalidCommand,
  kBusy,
  kTransportOverflow,
  kRfCrc,
  kRfFragment,
  kSensorIo,
  kSensorConfig,
  kNotReady,
  kProtocolInvalid,
  kProtocolCrc,
  kProtocolBounds,
  kProtocolOpcode,
  kStorage,
  kUnavailable,
};

enum class AckCode : uint8_t {
  kAccepted = 0U,
  kStatus = 1U,
};

struct SensorDesiredConfig {
  UpdateRate update_rate;
  ExcitationFrequency excitation_frequency;
  ExcitationLevel excitation_level;
  bool temperature_compensation;
};

struct SensorMeasurement {
  float raw_cap_pf;
  float temp_c;
  SensorStatusCode status;
  uint8_t status_register;
  bool valid;
};

struct PressureMeasurement {
  uint32_t pressure_pa;
  PressureStatusCode status;
  bool valid;
};

struct CurrentMeasurement {
  int32_t current_ua;
  bool valid;
};

struct InjectionSettings {
  InjectionMode mode;
  uint32_t duration_ms;
};

struct FluidSettings {
  InjectionMode injection_mode;
  uint32_t collection_duration_ms;
  uint32_t injection_duration_ms;
  uint32_t droplet_duration_ms;
};

struct ProtocolMetadata {
  uint8_t slot_count;
  uint8_t bge1_slot;
  uint8_t bge2_slot;
  uint8_t sample_1_slot;
  uint8_t sample_count;
  uint8_t repetitions;
  InjectionMode injection_mode;
  uint32_t collection_duration_ms;
  uint32_t injection_duration_ms;
  uint32_t droplet_duration_ms;
  uint16_t wait_times_100ms[kProtocolWaitPresetCount];
};

struct ProtocolInfo {
  bool valid;
  ProtocolMetadata metadata;
  uint16_t prepare_length;
  uint16_t program_length;
  uint16_t program_crc16;
};

struct RunProgress {
  bool active;
  RunStateCode state;
  uint16_t program_counter;
  uint8_t sample_index;
  uint8_t sample_slot;
  uint8_t repetition;
  uint8_t active_opcode;
};

struct TelemetrySnapshot {
  uint16_t seq;
  uint32_t uptime_ms;
  float cap_pf;
  float temp_c;
  SensorStatusCode sensor_status;
  uint32_t pressure_pa;
  PressureStatusCode pressure_status;
  int32_t current_ua;
  bool hv_enabled;
  bool pump_enabled;
  bool valve1_enabled;
  bool valve2_enabled;
  bool protocol_valid;
  LiftStateCode lift_state;
  uint8_t carousel_position;
  RunStateCode run_state;
  uint16_t run_pc;
  uint8_t sample_index;
  uint8_t sample_slot;
  uint8_t repetition;
  uint32_t analysis_time_ms;
  bool analysis_time_valid;
  uint16_t faults;
};

struct EventSnapshot {
  EventCode code;
  uint8_t sample_index;
  uint8_t sample_slot;
  uint8_t repetition;
  uint32_t uptime_ms;
  uint32_t analysis_time_ms;
  bool analysis_time_valid;
  ProtocolInfo protocol_info;
};

struct ParsedCommand {
  uint16_t seq;
  CommandType type;
  bool enable;
  bool clear_zero;
  int8_t carousel_steps;
  int8_t adjust_direction;
  uint8_t slot;
  uint8_t valve_id;
  LiftPosition lift_position;
  InjectionMode injection_mode;
  uint32_t duration_ms;
  UpdateRate update_rate;
  ExcitationFrequency excitation_frequency;
  ExcitationLevel excitation_level;
  bool has_excitation_frequency;
  bool has_excitation_level;
  ProtocolMetadata protocol_metadata;
  uint32_t collection_duration_ms;
  uint16_t offset;
  uint8_t chunk_data[kProtocolChunkDataBytes];
  uint8_t chunk_length;
  uint16_t prepare_length;
  uint16_t program_length;
  uint16_t crc16;
};

struct ReplyMessage {
  MessageKind kind;
  uint16_t seq;
  AckCode ack_code;
  ErrorCode error_code;
  uint32_t uptime_ms;
};

struct ParseResult {
  bool ok;
  ErrorCode error;
  uint16_t seq;
};

struct Rf24FrameHeader {
  uint8_t version;
  MessageKind kind;
  uint16_t seq;
  uint8_t fragment_index;
  uint8_t fragment_count;
  uint8_t payload_length;
  uint16_t crc16;
};

enum FaultBit : uint16_t {
  kFaultProtocolParse = 1U << 0,
  kFaultRf24 = 1U << 1,
  kFaultSensor = 1U << 2,
  kFaultRun = 1U << 3,
  kFaultProtocolStore = 1U << 4,
  kFaultPressure = 1U << 5,
};

}  // namespace ce_cube
