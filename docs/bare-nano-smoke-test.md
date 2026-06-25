# Bare-Nano USB Serial Smoke Test

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 10:06:32 +03:00

## Purpose

This guide validates that the firmware is alive on an Arduino Nano even when
no CE-CUBE peripherals are connected.

It checks:

- USB serial transport
- compact `v:2` JSON command parsing
- `ack`, `error`, `telemetry`, and `event` replies
- controller-side output latching
- lift and carousel state-machine behavior
- a short EEPROM-backed automation smoke test

It does not validate real sensors or real actuator motion.

## Important Notes

- open the serial port at `115200`
- opening the COM port usually resets the Nano, so wait `2-3` seconds before
  sending the first command
- `ack` and `error` echo the command sequence in `s`
- `telemetry` and `event` use the firmware's own outbound sequence numbers
- idle telemetry omits `at`
- events carry `gt` and run-related events carry `at` once analysis timing is valid
- `automation-smoke` overwrites the stored EEPROM protocol temporarily

## Compact Codes Used In Smoke Tests

Message kinds:

- `1` telemetry
- `2` command
- `3` ack
- `4` error
- `5` event

Ack codes:

- `0` accepted
- `1` status

Error codes used here:

- `2` invalid_field
- `3` invalid_command
- `4` busy
- `10` not_ready
- `16` unavailable

Event codes used here:

- `1` run_start
- `2` run_stop

## Expected Bare-Board Faults

### `nanoatmega328`

Expected steady-state telemetry after startup settling:

- `pv: 1`
- `hv: 0`
- `pm: 0`
- `v1: 0`
- `v2: 0`
- `rs: 0`
- `cs: 0`
- `ps: 1`
- `ss: 2`
- `ff: 38`

Reason for `ff: 38`:

- RF24 missing
- AD7745 missing
- pressure sensor missing

### `nanoatmega328_usb`

Expected steady-state telemetry is the same except:

- `ff: 36`

Reason for `ff: 36`:

- RF24 is compiled out, so only sensor and pressure faults remain

## Manual Serial Bring-Up

Use `pio device monitor -b 115200 -p COMx` or any serial terminal that sends a
single newline at the end of each JSON line.

### 1. Status Check

Send:

```json
{"v":2,"k":2,"s":1,"c":21}
```

Expect:

- one `ack` line with `a:1`
- one `telemetry` line with `k:1`
- no `at` field on the idle bare board

### 2. Parser Checks

Send:

```json
{"v":2,"k":2,"s":2,"c":99}
{"v":2,"k":2,"s":3,"c":9}
```

Expect:

- `e:3` for the unknown command id
- `e:2` for missing `en` on `hv.set`

### 3. Output-Latch Checks

Send each command and then poll with `status.get`:

```json
{"v":2,"k":2,"s":4,"c":9,"en":1}
{"v":2,"k":2,"s":6,"c":10,"en":1}
{"v":2,"k":2,"s":8,"c":11,"vi":1,"en":1}
{"v":2,"k":2,"s":10,"c":11,"vi":2,"en":1}
```

Expect telemetry to reflect:

- `hv: 1`
- `pm: 1`
- `v1: 1`
- `v2: 1`

Turn all outputs back off before continuing:

```json
{"v":2,"k":2,"s":12,"c":9,"en":0}
{"v":2,"k":2,"s":13,"c":10,"en":0}
{"v":2,"k":2,"s":14,"c":11,"vi":1,"en":0}
{"v":2,"k":2,"s":15,"c":11,"vi":2,"en":0}
```

### 4. Lift State-Machine Check

Send:

```json
{"v":2,"k":2,"s":20,"c":5,"lp":1}
{"v":2,"k":2,"s":21,"c":5,"lp":0}
```

Expect:

- first command accepted with `a:0`
- second command rejected with `e:4`

Wait about `500 ms`, then send `status.get`. Expect `lf: 2`.

Repeat once in the opposite direction and expect `lf: 0` after settling.

### 5. Carousel State-Machine Check

Send:

```json
{"v":2,"k":2,"s":30,"c":7,"sl":1}
{"v":2,"k":2,"s":31,"c":7,"sl":2}
```

Expect:

- first command accepted with `a:0`
- second command rejected with `e:4`

Wait about `1500 ms`, then send `status.get`. Expect `cs: 1`.

### 6. Sensor Negative Check

Send:

```json
{"v":2,"k":2,"s":40,"c":4}
```

Expect:

- `k:4`
- `e:10`

### 7. Default-Build Feature Check

Send:

```json
{"v":2,"k":2,"s":41,"c":19}
```

Expect on `nanoatmega328` and `nanoatmega328_usb`:

- `k:4`
- `e:16`

## Cleaner USB-Only Path

Reflash `nanoatmega328_usb` and repeat the same manual sequence. This is the
recommended baseline because RF24 fault reporting is removed from the expected
startup picture.

## Temporary Automation Smoke Test

This path overwrites the stored protocol in EEPROM.

### Upload Metadata

```json
{"v":2,"k":2,"s":40,"c":16,"sc":12,"b1":0,"b2":1,"s1":5,"sn":1,"rr":1,"w0":1000,"w1":0,"w2":0,"w3":0,"w4":0,"w5":0,"w6":0,"w7":0}
```

### Upload Program Chunk

Program bytes:

- `0x61`
- `0x50`
- `0x62`
- `0x00`

Meaning:

- `EVENT_RUN_START`
- `WAIT_0`
- `EVENT_RUN_STOP`
- `END_CYCLE`

Chunk command:

```json
{"v":2,"k":2,"s":41,"c":17,"of":0,"dt":"6150620000000000"}
```

### Commit

```json
{"v":2,"k":2,"s":42,"c":18,"ln":4,"cr":38944}
```

### Start Run

```json
{"v":2,"k":2,"s":43,"c":13}
```

Expect:

- `a:0` for `protocol.begin`
- `a:0` for `protocol.chunk`
- `a:0` for `protocol.commit`
- `a:0` for `run.start`
- one event with `k:5`, `ev:1`, a nonzero `gt`, and `at:0` almost immediately
- one event with `k:5`, `ev:2`, a later `gt`, and `at:1000` about `1` second later

After about `1.5` seconds, `status.get` should show:

- `rs: 2`
- `at: 1000`
- unchanged bare-board faults

After this smoke test, EEPROM no longer contains the previous stored protocol.
Overwrite it again before real automation use.

## Scripted Smoke Testing

Windows-first smoke harness:

- `tools/serial_smoke.ps1`

The script:

- opens a COM port at `115200`
- waits `3` seconds after open
- sends compact `v:2` JSON lines
- parses each returned JSON line
- validates expected numeric reply kinds and codes
- writes a plain-text transcript under `artifacts/serial-smoke`

### List Ports

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode list-ports
```

### Current Flashed Build

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode current-build-smoke -Port COM5
```

### USB-Only Build

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode usb-build-smoke -Port COM5
```

### Manual-Smoke Alias

`manual-smoke` runs the same non-destructive checks and defaults to
`ExpectedFaults=38`.

Example for USB-only firmware:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode manual-smoke -Port COM5 -ExpectedFaults 36
```

### Automation Smoke

For the recommended USB-only path:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\serial_smoke.ps1 -Mode automation-smoke -Port COM5 -ExpectedFaults 36
```

## Legacy Appendix

One verbose `v:1` command example is kept here only for migration:

```json
{"v":1,"kind":"command","seq":1,"cmd":"status.get"}
```

The Nano hardware builds in this repo set `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS=0`
for flash savings, so that example is for host-side migration reference only.
New tooling should use compact `v:2`.

## Acceptance Criteria

- USB serial opens reliably
- `status.get` returns both `ack` and `telemetry`
- invalid command and invalid field errors are deterministic
- output commands update controller telemetry
- lift and carousel busy/final-state behavior matches expectations
- `sensor.auto_zero` returns `e:10` on a bare board
- the short EEPROM test emits `ev:1` then `ev:2`
- the short EEPROM test reaches `rs:2`
