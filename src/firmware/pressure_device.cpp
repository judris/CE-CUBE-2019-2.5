#include "firmware/pressure_device.hpp"

#include <Wire.h>

namespace ce_cube {
namespace {

static constexpr uint8_t kBmpChipIdRegister = 0xD0U;
static constexpr uint8_t kBmp180ChipId = 0x55U;
static constexpr uint8_t kBmp280ChipId = 0x58U;
static constexpr uint8_t kBme280ChipId = 0x60U;
static constexpr uint8_t kBmp180ControlRegister = 0xF4U;
static constexpr uint8_t kBmp180DataRegister = 0xF6U;
static constexpr uint8_t kBmp180TemperatureCommand = 0x2EU;
static constexpr uint8_t kBmp180PressureCommand = 0x34U;
static constexpr uint8_t kBmp280ControlRegister = 0xF4U;
static constexpr uint8_t kBmp280ConfigRegister = 0xF5U;
static constexpr uint8_t kBmp280DataRegister = 0xF7U;
static constexpr uint8_t kBmp180CalibrationRegister = 0xAAU;
static constexpr uint8_t kBmp280CalibrationRegister = 0x88U;
static constexpr uint8_t kBmp180TemperatureWaitMs = 5U;
static constexpr uint8_t kBmp180PressureWaitMs = 8U;

}  // namespace

PressureDevice::PressureDevice()
    : sensor_kind_(SensorKind::kNone),
      sensor_address_(0U),
      next_poll_ms_(0U),
      next_action_ms_(0U),
      bmp180_phase_(Bmp180Phase::kIdle),
      bmp180_uncomp_temperature_(0U),
      bmp180_calibration_{0, 0, 0, 0U, 0U, 0U, 0, 0, 0, 0, 0},
      bmp280_calibration_{0U, 0, 0, 0U, 0, 0, 0, 0, 0, 0, 0, 0} {}

bool PressureDevice::Begin() {
  Wire.begin();
  next_poll_ms_ = 0U;
  next_action_ms_ = 0U;
  bmp180_phase_ = Bmp180Phase::kIdle;
  return DetectSensor();
}

bool PressureDevice::Poll(uint32_t now_ms, bool force_sample,
                          PressureMeasurement* measurement) {
  if (measurement == nullptr) {
    return false;
  }

  if (sensor_kind_ == SensorKind::kBmp180) {
    return PollBmp180(now_ms, force_sample, measurement);
  }
  if ((sensor_kind_ == SensorKind::kBmp280) ||
      (sensor_kind_ == SensorKind::kBme280)) {
    return PollBmp280Family(now_ms, force_sample, measurement);
  }

  if (!force_sample) {
    return false;
  }
  *measurement = {0U, PressureStatusCode::kUnavailable, false};
  return true;
}

bool PressureDevice::DetectSensor() {
  static const uint8_t kCandidateAddresses[2] = {0x76U, 0x77U};

  for (uint8_t index = 0U; index < 2U; ++index) {
    sensor_address_ = kCandidateAddresses[index];
    uint8_t chip_id = 0U;
    if (!ReadRegister(kBmpChipIdRegister, &chip_id)) {
      continue;
    }

    if (chip_id == kBmp180ChipId) {
      sensor_kind_ = SensorKind::kBmp180;
      return LoadBmp180Calibration();
    }
    if ((chip_id == kBmp280ChipId) || (chip_id == kBme280ChipId)) {
      sensor_kind_ = (chip_id == kBmp280ChipId) ? SensorKind::kBmp280
                                                : SensorKind::kBme280;
      return LoadBmp280Calibration() && ConfigureBmp280Family();
    }
  }

  sensor_kind_ = SensorKind::kNone;
  sensor_address_ = 0U;
  return false;
}

bool PressureDevice::ConfigureBmp280Family() {
  return WriteRegister(kBmp280ConfigRegister, 0x00U) &&
         WriteRegister(kBmp280ControlRegister, 0x27U);
}

bool PressureDevice::LoadBmp180Calibration() {
  uint8_t bytes[22] = {0U};
  if (!ReadBlock(kBmp180CalibrationRegister, bytes, sizeof(bytes))) {
    return false;
  }

  bmp180_calibration_.ac1 = ReadBeS16(&bytes[0]);
  bmp180_calibration_.ac2 = ReadBeS16(&bytes[2]);
  bmp180_calibration_.ac3 = ReadBeS16(&bytes[4]);
  bmp180_calibration_.ac4 = ReadBe16(&bytes[6]);
  bmp180_calibration_.ac5 = ReadBe16(&bytes[8]);
  bmp180_calibration_.ac6 = ReadBe16(&bytes[10]);
  bmp180_calibration_.b1 = ReadBeS16(&bytes[12]);
  bmp180_calibration_.b2 = ReadBeS16(&bytes[14]);
  bmp180_calibration_.mb = ReadBeS16(&bytes[16]);
  bmp180_calibration_.mc = ReadBeS16(&bytes[18]);
  bmp180_calibration_.md = ReadBeS16(&bytes[20]);
  return true;
}

bool PressureDevice::LoadBmp280Calibration() {
  uint8_t bytes[24] = {0U};
  if (!ReadBlock(kBmp280CalibrationRegister, bytes, sizeof(bytes))) {
    return false;
  }

  bmp280_calibration_.dig_t1 = ReadLe16(&bytes[0]);
  bmp280_calibration_.dig_t2 = ReadLeS16(&bytes[2]);
  bmp280_calibration_.dig_t3 = ReadLeS16(&bytes[4]);
  bmp280_calibration_.dig_p1 = ReadLe16(&bytes[6]);
  bmp280_calibration_.dig_p2 = ReadLeS16(&bytes[8]);
  bmp280_calibration_.dig_p3 = ReadLeS16(&bytes[10]);
  bmp280_calibration_.dig_p4 = ReadLeS16(&bytes[12]);
  bmp280_calibration_.dig_p5 = ReadLeS16(&bytes[14]);
  bmp280_calibration_.dig_p6 = ReadLeS16(&bytes[16]);
  bmp280_calibration_.dig_p7 = ReadLeS16(&bytes[18]);
  bmp280_calibration_.dig_p8 = ReadLeS16(&bytes[20]);
  bmp280_calibration_.dig_p9 = ReadLeS16(&bytes[22]);
  return bmp280_calibration_.dig_p1 != 0U;
}

bool PressureDevice::WriteRegister(uint8_t address, uint8_t value) {
  Wire.beginTransmission(sensor_address_);
  Wire.write(address);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool PressureDevice::ReadRegister(uint8_t address, uint8_t* value) {
  return ReadBlock(address, value, 1U);
}

bool PressureDevice::ReadBlock(uint8_t address, uint8_t* data, size_t length) {
  if ((data == nullptr) || (length == 0U)) {
    return false;
  }

  Wire.beginTransmission(sensor_address_);
  Wire.write(address);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  const uint8_t received =
      Wire.requestFrom(static_cast<int>(sensor_address_), static_cast<int>(length));
  if (received != static_cast<uint8_t>(length)) {
    return false;
  }

  for (size_t index = 0U; index < length; ++index) {
    if (Wire.available() <= 0) {
      return false;
    }
    data[index] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool PressureDevice::PollBmp180(uint32_t now_ms, bool force_sample,
                                PressureMeasurement* measurement) {
  if (bmp180_phase_ == Bmp180Phase::kIdle) {
    if (!force_sample && (now_ms < next_poll_ms_)) {
      return false;
    }
    if (!WriteRegister(kBmp180ControlRegister, kBmp180TemperatureCommand)) {
      *measurement = {0U, PressureStatusCode::kBusError, false};
      return true;
    }
    next_action_ms_ = now_ms + kBmp180TemperatureWaitMs;
    bmp180_phase_ = Bmp180Phase::kWaitTemperature;
    return false;
  }

  if (bmp180_phase_ == Bmp180Phase::kWaitTemperature) {
    if (now_ms < next_action_ms_) {
      return false;
    }

    uint8_t bytes[2] = {0U};
    if (!ReadBlock(kBmp180DataRegister, bytes, sizeof(bytes))) {
      bmp180_phase_ = Bmp180Phase::kIdle;
      *measurement = {0U, PressureStatusCode::kBusError, false};
      return true;
    }
    bmp180_uncomp_temperature_ = ReadBe16(bytes);
    if (!WriteRegister(kBmp180ControlRegister, kBmp180PressureCommand)) {
      bmp180_phase_ = Bmp180Phase::kIdle;
      *measurement = {0U, PressureStatusCode::kBusError, false};
      return true;
    }
    next_action_ms_ = now_ms + kBmp180PressureWaitMs;
    bmp180_phase_ = Bmp180Phase::kWaitPressure;
    return false;
  }

  if (now_ms < next_action_ms_) {
    return false;
  }

  uint8_t bytes[3] = {0U};
  if (!ReadBlock(kBmp180DataRegister, bytes, sizeof(bytes))) {
    bmp180_phase_ = Bmp180Phase::kIdle;
    *measurement = {0U, PressureStatusCode::kBusError, false};
    return true;
  }

  const uint32_t uncomp_pressure =
      static_cast<uint32_t>((static_cast<uint32_t>(bytes[0]) << 16U) |
                            (static_cast<uint32_t>(bytes[1]) << 8U) |
                            static_cast<uint32_t>(bytes[2])) >>
      8U;
  uint32_t pressure_pa = 0U;
  if (!ReadBmp180Pressure(bmp180_uncomp_temperature_, uncomp_pressure,
                          &pressure_pa)) {
    bmp180_phase_ = Bmp180Phase::kIdle;
    *measurement = {0U, PressureStatusCode::kConfigError, false};
    return true;
  }

  bmp180_phase_ = Bmp180Phase::kIdle;
  next_poll_ms_ = now_ms + kPollIntervalMs;
  *measurement = {pressure_pa, PressureStatusCode::kOk, true};
  return true;
}

bool PressureDevice::PollBmp280Family(uint32_t now_ms, bool force_sample,
                                      PressureMeasurement* measurement) {
  if (!force_sample && (now_ms < next_poll_ms_)) {
    return false;
  }

  uint32_t pressure_pa = 0U;
  if (!ReadBmp280Pressure(&pressure_pa)) {
    *measurement = {0U, PressureStatusCode::kBusError, false};
    return true;
  }

  next_poll_ms_ = now_ms + kPollIntervalMs;
  *measurement = {pressure_pa, PressureStatusCode::kOk, true};
  return true;
}

bool PressureDevice::ReadBmp280Pressure(uint32_t* pressure_pa) {
  if (pressure_pa == nullptr) {
    return false;
  }

  uint8_t bytes[6] = {0U};
  if (!ReadBlock(kBmp280DataRegister, bytes, sizeof(bytes))) {
    return false;
  }

  const int32_t adc_pressure =
      static_cast<int32_t>((static_cast<uint32_t>(bytes[0]) << 12U) |
                           (static_cast<uint32_t>(bytes[1]) << 4U) |
                           (static_cast<uint32_t>(bytes[2]) >> 4U));
  const int32_t adc_temperature =
      static_cast<int32_t>((static_cast<uint32_t>(bytes[3]) << 12U) |
                           (static_cast<uint32_t>(bytes[4]) << 4U) |
                           (static_cast<uint32_t>(bytes[5]) >> 4U));
  if ((adc_pressure <= 0) || (adc_temperature <= 0)) {
    return false;
  }

  const int32_t temp_delta =
      (adc_temperature >> 3) -
      (static_cast<int32_t>(bmp280_calibration_.dig_t1) << 1);
  const int32_t var1 =
      (temp_delta * static_cast<int32_t>(bmp280_calibration_.dig_t2)) >> 11;
  const int32_t temp_delta2 =
      (adc_temperature >> 4) - static_cast<int32_t>(bmp280_calibration_.dig_t1);
  const int32_t var2 =
      (((temp_delta2 * temp_delta2) >> 12) *
       static_cast<int32_t>(bmp280_calibration_.dig_t3)) >>
      14;
  const int32_t t_fine = var1 + var2;

  int64_t pressure_var1 = static_cast<int64_t>(t_fine) - 128000LL;
  int64_t pressure_var2 = pressure_var1 * pressure_var1 *
                          static_cast<int64_t>(bmp280_calibration_.dig_p6);
  pressure_var2 +=
      (pressure_var1 * static_cast<int64_t>(bmp280_calibration_.dig_p5)) << 17U;
  pressure_var2 +=
      static_cast<int64_t>(bmp280_calibration_.dig_p4) << 35U;

  pressure_var1 =
      ((pressure_var1 * pressure_var1 *
        static_cast<int64_t>(bmp280_calibration_.dig_p3)) >>
       8U) +
      ((pressure_var1 * static_cast<int64_t>(bmp280_calibration_.dig_p2))
       << 12U);
  pressure_var1 =
      ((((static_cast<int64_t>(1) << 47U) + pressure_var1) *
        static_cast<int64_t>(bmp280_calibration_.dig_p1))) >>
      33U;
  if (pressure_var1 == 0LL) {
    return false;
  }

  int64_t pressure = 1048576LL - static_cast<int64_t>(adc_pressure);
  pressure = (((pressure << 31U) - pressure_var2) * 3125LL) / pressure_var1;
  pressure_var1 =
      (static_cast<int64_t>(bmp280_calibration_.dig_p9) *
       (pressure >> 13U) * (pressure >> 13U)) >>
      25U;
  pressure_var2 =
      (static_cast<int64_t>(bmp280_calibration_.dig_p8) * pressure) >> 19U;
  pressure =
      ((pressure + pressure_var1 + pressure_var2) >> 8U) +
      (static_cast<int64_t>(bmp280_calibration_.dig_p7) << 4U);
  if (pressure < 0LL) {
    return false;
  }

  *pressure_pa = static_cast<uint32_t>(pressure >> 8U);
  return true;
}

bool PressureDevice::ReadBmp180Pressure(uint32_t uncomp_temp,
                                        uint32_t uncomp_pressure,
                                        uint32_t* pressure_pa) const {
  if (pressure_pa == nullptr) {
    return false;
  }

  const int32_t x1 =
      ((static_cast<int32_t>(uncomp_temp) -
        static_cast<int32_t>(bmp180_calibration_.ac6)) *
       static_cast<int32_t>(bmp180_calibration_.ac5)) >>
      15U;
  const int32_t denominator =
      x1 + static_cast<int32_t>(bmp180_calibration_.md);
  if (denominator == 0) {
    return false;
  }
  const int32_t x2 =
      (static_cast<int32_t>(bmp180_calibration_.mc) << 11U) / denominator;
  const int32_t b5 = x1 + x2;
  const int32_t b6 = b5 - 4000;
  const int32_t x3 =
      ((static_cast<int32_t>(bmp180_calibration_.b2) *
        ((b6 * b6) >> 12U)) >>
       11U) +
      ((static_cast<int32_t>(bmp180_calibration_.ac2) * b6) >> 11U);
  const int32_t b3 =
      (((static_cast<int32_t>(bmp180_calibration_.ac1) * 4) + x3) + 2) >> 2U;
  const int32_t x4 =
      (static_cast<int32_t>(bmp180_calibration_.ac3) * b6) >> 13U;
  const int32_t x5 =
      (static_cast<int32_t>(bmp180_calibration_.b1) *
       ((b6 * b6) >> 12U)) >>
      16U;
  const int32_t x6 = (x4 + x5 + 2) >> 2U;
  const uint32_t b4 =
      (static_cast<uint32_t>(bmp180_calibration_.ac4) *
       static_cast<uint32_t>(x6 + 32768)) >>
      15U;
  if (b4 == 0U) {
    return false;
  }

  const uint32_t b7 =
      (static_cast<uint32_t>(static_cast<int32_t>(uncomp_pressure) - b3)) *
      50000U;
  uint32_t pressure = 0U;
  if (b7 < 0x80000000UL) {
    pressure = (b7 * 2U) / b4;
  } else {
    pressure = (b7 / b4) * 2U;
  }

  const int32_t p_term1 =
      static_cast<int32_t>((pressure >> 8U) * (pressure >> 8U));
  const int32_t p_term2 = (p_term1 * 3038) >> 16U;
  const int32_t p_term3 =
      (-7357 * static_cast<int32_t>(pressure)) >> 16U;
  pressure = static_cast<uint32_t>(
      static_cast<int32_t>(pressure) +
      ((p_term2 + p_term3 + 3791) >> 4U));
  *pressure_pa = pressure;
  return true;
}

uint16_t PressureDevice::ReadBe16(const uint8_t* bytes) {
  return static_cast<uint16_t>(static_cast<uint16_t>(bytes[0]) << 8U) |
         static_cast<uint16_t>(bytes[1]);
}

int16_t PressureDevice::ReadBeS16(const uint8_t* bytes) {
  return static_cast<int16_t>(ReadBe16(bytes));
}

uint16_t PressureDevice::ReadLe16(const uint8_t* bytes) {
  return static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8U) |
         static_cast<uint16_t>(bytes[0]);
}

int16_t PressureDevice::ReadLeS16(const uint8_t* bytes) {
  return static_cast<int16_t>(ReadLe16(bytes));
}

}  // namespace ce_cube
