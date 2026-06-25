#pragma once

#include "ce_cube/controllers.hpp"
#include "ce_cube/types.hpp"

namespace ce_cube {

class InstrumentController {
 public:
  InstrumentController();

  void Initialize(uint32_t now_ms);
  ReplyMessage HandleCommand(const ParsedCommand& command, uint32_t now_ms);
  void Tick(uint32_t now_ms);
  void OnMeasurement(const SensorMeasurement& measurement);
  void OnPressureMeasurement(const PressureMeasurement& measurement);
  void OnCurrentMeasurement(const CurrentMeasurement& measurement);
  bool TakePendingSensorConfig(SensorDesiredConfig* config);
  bool TakePendingPressureSampleRequest();
  bool TakeStatusRequest();
  bool TakePendingEvent(EventSnapshot* event);
  void MarkFault(uint16_t fault_bit);
  void ClearFault(uint16_t fault_bit);
 TelemetrySnapshot BuildTelemetry(uint16_t seq, uint32_t now_ms) const;
  const SensorDesiredConfig& desired_sensor_config() const;
  uint16_t faults() const;
  bool run_active() const;
  const LiftController& lift() const;
  LiftController* lift_mutable();
  const CarouselController& carousel() const;
  CarouselController* carousel_mutable();
  const ReplenishController& replenish() const;
  ReplenishController* replenish_mutable();
  const BinaryOutputController& hv() const;
  BinaryOutputController* hv_mutable();
  const BinaryOutputController& pump() const;
  BinaryOutputController* pump_mutable();
  const BinaryOutputController& valve1() const;
  BinaryOutputController* valve1_mutable();
  const BinaryOutputController& valve2() const;
  BinaryOutputController* valve2_mutable();

 private:
  ReplyMessage MakeAck(uint16_t seq, AckCode code, uint32_t now_ms) const;
  ReplyMessage MakeError(uint16_t seq, ErrorCode code, uint32_t now_ms) const;
  static ProtocolInfo InvalidProtocolInfo();
  static ProtocolMetadata DefaultProtocolMetadata();
  static bool QueueProtocolInfoEvent(const ProtocolInfo& protocol_info,
                                     uint32_t now_ms,
                                     EventSnapshot* event);
  bool ControllerBusy(CommandType type) const;
  bool ManualCommandsAllowed(CommandType type) const;
  void ResetAnalysisClock();
  void StartAnalysisClock(uint32_t now_ms);
  void StopAnalysisClock(uint32_t now_ms);
  uint32_t CurrentAnalysisTimeMs(uint32_t now_ms) const;
  void QueueRunEvent(EventCode code, uint32_t now_ms);
  void DisableAllOutputs();
  void LoadProtocolInfoFromStore();
  void ApplyRunRequests(const RunStepRequest& requests, uint32_t now_ms);
  bool StartFluidAction(FluidActionType action, const FluidSettings& settings,
                        uint8_t home_slot, uint32_t now_ms);

  SensorDesiredConfig sensor_config_;
  SensorMeasurement latest_measurement_;
  PressureMeasurement latest_pressure_;
  CurrentMeasurement latest_current_;
  float zero_offset_pf_;
  FluidSettings manual_fluid_settings_;
  ProtocolStore protocol_store_;
  ProtocolInfo protocol_info_;
  LiftController lift_;
  CarouselController carousel_;
  ReplenishController replenish_;
  FluidController fluid_;
  BinaryOutputController hv_;
  BinaryOutputController pump_;
  BinaryOutputController valve1_;
  BinaryOutputController valve2_;
  RunController run_;
  uint16_t fault_flags_;
  uint32_t analysis_start_ms_;
  uint32_t analysis_elapsed_ms_;
  bool sensor_config_dirty_;
  bool status_request_pending_;
  bool pressure_sample_request_pending_;
  bool event_pending_;
  bool analysis_time_valid_;
  bool analysis_time_running_;
  EventSnapshot pending_event_;
};

}  // namespace ce_cube
