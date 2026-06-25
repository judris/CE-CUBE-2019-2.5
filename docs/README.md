# CE-CUBE Project Documentation

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 22:30:26 +03:00

## Purpose

This `docs` directory records the current state of the CE-CUBE system firmware
repository.

The repo is a structured rewrite of the older CE-CUBE instrument control
firmware into a safer, more testable PlatformIO project for AVR targets,
especially Arduino Nano ATmega328P.

## Documents

- `project-overview.md`
  - repository purpose
  - architecture
  - hardware model
  - build targets
  - dependency policy
  - testing strategy
  - current limitations
  - AVR-specific startup-construction note for `src/main.cpp`
  - legacy-versus-current note about debug `Serial` usage versus the real USB
    serial control path
- `protocol-reference.md`
  - normative compact `v:2` wire protocol
  - compact timing fields for `gt` and `at`
  - RF24 size budgets
  - EEPROM-backed run protocol format
  - opcode map and status/fault summary
- `eeprom-seeding.md`
  - explicit EEPROM boot behavior
  - minimal valid stored protocol
  - PlatformIO and PowerShell seeding commands
  - EEPROM round-trip test flow
- `protocol-v1-to-v2-migration.md`
  - verbose-to-compact command mapping
  - reply/event numeric-code migration
  - field alias conversion
- `protocol-v2-dictionary.json`
  - machine-readable compact alias dictionary
  - canonical names for GUI normalization
- `bare-nano-smoke-test.md`
  - manual USB serial smoke testing on a Nano with no peripherals
  - scripted Windows PowerShell smoke test flow
  - compact `v:2` copy-paste command examples
  - expected bare-board replies, faults, and temporary EEPROM test behavior
- `nano-flash-optimization.md`
  - flash and RAM reduction work for Nano targets
  - feature flags
  - build profile differences
  - measured memory usage
  - note about the USB Nano profile disabling current-sense polling to fit the
    older AVR toolchain path

## Recommended Reading Order

1. `project-overview.md`
2. `protocol-reference.md`
3. `eeprom-seeding.md`
4. `protocol-v1-to-v2-migration.md`
5. `protocol-v2-dictionary.json`
6. `bare-nano-smoke-test.md`
7. `nano-flash-optimization.md`

## Documentation Scope

These notes describe the current repository contents and current build
configuration. They are intended to help future work stay aligned with the
project's embedded safety constraints:

- no Arduino `String`
- no dynamic allocation after startup
- bounded control flow
- fixed-capacity buffers
- explicit status/error handling
