#include "firmware/ad7745_device.hpp"

#include <Wire.h>

#include "ce_cube/ad7745_math.hpp"

namespace ce_cube {
namespace {

static constexpr uint8_t kAd7745Address = 0x48U;
static constexpr uint8_t kRegisterStatus = 0x00U;
static constexpr uint8_t kRegisterCapSetup = 0x07U;
static constexpr uint8_t kRegisterVtSetup = 0x08U;
static constexpr uint8_t kRegisterExcSetup = 0x09U;
static constexpr uint8_t kRegisterConfiguration = 0x0AU;
static constexpr uint8_t kRegisterCapDacA = 0x0BU;
static constexpr uint8_t kRegisterCapOffsetHigh = 0x0DU;
static constexpr uint8_t kRegisterCapOffsetLow = 0x0EU;
static constexpr uint8_t kRegisterCapGainHigh = 0x0FU;
static constexpr uint8_t kRegisterCapGainLow = 0x10U;

static constexpr uint8_t kCapSetupDefault = 0xA1U;
static constexpr uint8_t kVtEnabled = 0x81U;
static constexpr uint8_t kVtDisabled = 0x00U;
static constexpr uint8_t kCapDacDefault = 0x00U;
static constexpr uint8_t kCapOffsetHighDefault = 0x77U;
static constexpr uint8_t kCapOffsetLowDefault = 0x1AU;
static constexpr uint8_t kCapGainHighDefault = 0x5DU;
static constexpr uint8_t kCapGainLowDefault = 0xBDU;

}  // namespace

Ad7745Device::Ad7745Device()
    : config_{UpdateRate::k9Hz, ExcitationFrequency::k32kHz,
              ExcitationLevel::kVddDiv2, true},
      next_poll_ms_(0U) {}

bool Ad7745Device::Begin() {
  Wire.begin();
  next_poll_ms_ = 0U;
  return ApplyConfig(config_);
}

bool Ad7745Device::ApplyConfig(const SensorDesiredConfig& config) {
  config_ = config;
  const bool ok = WriteRegister(kRegisterCapSetup, kCapSetupDefault) &&
                  WriteDefaultTrim() &&
                  WriteRegister(kRegisterExcSetup,
                                ExcitationByte(config.excitation_frequency,
                                               config.excitation_level)) &&
                  WriteRegister(kRegisterConfiguration, 0x00U) &&
                  WriteRegister(kRegisterVtSetup,
                                config.temperature_compensation ? kVtEnabled
                                                                : kVtDisabled) &&
                  WriteRegister(kRegisterConfiguration,
                                ConfigByte(config.update_rate,
                                           config.temperature_compensation));
  return ok;
}

bool Ad7745Device::Poll(uint32_t now_ms, SensorMeasurement* measurement) {
  if ((measurement == nullptr) || (now_ms < next_poll_ms_)) {
    return false;
  }

  next_poll_ms_ = now_ms + PollIntervalMs(config_.update_rate);
  uint8_t raw_data[7] = {0U};
  if (!ReadBlock(kRegisterStatus, raw_data, sizeof(raw_data))) {
    *measurement = {0.0F, 0.0F, SensorStatusCode::kBusError, 0U, false};
    return true;
  }

  *measurement = {
      CapacitanceFromRawBytes(raw_data[1], raw_data[2], raw_data[3]),
      TemperatureFromRawBytes(raw_data[4], raw_data[5], raw_data[6]),
      SensorStatusCode::kOk,
      raw_data[0],
      true,
  };
  return true;
}

uint32_t Ad7745Device::PollIntervalMs(UpdateRate rate) {
  switch (rate) {
    case UpdateRate::k9Hz:
      return 112U;
    case UpdateRate::k11Hz:
      return 92U;
    case UpdateRate::k13Hz:
      return 78U;
    case UpdateRate::k16Hz:
      return 64U;
    case UpdateRate::k26Hz:
      return 39U;
    case UpdateRate::k50Hz:
      return 20U;
    case UpdateRate::k84Hz:
      return 12U;
    case UpdateRate::k91Hz:
      return 11U;
    default:
      return 112U;
  }
}

uint8_t Ad7745Device::ConfigByte(UpdateRate rate,
                                 bool temperature_compensation) {
  uint8_t base = 0x39U;
  switch (rate) {
    case UpdateRate::k9Hz:
      base = 0x39U;
      break;
    case UpdateRate::k11Hz:
      base = 0x31U;
      break;
    case UpdateRate::k13Hz:
      base = 0x29U;
      break;
    case UpdateRate::k16Hz:
      base = 0x21U;
      break;
    case UpdateRate::k26Hz:
      base = 0x19U;
      break;
    case UpdateRate::k50Hz:
      base = 0x11U;
      break;
    case UpdateRate::k84Hz:
      base = 0x09U;
      break;
    case UpdateRate::k91Hz:
      base = 0x01U;
      break;
    default:
      break;
  }

  if (temperature_compensation) {
    base = static_cast<uint8_t>(base | 0xC0U);
  }
  return base;
}

uint8_t Ad7745Device::ExcitationByte(ExcitationFrequency frequency,
                                     ExcitationLevel level) {
  uint8_t value = 0x60U;
  if (frequency == ExcitationFrequency::k16kHz) {
    value = static_cast<uint8_t>(value | 0x80U);
  }
  value = static_cast<uint8_t>(value | static_cast<uint8_t>(level));
  return value;
}

bool Ad7745Device::WriteRegister(uint8_t address, uint8_t value) {
  Wire.beginTransmission(kAd7745Address);
  Wire.write(address);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool Ad7745Device::WriteDefaultTrim() {
  return WriteRegister(kRegisterCapDacA, kCapDacDefault) &&
         WriteRegister(kRegisterCapOffsetHigh, kCapOffsetHighDefault) &&
         WriteRegister(kRegisterCapOffsetLow, kCapOffsetLowDefault) &&
         WriteRegister(kRegisterCapGainHigh, kCapGainHighDefault) &&
         WriteRegister(kRegisterCapGainLow, kCapGainLowDefault);
}

bool Ad7745Device::ReadBlock(uint8_t start_address, uint8_t* data,
                             size_t data_length) {
  if ((data == nullptr) || (data_length == 0U)) {
    return false;
  }

  Wire.beginTransmission(kAd7745Address);
  Wire.write(start_address);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  const uint8_t requested =
      Wire.requestFrom(static_cast<int>(kAd7745Address),
                       static_cast<int>(data_length));
  if (requested != static_cast<uint8_t>(data_length)) {
    return false;
  }

  for (size_t index = 0U; index < data_length; ++index) {
    if (Wire.available() <= 0) {
      return false;
    }
    data[index] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

}  // namespace ce_cube
