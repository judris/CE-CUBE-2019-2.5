#include "ce_cube/instrument_controller.hpp"

#include "ce_cube/crc16.hpp"
#include "ce_cube/feature_flags.hpp"

namespace ce_cube {
namespace {

static constexpr uint8_t kDefaultPrepareProgram[] = {
    static_cast<uint8_t>(ProtocolOpcode::kLiftDown),
    static_cast<uint8_t>(ProtocolOpcode::kGotoSampleCurrent),
    static_cast<uint8_t>(ProtocolOpcode::kLiftUp),
    static_cast<uint8_t>(ProtocolOpcode::kCollectSample),
    static_cast<uint8_t>(ProtocolOpcode::kEndSection),
    static_cast<uint8_t>(ProtocolOpcode::kLiftDown),
    static_cast<uint8_t>(ProtocolOpcode::kGotoBge1),
    static_cast<uint8_t>(ProtocolOpcode::kLiftUp),
    static_cast<uint8_t>(ProtocolOpcode::kHvOn),
    static_cast<uint8_t>(static_cast<uint8_t>(ProtocolOpcode::kWaitBase) + 0U),
    static_cast<uint8_t>(ProtocolOpcode::kHvOff),
    static_cast<uint8_t>(ProtocolOpcode::kLiftDown),
    static_cast<uint8_t>(ProtocolOpcode::kGotoSampleCurrent),
    static_cast<uint8_t>(ProtocolOpcode::kLiftUp),
    static_cast<uint8_t>(ProtocolOpcode::kEventSampleReady),
    static_cast<uint8_t>(ProtocolOpcode::kInjectSample),
    static_cast<uint8_t>(ProtocolOpcode::kLiftDown),
    static_cast<uint8_t>(ProtocolOpcode::kGotoBge2),
    static_cast<uint8_t>(ProtocolOpcode::kLiftUp),
    static_cast<uint8_t>(ProtocolOpcode::kAutoZero),
    static_cast<uint8_t>(ProtocolOpcode::kEventRunStart),
    static_cast<uint8_t>(ProtocolOpcode::kHvOn),
    static_cast<uint8_t>(static_cast<uint8_t>(ProtocolOpcode::kWaitBase) + 1U),
    static_cast<uint8_t>(ProtocolOpcode::kHvOff),
    static_cast<uint8_t>(ProtocolOpcode::kEventRunStop),
    static_cast<uint8_t>(ProtocolOpcode::kEndSection),
};

static constexpr uint16_t kDefaultPrepareLength = 5U;
static constexpr uint32_t kDefaultCollectionDurationMs = 10000UL;
static constexpr uint32_t kDefaultInjectionDurationMs = 10000UL;
static constexpr uint32_t kDefaultDropletDurationMs = 5000UL;

bool ActionNeedsPressure(const FluidSettings& settings, FluidActionType action) {
  switch (action) {
    case FluidActionType::kCollectSample:
    case FluidActionType::kMakeBgeDroplet:
    case FluidActionType::kReplenish:
      return true;
    case FluidActionType::kInjectSample:
      return settings.injection_mode == InjectionMode::kHydrodynamic;
    case FluidActionType::kDrain:
    case FluidActionType::kCarouselHome:
    case FluidActionType::kNone:
    default:
      return false;
  }
}

}  // namespace

InstrumentController::InstrumentController()
    : sensor_config_{UpdateRate::k9Hz, ExcitationFrequency::k32kHz,
                     ExcitationLevel::kVddDiv2, true},
      latest_measurement_{0.0F, 25.0F, SensorStatusCode::kNotReady, 0U, false},
      latest_pressure_{0U, PressureStatusCode::kUnavailable, false},
      latest_current_{0L, false},
      zero_offset_pf_(0.0F),
      manual_fluid_settings_{InjectionMode::kElectrokinetic,
                             kDefaultCollectionDurationMs,
                             kDefaultInjectionDurationMs,
                             kDefaultDropletDurationMs},
      protocol_store_(),
      protocol_info_(InvalidProtocolInfo()),
      lift_(),
      carousel_(),
      replenish_(),
      fluid_(),
      hv_(),
      pump_(),
      valve1_(),
      valve2_(),
      run_(),
      fault_flags_(0U),
      analysis_start_ms_(0U),
      analysis_elapsed_ms_(0U),
      sensor_config_dirty_(true),
      status_request_pending_(false),
      pressure_sample_request_pending_(false),
      event_pending_(false),
      analysis_time_valid_(false),
      analysis_time_running_(false),
      pending_event_{} {}

void InstrumentController::Initialize(uint32_t now_ms) {
  (void)now_ms;
  lift_.Configure({55U, 180U, 500U});
  carousel_.Configure({4076U, kDefaultSlotCount, 4U});
  replenish_.Configure({85U, 178U, 500U});
  fluid_.Configure({300U, 200U, 5000U, 2U, 20000L, 40000L});
  protocol_store_.Begin();

  const ProtocolMetadata metadata = DefaultProtocolMetadata();
  const ProtocolStoreStatus default_status = protocol_store_.EnsureDefaultProtocol(
      metadata, kDefaultPrepareLength, kDefaultPrepareProgram,
      static_cast<uint16_t>(sizeof(kDefaultPrepareProgram)));
  if (default_status != ProtocolStoreStatus::kOk) {
    MarkFault(kFaultProtocolStore);
  }

  LoadProtocolInfoFromStore();
  ResetAnalysisClock();
  sensor_config_dirty_ = true;
}

ReplyMessage InstrumentController::MakeAck(uint16_t seq, AckCode code) {
  return {MessageKind::kAck, seq, code, ErrorCode::kNone};
}

ReplyMessage InstrumentController::MakeError(uint16_t seq, ErrorCode code) {
  return {MessageKind::kError, seq, AckCode::kAccepted, code};
}

ProtocolInfo InstrumentController::InvalidProtocolInfo() {
  ProtocolInfo info = {};
  info.valid = false;
  info.metadata = DefaultProtocolMetadata();
  info.prepare_length = 0U;
  info.program_length = 0U;
  info.program_crc16 = 0U;
  return info;
}

ProtocolMetadata InstrumentController::DefaultProtocolMetadata() {
  ProtocolMetadata metadata = {};
  metadata.slot_count = kDefaultSlotCount;
  metadata.bge1_slot = 0U;
  metadata.bge2_slot = 1U;
  metadata.sample_1_slot = 5U;
  metadata.sample_count = 1U;
  metadata.repetitions = 1U;
  metadata.injection_mode = InjectionMode::kElectrokinetic;
  metadata.collection_duration_ms = kDefaultCollectionDurationMs;
  metadata.injection_duration_ms = kDefaultInjectionDurationMs;
  metadata.droplet_duration_ms = kDefaultDropletDurationMs;
  metadata.wait_times_100ms[0] = 1800U;
  metadata.wait_times_100ms[1] = 36000U;
  metadata.wait_times_100ms[2] = 600U;
  for (uint8_t index = 3U; index < kProtocolWaitPresetCount; ++index) {
    metadata.wait_times_100ms[index] = 0U;
  }
  return metadata;
}

bool InstrumentController::QueueProtocolInfoEvent(const ProtocolInfo& protocol_info,
                                                  uint32_t now_ms,
                                                  EventSnapshot* event) {
#if CE_CUBE_ENABLE_PROTOCOL_INFO
  if (event == nullptr) {
    return false;
  }

  *event = {};
  event->code = EventCode::kProtocolInfo;
  event->uptime_ms = now_ms;
  event->protocol_info = protocol_info;
  return true;
#else
  (void)protocol_info;
  (void)now_ms;
  (void)event;
  return false;
#endif
}

bool InstrumentController::ControllerBusy(CommandType type) const {
  if ((type == CommandType::kStatusGet) || (type == CommandType::kRunStatus) ||
      (type == CommandType::kRunStop) || (type == CommandType::kSampleStop)) {
    return false;
  }

  return run_.active() || fluid_.active() || lift_.busy() || carousel_.busy() ||
         replenish_.busy();
}

bool InstrumentController::ManualCommandsAllowed(CommandType type) const {
  return !ControllerBusy(type);
}

void InstrumentController::ResetAnalysisClock() {
  analysis_start_ms_ = 0U;
  analysis_elapsed_ms_ = 0U;
  analysis_time_valid_ = false;
  analysis_time_running_ = false;
}

void InstrumentController::StartAnalysisClock(uint32_t now_ms) {
  analysis_start_ms_ = now_ms;
  analysis_elapsed_ms_ = 0U;
  analysis_time_valid_ = true;
  analysis_time_running_ = true;
}

void InstrumentController::StopAnalysisClock(uint32_t now_ms) {
  if (!analysis_time_valid_) {
    return;
  }

  if (analysis_time_running_) {
    analysis_elapsed_ms_ = now_ms - analysis_start_ms_;
    analysis_time_running_ = false;
  }
}

uint32_t InstrumentController::CurrentAnalysisTimeMs(uint32_t now_ms) const {
  if (!analysis_time_valid_) {
    return 0U;
  }

  if (!analysis_time_running_) {
    return analysis_elapsed_ms_;
  }

  return now_ms - analysis_start_ms_;
}

void InstrumentController::QueueRunEvent(EventCode code, uint32_t now_ms) {
  if ((code == EventCode::kNone) || event_pending_) {
    return;
  }

  pending_event_ = {};
  pending_event_.code = code;
  pending_event_.sample_index = run_.progress().sample_index;
  pending_event_.sample_slot = run_.progress().sample_slot;
  pending_event_.repetition = run_.progress().repetition;
  pending_event_.uptime_ms = now_ms;
  pending_event_.analysis_time_ms = CurrentAnalysisTimeMs(now_ms);
  pending_event_.analysis_time_valid = analysis_time_valid_;
  event_pending_ = true;
}

void InstrumentController::DisableAllOutputs() {
  hv_.SetEnabled(false);
  pump_.SetEnabled(false);
  valve1_.SetEnabled(false);
  valve2_.SetEnabled(false);
}

void InstrumentController::LoadProtocolInfoFromStore() {
  protocol_info_ = InvalidProtocolInfo();
  const ProtocolStoreStatus load_status = protocol_store_.LoadInfo(&protocol_info_);
  if (load_status == ProtocolStoreStatus::kOk) {
    ClearFault(kFaultProtocolStore);
    return;
  }

  protocol_info_.valid = false;
  MarkFault(kFaultProtocolStore);
}

bool InstrumentController::StartFluidAction(FluidActionType action,
                                            const FluidSettings& settings,
                                            uint8_t home_slot,
                                            uint32_t now_ms) {
  if (ActionNeedsPressure(settings, action) &&
      (!latest_pressure_.valid ||
       (latest_pressure_.status != PressureStatusCode::kOk))) {
    return false;
  }
  return fluid_.StartAction(action, settings, home_slot, now_ms, latest_pressure_);
}

void InstrumentController::ApplyRunRequests(const RunStepRequest& requests,
                                            uint32_t now_ms) {
  if (requests.auto_zero_requested) {
    if (latest_measurement_.valid &&
        (latest_measurement_.status == SensorStatusCode::kOk)) {
      zero_offset_pf_ = latest_measurement_.raw_cap_pf;
    } else {
      run_.Fault();
    }
  }

  if (requests.pressure_sample_requested) {
#if CE_CUBE_ENABLE_PRESSURE
    pressure_sample_request_pending_ = true;
#else
    MarkFault(kFaultPressure);
    run_.Fault();
#endif
  }

  if (requests.event_code != EventCode::kNone) {
    if (requests.event_code == EventCode::kRunStart) {
      StartAnalysisClock(now_ms);
    } else if (requests.event_code == EventCode::kRunStop) {
      StopAnalysisClock(now_ms);
    }
    QueueRunEvent(requests.event_code, now_ms);
  }
}

__attribute__((noinline)) ReplyMessage InstrumentController::HandleCommand(
    const ParsedCommand& command, uint32_t now_ms) {
  if (!ManualCommandsAllowed(command.type)) {
    return MakeError(command.seq, ErrorCode::kBusy);
  }

  switch (command.type) {
    case CommandType::kSensorSetRate:
      sensor_config_.update_rate = command.update_rate;
      sensor_config_dirty_ = true;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kSensorSetExcitation:
      if (command.has_excitation_frequency) {
        sensor_config_.excitation_frequency = command.excitation_frequency;
      }
      if (command.has_excitation_level) {
        sensor_config_.excitation_level = command.excitation_level;
      }
      sensor_config_dirty_ = true;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kSensorTempComp:
      sensor_config_.temperature_compensation = command.enable;
      sensor_config_dirty_ = true;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kSensorAutoZero:
      if (command.clear_zero) {
        zero_offset_pf_ = 0.0F;
        return MakeAck(command.seq, AckCode::kAccepted);
      }
      if (!latest_measurement_.valid ||
          (latest_measurement_.status != SensorStatusCode::kOk)) {
        return MakeError(command.seq, ErrorCode::kNotReady);
      }
      zero_offset_pf_ = latest_measurement_.raw_cap_pf;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kLiftMove:
      if (!lift_.CommandMove(command.lift_position, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kCarouselStep:
      if (!carousel_.CommandRelativeSlots(command.carousel_steps, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kCarouselGotoSlot:
      if (!carousel_.CommandGotoSlot(command.slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kCarouselAdjust:
      if (!carousel_.CommandAdjust(command.adjust_direction, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kCarouselHome:
      if (!StartFluidAction(FluidActionType::kCarouselHome, manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kHvSet:
      hv_.SetEnabled(command.enable);
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kPumpSet:
      pump_.SetEnabled(command.enable);
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kValveSet:
      if (command.valve_id == 1U) {
        valve1_.SetEnabled(command.enable);
        return MakeAck(command.seq, AckCode::kAccepted);
      }
      if (command.valve_id == 2U) {
        valve2_.SetEnabled(command.enable);
        return MakeAck(command.seq, AckCode::kAccepted);
      }
      return MakeError(command.seq, ErrorCode::kInvalidField);
    case CommandType::kCollectionConfigure:
      manual_fluid_settings_.collection_duration_ms =
          command.collection_duration_ms;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kInjectionConfigure:
      manual_fluid_settings_.injection_mode = command.injection_mode;
      manual_fluid_settings_.injection_duration_ms = command.duration_ms;
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kSampleCollect:
      if (!StartFluidAction(FluidActionType::kCollectSample, manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kNotReady);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kSampleStop: {
      const FluidExecutionContext fluid_context = {
          &latest_pressure_, &lift_, &carousel_, &replenish_,
          &hv_,              &pump_, &valve1_,   &valve2_};
      if (run_.active()) {
        run_.Fault();
        MarkFault(kFaultRun);
      }
      fluid_.Stop(fluid_context, now_ms);
      DisableAllOutputs();
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kInjectionRun:
      if (!StartFluidAction(FluidActionType::kInjectSample, manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kNotReady);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kDropletMake:
      if (!StartFluidAction(FluidActionType::kMakeBgeDroplet,
                            manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kNotReady);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kReplenishRun:
      if (!StartFluidAction(FluidActionType::kReplenish, manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kNotReady);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kDrainRun:
      if (!StartFluidAction(FluidActionType::kDrain, manual_fluid_settings_,
                            protocol_info_.metadata.bge1_slot, now_ms)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kRunStart:
      if (lift_.busy() || carousel_.busy() || replenish_.busy() || fluid_.active()) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      if (!protocol_info_.valid) {
        return MakeError(command.seq, ErrorCode::kProtocolInvalid);
      }
      if (!run_.Start(protocol_info_)) {
        return MakeError(command.seq, ErrorCode::kBusy);
      }
      ResetAnalysisClock();
      ClearFault(kFaultRun);
      return MakeAck(command.seq, AckCode::kAccepted);
    case CommandType::kRunStop: {
      const FluidExecutionContext fluid_context = {
          &latest_pressure_, &lift_, &carousel_, &replenish_,
          &hv_,              &pump_, &valve1_,   &valve2_};
      run_.Stop();
      fluid_.Stop(fluid_context, now_ms);
      StopAnalysisClock(now_ms);
      DisableAllOutputs();
      QueueRunEvent(EventCode::kRunStop, now_ms);
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kRunStatus:
    case CommandType::kStatusGet:
      status_request_pending_ = true;
      return MakeAck(command.seq, AckCode::kStatus);
    case CommandType::kProtocolBegin: {
      const ProtocolStoreStatus status =
          protocol_store_.BeginUpload(command.protocol_metadata);
      if (status != ProtocolStoreStatus::kOk) {
        MarkFault(kFaultProtocolStore);
        return MakeError(command.seq, ProtocolStoreStatusToErrorCode(status));
      }
      protocol_info_ = InvalidProtocolInfo();
      protocol_info_.metadata = command.protocol_metadata;
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kProtocolChunk: {
      const ProtocolStoreStatus status = protocol_store_.WriteChunk(
          command.offset, command.chunk_data, command.chunk_length);
      if (status != ProtocolStoreStatus::kOk) {
        MarkFault(kFaultProtocolStore);
        return MakeError(command.seq, ProtocolStoreStatusToErrorCode(status));
      }
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kProtocolCommit: {
      const ProtocolStoreStatus commit_status = protocol_store_.Commit(
          command.prepare_length, command.program_length, command.crc16);
      if (commit_status != ProtocolStoreStatus::kOk) {
        MarkFault(kFaultProtocolStore);
        return MakeError(command.seq,
                         ProtocolStoreStatusToErrorCode(commit_status));
      }
      LoadProtocolInfoFromStore();
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kProtocolInfo:
#if CE_CUBE_ENABLE_PROTOCOL_INFO
      if (!event_pending_ &&
          QueueProtocolInfoEvent(protocol_info_, now_ms, &pending_event_)) {
        event_pending_ = true;
      }
      return MakeAck(command.seq, AckCode::kStatus);
#else
      return MakeError(command.seq, ErrorCode::kUnavailable);
#endif
    case CommandType::kProtocolClear: {
      const ProtocolStoreStatus clear_status = protocol_store_.Clear();
      if (clear_status != ProtocolStoreStatus::kOk) {
        MarkFault(kFaultProtocolStore);
        return MakeError(command.seq,
                         ProtocolStoreStatusToErrorCode(clear_status));
      }
      protocol_info_ = InvalidProtocolInfo();
      MarkFault(kFaultProtocolStore);
      return MakeAck(command.seq, AckCode::kAccepted);
    }
    case CommandType::kInvalid:
    default:
      return MakeError(command.seq, ErrorCode::kInvalidCommand);
  }
}

void InstrumentController::Tick(uint32_t now_ms) {
  const RunStateCode previous_state = run_.progress().state;
  RunExecutionContext run_context = {&protocol_store_, &protocol_info_, &lift_,
                                     &carousel_,      &replenish_,     &fluid_,
                                     &hv_,            &pump_,          &valve1_,
                                     &valve2_};
  FluidExecutionContext fluid_context = {&latest_pressure_, &lift_, &carousel_,
                                         &replenish_,       &hv_,   &pump_,
                                         &valve1_,          &valve2_};
  RunStepRequest requests = {};
  bool pressure_request = false;

  lift_.Tick(now_ms);
  carousel_.Tick(now_ms);
  replenish_.Tick(now_ms);
  run_.Tick(now_ms, run_context, &requests);
  ApplyRunRequests(requests, now_ms);
  fluid_.Tick(now_ms, fluid_context, &pressure_request);

  if (pressure_request) {
#if CE_CUBE_ENABLE_PRESSURE
    pressure_sample_request_pending_ = true;
#else
    MarkFault(kFaultPressure);
    if (run_.active()) {
      run_.Fault();
    }
#endif
  }

  if (fluid_.faulted()) {
    MarkFault(kFaultRun);
    if (run_.active()) {
      run_.Fault();
    }
    fluid_.Stop(fluid_context, now_ms);
    DisableAllOutputs();
  }

  if (run_.progress().state == RunStateCode::kFault) {
    MarkFault(kFaultRun);
  }

  if ((previous_state == RunStateCode::kRunning) &&
      (run_.progress().state != RunStateCode::kRunning)) {
    StopAnalysisClock(now_ms);
    DisableAllOutputs();
  }
}

void InstrumentController::OnMeasurement(const SensorMeasurement& measurement) {
  latest_measurement_ = measurement;
  if (!measurement.valid || (measurement.status != SensorStatusCode::kOk)) {
    MarkFault(kFaultSensor);
  } else {
    ClearFault(kFaultSensor);
  }
}

void InstrumentController::OnPressureMeasurement(
    const PressureMeasurement& measurement) {
#if CE_CUBE_ENABLE_PRESSURE
  latest_pressure_ = measurement;
  pressure_sample_request_pending_ = false;
  if (!measurement.valid || (measurement.status != PressureStatusCode::kOk)) {
    MarkFault(kFaultPressure);
  } else {
    ClearFault(kFaultPressure);
  }
#else
  (void)measurement;
#endif
}

void InstrumentController::OnCurrentMeasurement(
    const CurrentMeasurement& measurement) {
  latest_current_ = measurement;
}

bool InstrumentController::TakePendingSensorConfig(SensorDesiredConfig* config) {
  if ((config == nullptr) || !sensor_config_dirty_) {
    return false;
  }

  *config = sensor_config_;
  sensor_config_dirty_ = false;
  return true;
}

bool InstrumentController::TakePendingPressureSampleRequest() {
  if (!pressure_sample_request_pending_) {
    return false;
  }

  pressure_sample_request_pending_ = false;
  return true;
}

bool InstrumentController::TakeStatusRequest() {
  if (!status_request_pending_) {
    return false;
  }

  status_request_pending_ = false;
  return true;
}

bool InstrumentController::TakePendingEvent(EventSnapshot* event) {
  if ((event == nullptr) || !event_pending_) {
    return false;
  }

  *event = pending_event_;
  event_pending_ = false;
  return true;
}

void InstrumentController::MarkFault(uint16_t fault_bit) { fault_flags_ |= fault_bit; }

void InstrumentController::ClearFault(uint16_t fault_bit) {
  fault_flags_ =
      static_cast<uint16_t>(fault_flags_ & static_cast<uint16_t>(~fault_bit));
}

TelemetrySnapshot InstrumentController::BuildTelemetry(uint16_t seq,
                                                       uint32_t now_ms) const {
  const float corrected_cap =
      latest_measurement_.raw_cap_pf - zero_offset_pf_;
  const RunProgress progress = run_.progress();
  return {seq,
          corrected_cap,
          latest_measurement_.temp_c,
          latest_measurement_.status,
          latest_pressure_.pressure_pa,
          latest_pressure_.status,
          latest_current_.current_ua,
          hv_.enabled(),
          pump_.enabled(),
          valve1_.enabled(),
          valve2_.enabled(),
          protocol_info_.valid,
          lift_.state(),
          carousel_.slot(),
          progress.state,
          progress.program_counter,
          progress.sample_index,
          progress.sample_slot,
          progress.repetition,
          CurrentAnalysisTimeMs(now_ms),
          analysis_time_valid_,
          fault_flags_};
}

const SensorDesiredConfig& InstrumentController::desired_sensor_config() const {
  return sensor_config_;
}

uint16_t InstrumentController::faults() const { return fault_flags_; }

bool InstrumentController::run_active() const { return run_.active(); }

const LiftController& InstrumentController::lift() const { return lift_; }

LiftController* InstrumentController::lift_mutable() { return &lift_; }

const CarouselController& InstrumentController::carousel() const {
  return carousel_;
}

CarouselController* InstrumentController::carousel_mutable() { return &carousel_; }

const ReplenishController& InstrumentController::replenish() const {
  return replenish_;
}

ReplenishController* InstrumentController::replenish_mutable() {
  return &replenish_;
}

const BinaryOutputController& InstrumentController::hv() const { return hv_; }

BinaryOutputController* InstrumentController::hv_mutable() { return &hv_; }

const BinaryOutputController& InstrumentController::pump() const { return pump_; }

BinaryOutputController* InstrumentController::pump_mutable() { return &pump_; }

const BinaryOutputController& InstrumentController::valve1() const {
  return valve1_;
}

BinaryOutputController* InstrumentController::valve1_mutable() { return &valve1_; }

const BinaryOutputController& InstrumentController::valve2() const {
  return valve2_;
}

BinaryOutputController* InstrumentController::valve2_mutable() { return &valve2_; }

}  // namespace ce_cube
