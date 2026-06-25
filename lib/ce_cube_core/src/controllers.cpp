#include "ce_cube/controllers.hpp"

namespace ce_cube {
namespace {

bool ContextIsValid(const RunExecutionContext& context) {
  return (context.store != nullptr) && (context.protocol_info != nullptr) &&
         (context.lift != nullptr) && (context.carousel != nullptr) &&
         (context.replenish != nullptr) && (context.fluid != nullptr) &&
         (context.hv != nullptr) && (context.pump != nullptr) &&
         (context.valve1 != nullptr) && (context.valve2 != nullptr);
}

bool FluidContextIsValid(const FluidExecutionContext& context) {
  return (context.pressure != nullptr) && (context.lift != nullptr) &&
         (context.carousel != nullptr) && (context.replenish != nullptr) &&
         (context.hv != nullptr) && (context.pump != nullptr) &&
         (context.valve1 != nullptr) && (context.valve2 != nullptr);
}

bool WaitOpcodeIndex(uint8_t opcode, uint8_t* wait_index) {
  if ((wait_index == nullptr) ||
      (opcode < static_cast<uint8_t>(ProtocolOpcode::kWaitBase)) ||
      (opcode > static_cast<uint8_t>(ProtocolOpcode::kWaitMax))) {
    return false;
  }

  *wait_index = static_cast<uint8_t>(
      opcode - static_cast<uint8_t>(ProtocolOpcode::kWaitBase));
  return true;
}

bool IsMotionOpcode(uint8_t opcode) {
  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kLiftDown:
    case ProtocolOpcode::kLiftUp:
    case ProtocolOpcode::kGotoBge1:
    case ProtocolOpcode::kGotoBge2:
    case ProtocolOpcode::kGotoSampleCurrent:
      return true;
    default:
      break;
  }

  return ProtocolStore::IsDirectSlotOpcode(opcode);
}

bool IsOutputOpcode(uint8_t opcode) {
  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kHvOn:
    case ProtocolOpcode::kHvOff:
    case ProtocolOpcode::kPumpOn:
    case ProtocolOpcode::kPumpOff:
    case ProtocolOpcode::kValve1On:
    case ProtocolOpcode::kValve1Off:
    case ProtocolOpcode::kValve2On:
    case ProtocolOpcode::kValve2Off:
      return true;
    default:
      return false;
  }
}

bool IsFluidOpcode(uint8_t opcode) {
  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kCollectSample:
    case ProtocolOpcode::kInjectSample:
    case ProtocolOpcode::kMakeBgeDroplet:
    case ProtocolOpcode::kReplenish:
    case ProtocolOpcode::kDrain:
      return true;
    default:
      return false;
  }
}

BinaryOutputController* OutputForOpcode(const RunExecutionContext& context,
                                        uint8_t opcode, bool* enable) {
  if (enable == nullptr) {
    return nullptr;
  }

  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kHvOn:
      *enable = true;
      return context.hv;
    case ProtocolOpcode::kHvOff:
      *enable = false;
      return context.hv;
    case ProtocolOpcode::kPumpOn:
      *enable = true;
      return context.pump;
    case ProtocolOpcode::kPumpOff:
      *enable = false;
      return context.pump;
    case ProtocolOpcode::kValve1On:
      *enable = true;
      return context.valve1;
    case ProtocolOpcode::kValve1Off:
      *enable = false;
      return context.valve1;
    case ProtocolOpcode::kValve2On:
      *enable = true;
      return context.valve2;
    case ProtocolOpcode::kValve2Off:
      *enable = false;
      return context.valve2;
    default:
      *enable = false;
      return nullptr;
  }
}

bool EventForOpcode(uint8_t opcode, EventCode* code) {
  if (code == nullptr) {
    return false;
  }

  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kEventRunStart:
      *code = EventCode::kRunStart;
      return true;
    case ProtocolOpcode::kEventRunStop:
      *code = EventCode::kRunStop;
      return true;
    case ProtocolOpcode::kEventSampleReady:
      *code = EventCode::kSampleReady;
      return true;
    default:
      *code = EventCode::kNone;
      return false;
  }
}

FluidActionType FluidActionForOpcode(uint8_t opcode) {
  switch (static_cast<ProtocolOpcode>(opcode)) {
    case ProtocolOpcode::kCollectSample:
      return FluidActionType::kCollectSample;
    case ProtocolOpcode::kInjectSample:
      return FluidActionType::kInjectSample;
    case ProtocolOpcode::kMakeBgeDroplet:
      return FluidActionType::kMakeBgeDroplet;
    case ProtocolOpcode::kReplenish:
      return FluidActionType::kReplenish;
    case ProtocolOpcode::kDrain:
      return FluidActionType::kDrain;
    default:
      return FluidActionType::kNone;
  }
}

bool PressureIsValid(const PressureMeasurement& pressure) {
  return pressure.valid && (pressure.status == PressureStatusCode::kOk);
}

}  // namespace

LiftController::LiftController()
    : config_{35U, 100U, 400U},
      position_(LiftPosition::kDown),
      target_(LiftPosition::kDown),
      state_(LiftStateCode::kDown),
      deadline_ms_(0U),
      pending_action_{LiftHardwareActionType::kNone, 0U} {}

void LiftController::Configure(const LiftMotionConfig& config) { config_ = config; }

bool LiftController::CommandMove(LiftPosition position, uint32_t now_ms) {
  if (busy()) {
    return false;
  }
  if (position_ == position) {
    return true;
  }

  target_ = position;
  deadline_ms_ = now_ms + config_.settle_time_ms;
  if (position == LiftPosition::kUp) {
    state_ = LiftStateCode::kMovingUp;
    pending_action_ = {LiftHardwareActionType::kAttachMove, config_.up_angle};
  } else {
    state_ = LiftStateCode::kMovingDown;
    pending_action_ = {LiftHardwareActionType::kAttachMove, config_.down_angle};
  }
  return true;
}

void LiftController::Tick(uint32_t now_ms) {
  if (!busy() || (now_ms < deadline_ms_)) {
    return;
  }

  position_ = target_;
  state_ = (position_ == LiftPosition::kUp) ? LiftStateCode::kUp
                                            : LiftStateCode::kDown;
  deadline_ms_ = 0U;
  pending_action_ = {LiftHardwareActionType::kDetach, 0U};
}

bool LiftController::TakeHardwareAction(LiftHardwareAction* action) {
  if ((action == nullptr) ||
      (pending_action_.type == LiftHardwareActionType::kNone)) {
    return false;
  }

  *action = pending_action_;
  pending_action_ = {LiftHardwareActionType::kNone, 0U};
  return true;
}

LiftStateCode LiftController::state() const { return state_; }

LiftPosition LiftController::position() const { return position_; }

bool LiftController::busy() const {
  return (state_ == LiftStateCode::kMovingUp) ||
         (state_ == LiftStateCode::kMovingDown);
}

CarouselController::CarouselController()
    : config_{4076U, kDefaultSlotCount, 4U},
      slot_(0U),
      target_slot_(0U),
      position_steps_(0U),
      target_steps_(0U),
      steps_remaining_(0U),
      direction_(0),
      next_step_ms_(0U),
      update_slot_on_complete_(true),
      pending_action_{0} {}

void CarouselController::Configure(const CarouselMotionConfig& config) {
  config_ = config;
  if (slot_ >= config_.total_slots) {
    slot_ = 0U;
  }
  position_steps_ = SlotCenterSteps(slot_);
  target_steps_ = position_steps_;
}

uint16_t CarouselController::SlotCenterSteps(uint8_t slot) const {
  const uint32_t numerator =
      static_cast<uint32_t>(slot) *
      static_cast<uint32_t>(config_.total_steps_per_revolution);
  const uint32_t rounded =
      (numerator + (static_cast<uint32_t>(config_.total_slots) / 2U)) /
      static_cast<uint32_t>(config_.total_slots);
  return static_cast<uint16_t>(rounded);
}

uint16_t CarouselController::FineAdjustSteps() const {
  const uint32_t denominator =
      static_cast<uint32_t>(config_.total_slots) * 5U;
  const uint32_t rounded =
      (static_cast<uint32_t>(config_.total_steps_per_revolution) +
       (denominator / 2U)) /
      denominator;
  return static_cast<uint16_t>((rounded == 0U) ? 1U : rounded);
}

uint16_t CarouselController::WrapSteps(int32_t steps) const {
  const int32_t revolution =
      static_cast<int32_t>(config_.total_steps_per_revolution);
  int32_t wrapped = steps;
  while (wrapped < 0) {
    wrapped += revolution;
  }
  while (wrapped >= revolution) {
    wrapped -= revolution;
  }
  return static_cast<uint16_t>(wrapped);
}

bool CarouselController::StartMove(uint16_t target_steps, uint8_t target_slot,
                                   bool update_slot_on_complete,
                                   uint32_t now_ms) {
  const uint16_t revolution = config_.total_steps_per_revolution;
  const uint16_t forward_steps =
      static_cast<uint16_t>((target_steps + revolution - position_steps_) %
                            revolution);
  const uint16_t backward_steps =
      static_cast<uint16_t>((position_steps_ + revolution - target_steps) %
                            revolution);

  uint16_t steps = forward_steps;
  int8_t direction = 1;
  if ((backward_steps < forward_steps) && (backward_steps > 0U)) {
    steps = backward_steps;
    direction = -1;
  } else if ((forward_steps == 0U) && (backward_steps == 0U)) {
    direction = 0;
  }

  target_slot_ = target_slot;
  target_steps_ = target_steps;
  update_slot_on_complete_ = update_slot_on_complete;
  steps_remaining_ = steps;
  direction_ = direction;
  next_step_ms_ = now_ms;

  if (steps == 0U) {
    position_steps_ = target_steps_;
    if (update_slot_on_complete_) {
      slot_ = target_slot_;
    }
    direction_ = 0;
  }

  return true;
}

bool CarouselController::CommandRelativeSlots(int8_t signed_slots,
                                              uint32_t now_ms) {
  if (busy() || (signed_slots == 0) || (config_.total_slots == 0U)) {
    return false;
  }

  int16_t target_slot = static_cast<int16_t>(slot_) - signed_slots;
  while (target_slot < 0) {
    target_slot += config_.total_slots;
  }
  while (target_slot >= config_.total_slots) {
    target_slot -= config_.total_slots;
  }

  const uint8_t wrapped_target = static_cast<uint8_t>(target_slot);
  return StartMove(SlotCenterSteps(wrapped_target), wrapped_target, true,
                   now_ms);
}

bool CarouselController::CommandGotoSlot(uint8_t slot, uint32_t now_ms) {
  if (busy() || (slot >= config_.total_slots)) {
    return false;
  }

  return StartMove(SlotCenterSteps(slot), slot, true, now_ms);
}

bool CarouselController::CommandAdjust(int8_t direction, uint32_t now_ms) {
  if (busy() || ((direction != 1) && (direction != -1))) {
    return false;
  }

  const int32_t target = static_cast<int32_t>(position_steps_) +
                         static_cast<int32_t>(direction) *
                             static_cast<int32_t>(FineAdjustSteps());
  return StartMove(WrapSteps(target), slot_, false, now_ms);
}

void CarouselController::Tick(uint32_t now_ms) {
  if (!busy() || (now_ms < next_step_ms_)) {
    return;
  }

  pending_action_.step_direction = direction_;
  if (direction_ > 0) {
    position_steps_ = WrapSteps(static_cast<int32_t>(position_steps_) + 1);
  } else if (direction_ < 0) {
    position_steps_ = WrapSteps(static_cast<int32_t>(position_steps_) - 1);
  }

  next_step_ms_ = now_ms + config_.step_interval_ms;
  --steps_remaining_;
  if (steps_remaining_ == 0U) {
    position_steps_ = target_steps_;
    direction_ = 0;
    if (update_slot_on_complete_) {
      slot_ = target_slot_;
    }
  }
}

bool CarouselController::TakeHardwareAction(CarouselHardwareAction* action) {
  if ((action == nullptr) || (pending_action_.step_direction == 0)) {
    return false;
  }

  *action = pending_action_;
  pending_action_.step_direction = 0;
  return true;
}

bool CarouselController::busy() const { return steps_remaining_ > 0U; }

uint8_t CarouselController::slot() const { return slot_; }

BinaryOutputController::BinaryOutputController()
    : enabled_(false), pending_action_{false, false} {}

bool BinaryOutputController::SetEnabled(bool enable) {
  if (enabled_ == enable) {
    return true;
  }

  enabled_ = enable;
  pending_action_ = {true, enable};
  return true;
}

bool BinaryOutputController::TakeHardwareAction(OutputHardwareAction* action) {
  if ((action == nullptr) || !pending_action_.has_change) {
    return false;
  }

  *action = pending_action_;
  pending_action_.has_change = false;
  return true;
}

bool BinaryOutputController::enabled() const { return enabled_; }

FluidController::FluidController()
    : config_{300U, 200U, 5000U, 2U, 20000L, 40000L},
      settings_{InjectionMode::kElectrokinetic, 10000UL, 10000UL, 5000UL},
      action_(FluidActionType::kNone),
      phase_(Phase::kIdle),
      resume_phase_(Phase::kIdle),
      phase_deadline_ms_(0U),
      last_integral_ms_(0U),
      pressure_integral_(0U),
      ambient_pressure_pa_(0U),
      active_duration_ms_(0U),
      droplet_repeats_remaining_(0U),
      home_slot_(0U),
      positive_pressure_(false),
      pressure_ready_(false) {}

void FluidController::Configure(const FluidControlConfig& config) { config_ = config; }

bool FluidController::StartAction(FluidActionType action,
                                  const FluidSettings& settings,
                                  uint8_t home_slot, uint32_t now_ms,
                                  const PressureMeasurement& pressure) {
  if ((action == FluidActionType::kNone) || active()) {
    return false;
  }

  settings_ = settings;
  action_ = action;
  resume_phase_ = Phase::kIdle;
  phase_deadline_ms_ = now_ms + 2000U;
  last_integral_ms_ = now_ms;
  pressure_integral_ = 0U;
  ambient_pressure_pa_ = pressure.pressure_pa;
  active_duration_ms_ = 0U;
  droplet_repeats_remaining_ = 0U;
  home_slot_ = home_slot;
  positive_pressure_ = false;
  pressure_ready_ = PressureIsValid(pressure);

  switch (action) {
    case FluidActionType::kCollectSample:
      phase_ = Phase::kCollectStart;
      break;
    case FluidActionType::kInjectSample:
      phase_ = Phase::kInjectStart;
      break;
    case FluidActionType::kMakeBgeDroplet:
      phase_ = Phase::kDropletStart;
      resume_phase_ = Phase::kComplete;
      break;
    case FluidActionType::kReplenish:
      phase_ = Phase::kReplenishServoDown;
      droplet_repeats_remaining_ = config_.replenish_droplet_count;
      break;
    case FluidActionType::kDrain:
      phase_ = Phase::kDrainServoDown;
      break;
    case FluidActionType::kCarouselHome:
      phase_ = Phase::kHomeLiftDown;
      break;
    default:
      phase_ = Phase::kFault;
      return false;
  }

  return true;
}

void FluidController::Stop(const FluidExecutionContext& context, uint32_t now_ms) {
  if (FluidContextIsValid(context) && !context.replenish->busy()) {
    (void)context.replenish->CommandMove(LiftPosition::kUp, now_ms);
  }
  if (FluidContextIsValid(context)) {
    ClearOutputs(context);
  }
  action_ = FluidActionType::kNone;
  phase_ = Phase::kIdle;
  resume_phase_ = Phase::kIdle;
  phase_deadline_ms_ = 0U;
  last_integral_ms_ = 0U;
  pressure_integral_ = 0U;
  ambient_pressure_pa_ = 0U;
  active_duration_ms_ = 0U;
  droplet_repeats_remaining_ = 0U;
  home_slot_ = 0U;
  positive_pressure_ = false;
  pressure_ready_ = false;
}

bool FluidController::active() const { return action_ != FluidActionType::kNone; }

FluidActionType FluidController::action() const { return action_; }

bool FluidController::faulted() const { return phase_ == Phase::kFault; }

bool FluidController::requires_pressure() const {
  switch (phase_) {
    case Phase::kCollectStart:
    case Phase::kCollectRun:
    case Phase::kInjectStart:
    case Phase::kInjectHydroRun:
    case Phase::kDropletStart:
    case Phase::kDropletRun:
      return true;
    default:
      return false;
  }
}

void FluidController::ClearOutputs(const FluidExecutionContext& context) const {
  (void)context.hv->SetEnabled(false);
  (void)context.pump->SetEnabled(false);
  (void)context.valve1->SetEnabled(false);
  (void)context.valve2->SetEnabled(false);
}

void FluidController::StartDeaeration(uint32_t now_ms, Phase resume_phase) {
  resume_phase_ = resume_phase;
  phase_ = Phase::kDeaerateLow1;
  phase_deadline_ms_ = now_ms + config_.deaerate_toggle_ms;
}

bool FluidController::UpdatePressureIntegral(uint32_t now_ms,
                                             const PressureMeasurement& pressure) {
  if (!PressureIsValid(pressure)) {
    return false;
  }

  if (!pressure_ready_) {
    ambient_pressure_pa_ = pressure.pressure_pa;
    last_integral_ms_ = now_ms;
    pressure_ready_ = true;
  }

  const uint32_t delta_ms = now_ms - last_integral_ms_;
  last_integral_ms_ = now_ms;
  phase_deadline_ms_ = now_ms + 2000U;

  int32_t delta_pa = 0;
  if (positive_pressure_) {
    delta_pa = static_cast<int32_t>(pressure.pressure_pa) -
               static_cast<int32_t>(ambient_pressure_pa_);
  } else {
    delta_pa = static_cast<int32_t>(ambient_pressure_pa_) -
               static_cast<int32_t>(pressure.pressure_pa);
  }
  if (delta_pa < 0) {
    delta_pa = 0;
  }

  pressure_integral_ +=
      static_cast<uint32_t>(delta_ms) * static_cast<uint32_t>(delta_pa) / 1000U;
  return true;
}

uint32_t FluidController::TargetIntegral() const {
  const uint32_t target_delta_pa = positive_pressure_
                                       ? static_cast<uint32_t>(config_.pressure_target_delta_pa)
                                       : static_cast<uint32_t>(config_.vacuum_target_delta_pa);
  return (active_duration_ms_ * target_delta_pa) / 1000U;
}

void FluidController::AdvanceToComplete() { phase_ = Phase::kComplete; }

void FluidController::Tick(uint32_t now_ms, const FluidExecutionContext& context,
                           bool* pressure_sample_requested) {
  if (pressure_sample_requested != nullptr) {
    *pressure_sample_requested = false;
  }
  if (!active()) {
    return;
  }
  if (!FluidContextIsValid(context)) {
    phase_ = Phase::kFault;
    return;
  }

  switch (phase_) {
    case Phase::kCollectStart:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(true);
      pressure_integral_ = 0U;
      active_duration_ms_ = settings_.collection_duration_ms;
      positive_pressure_ = false;
      last_integral_ms_ = now_ms;
      phase_ = Phase::kCollectRun;
      break;

    case Phase::kCollectRun:
      if (pressure_sample_requested != nullptr) {
        *pressure_sample_requested = true;
      }
      (void)context.valve2->SetEnabled(true);
      if (PressureIsValid(*context.pressure)) {
        const bool updated = UpdatePressureIntegral(now_ms, *context.pressure);
        if (!updated) {
          phase_ = Phase::kFault;
          break;
        }
        const int32_t delta_pa =
            static_cast<int32_t>(ambient_pressure_pa_) -
            static_cast<int32_t>(context.pressure->pressure_pa);
        (void)context.pump->SetEnabled(delta_pa < config_.vacuum_target_delta_pa);
      } else {
        (void)context.pump->SetEnabled(false);
        if (now_ms >= phase_deadline_ms_) {
          phase_ = Phase::kFault;
          break;
        }
      }
      if (pressure_integral_ >= TargetIntegral()) {
        phase_ = Phase::kCollectCleanup;
      }
      break;

    case Phase::kCollectCleanup:
      ClearOutputs(context);
      StartDeaeration(now_ms, Phase::kComplete);
      break;

    case Phase::kInjectStart:
      ClearOutputs(context);
      if (settings_.injection_mode == InjectionMode::kElectrokinetic) {
        phase_deadline_ms_ = now_ms + settings_.injection_duration_ms;
        phase_ = Phase::kInjectElectroRun;
      } else {
        pressure_integral_ = 0U;
        active_duration_ms_ = settings_.injection_duration_ms;
        positive_pressure_ = false;
        last_integral_ms_ = now_ms;
        phase_ = Phase::kInjectHydroRun;
      }
      break;

    case Phase::kInjectHydroRun:
      if (pressure_sample_requested != nullptr) {
        *pressure_sample_requested = true;
      }
      if (PressureIsValid(*context.pressure)) {
        const bool updated = UpdatePressureIntegral(now_ms, *context.pressure);
        if (!updated) {
          phase_ = Phase::kFault;
          break;
        }
        const int32_t delta_pa =
            static_cast<int32_t>(ambient_pressure_pa_) -
            static_cast<int32_t>(context.pressure->pressure_pa);
        (void)context.pump->SetEnabled(delta_pa < config_.vacuum_target_delta_pa);
      } else {
        (void)context.pump->SetEnabled(false);
        if (now_ms >= phase_deadline_ms_) {
          phase_ = Phase::kFault;
          break;
        }
      }
      if (pressure_integral_ >= TargetIntegral()) {
        phase_ = Phase::kInjectCleanup;
      }
      break;

    case Phase::kInjectElectroRun:
      (void)context.hv->SetEnabled(true);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kInjectCleanup;
      }
      break;

    case Phase::kInjectCleanup:
      ClearOutputs(context);
      AdvanceToComplete();
      break;

    case Phase::kDropletStart:
      ClearOutputs(context);
      pressure_integral_ = 0U;
      active_duration_ms_ = settings_.droplet_duration_ms;
      positive_pressure_ = true;
      last_integral_ms_ = now_ms;
      phase_ = Phase::kDropletRun;
      break;

    case Phase::kDropletRun:
      if (pressure_sample_requested != nullptr) {
        *pressure_sample_requested = true;
      }
      (void)context.valve1->SetEnabled(true);
      if (PressureIsValid(*context.pressure)) {
        const bool updated = UpdatePressureIntegral(now_ms, *context.pressure);
        if (!updated) {
          phase_ = Phase::kFault;
          break;
        }
        const int32_t delta_pa =
            static_cast<int32_t>(context.pressure->pressure_pa) -
            static_cast<int32_t>(ambient_pressure_pa_);
        (void)context.pump->SetEnabled(delta_pa < config_.pressure_target_delta_pa);
      } else {
        (void)context.pump->SetEnabled(false);
        if (now_ms >= phase_deadline_ms_) {
          phase_ = Phase::kFault;
          break;
        }
      }
      if (pressure_integral_ >= TargetIntegral()) {
        phase_ = Phase::kDropletCleanup;
      }
      break;

    case Phase::kDropletCleanup:
      ClearOutputs(context);
      StartDeaeration(now_ms, resume_phase_);
      break;

    case Phase::kReplenishServoDown:
      if (!context.replenish->CommandMove(LiftPosition::kDown, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kReplenishWaitServoDown;
      break;

    case Phase::kReplenishWaitServoDown:
      if (!context.replenish->busy()) {
        phase_deadline_ms_ = now_ms + config_.replenish_pump_ms;
        phase_ = Phase::kReplenishPumpRun;
      }
      break;

    case Phase::kReplenishPumpRun:
      (void)context.valve2->SetEnabled(true);
      (void)context.pump->SetEnabled(true);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kReplenishCleanup;
      }
      break;

    case Phase::kReplenishCleanup:
      ClearOutputs(context);
      StartDeaeration(now_ms, Phase::kReplenishDropletStart);
      break;

    case Phase::kReplenishDropletStart:
      if (droplet_repeats_remaining_ == 0U) {
        if (!context.replenish->CommandMove(LiftPosition::kUp, now_ms)) {
          phase_ = Phase::kFault;
          break;
        }
        phase_ = Phase::kReplenishWaitServoUp;
        break;
      }
      --droplet_repeats_remaining_;
      resume_phase_ = Phase::kReplenishDropletStart;
      phase_ = Phase::kDropletStart;
      break;

    case Phase::kReplenishWaitServoUp:
      if (!context.replenish->busy()) {
        AdvanceToComplete();
      }
      break;

    case Phase::kDrainServoDown:
      if (!context.replenish->CommandMove(LiftPosition::kDown, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kDrainWaitServoDown;
      break;

    case Phase::kDrainWaitServoDown:
      if (!context.replenish->busy()) {
        phase_deadline_ms_ = now_ms + config_.replenish_pump_ms;
        phase_ = Phase::kDrainPumpRun;
      }
      break;

    case Phase::kDrainPumpRun:
      (void)context.valve2->SetEnabled(true);
      (void)context.pump->SetEnabled(true);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kDrainCleanup;
      }
      break;

    case Phase::kDrainCleanup:
      ClearOutputs(context);
      StartDeaeration(now_ms, Phase::kDrainServoUp);
      break;

    case Phase::kDrainServoUp:
      if (!context.replenish->CommandMove(LiftPosition::kUp, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kDrainWaitServoUp;
      break;

    case Phase::kDrainWaitServoUp:
      if (!context.replenish->busy()) {
        AdvanceToComplete();
      }
      break;

    case Phase::kHomeLiftDown:
      if (!context.lift->CommandMove(LiftPosition::kDown, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kHomeWaitLiftDown;
      break;

    case Phase::kHomeWaitLiftDown:
      if (!context.lift->busy()) {
        phase_ = Phase::kHomeCarousel;
      }
      break;

    case Phase::kHomeCarousel:
      if (!context.carousel->CommandGotoSlot(home_slot_, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kHomeWaitCarousel;
      break;

    case Phase::kHomeWaitCarousel:
      if (!context.carousel->busy()) {
        phase_ = Phase::kHomeLiftUp;
      }
      break;

    case Phase::kHomeLiftUp:
      if (!context.lift->CommandMove(LiftPosition::kUp, now_ms)) {
        phase_ = Phase::kFault;
        break;
      }
      phase_ = Phase::kHomeWaitLiftUp;
      break;

    case Phase::kHomeWaitLiftUp:
      if (!context.lift->busy()) {
        AdvanceToComplete();
      }
      break;

    case Phase::kDeaerateLow1:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(false);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kDeaerateHigh1;
        phase_deadline_ms_ = now_ms + config_.deaerate_toggle_ms;
      }
      break;

    case Phase::kDeaerateHigh1:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(true);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kDeaerateLow2;
        phase_deadline_ms_ = now_ms + config_.deaerate_toggle_ms;
      }
      break;

    case Phase::kDeaerateLow2:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(false);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kDeaerateHigh2;
        phase_deadline_ms_ = now_ms + config_.deaerate_toggle_ms;
      }
      break;

    case Phase::kDeaerateHigh2:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(true);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = Phase::kDeaerateLow3;
        phase_deadline_ms_ = now_ms + config_.deaerate_toggle_ms;
      }
      break;

    case Phase::kDeaerateLow3:
      ClearOutputs(context);
      (void)context.valve2->SetEnabled(false);
      if (now_ms >= phase_deadline_ms_) {
        phase_ = resume_phase_;
      }
      break;

    case Phase::kComplete:
      ClearOutputs(context);
      action_ = FluidActionType::kNone;
      phase_ = Phase::kIdle;
      break;

    case Phase::kFault:
      ClearOutputs(context);
      break;

    case Phase::kIdle:
    case Phase::kReplenishServoUp:
      break;
  }
}

RunController::RunController()
    : state_(RunStateCode::kIdle),
      program_counter_(0U),
      program_length_(0U),
      deadline_ms_(0U),
      sample_index_(0U),
      repetition_(0U),
      sample_slot_(0U),
      active_opcode_(kNoActiveOpcode),
      in_prepare_section_(true),
      active_started_(false) {}

bool RunController::Start(const ProtocolInfo& protocol_info) {
  if ((state_ == RunStateCode::kRunning) || !protocol_info.valid ||
      (protocol_info.prepare_length == 0U) ||
      (protocol_info.program_length == 0U)) {
    return false;
  }

  state_ = RunStateCode::kRunning;
  program_counter_ = 0U;
  program_length_ = protocol_info.program_length;
  deadline_ms_ = 0U;
  sample_index_ = 1U;
  repetition_ = 1U;
  sample_slot_ = protocol_info.metadata.sample_1_slot;
  active_opcode_ = kNoActiveOpcode;
  in_prepare_section_ = true;
  active_started_ = false;
  return true;
}

void RunController::Stop() {
  if (state_ == RunStateCode::kRunning) {
    state_ = RunStateCode::kStopped;
  }
  deadline_ms_ = 0U;
  active_opcode_ = kNoActiveOpcode;
  in_prepare_section_ = true;
  active_started_ = false;
}

void RunController::Fault() {
  state_ = RunStateCode::kFault;
  deadline_ms_ = 0U;
  active_opcode_ = kNoActiveOpcode;
  in_prepare_section_ = true;
  active_started_ = false;
}

bool RunController::LoadNextOpcode(const RunExecutionContext& context) {
  if ((context.store == nullptr) || (program_counter_ >= program_length_)) {
    return false;
  }

  uint8_t opcode = 0U;
  if (context.store->ReadOpcode(program_counter_, &opcode) !=
      ProtocolStoreStatus::kOk) {
    return false;
  }
  active_opcode_ = opcode;
  active_started_ = false;
  deadline_ms_ = 0U;
  return true;
}

void RunController::AdvanceProgramCounter() {
  ++program_counter_;
  active_opcode_ = kNoActiveOpcode;
  active_started_ = false;
  deadline_ms_ = 0U;
}

void RunController::AdvanceCycle(const ProtocolInfo& protocol_info) {
  active_opcode_ = kNoActiveOpcode;
  active_started_ = false;
  deadline_ms_ = 0U;

  if (in_prepare_section_) {
    in_prepare_section_ = false;
    program_counter_ = protocol_info.prepare_length;
    return;
  }

  if (repetition_ < protocol_info.metadata.repetitions) {
    ++repetition_;
    program_counter_ = protocol_info.prepare_length;
    return;
  }

  repetition_ = 1U;
  if (sample_index_ < protocol_info.metadata.sample_count) {
    ++sample_index_;
    sample_slot_ = CurrentSampleSlot(protocol_info);
    in_prepare_section_ = true;
    program_counter_ = 0U;
    return;
  }

  state_ = RunStateCode::kComplete;
}

uint8_t RunController::CurrentSampleSlot(const ProtocolInfo& protocol_info) const {
  const uint32_t sample_slot =
      static_cast<uint16_t>(protocol_info.metadata.sample_1_slot) +
      static_cast<uint16_t>(sample_index_) - 1U;
  return static_cast<uint8_t>(sample_slot);
}

bool RunController::ExecuteOpcode(uint32_t now_ms,
                                  const RunExecutionContext& context,
                                  RunStepRequest* request) {
  const ProtocolInfo& protocol_info = *context.protocol_info;

  if (IsMotionOpcode(active_opcode_)) {
    if (!active_started_) {
      bool accepted = false;
      switch (static_cast<ProtocolOpcode>(active_opcode_)) {
        case ProtocolOpcode::kLiftDown:
          accepted = context.lift->CommandMove(LiftPosition::kDown, now_ms);
          break;
        case ProtocolOpcode::kLiftUp:
          accepted = context.lift->CommandMove(LiftPosition::kUp, now_ms);
          break;
        case ProtocolOpcode::kGotoBge1:
          accepted =
              context.carousel->CommandGotoSlot(protocol_info.metadata.bge1_slot,
                                                now_ms);
          break;
        case ProtocolOpcode::kGotoBge2:
          accepted =
              context.carousel->CommandGotoSlot(protocol_info.metadata.bge2_slot,
                                                now_ms);
          break;
        case ProtocolOpcode::kGotoSampleCurrent:
          accepted = context.carousel->CommandGotoSlot(CurrentSampleSlot(
                                                           protocol_info),
                                                       now_ms);
          break;
        default:
          if (ProtocolStore::IsDirectSlotOpcode(active_opcode_)) {
            accepted = context.carousel->CommandGotoSlot(
                ProtocolStore::SlotFromOpcode(active_opcode_), now_ms);
          }
          break;
      }

      if (!accepted) {
        return false;
      }
      active_started_ = true;
    }

    if ((active_opcode_ == static_cast<uint8_t>(ProtocolOpcode::kLiftDown)) ||
        (active_opcode_ == static_cast<uint8_t>(ProtocolOpcode::kLiftUp))) {
      if (context.lift->busy()) {
        return true;
      }
    } else if (context.carousel->busy()) {
      return true;
    }

    AdvanceProgramCounter();
    return true;
  }

  if (IsOutputOpcode(active_opcode_)) {
    bool enable = false;
    BinaryOutputController* output =
        OutputForOpcode(context, active_opcode_, &enable);
    if (output == nullptr) {
      return false;
    }
    output->SetEnabled(enable);
    AdvanceProgramCounter();
    return true;
  }

  if (IsFluidOpcode(active_opcode_)) {
    if (!active_started_) {
      const FluidSettings settings = {
          protocol_info.metadata.injection_mode,
          protocol_info.metadata.collection_duration_ms,
          protocol_info.metadata.injection_duration_ms,
          protocol_info.metadata.droplet_duration_ms,
      };
      if (!context.fluid->StartAction(FluidActionForOpcode(active_opcode_),
                                      settings, protocol_info.metadata.bge1_slot,
                                      now_ms, {0U, PressureStatusCode::kUnavailable,
                                               false})) {
        return false;
      }
      active_started_ = true;
    }
    if (context.fluid->faulted()) {
      return false;
    }
    if (context.fluid->active()) {
      return true;
    }
    AdvanceProgramCounter();
    return true;
  }

  uint8_t wait_index = 0U;
  if (WaitOpcodeIndex(active_opcode_, &wait_index)) {
    if (!active_started_) {
      deadline_ms_ =
          now_ms +
          static_cast<uint32_t>(
              protocol_info.metadata.wait_times_100ms[wait_index]) *
              100U;
      active_started_ = true;
    }
    if (now_ms < deadline_ms_) {
      return true;
    }
    AdvanceProgramCounter();
    return true;
  }

  switch (static_cast<ProtocolOpcode>(active_opcode_)) {
    case ProtocolOpcode::kEndSection:
      AdvanceCycle(protocol_info);
      return true;
    case ProtocolOpcode::kNop:
      AdvanceProgramCounter();
      return true;
    case ProtocolOpcode::kAutoZero:
      request->auto_zero_requested = true;
      AdvanceProgramCounter();
      return true;
    case ProtocolOpcode::kPressureSample:
      request->pressure_sample_requested = true;
      AdvanceProgramCounter();
      return true;
    case ProtocolOpcode::kEventRunStart:
    case ProtocolOpcode::kEventRunStop:
    case ProtocolOpcode::kEventSampleReady:
      if (!EventForOpcode(active_opcode_, &request->event_code)) {
        return false;
      }
      AdvanceProgramCounter();
      return true;
    default:
      return false;
  }
}

void RunController::Tick(uint32_t now_ms, const RunExecutionContext& context,
                         RunStepRequest* request) {
  if (state_ != RunStateCode::kRunning) {
    return;
  }
  if ((request == nullptr) || !ContextIsValid(context)) {
    Fault();
    return;
  }

  request->auto_zero_requested = false;
  request->pressure_sample_requested = false;
  request->event_code = EventCode::kNone;

  if ((sample_index_ == 0U) || (repetition_ == 0U)) {
    Fault();
    return;
  }
  sample_slot_ = CurrentSampleSlot(*context.protocol_info);

  if ((active_opcode_ == kNoActiveOpcode) && !LoadNextOpcode(context)) {
    Fault();
    return;
  }

  if (!ExecuteOpcode(now_ms, context, request)) {
    Fault();
  }
}

RunProgress RunController::progress() const {
  return {state_ == RunStateCode::kRunning,
          state_,
          program_counter_,
          sample_index_,
          sample_slot_,
          repetition_,
          static_cast<uint8_t>((active_opcode_ == kNoActiveOpcode)
                                   ? 0U
                                   : active_opcode_)};
}

bool RunController::active() const { return state_ == RunStateCode::kRunning; }

}  // namespace ce_cube
