#pragma once

#include <stdint.h>

#include "ce_cube/protocol_store.hpp"
#include "ce_cube/types.hpp"

namespace ce_cube {

struct LiftMotionConfig {
  uint8_t up_angle;
  uint8_t down_angle;
  uint16_t settle_time_ms;
};

enum class LiftHardwareActionType : uint8_t {
  kNone = 0U,
  kAttachMove = 1U,
  kDetach = 2U,
};

struct LiftHardwareAction {
  LiftHardwareActionType type;
  uint8_t angle;
};

class LiftController {
 public:
  LiftController();

  void Configure(const LiftMotionConfig& config);
  bool CommandMove(LiftPosition position, uint32_t now_ms);
  void Tick(uint32_t now_ms);
  bool TakeHardwareAction(LiftHardwareAction* action);
  LiftStateCode state() const;
  LiftPosition position() const;
  bool busy() const;

 private:
  LiftMotionConfig config_;
  LiftPosition position_;
  LiftPosition target_;
  LiftStateCode state_;
  uint32_t deadline_ms_;
  LiftHardwareAction pending_action_;
};

using ReplenishController = LiftController;
using ReplenishHardwareAction = LiftHardwareAction;

struct CarouselMotionConfig {
  uint16_t total_steps_per_revolution;
  uint8_t total_slots;
  uint16_t step_interval_ms;
};

struct CarouselHardwareAction {
  int8_t step_direction;
};

class CarouselController {
 public:
  CarouselController();

  void Configure(const CarouselMotionConfig& config);
  bool CommandRelativeSlots(int8_t signed_slots, uint32_t now_ms);
  bool CommandGotoSlot(uint8_t slot, uint32_t now_ms);
  bool CommandAdjust(int8_t direction, uint32_t now_ms);
  void Tick(uint32_t now_ms);
  bool TakeHardwareAction(CarouselHardwareAction* action);
  bool busy() const;
  uint8_t slot() const;

 private:
  uint16_t SlotCenterSteps(uint8_t slot) const;
  uint16_t FineAdjustSteps() const;
  bool StartMove(uint16_t target_steps, uint8_t target_slot,
                 bool update_slot_on_complete, uint32_t now_ms);
  uint16_t WrapSteps(int32_t steps) const;

  CarouselMotionConfig config_;
  uint8_t slot_;
  uint8_t target_slot_;
  uint16_t position_steps_;
  uint16_t target_steps_;
  uint16_t steps_remaining_;
  int8_t direction_;
  uint32_t next_step_ms_;
  bool update_slot_on_complete_;
  CarouselHardwareAction pending_action_;
};

struct OutputHardwareAction {
  bool has_change;
  bool enable;
};

class BinaryOutputController {
 public:
  BinaryOutputController();

  bool SetEnabled(bool enable);
  bool TakeHardwareAction(OutputHardwareAction* action);
  bool enabled() const;

 private:
  bool enabled_;
  OutputHardwareAction pending_action_;
};

enum class FluidActionType : uint8_t {
  kNone = 0U,
  kCollectSample = 1U,
  kInjectSample = 2U,
  kMakeBgeDroplet = 3U,
  kReplenish = 4U,
  kDrain = 5U,
  kCarouselHome = 6U,
};

struct FluidControlConfig {
  uint16_t relay_window_ms;
  uint16_t deaerate_toggle_ms;
  uint16_t replenish_pump_ms;
  uint8_t replenish_droplet_count;
  int32_t vacuum_target_delta_pa;
  int32_t pressure_target_delta_pa;
};

struct FluidExecutionContext {
  const PressureMeasurement* pressure;
  LiftController* lift;
  CarouselController* carousel;
  ReplenishController* replenish;
  BinaryOutputController* hv;
  BinaryOutputController* pump;
  BinaryOutputController* valve1;
  BinaryOutputController* valve2;
};

class FluidController {
 public:
  FluidController();

  void Configure(const FluidControlConfig& config);
  bool StartAction(FluidActionType action, const FluidSettings& settings,
                   uint8_t home_slot, uint32_t now_ms,
                   const PressureMeasurement& pressure);
  void Stop(const FluidExecutionContext& context, uint32_t now_ms);
  void Tick(uint32_t now_ms, const FluidExecutionContext& context,
            bool* pressure_sample_requested);
  bool active() const;
  FluidActionType action() const;
  bool faulted() const;
  bool requires_pressure() const;

 private:
  enum class Phase : uint8_t {
    kIdle = 0U,
    kCollectStart,
    kCollectRun,
    kCollectCleanup,
    kInjectStart,
    kInjectHydroRun,
    kInjectElectroRun,
    kInjectCleanup,
    kDropletStart,
    kDropletRun,
    kDropletCleanup,
    kReplenishServoDown,
    kReplenishWaitServoDown,
    kReplenishPumpRun,
    kReplenishCleanup,
    kReplenishDropletStart,
    kReplenishServoUp,
    kReplenishWaitServoUp,
    kDrainServoDown,
    kDrainWaitServoDown,
    kDrainPumpRun,
    kDrainCleanup,
    kDrainServoUp,
    kDrainWaitServoUp,
    kHomeLiftDown,
    kHomeWaitLiftDown,
    kHomeCarousel,
    kHomeWaitCarousel,
    kHomeLiftUp,
    kHomeWaitLiftUp,
    kDeaerateLow1,
    kDeaerateHigh1,
    kDeaerateLow2,
    kDeaerateHigh2,
    kDeaerateLow3,
    kComplete,
    kFault,
  };

  void ClearOutputs(const FluidExecutionContext& context) const;
  void StartDeaeration(uint32_t now_ms, Phase resume_phase);
  bool UpdatePressureIntegral(uint32_t now_ms,
                              const PressureMeasurement& pressure);
  uint32_t TargetIntegral() const;
  void AdvanceToComplete();

  FluidControlConfig config_;
  FluidSettings settings_;
  FluidActionType action_;
  Phase phase_;
  Phase resume_phase_;
  uint32_t phase_deadline_ms_;
  uint32_t last_integral_ms_;
  uint32_t pressure_integral_;
  uint32_t ambient_pressure_pa_;
  uint32_t active_duration_ms_;
  uint8_t droplet_repeats_remaining_;
  uint8_t home_slot_;
  bool positive_pressure_;
  bool pressure_ready_;
};

struct RunExecutionContext {
  const ProtocolStore* store;
  const ProtocolInfo* protocol_info;
  LiftController* lift;
  CarouselController* carousel;
  ReplenishController* replenish;
  FluidController* fluid;
  BinaryOutputController* hv;
  BinaryOutputController* pump;
  BinaryOutputController* valve1;
  BinaryOutputController* valve2;
};

struct RunStepRequest {
  bool auto_zero_requested;
  bool pressure_sample_requested;
  EventCode event_code;
};

class RunController {
 public:
  RunController();

  bool Start(const ProtocolInfo& protocol_info);
  void Stop();
  void Fault();
  void Tick(uint32_t now_ms, const RunExecutionContext& context,
            RunStepRequest* request);
  RunProgress progress() const;
  bool active() const;

 private:
  static constexpr uint8_t kNoActiveOpcode = 0xFFU;

  bool LoadNextOpcode(const RunExecutionContext& context);
  void AdvanceProgramCounter();
  void AdvanceCycle(const ProtocolInfo& protocol_info);
  uint8_t CurrentSampleSlot(const ProtocolInfo& protocol_info) const;
  bool ExecuteOpcode(uint32_t now_ms, const RunExecutionContext& context,
                     RunStepRequest* request);

  RunStateCode state_;
  uint16_t program_counter_;
  uint16_t program_length_;
  uint32_t deadline_ms_;
  uint8_t sample_index_;
  uint8_t repetition_;
  uint8_t sample_slot_;
  uint8_t active_opcode_;
  bool in_prepare_section_;
  bool active_started_;
};

}  // namespace ce_cube
