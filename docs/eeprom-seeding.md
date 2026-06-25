# EEPROM Seeding

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 23:10:00 +03:00

## Purpose

This repo no longer seeds a default run protocol during normal firmware
startup.

That change keeps the production firmware behavior explicit:

- if EEPROM already contains a valid protocol, the controller loads it
- if EEPROM is empty or invalid, telemetry reports `pv:0`
- the controller raises `kFaultProtocolStore` until a protocol is uploaded

## Minimal Seeded Protocol

The host-side seeding flow installs a deliberately tiny valid program:

- metadata:
  - `sc=12`
  - `b1=0`
  - `b2=1`
  - `s1=5`
  - `sn=1`
  - `rr=1`
  - `im=1`
  - `cd=1000`
  - `jd=1000`
  - `dd=1000`
  - `w0=1000`
  - `w1..w7=0`
- `prepare_length=1`
- program bytes: `00 61 50 62 00`

Opcode meaning:

- `00` `END_CYCLE`
- `61` `EVENT_RUN_START`
- `50` `WAIT_0`
- `62` `EVENT_RUN_STOP`
- `00` `END_CYCLE`

This gives the board a valid EEPROM image without restoring the older,
larger built-in fallback program.

## How To Seed

If the firmware is already flashed:

```powershell
pio run -e nanoatmega328_usb -t seed_minimal_protocol --upload-port COM5
```

If you want to flash the firmware and then seed EEPROM in one command:

```powershell
pio run -e nanoatmega328_usb -t upload -t seed_minimal_protocol --upload-port COM5
```

The same host flow is also available directly:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode seed-minimal-protocol -Port COM5
```

## Round-Trip EEPROM Test

For a hardware write/readback test, use the dedicated EEPROM-test build:

```powershell
pio run -e nanoatmega328_usb_eepromtest -t upload -t eeprom_roundtrip_test --upload-port COM5
```

Direct PowerShell form after that test firmware is flashed:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode eeprom-roundtrip -Port COM5
```

The round-trip test:

- clears the stored protocol
- confirms EEPROM reports an invalid image
- writes the minimal protocol
- reads back stored metadata, lengths, and CRC through `protocol.info`
- starts the stored program and confirms it executes from EEPROM
- accepts a small hardware timing tolerance around the `1000 ms` wait step

## Expected Result

After successful seeding:

- `status.get` telemetry reports `pv:1`
- the protocol-store fault bit clears if it was present only because EEPROM was
  empty
- later boots reuse the stored EEPROM image without rewriting it
