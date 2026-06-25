#pragma once

#include <stdint.h>

namespace ce_cube {

static constexpr uint8_t kStepperIn1Pin = 3U;
static constexpr uint8_t kStepperIn2Pin = 4U;
static constexpr uint8_t kStepperIn3Pin = 5U;
static constexpr uint8_t kStepperIn4Pin = 6U;
static constexpr uint8_t kReplenishServoPin = 2U;
static constexpr uint8_t kHvPin = 17U;
static constexpr uint8_t kServoPin = 8U;
static constexpr uint8_t kRf24CePin = 9U;
static constexpr uint8_t kRf24CsnPin = 10U;
static constexpr uint8_t kPumpPin = 15U;
static constexpr uint8_t kValve1Pin = 16U;
static constexpr uint8_t kValve2Pin = 14U;
static constexpr uint8_t kCurrentSensePin = 20U;
static constexpr unsigned long kSerialBaudRate = 115200UL;
static constexpr uint32_t kUsbStartupTimeoutMs = 1500U;
static constexpr uint64_t kRf24LocalPipe = 0xDEDEDEDEE7ULL;
static constexpr uint64_t kRf24RemotePipe = 0xDEDEDEDEE9ULL;

}  // namespace ce_cube
