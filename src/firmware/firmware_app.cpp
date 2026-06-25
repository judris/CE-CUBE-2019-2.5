#include "firmware/firmware_app.hpp"

#include "ce_cube/protocol.hpp"
#include "firmware/firmware_config.hpp"

namespace ce_cube {
namespace {

bool ShouldPublishSensorTelemetry(const SensorMeasurement& measurement,
                                  const InstrumentController& controller) {
  return measurement.valid &&
         (measurement.status == SensorStatusCode::kOk) &&
         !controller.lift().busy() &&
         !controller.carousel().busy();
}

bool ShouldPublishPressureTelemetry(const PressureMeasurement& measurement,
                                    bool force_sample,
                                    const InstrumentController& controller) {
  if (force_sample) {
    return true;
  }

  return measurement.valid &&
         (measurement.status == PressureStatusCode::kOk) &&
         !controller.lift().busy() &&
         !controller.carousel().busy();
}

}  // namespace

FirmwareApp::FirmwareApp()
    : controller_(),
      sensor_(),
#if CE_CUBE_ENABLE_CURRENT
      current_(kCurrentSensePin),
#endif
#if CE_CUBE_ENABLE_PRESSURE
      pressure_(),
#endif
      usb_(),
#if CE_CUBE_ENABLE_RF24
      rf24_(kRf24CePin, kRf24CsnPin),
#endif
      actuators_(kStepperIn1Pin, kStepperIn2Pin, kStepperIn3Pin, kStepperIn4Pin,
                 kHvPin, kServoPin, kReplenishServoPin, kPumpPin, kValve1Pin,
                 kValve2Pin),
      usb_rx_buffer_(),
#if CE_CUBE_ENABLE_RF24
      rf_rx_buffer_(),
#endif
      tx_buffer_(),
      outbound_seq_(1U) {
  usb_rx_buffer_[0] = '\0';
#if CE_CUBE_ENABLE_RF24
  rf_rx_buffer_[0] = '\0';
#endif
  tx_buffer_[0] = '\0';
}

void FirmwareApp::Setup() {
  controller_.Initialize(millis());
  actuators_.Begin();
  usb_.Begin(kSerialBaudRate, kUsbStartupTimeoutMs);
#if CE_CUBE_ENABLE_RF24
  if (!rf24_.Begin(kRf24LocalPipe, kRf24RemotePipe)) {
    controller_.MarkFault(kFaultRf24);
  }
#endif
  if (!sensor_.Begin()) {
    controller_.MarkFault(kFaultSensor);
  }
#if CE_CUBE_ENABLE_CURRENT
  (void)current_.Begin();
#endif
#if CE_CUBE_ENABLE_PRESSURE
  if (!pressure_.Begin()) {
    controller_.MarkFault(kFaultPressure);
  }
#endif
  ApplyPendingSensorConfig();
}

__attribute__((noinline)) void FirmwareApp::Loop() {
  const uint32_t now_ms = millis();
  PollUsb(now_ms);
  PollRf24(now_ms);
  controller_.Tick(now_ms);
  ApplyPendingSensorConfig();
  ApplyPendingHardwareActions();
  PollSensor(now_ms);
  PollCurrent(now_ms);
  PollPressure(now_ms);
  DrainPendingEvents();
  if (controller_.TakeStatusRequest()) {
    BroadcastTelemetry(now_ms);
  }
}

__attribute__((noinline)) void FirmwareApp::PollUsb(uint32_t now_ms) {
  while (true) {
    const LineReceiveStatus status =
        usb_.PollReceive(usb_rx_buffer_, sizeof(usb_rx_buffer_));
    if (status == LineReceiveStatus::kNone) {
      break;
    }
    if (status == LineReceiveStatus::kOverflow) {
      controller_.MarkFault(kFaultProtocolParse);
      SendReply(MessageSource::kUsb,
                {MessageKind::kError, 0U, AckCode::kAccepted,
                 ErrorCode::kTransportOverflow, now_ms});
      continue;
    }
    HandleIncomingJson(usb_rx_buffer_, MessageSource::kUsb, now_ms);
  }
}

__attribute__((noinline)) void FirmwareApp::PollRf24(uint32_t now_ms) {
#if CE_CUBE_ENABLE_RF24
  while (true) {
    const LineReceiveStatus status =
        rf24_.PollReceive(rf_rx_buffer_, sizeof(rf_rx_buffer_));
    if (status == LineReceiveStatus::kNone) {
      break;
    }
    if (status == LineReceiveStatus::kOverflow) {
      controller_.MarkFault(kFaultRf24);
      continue;
    }
    HandleIncomingJson(rf_rx_buffer_, MessageSource::kRf24, now_ms);
  }
#else
  (void)now_ms;
#endif
}

__attribute__((noinline)) void FirmwareApp::HandleIncomingJson(
    const char* json, MessageSource source, uint32_t now_ms) {
  ParsedCommand command = {};
  const ParseResult result = ParseCommandJson(json, &command);
  if (!result.ok) {
    controller_.MarkFault(kFaultProtocolParse);
    SendReply(source, {MessageKind::kError, result.seq, AckCode::kAccepted,
                       result.error, now_ms});
    return;
  }

  const ReplyMessage reply = controller_.HandleCommand(command, now_ms);
  SendReply(source, reply);
  ApplyPendingHardwareActions();
  DrainPendingEvents();
}

void FirmwareApp::ApplyPendingSensorConfig() {
  SensorDesiredConfig config = controller_.desired_sensor_config();
  while (controller_.TakePendingSensorConfig(&config)) {
    if (!sensor_.ApplyConfig(config)) {
      controller_.MarkFault(kFaultSensor);
    }
  }
}

__attribute__((noinline)) void FirmwareApp::ApplyPendingHardwareActions() {
  LiftHardwareAction lift_action = {LiftHardwareActionType::kNone, 0U};
  if (controller_.lift_mutable()->TakeHardwareAction(&lift_action)) {
    actuators_.ApplyLiftAction(lift_action);
  }

  CarouselHardwareAction carousel_action = {0};
  if (controller_.carousel_mutable()->TakeHardwareAction(&carousel_action)) {
    actuators_.ApplyCarouselAction(carousel_action);
  }

  ReplenishHardwareAction replenish_action = {LiftHardwareActionType::kNone, 0U};
  if (controller_.replenish_mutable()->TakeHardwareAction(&replenish_action)) {
    actuators_.ApplyReplenishAction(replenish_action);
  }

  OutputHardwareAction output_action = {false, false};
  if (controller_.hv_mutable()->TakeHardwareAction(&output_action)) {
    actuators_.ApplyHvAction(output_action);
  }
  if (controller_.pump_mutable()->TakeHardwareAction(&output_action)) {
    actuators_.ApplyPumpAction(output_action);
  }
  if (controller_.valve1_mutable()->TakeHardwareAction(&output_action)) {
    actuators_.ApplyValve1Action(output_action);
  }
  if (controller_.valve2_mutable()->TakeHardwareAction(&output_action)) {
    actuators_.ApplyValve2Action(output_action);
  }
}

void FirmwareApp::PollSensor(uint32_t now_ms) {
  SensorMeasurement measurement = {};
  if (!sensor_.Poll(now_ms, &measurement)) {
    return;
  }

  controller_.OnMeasurement(measurement);
  if (ShouldPublishSensorTelemetry(measurement, controller_)) {
    BroadcastTelemetry(now_ms);
  }
}

void FirmwareApp::PollCurrent(uint32_t now_ms) {
#if CE_CUBE_ENABLE_CURRENT
  CurrentMeasurement measurement = {};
  if (!current_.Poll(now_ms, &measurement)) {
    return;
  }

  controller_.OnCurrentMeasurement(measurement);
#else
  (void)now_ms;
#endif
}

void FirmwareApp::PollPressure(uint32_t now_ms) {
#if CE_CUBE_ENABLE_PRESSURE
  const bool force_sample = controller_.TakePendingPressureSampleRequest();
  PressureMeasurement measurement = {};
  if (!pressure_.Poll(now_ms, force_sample, &measurement)) {
    return;
  }

  controller_.OnPressureMeasurement(measurement);
  if (ShouldPublishPressureTelemetry(measurement, force_sample, controller_)) {
    BroadcastTelemetry(now_ms);
  }
#else
  (void)now_ms;
#endif
}

__attribute__((noinline)) void FirmwareApp::DrainPendingEvents() {
  EventSnapshot event = {};
  while (controller_.TakePendingEvent(&event)) {
    if (!SendEvent(event)) {
      controller_.MarkFault(kFaultRf24);
      break;
    }
  }
}

__attribute__((noinline)) void FirmwareApp::BroadcastTelemetry(
    uint32_t now_ms) {
  size_t length = 0U;
  const uint16_t seq = outbound_seq_;
  ++outbound_seq_;
  const TelemetrySnapshot snapshot = controller_.BuildTelemetry(seq, now_ms);
  if (!EncodeTelemetryJson(snapshot, tx_buffer_, sizeof(tx_buffer_), &length) ||
      (length == 0U)) {
    controller_.MarkFault(kFaultProtocolParse);
    return;
  }

  (void)SendJson(MessageSource::kUsb, MessageKind::kTelemetry, seq, tx_buffer_);
#if CE_CUBE_ENABLE_RF24
  if (!SendJson(MessageSource::kRf24, MessageKind::kTelemetry, seq, tx_buffer_)) {
    controller_.MarkFault(kFaultRf24);
  }
#endif
}

__attribute__((noinline)) void FirmwareApp::SendReply(
    MessageSource source, const ReplyMessage& reply) {
  size_t length = 0U;
  bool encoded = false;
  tx_buffer_[0] = '\0';
  if (reply.kind == MessageKind::kAck) {
    encoded = EncodeAckJson(reply, tx_buffer_, sizeof(tx_buffer_), &length);
  } else if (reply.kind == MessageKind::kError) {
    encoded = EncodeErrorJson(reply, tx_buffer_, sizeof(tx_buffer_), &length);
  }
  if (!encoded || (length == 0U)) {
    controller_.MarkFault(kFaultProtocolParse);
    return;
  }

  if (!SendJson(source, reply.kind, reply.seq, tx_buffer_) &&
      (source == MessageSource::kRf24)) {
    controller_.MarkFault(kFaultRf24);
  }
}

__attribute__((noinline)) bool FirmwareApp::SendEvent(
    const EventSnapshot& event) {
  size_t length = 0U;
  const uint16_t seq = outbound_seq_;
  ++outbound_seq_;
  if (!EncodeEventJson(event, seq, tx_buffer_, sizeof(tx_buffer_), &length) ||
      (length == 0U)) {
    controller_.MarkFault(kFaultProtocolParse);
    return false;
  }

  const bool usb_ok =
      SendJson(MessageSource::kUsb, MessageKind::kEvent, seq, tx_buffer_);
#if CE_CUBE_ENABLE_RF24
  const bool rf_ok =
      SendJson(MessageSource::kRf24, MessageKind::kEvent, seq, tx_buffer_);
  return usb_ok && rf_ok;
#else
  return usb_ok;
#endif
}

bool FirmwareApp::SendJson(MessageSource source, MessageKind kind, uint16_t seq,
                           const char* json) {
  (void)kind;
  (void)seq;
  if (source == MessageSource::kUsb) {
    return usb_.Send(json);
  }
#if CE_CUBE_ENABLE_RF24
  return rf24_.Send(kind, seq, json);
#else
  return false;
#endif
}

}  // namespace ce_cube
