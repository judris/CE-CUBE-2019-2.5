# Nano Flash Optimization Notes

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 10:06:32 +03:00

## Scope

This note documents the Nano flash and RAM reduction work, including the
compact protocol `v:2` follow-up completed on 2026-06-25.

The goal was to move the firmware away from the flash limit and create safer
build-time feature choices without changing the core CE-CUBE control model.

This note now also covers the timing-field follow-up completed later on
2026-06-25, where compact run timestamps were added and the compatibility-only
verbose `v:1` parser was disabled on Nano hardware builds to keep them fitting.

## Summary Of Changes

- Added a small compile-time feature flag header:
  `lib/ce_cube_core/include/ce_cube/feature_flags.hpp`
- Updated `platformio.ini` to add:
  - AVR size optimization flag `-mcall-prologues`
  - constant-merging flag `-fmerge-all-constants`
  - RTTI disable flag `-fno-rtti`
  - default Nano feature switches
  - two alternate Nano build profiles
- Moved `FirmwareApp` construction from static startup into `setup()` so the
  Nano avoids unnecessary global-constructor flash overhead
- Gated optional firmware paths so unused code does not stay linked in:
  - RF24 transport
  - pressure polling integration
  - `protocol.info` event payload support
- Disabled the legacy verbose `v:1` command parser on Nano hardware builds:
  - `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS=0`
- Added a true RF24-disabled stub path so USB-only builds do not require
  `RF24.h` or the RF24 library at compile time.

## New Build Profiles

### `nanoatmega328`

RF24-enabled Nano build.

- RF24 enabled
- pressure sensor enabled
- `protocol.info` disabled

This currently exceeds Nano flash under the present toolchain and is retained
as a non-default profile for future flash-reduction work.

### `nanoatmega328_full`

Feature-complete Nano build.

- RF24 enabled
- pressure sensor enabled
- `protocol.info` enabled

This no longer fits after the compact protocol update and needs another flash
reduction pass before it can be treated as shippable again.

### `nanoatmega328_usb`

Lean USB-only Nano build.

- RF24 disabled
- pressure sensor enabled
- `protocol.info` disabled

Use this when the instrument is connected directly over USB and the RF24 link
is not required. This is the current default and recommended Nano profile.

## Feature Flags

The build is controlled with these macros:

- `CE_CUBE_ENABLE_RF24`
- `CE_CUBE_ENABLE_PRESSURE`
- `CE_CUBE_ENABLE_PROTOCOL_INFO`
- `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS`

Default values live in
`lib/ce_cube_core/include/ce_cube/feature_flags.hpp`, and each PlatformIO
environment can override them in `build_flags`.

## Files Updated

- `platformio.ini`
- `src/main.cpp`
- `lib/ce_cube_core/include/ce_cube/feature_flags.hpp`
- `lib/ce_cube_core/src/instrument_controller.cpp`
- `lib/ce_cube_core/src/protocol.cpp`
- `src/firmware/firmware_app.hpp`
- `src/firmware/firmware_app.cpp`
- `src/firmware/rf24_transport.hpp`
- `src/firmware/rf24_transport.cpp`

## Behavior Change

The default `nanoatmega328_usb` build now rejects the `protocol.info` command with
`ErrorCode::kUnavailable`.

That change is intentional to save flash in the default Nano configuration.
If `protocol.info` is needed, use `nanoatmega328_full`.

## Measured Results

### Before This Optimization Pass

- flash was near or above the Nano limit
- one reported failing build was:
  - flash: `30764 / 30720`
  - RAM: `1851 / 2048`

### After This Optimization Pass

#### `nanoatmega328`

- flash: `32688 / 30720`
- RAM: `1742 / 2048`
- status: does not fit

#### `nanoatmega328_full`

- flash: not remeasured in this workspace
- RAM: not remeasured in this workspace
- status: does not fit

#### `nanoatmega328_usb`

- flash: `30680 / 30720`
- RAM: `1488 / 2048`
- status: builds successfully

## Validation Performed

- `pio run -e nanoatmega328_usb`
- `pio run -e nanoatmega328`
- `pio test -e native`

Current note about `nanoatmega328_full`:

- it was not revalidated in this timing follow-up
- it still needs a dedicated flash check before it is treated as shippable

## Notes About Warnings

The project-owned firmware builds successfully for:

- `nanoatmega328_usb`
- `native`

Compiler warnings still appear from upstream Arduino core and third-party
libraries such as RF24, Servo, Wire, EEPROM, and Unity. Those warnings are not
from the CE-CUBE project sources changed in this pass.

## Recommended Next Step

The best next flash-focused follow-up is:

- recover enough space for an RF24-enabled Nano build if wireless transport is
  still required on ATmega328P hardware
- reduce or gate `protocol.info` further so `nanoatmega328_full` fits again
- decide whether a future non-Nano target should re-enable verbose `v:1`
  compatibility, or whether the project can retire it entirely
- keep compact `v:2` as the only outbound wire format
- continue using host tests plus the serial smoke harness after each size cut
