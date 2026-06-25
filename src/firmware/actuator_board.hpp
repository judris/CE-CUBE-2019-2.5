#pragma once

#include <Arduino.h>
#include <Servo.h>

#include "ce_cube/controllers.hpp"

namespace ce_cube {

class ActuatorBoard {
 public:
  ActuatorBoard(uint8_t in1_pin, uint8_t in2_pin, uint8_t in3_pin, uint8_t in4_pin,
                uint8_t hv_pin, uint8_t servo_pin, uint8_t replenish_servo_pin,
                uint8_t pump_pin, uint8_t valve1_pin, uint8_t valve2_pin);

  void Begin();
  void ApplyLiftAction(const LiftHardwareAction& action);
  void ApplyReplenishAction(const ReplenishHardwareAction& action);
  void ApplyCarouselAction(const CarouselHardwareAction& action);
  void ApplyHvAction(const OutputHardwareAction& action);
  void ApplyPumpAction(const OutputHardwareAction& action);
  void ApplyValve1Action(const OutputHardwareAction& action);
  void ApplyValve2Action(const OutputHardwareAction& action);

 private:
  void WriteStepperPattern(uint8_t index);
  void ApplyOutputAction(uint8_t pin, const OutputHardwareAction& action,
                         bool active_low);

  Servo lift_servo_;
  Servo replenish_servo_;
  uint8_t in1_pin_;
  uint8_t in2_pin_;
  uint8_t in3_pin_;
  uint8_t in4_pin_;
  uint8_t hv_pin_;
  uint8_t servo_pin_;
  uint8_t replenish_servo_pin_;
  uint8_t pump_pin_;
  uint8_t valve1_pin_;
  uint8_t valve2_pin_;
  uint8_t step_index_;
};

}  // namespace ce_cube
