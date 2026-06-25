# CE-CUBE Project Overview

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 22:30:26 +03:00

## What This Repository Is

This repository contains the current CE-CUBE instrument firmware rewrite.

The project is organized as a PlatformIO firmware codebase instead of a legacy
multi-`.ino` sketch. The rewrite focuses on deterministic behavior, static
memory use, structured protocol handling, and host-testable logic.

## Main Goals

- run on Arduino Nano ATmega328P as the primary target
- support direct USB control and optional NRF24 transport
- control the CE-CUBE mechanics and outputs through small state machines
- measure capacitance and temperature through AD7745
- measure pressure through a BMP180, BMP280, or BME280 used in pressure-only mode
- store one automation program in EEPROM and execute it one opcode at a time
- keep RAM use bounded and avoid loading the entire stored protocol into SRAM

## Current Build Targets

The current PlatformIO environments are:

- `nanoatmega328`
  - RF24 enabled
  - pressure enabled
  - `protocol.info` disabled to save flash
  - currently over the Nano flash limit under the present toolchain
- `nanoatmega328_usb`
  - default target
  - RF24 disabled
  - pressure enabled
  - current-sense polling disabled to stay within Nano flash on the older
    PlatformIO AVR toolchain used by the IDE build button
  - intended for direct USB operation with lower flash/RAM use
  - current recommended Nano build
- `nanoatmega328_usb_eepromtest`
  - RF24 disabled
  - pressure disabled
  - current-sense polling disabled
  - `protocol.info` enabled
  - intended only for EEPROM write/readback hardware tests over USB
- `native`
  - host-side unit tests

There is also a commented Leonardo environment retained only as a reference.

## Dependency Policy

The repo intentionally keeps dependencies small.

External dependencies in normal embedded builds:

- `RF24`
- `Servo`

Arduino framework/core facilities used directly:

- `Wire`
- `SPI`
- `EEPROM`
- `Serial`

Not used by design:

- Arduino `String`
- ArduinoJson
- dynamic containers such as `std::vector`
- heap allocation during steady-state firmware operation

## Repository Layout

- `src/main.cpp`
  - firmware entry point
  - performs explicit startup construction for the long-lived `FirmwareApp`
  - uses a local AVR-safe placement-new declaration because some AVR toolchains
    used with PlatformIO do not provide the standard `<new>` header
- `src/firmware`
  - board-facing adapters and transport implementations
- `lib/ce_cube_core/include/ce_cube`
  - public interfaces and shared types
- `lib/ce_cube_core/src`
  - platform-independent control logic, protocol logic, EEPROM logic, CRC, and math
- `test`
  - host-side unit tests for math, CRC, protocol, and controller behavior
- `docs`
  - project documentation

## Architecture

The firmware is split into two broad layers.

### 1. Core Logic Layer

Located mainly in `lib/ce_cube_core`.

Responsibilities:

- command interpretation
- telemetry/event creation
- motion/output state machines
- run protocol execution
- EEPROM-backed protocol storage
- CRC16 handling
- AD7745 conversion math

Key types and modules:

- `types.hpp`
  - protocol constants, enums, status codes, snapshots, and command structures
- `controllers.hpp` and `controllers.cpp`
  - lift controller
  - carousel controller
  - binary output controllers
  - run controller
- `instrument_controller.hpp` and `.cpp`
  - top-level application control state
  - command handling
  - fault tracking
  - sensor/pressure/run integration
- `protocol.hpp` and `.cpp`
  - JSON parsing and encoding
- `protocol_store.hpp` and `.cpp`
  - EEPROM storage for one active run program
- `rf24_framing.hpp` and `.cpp`
  - fragmentation and reassembly of JSON for NRF24 transport

### 2. Firmware/Board Layer

Located mainly in `src/firmware`.

Responsibilities:

- hardware initialization
- hardware pin writes
- USB line transport
- RF24 transport
- AD7745 I2C access
- pressure sensor I2C access
- translating controller hardware actions into physical output changes

Key modules:

- `firmware_app.hpp` and `.cpp`
  - top-level firmware loop orchestration
- `actuator_board.hpp` and `.cpp`
  - servo
  - stepper coil sequencing
  - HV/pump/valve pin writes
- `ad7745_device.hpp` and `.cpp`
  - sensor register setup and polling
- `pressure_device.hpp` and `.cpp`
  - runtime detection and polling for BMP180/BMP280/BME280
- `usb_transport.hpp` and `.cpp`
  - newline-delimited JSON over `Serial`
- `rf24_transport.hpp` and `.cpp`
  - NRF24 send/receive path

## Runtime Flow

The firmware entry point is very small:

- `setup()` calls `FirmwareApp::Setup()`
- `loop()` calls `FirmwareApp::Loop()`

The steady-state loop is tick-driven and bounded:

1. poll USB input
2. poll RF24 input if enabled
3. tick the controller state machines
4. apply pending sensor configuration
5. apply pending actuator/output actions
6. poll AD7745
7. poll pressure sensor if enabled
8. drain pending events
9. send telemetry when requested

This keeps control flow explicit and avoids hidden background behavior.

## Hardware Model

Current hardware abstractions represented by the code:

- `1` lift servo
- `1` carousel stepper using four GPIO outputs
- `1` HV output
- `1` pump output
- `2` valve outputs
- `1` AD7745 capacitance/temperature sensor on I2C
- `1` optional pressure sensor on I2C
- `1` optional NRF24 radio on SPI

## Current Pin Configuration

The current default pins are defined in `src/firmware/firmware_config.hpp`.

- stepper: `3`, `4`, `5`, `6`
- HV: `7`
- servo: `8`
- RF24 CE: `9`
- RF24 CSN: `10`
- pump: `14`
- valve1: `15`
- valve2: `16`

These values are the current build-time mapping. Hardware bench validation may
still adjust them later.

## Sensor Support

### AD7745

The AD7745 driver currently provides:

- startup configuration
- selectable update rates
- selectable excitation frequency
- selectable excitation level
- temperature compensation enable/disable
- bounded polling
- conversion to capacitance and temperature values

### Pressure Sensor

The pressure driver currently provides:

- runtime detection of BMP180, BMP280, or BME280
- pressure-only reporting even for BME280
- periodic sampling
- forced sample support for protocol requests

## Actuator Model

### Lift

- implemented as a small state machine
- uses servo attach, move, settle, and detach actions
- supports `up` and `down`

### Carousel

- implemented as a bounded stepper state machine
- supports relative move, goto-slot, and fine adjust
- fine adjust is a smaller move that does not change the logical slot

### Binary Outputs

Implemented through separate controllers for:

- HV
- pump
- valve1
- valve2

The board layer applies the logical state changes to pins. HV is currently
implemented as active-low in the board adapter; pump and valves are active-high.

## Automation Model

The project no longer uses a hard-coded, monolithic automation flow as the
main model. Instead, it uses an EEPROM-backed run program.

Important properties:

- only one active protocol is stored in EEPROM
- normal startup does not install a fallback protocol image
- empty or cleared EEPROM reports `protocol_valid=0` and sets the
  `kFaultProtocolStore` bit until a host uploads or seeds a valid program
- the runner reads one opcode at a time
- the whole protocol is not loaded into RAM
- sample iteration is controlled by metadata:
  - `sample_1_slot`
  - `sample_count`
  - `repetitions`
- total run count is effectively `sample_count * repetitions`

The minimal explicit seeding workflow is documented in `eeprom-seeding.md`.

This design is specifically intended to be Nano-safe.

## Transport Model

### USB

- newline-delimited JSON over `Serial`
- this is a true command and telemetry transport in the refactored firmware,
  not just a debug console
- outgoing wire format is compact `v:2`
- all outbound `ack`, `error`, `telemetry`, and `event` messages carry a
  monotonic `gt` uptime timestamp; `telemetry` and `event` also carry `at`
  once CE analysis timing is valid
- incoming commands accept compact `v:2` and legacy verbose `v:1`
- bounded startup wait
- does not require a permanently connected host to boot

Legacy note:
- the older CE-CUBE 2.5 code did call `Serial.begin(...)`, but its actual
  command and telemetry path was RF24
- `Serial` in that legacy code was used mainly for debug prints and RF24 trace
  output, not as a first-class host control interface

### RF24

- same compact JSON payload model as USB
- JSON is fragmented into RF24 frames
- application-level CRC16 is carried in the frame header
- RF24 hardware CRC16 is also used

The transport feature can now be compiled out entirely for USB-only builds.

## Fault Handling

Faults are accumulated as bit flags in the controller.

Current fault groups:

- protocol parse
- RF24
- sensor
- run controller
- protocol store
- pressure

Fault status is reported in telemetry.

## Testing

The repo contains host-side tests that currently cover:

- AD7745 math
- CRC16
- compact `v:2` protocol parsing/encoding
- legacy `v:1` inbound command compatibility
- RF24 framing round-trip
- controller timing and run progression behavior

These tests run under the `native` PlatformIO environment.

For hardware-free Nano bring-up, the repo also includes:

- `docs/bare-nano-smoke-test.md`
- `tools/serial_smoke.ps1`

## Current Design Constraints

This project is intentionally written under embedded safety-oriented rules.

Important repo rules:

- no Arduino `String`
- no dynamic allocation after startup
- no recursion
- fixed-capacity buffers
- explicit status checking
- bounded loops and bounded work per iteration

## Current Limitations And Notes

- the default Nano build disables `protocol.info` to save flash
- the default Nano USB build also disables current-sense polling so the
  legacy AVR toolchain path still fits in 30 KB flash
- the RF24-enabled `nanoatmega328` profile currently exceeds Nano flash under
  the present PlatformIO and dependency set
- third-party library warnings still exist in Arduino core and external
  libraries even when the project code builds correctly
- hardware timing and pin mapping may still require bench validation
- the receiver/host-side implementation is not documented in this repo
- runtime configuration persistence is focused on the stored protocol, not a
  broader configuration database

## Suggested Next Documentation Maintenance

When major firmware changes happen, update:

1. `project-overview.md` if architecture or hardware model changes
2. `protocol-reference.md` if compact wire or EEPROM protocol changes
3. `protocol-v1-to-v2-migration.md` if transition compatibility changes
4. `protocol-v2-dictionary.json` if compact aliases or numeric ids change
5. `nano-flash-optimization.md` if memory-saving build behavior changes
