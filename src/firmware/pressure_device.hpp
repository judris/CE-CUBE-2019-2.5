#pragma once

#include <Arduino.h>

#include "ce_cube/types.hpp"

namespace ce_cube {

class PressureDevice {
 public:
  PressureDevice();

  bool Begin();
  bool Poll(uint32_t now_ms, bool force_sample, PressureMeasurement* measurement);

 private:
  enum class SensorKind : uint8_t {
    kNone = 0U,
    kBmp180 = 1U,
    kBmp280 = 2U,
    kBme280 = 3U,
  };

  enum class Bmp180Phase : uint8_t {
    kIdle = 0U,
    kWaitTemperature = 1U,
    kWaitPressure = 2U,
  };

  struct Bmp180Calibration {
    int16_t ac1;
    int16_t ac2;
    int16_t ac3;
    uint16_t ac4;
    uint16_t ac5;
    uint16_t ac6;
    int16_t b1;
    int16_t b2;
    int16_t mb;
    int16_t mc;
    int16_t md;
  };

  struct Bmp280Calibration {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
  };

  static constexpr uint32_t kPollIntervalMs = 250U;

  bool DetectSensor();
  bool ConfigureBmp280Family();
  bool LoadBmp180Calibration();
  bool LoadBmp280Calibration();
  bool WriteRegister(uint8_t address, uint8_t value);
  bool ReadRegister(uint8_t address, uint8_t* value);
  bool ReadBlock(uint8_t address, uint8_t* data, size_t length);
  bool PollBmp180(uint32_t now_ms, bool force_sample,
                  PressureMeasurement* measurement);
  bool PollBmp280Family(uint32_t now_ms, bool force_sample,
                        PressureMeasurement* measurement);
  bool ReadBmp280Pressure(uint32_t* pressure_pa);
  bool ReadBmp180Pressure(uint32_t uncomp_temp, uint32_t uncomp_pressure,
                          uint32_t* pressure_pa) const;
  static uint16_t ReadBe16(const uint8_t* bytes);
  static int16_t ReadBeS16(const uint8_t* bytes);
  static uint16_t ReadLe16(const uint8_t* bytes);
  static int16_t ReadLeS16(const uint8_t* bytes);

  SensorKind sensor_kind_;
  uint8_t sensor_address_;
  uint32_t next_poll_ms_;
  uint32_t next_action_ms_;
  Bmp180Phase bmp180_phase_;
  uint16_t bmp180_uncomp_temperature_;
  Bmp180Calibration bmp180_calibration_;
  Bmp280Calibration bmp280_calibration_;
};

}  // namespace ce_cube
