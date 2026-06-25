#pragma once

#include "ce_cube/feature_flags.hpp"
#include "ce_cube/instrument_controller.hpp"
#include "firmware/actuator_board.hpp"
#include "firmware/ad7745_device.hpp"
#include "firmware/current_device.hpp"
#include "firmware/pressure_device.hpp"
#if CE_CUBE_ENABLE_RF24
#include "firmware/rf24_transport.hpp"
#endif
#include "firmware/usb_transport.hpp"

namespace ce_cube {

class FirmwareApp {
 public:
  FirmwareApp();

  void Setup();
  void Loop();

 private:
  static constexpr size_t kTxJsonBufferLength =
      (kMaxCommandJsonLength > kMaxTelemetryJsonLength)
          ? ((kMaxCommandJsonLength > kMaxEventJsonLength)
                 ? kMaxCommandJsonLength
                 : kMaxEventJsonLength)
          : ((kMaxTelemetryJsonLength > kMaxEventJsonLength)
                 ? kMaxTelemetryJsonLength
                 : kMaxEventJsonLength);

  void PollUsb(uint32_t now_ms);
  void PollRf24(uint32_t now_ms);
  void HandleIncomingJson(const char* json, MessageSource source,
                          uint32_t now_ms);
  void ApplyPendingSensorConfig();
  void ApplyPendingHardwareActions();
  void PollSensor(uint32_t now_ms);
  void PollCurrent(uint32_t now_ms);
  void PollPressure(uint32_t now_ms);
  void DrainPendingEvents();
  void BroadcastTelemetry(uint32_t now_ms);
  void SendReply(MessageSource source, const ReplyMessage& reply);
  bool SendEvent(const EventSnapshot& event);
  bool SendJson(MessageSource source, MessageKind kind, uint16_t seq,
                const char* json);

  InstrumentController controller_;
  Ad7745Device sensor_;
#if CE_CUBE_ENABLE_CURRENT
  CurrentDevice current_;
#endif
#if CE_CUBE_ENABLE_PRESSURE
  PressureDevice pressure_;
#endif
  UsbTransport usb_;
#if CE_CUBE_ENABLE_RF24
  Rf24Transport rf24_;
#endif
  ActuatorBoard actuators_;
  char usb_rx_buffer_[kMaxCommandJsonLength + 1U];
#if CE_CUBE_ENABLE_RF24
  char rf_rx_buffer_[kMaxCommandJsonLength + 1U];
#endif
  char tx_buffer_[kTxJsonBufferLength + 1U];
  uint16_t outbound_seq_;
};

}  // namespace ce_cube
