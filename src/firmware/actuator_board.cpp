#include "firmware/actuator_board.hpp"

namespace ce_cube {
namespace {

static const uint8_t kStepperPatterns[8][4] = {
    {LOW, LOW, LOW, HIGH}, {LOW, LOW, HIGH, HIGH}, {LOW, LOW, HIGH, LOW},
    {LOW, HIGH, HIGH, LOW}, {LOW, HIGH, LOW, LOW}, {HIGH, HIGH, LOW, LOW},
    {HIGH, LOW, LOW, LOW}, {HIGH, LOW, LOW, HIGH},
};

}  // namespace

ActuatorBoard::ActuatorBoard(uint8_t in1_pin, uint8_t in2_pin, uint8_t in3_pin,
                             uint8_t in4_pin, uint8_t hv_pin,
                             uint8_t servo_pin, uint8_t replenish_servo_pin,
                             uint8_t pump_pin, uint8_t valve1_pin,
                             uint8_t valve2_pin)
    : lift_servo_(),
      replenish_servo_(),
      in1_pin_(in1_pin),
      in2_pin_(in2_pin),
      in3_pin_(in3_pin),
      in4_pin_(in4_pin),
      hv_pin_(hv_pin),
      servo_pin_(servo_pin),
      replenish_servo_pin_(replenish_servo_pin),
      pump_pin_(pump_pin),
      valve1_pin_(valve1_pin),
      valve2_pin_(valve2_pin),
      step_index_(0U) {}

void ActuatorBoard::Begin() {
  pinMode(hv_pin_, OUTPUT);
  pinMode(pump_pin_, OUTPUT);
  pinMode(valve1_pin_, OUTPUT);
  pinMode(valve2_pin_, OUTPUT);
  ApplyOutputAction(hv_pin_, {true, false}, true);
  ApplyOutputAction(pump_pin_, {true, false}, false);
  ApplyOutputAction(valve1_pin_, {true, false}, false);
  ApplyOutputAction(valve2_pin_, {true, false}, false);

  pinMode(in1_pin_, OUTPUT);
  pinMode(in2_pin_, OUTPUT);
  pinMode(in3_pin_, OUTPUT);
  pinMode(in4_pin_, OUTPUT);
  WriteStepperPattern(0U);
}

void ActuatorBoard::ApplyLiftAction(const LiftHardwareAction& action) {
  switch (action.type) {
    case LiftHardwareActionType::kAttachMove:
      lift_servo_.attach(servo_pin_);
      lift_servo_.write(action.angle);
      break;
    case LiftHardwareActionType::kDetach:
      lift_servo_.detach();
      break;
    case LiftHardwareActionType::kNone:
    default:
      break;
  }
}

void ActuatorBoard::ApplyReplenishAction(const ReplenishHardwareAction& action) {
  switch (action.type) {
    case LiftHardwareActionType::kAttachMove:
      replenish_servo_.attach(replenish_servo_pin_);
      replenish_servo_.write(action.angle);
      break;
    case LiftHardwareActionType::kDetach:
      replenish_servo_.detach();
      break;
    case LiftHardwareActionType::kNone:
    default:
      break;
  }
}

void ActuatorBoard::ApplyCarouselAction(const CarouselHardwareAction& action) {
  if (action.step_direction > 0) {
    step_index_ = static_cast<uint8_t>((step_index_ + 1U) % 8U);
  } else if (action.step_direction < 0) {
    step_index_ = (step_index_ == 0U) ? 7U : static_cast<uint8_t>(step_index_ - 1U);
  } else {
    return;
  }

  WriteStepperPattern(step_index_);
}

void ActuatorBoard::ApplyHvAction(const OutputHardwareAction& action) {
  ApplyOutputAction(hv_pin_, action, true);
}

void ActuatorBoard::ApplyPumpAction(const OutputHardwareAction& action) {
  ApplyOutputAction(pump_pin_, action, false);
}

void ActuatorBoard::ApplyValve1Action(const OutputHardwareAction& action) {
  ApplyOutputAction(valve1_pin_, action, false);
}

void ActuatorBoard::ApplyValve2Action(const OutputHardwareAction& action) {
  ApplyOutputAction(valve2_pin_, action, false);
}

void ActuatorBoard::WriteStepperPattern(uint8_t index) {
  const uint8_t wrapped_index = static_cast<uint8_t>(index % 8U);
  digitalWrite(in1_pin_, kStepperPatterns[wrapped_index][0]);
  digitalWrite(in2_pin_, kStepperPatterns[wrapped_index][1]);
  digitalWrite(in3_pin_, kStepperPatterns[wrapped_index][2]);
  digitalWrite(in4_pin_, kStepperPatterns[wrapped_index][3]);
}

void ActuatorBoard::ApplyOutputAction(uint8_t pin,
                                      const OutputHardwareAction& action,
                                      bool active_low) {
  if (!action.has_change) {
    return;
  }

  const uint8_t active_level = active_low ? LOW : HIGH;
  const uint8_t inactive_level = active_low ? HIGH : LOW;
  digitalWrite(pin, action.enable ? active_level : inactive_level);
}

}  // namespace ce_cube
