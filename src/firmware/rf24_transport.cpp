#include "firmware/rf24_transport.hpp"

namespace ce_cube {

#if CE_CUBE_ENABLE_RF24

namespace {

static constexpr uint8_t kRegisterConfig = 0x00U;
static constexpr uint8_t kRegisterEnAa = 0x01U;
static constexpr uint8_t kRegisterEnRxAddr = 0x02U;
static constexpr uint8_t kRegisterSetupAw = 0x03U;
static constexpr uint8_t kRegisterSetupRetr = 0x04U;
static constexpr uint8_t kRegisterRfCh = 0x05U;
static constexpr uint8_t kRegisterRfSetup = 0x06U;
static constexpr uint8_t kRegisterStatus = 0x07U;
static constexpr uint8_t kRegisterRxAddrP0 = 0x0AU;
static constexpr uint8_t kRegisterRxAddrP1 = 0x0BU;
static constexpr uint8_t kRegisterRxPwP0 = 0x11U;
static constexpr uint8_t kRegisterRxPwP1 = 0x12U;
static constexpr uint8_t kRegisterFifoStatus = 0x17U;
static constexpr uint8_t kRegisterDynpd = 0x1CU;
static constexpr uint8_t kRegisterFeature = 0x1DU;
static constexpr uint8_t kRegisterTxAddr = 0x10U;

static constexpr uint8_t kCommandReadRegister = 0x00U;
static constexpr uint8_t kCommandWriteRegister = 0x20U;
static constexpr uint8_t kCommandReadRxPayload = 0x61U;
static constexpr uint8_t kCommandWriteTxPayload = 0xA0U;
static constexpr uint8_t kCommandFlushTx = 0xE1U;
static constexpr uint8_t kCommandFlushRx = 0xE2U;
static constexpr uint8_t kCommandActivate = 0x50U;
static constexpr uint8_t kCommandReadPayloadWidth = 0x60U;
static constexpr uint8_t kCommandNop = 0xFFU;
static constexpr uint8_t kActivateData = 0x73U;

static constexpr uint8_t kConfigBase = 0x0CU;
static constexpr uint8_t kConfigTx = kConfigBase | 0x02U;
static constexpr uint8_t kConfigRx = kConfigBase | 0x03U;
static constexpr uint8_t kStatusClearMask = 0x70U;
static constexpr uint8_t kStatusTxDataSent = 0x20U;
static constexpr uint8_t kStatusMaxRetries = 0x10U;
static constexpr uint8_t kFifoStatusRxEmpty = 0x01U;
static constexpr uint8_t kAddressWidth5Bytes = 0x03U;
static constexpr uint8_t kEnabledPipesMask = 0x03U;
static constexpr uint8_t kDynamicPayloadMask = 0x03U;
static constexpr uint8_t kFeatureDynamicPayload = 0x04U;
static constexpr uint8_t kRfChannel = 64U;
static constexpr uint8_t kRfSetup250kbpsMaxPower = 0x26U;
static constexpr uint8_t kRetryConfig = 0xFFU;
static constexpr uint8_t kAddressLength = 5U;
static constexpr uint8_t kMaxPayloadWidth = 32U;
static constexpr uint16_t kPowerUpDelayUs = 1500U;
static constexpr uint16_t kModeSwitchDelayUs = 150U;
static constexpr uint16_t kTransmitPulseUs = 15U;
static constexpr uint16_t kTransmitPollDelayUs = 10U;
static constexpr uint16_t kTransmitPollLimit = 3000U;

void ChipSelectLow(uint8_t csn_pin) { digitalWrite(csn_pin, LOW); }

void ChipSelectHigh(uint8_t csn_pin) { digitalWrite(csn_pin, HIGH); }

uint8_t TransferByte(uint8_t value) {
  SPDR = value;
  while ((SPSR & static_cast<uint8_t>(_BV(SPIF))) == 0U) {
  }
  return SPDR;
}

uint8_t ReadRegister8(uint8_t csn_pin, uint8_t reg) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(static_cast<uint8_t>(kCommandReadRegister | (reg & 0x1FU)));
  const uint8_t value = TransferByte(kCommandNop);
  ChipSelectHigh(csn_pin);
  return value;
}

void ReadPayload(uint8_t csn_pin, uint8_t* data, uint8_t length) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(kCommandReadRxPayload);
  for (uint8_t index = 0U; index < length; ++index) {
    data[index] = TransferByte(kCommandNop);
  }
  ChipSelectHigh(csn_pin);
}

void WriteRegister8(uint8_t csn_pin, uint8_t reg, uint8_t value) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(static_cast<uint8_t>(kCommandWriteRegister | (reg & 0x1FU)));
  (void)TransferByte(value);
  ChipSelectHigh(csn_pin);
}

void WriteRegisterBlock(uint8_t csn_pin, uint8_t reg, const uint8_t* data,
                        uint8_t length) {
  if (data == nullptr) {
    return;
  }

  ChipSelectLow(csn_pin);
  (void)TransferByte(static_cast<uint8_t>(kCommandWriteRegister | (reg & 0x1FU)));
  for (uint8_t index = 0U; index < length; ++index) {
    (void)TransferByte(data[index]);
  }
  ChipSelectHigh(csn_pin);
}

void WritePayload(uint8_t csn_pin, const uint8_t* data, uint8_t length) {
  if (data == nullptr) {
    return;
  }

  ChipSelectLow(csn_pin);
  (void)TransferByte(kCommandWriteTxPayload);
  for (uint8_t index = 0U; index < length; ++index) {
    (void)TransferByte(data[index]);
  }
  ChipSelectHigh(csn_pin);
}

void SendCommand(uint8_t csn_pin, uint8_t command) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(command);
  ChipSelectHigh(csn_pin);
}

void SendActivate(uint8_t csn_pin) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(kCommandActivate);
  (void)TransferByte(kActivateData);
  ChipSelectHigh(csn_pin);
}

uint8_t ReadPayloadWidth(uint8_t csn_pin) {
  ChipSelectLow(csn_pin);
  (void)TransferByte(kCommandReadPayloadWidth);
  const uint8_t width = TransferByte(kCommandNop);
  ChipSelectHigh(csn_pin);
  return width;
}

void SerializePipeAddress(uint64_t pipe, uint8_t* address) {
  if (address == nullptr) {
    return;
  }

  for (uint8_t index = 0U; index < kAddressLength; ++index) {
    address[index] =
        static_cast<uint8_t>((pipe >> (static_cast<uint8_t>(index) * 8U)) &
                             0xFFU);
  }
}

bool EnableDynamicPayload(uint8_t csn_pin) {
  WriteRegister8(csn_pin, kRegisterFeature, kFeatureDynamicPayload);
  if (ReadRegister8(csn_pin, kRegisterFeature) != kFeatureDynamicPayload) {
    SendActivate(csn_pin);
    WriteRegister8(csn_pin, kRegisterFeature, kFeatureDynamicPayload);
  }
  if (ReadRegister8(csn_pin, kRegisterFeature) != kFeatureDynamicPayload) {
    return false;
  }

  WriteRegister8(csn_pin, kRegisterDynpd, kDynamicPayloadMask);
  return ReadRegister8(csn_pin, kRegisterDynpd) == kDynamicPayloadMask;
}

void EnterReceiveMode(uint8_t ce_pin, uint8_t csn_pin) {
  digitalWrite(ce_pin, LOW);
  WriteRegister8(csn_pin, kRegisterStatus, kStatusClearMask);
  WriteRegister8(csn_pin, kRegisterConfig, kConfigRx);
  delayMicroseconds(kPowerUpDelayUs);
  digitalWrite(ce_pin, HIGH);
  delayMicroseconds(kModeSwitchDelayUs);
}

void EnterTransmitMode(uint8_t ce_pin, uint8_t csn_pin) {
  digitalWrite(ce_pin, LOW);
  WriteRegister8(csn_pin, kRegisterStatus, kStatusClearMask);
  WriteRegister8(csn_pin, kRegisterConfig, kConfigTx);
  delayMicroseconds(kPowerUpDelayUs);
}

bool WaitForTransmitResult(uint8_t csn_pin) {
  for (uint16_t attempt = 0U; attempt < kTransmitPollLimit; ++attempt) {
    const uint8_t status = ReadRegister8(csn_pin, kRegisterStatus);
    if ((status & kStatusTxDataSent) != 0U) {
      WriteRegister8(csn_pin, kRegisterStatus, kStatusTxDataSent);
      return true;
    }
    if ((status & kStatusMaxRetries) != 0U) {
      WriteRegister8(csn_pin, kRegisterStatus, kStatusMaxRetries);
      SendCommand(csn_pin, kCommandFlushTx);
      return false;
    }
    delayMicroseconds(kTransmitPollDelayUs);
  }

  SendCommand(csn_pin, kCommandFlushTx);
  return false;
}

bool ConfigureRadioPins(uint8_t ce_pin, uint8_t csn_pin) {
  pinMode(ce_pin, OUTPUT);
  pinMode(csn_pin, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);
  digitalWrite(ce_pin, LOW);
  ChipSelectHigh(csn_pin);
  SPCR = static_cast<uint8_t>(_BV(SPE) | _BV(MSTR));
  SPSR = static_cast<uint8_t>(_BV(SPI2X));
  return true;
}

}  // namespace

Rf24Transport::Rf24Transport(uint8_t ce_pin, uint8_t csn_pin)
    : reassembler_(),
      ce_pin_(ce_pin),
      csn_pin_(csn_pin),
      initialized_(false) {}

bool Rf24Transport::Begin(uint64_t local_pipe, uint64_t remote_pipe) {
  uint8_t local_address[kAddressLength] = {0U};
  uint8_t remote_address[kAddressLength] = {0U};
  SerializePipeAddress(local_pipe, local_address);
  SerializePipeAddress(remote_pipe, remote_address);

  if (!ConfigureRadioPins(ce_pin_, csn_pin_)) {
    return false;
  }

  WriteRegister8(csn_pin_, kRegisterConfig, kConfigBase);
  WriteRegister8(csn_pin_, kRegisterEnAa, kEnabledPipesMask);
  WriteRegister8(csn_pin_, kRegisterEnRxAddr, kEnabledPipesMask);
  WriteRegister8(csn_pin_, kRegisterSetupAw, kAddressWidth5Bytes);
  WriteRegister8(csn_pin_, kRegisterSetupRetr, kRetryConfig);
  WriteRegister8(csn_pin_, kRegisterRfCh, kRfChannel);
  WriteRegister8(csn_pin_, kRegisterRfSetup, kRfSetup250kbpsMaxPower);
  WriteRegisterBlock(csn_pin_, kRegisterTxAddr, remote_address, kAddressLength);
  WriteRegisterBlock(csn_pin_, kRegisterRxAddrP0, remote_address,
                     kAddressLength);
  WriteRegisterBlock(csn_pin_, kRegisterRxAddrP1, local_address, kAddressLength);
  WriteRegister8(csn_pin_, kRegisterRxPwP0, kMaxPayloadWidth);
  WriteRegister8(csn_pin_, kRegisterRxPwP1, kMaxPayloadWidth);

  if ((ReadRegister8(csn_pin_, kRegisterSetupAw) != kAddressWidth5Bytes) ||
      !EnableDynamicPayload(csn_pin_)) {
    return false;
  }

  SendCommand(csn_pin_, kCommandFlushTx);
  SendCommand(csn_pin_, kCommandFlushRx);
  EnterReceiveMode(ce_pin_, csn_pin_);
  initialized_ = true;
  return true;
}

LineReceiveStatus Rf24Transport::PollReceive(char* message,
                                             size_t message_capacity) {
  if (!initialized_ || (message == nullptr) || (message_capacity == 0U)) {
    return LineReceiveStatus::kNone;
  }

  while ((ReadRegister8(csn_pin_, kRegisterFifoStatus) & kFifoStatusRxEmpty) ==
         0U) {
    const uint8_t payload_size = ReadPayloadWidth(csn_pin_);
    if ((payload_size == 0U) || (payload_size > kMaxRf24FrameSize) ||
        (payload_size > kMaxPayloadWidth)) {
      SendCommand(csn_pin_, kCommandFlushRx);
      WriteRegister8(csn_pin_, kRegisterStatus, kStatusClearMask);
      return LineReceiveStatus::kOverflow;
    }

    uint8_t frame[kMaxRf24FrameSize] = {0U};
    ReadPayload(csn_pin_, frame, payload_size);
    WriteRegister8(csn_pin_, kRegisterStatus, 0x40U);
    const ErrorCode push_result =
        reassembler_.PushFrame(frame, payload_size, message, message_capacity);
    if ((push_result != ErrorCode::kNone) && (push_result != ErrorCode::kRfCrc) &&
        (push_result != ErrorCode::kRfFragment)) {
      continue;
    }
    if ((push_result == ErrorCode::kRfCrc) || (push_result == ErrorCode::kRfFragment)) {
      return LineReceiveStatus::kOverflow;
    }

    MessageKind kind = MessageKind::kInvalid;
    uint16_t seq = 0U;
    size_t length = 0U;
    if (reassembler_.HasMessage() &&
        reassembler_.TakeMessage(&kind, &seq, &length)) {
      (void)seq;
      return ((kind == MessageKind::kCommand) && (length > 0U))
                 ? LineReceiveStatus::kMessage
                 : LineReceiveStatus::kNone;
    }
  }

  return LineReceiveStatus::kNone;
}

bool Rf24Transport::Send(MessageKind kind, uint16_t seq, const char* json) {
  if (!initialized_ || (json == nullptr)) {
    return false;
  }

  Rf24Fragmenter fragmenter;
  if (!fragmenter.Begin(kind, seq, json)) {
    return false;
  }

  EnterTransmitMode(ce_pin_, csn_pin_);
  bool success = true;
  while (!fragmenter.Done()) {
    uint8_t frame[kMaxRf24FrameSize] = {0U};
    size_t frame_length = 0U;
    if (!fragmenter.NextFrame(frame, sizeof(frame), &frame_length)) {
      success = false;
      break;
    }
    WritePayload(csn_pin_, frame, static_cast<uint8_t>(frame_length));
    digitalWrite(ce_pin_, HIGH);
    delayMicroseconds(kTransmitPulseUs);
    digitalWrite(ce_pin_, LOW);
    if (!WaitForTransmitResult(csn_pin_)) {
      success = false;
      break;
    }
  }
  EnterReceiveMode(ce_pin_, csn_pin_);
  return success;
}

#else

Rf24Transport::Rf24Transport(uint8_t ce_pin, uint8_t csn_pin)
    : reassembler_(), ce_pin_(ce_pin), csn_pin_(csn_pin), initialized_(false) {
  (void)ce_pin;
  (void)csn_pin;
}

bool Rf24Transport::Begin(uint64_t local_pipe, uint64_t remote_pipe) {
  (void)local_pipe;
  (void)remote_pipe;
  initialized_ = false;
  return false;
}

LineReceiveStatus Rf24Transport::PollReceive(char* message,
                                             size_t message_capacity) {
  (void)message;
  (void)message_capacity;
  return LineReceiveStatus::kNone;
}

bool Rf24Transport::Send(MessageKind kind, uint16_t seq, const char* json) {
  (void)kind;
  (void)seq;
  (void)json;
  return false;
}

#endif

}  // namespace ce_cube
