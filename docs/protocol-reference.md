# CE-CUBE Protocol Reference

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 10:06:32 +03:00

## Scope

This is the normative wire-level reference for the CE-CUBE compact protocol.

- USB and RF24 use the same minified JSON payloads.
- Outgoing firmware messages use compact protocol `v:2` only.
- Incoming verbose `v:1` commands are still accepted during transition.
- The EEPROM bytecode schema is unchanged by this protocol revision.

## Version Policy

- `v:2`
  - required for all outgoing `telemetry`, `ack`, `error`, and `event`
  - accepted for incoming compact commands
- `v:1`
  - accepted for incoming verbose commands only when
    `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS=1`
- mixed-schema messages
  - rejected with `e:2` (`invalid_field`)

## RF24 Budget Snapshot

RF24 payload budget remains `23` JSON bytes per radio frame after the fixed
`9`-byte application header.

Observed compact payload sizes used by host tests:

| Message | Example bytes | RF24 frames |
| --- | ---: | ---: |
| `status.get` command | `26` | `2` |
| bare telemetry | `171` | `8` |
| timed run telemetry | `193` | `9` |
| standard run event | `65` | `3` |
| `protocol.begin` command | `129` | `6` |

## Compact Message Envelope

All `v:2` messages are JSON objects.

Top-level keys:

| Key | Meaning | Used By |
| --- | --- | --- |
| `v` | protocol version | all messages |
| `k` | message kind code | all messages |
| `s` | 16-bit sequence number | all messages |
| `c` | command id | command |
| `a` | ack code | ack |
| `e` | error code | error |
| `ev` | event code | event |

Message kind codes:

| Code | Meaning |
| ---: | --- |
| `1` | telemetry |
| `2` | command |
| `3` | ack |
| `4` | error |
| `5` | event |

Fixed `v:2` message shapes:

```json
{"v":2,"k":2,"s":N,"c":ID,...}
{"v":2,"k":3,"s":N,"a":CODE,"gt":...}
{"v":2,"k":4,"s":N,"e":CODE,"gt":...}
{"v":2,"k":1,"s":N,"gt":...,...telemetry fields...}
{"v":2,"k":5,"s":N,"ev":CODE,...event fields...}
```

## Compact Field Dictionary

These keys are the transport-facing aliases. The GUI should normalize them into
the canonical long names listed here.

| Key | Canonical Name | Notes |
| --- | --- | --- |
| `en` | `enable` | `0` or `1` |
| `cl` | `clear_zero` | `0` or `1` |
| `lp` | `lift_position` | `0` down, `1` up |
| `st` | `steps` | signed integer |
| `sl` | `slot` or `sample_slot` | context-dependent |
| `ad` | `adjust_direction` | `-1` counterclockwise, `1` clockwise |
| `vi` | `valve_id` | `1` or `2` |
| `im` | `injection_mode` | numeric enum |
| `du` | `duration_ms` | unsigned |
| `rh` | `rate_hz` | `9`, `11`, `13`, `16`, `26`, `50`, `84`, `91` |
| `fh` | `freq_hz` | `16000` or `32000` |
| `xl` | `excitation_level` | numeric enum |
| `sc` | `slot_count` | protocol metadata |
| `b1` | `bge1_slot` | protocol metadata |
| `b2` | `bge2_slot` | protocol metadata |
| `s1` | `sample_1_slot` | protocol metadata |
| `sn` | `sample_count` | protocol metadata |
| `rr` | `repetitions` | protocol metadata |
| `cd` | `collection_duration_ms` | protocol metadata |
| `jd` | `injection_duration_ms` | protocol metadata |
| `dd` | `droplet_duration_ms` | protocol metadata |
| `w0`..`w7` | `wait_0_ms`..`wait_7_ms` | milliseconds on wire |
| `of` | `offset` | protocol chunk offset |
| `dt` | `data` | `16` uppercase hex chars for `8` raw bytes |
| `ln` | `program_length` | bytes |
| `cr` | `crc16` | CRC16-CCITT |
| `gt` | `global_time_ms` | outbound uptime since firmware boot |
| `at` | `analysis_time_ms` | telemetry/event time since `EVENT_RUN_START` |
| `cp` | `cap_pf` | telemetry |
| `tc` | `temp_c` | telemetry |
| `ss` | `sensor_status` | numeric enum |
| `pp` | `pressure_pa` | telemetry |
| `ps` | `pressure_status` | numeric enum |
| `cu` | `current_ua` | telemetry |
| `hv` | `hv` | `0` or `1` |
| `pm` | `pump` | `0` or `1` |
| `v1` | `valve1` | `0` or `1` |
| `v2` | `valve2` | `0` or `1` |
| `pv` | `protocol_valid` | `0` or `1` |
| `lf` | `lift` | lift state enum |
| `cs` | `carousel_pos` | telemetry |
| `rs` | `run_state` | numeric enum |
| `pc` | `run_pc` | telemetry |
| `si` | `sample_index` | telemetry or event |
| `ri` | `repetition` | telemetry or event |
| `ff` | `faults` | bit mask |

## Timing Model

- `s`
  - sequence number only
  - never a clock or timestamp
- `gt`
  - monotonic firmware uptime in milliseconds since boot
  - emitted on outbound `telemetry`, `ack`, `error`, and `event` messages
- `at`
  - analysis-relative milliseconds since the most recent `EVENT_RUN_START`
    opcode executed by the stored run protocol
  - emitted on outbound `telemetry` and `event` messages only after that
    analysis reference is valid
  - omitted from idle telemetry

Timing consequence:

- the stored protocol itself decides where analysis time zero lives
- if the GUI wants the electropherogram origin at a different physical step,
  move the `EVENT_RUN_START` opcode to that step in the EEPROM bytecode

## Command IDs

| Id | Canonical Command | Required Compact Fields |
| ---: | --- | --- |
| `1` | `sensor.set_rate` | `rh` |
| `2` | `sensor.set_excitation` | at least one of `fh`, `xl` |
| `3` | `sensor.temp_comp` | `en` |
| `4` | `sensor.auto_zero` | optional `cl` |
| `5` | `lift.move` | `lp` |
| `6` | `carousel.step` | `st` |
| `7` | `carousel.goto_slot` | `sl` |
| `8` | `carousel.adjust` | `ad` |
| `9` | `hv.set` | `en` |
| `10` | `pump.set` | `en` |
| `11` | `valve.set` | `vi`, `en` |
| `12` | `injection.configure` | `im`, `du` |
| `13` | `run.start` | none |
| `14` | `run.stop` | none |
| `15` | `run.status` | none |
| `16` | `protocol.begin` | `sc,b1,b2,s1,sn,rr,im,cd,jd,dd,w0..w7` |
| `17` | `protocol.chunk` | `of`, `dt` |
| `18` | `protocol.commit` | `ln`, `cr` |
| `19` | `protocol.info` | none |
| `20` | `protocol.clear` | none |
| `21` | `status.get` | none |

Compact command examples:

```json
{"v":2,"k":2,"s":1,"c":21}
{"v":2,"k":2,"s":2,"c":9,"en":1}
{"v":2,"k":2,"s":3,"c":11,"vi":1,"en":0}
{"v":2,"k":2,"s":4,"c":5,"lp":1}
{"v":2,"k":2,"s":5,"c":7,"sl":4}
{"v":2,"k":2,"s":6,"c":1,"rh":9}
```

## Replies And Events

Ack codes:

| Code | Meaning |
| ---: | --- |
| `0` | accepted |
| `1` | status |

Error codes:

| Code | Meaning |
| ---: | --- |
| `1` | invalid_json |
| `2` | invalid_field |
| `3` | invalid_command |
| `4` | busy |
| `5` | overflow |
| `6` | rf_crc |
| `7` | rf_fragment |
| `8` | sensor_io |
| `9` | sensor_config |
| `10` | not_ready |
| `11` | protocol_invalid |
| `12` | protocol_crc |
| `13` | protocol_bounds |
| `14` | protocol_opcode |
| `15` | storage |
| `16` | unavailable |

Event codes:

| Code | Meaning |
| ---: | --- |
| `1` | run_start |
| `2` | run_stop |
| `3` | sample_ready |
| `4` | protocol_info |

Standard event shape:

```json
{"v":2,"k":5,"s":N,"ev":CODE,"gt":...,"si":...,"sl":...,"ri":...}
```

Standard event example:

```json
{"v":2,"k":5,"s":12,"ev":1,"gt":1234,"at":0,"si":1,"sl":5,"ri":2}
```

`protocol_info` event shape:

```json
{"v":2,"k":5,"s":N,"ev":4,"gt":...,"pv":...,"sc":...,"b1":...,"b2":...,"s1":...,"sn":...,"rr":...,"im":...,"cd":...,"jd":...,"dd":...,"w0":...,"w1":...,"w2":...,"w3":...,"w4":...,"w5":...,"w6":...,"w7":...,"pl":...,"ln":...,"cr":...}
```

## Telemetry

Telemetry shape:

```json
{"v":2,"k":1,"s":N,"gt":...,"cp":...,"tc":...,"ss":...,"pp":...,"ps":...,"cu":...,"hv":...,"pm":...,"v1":...,"v2":...,"pv":...,"lf":...,"cs":...,"rs":...,"pc":...,"si":...,"sl":...,"ri":...,"ff":...}
{"v":2,"k":1,"s":N,"gt":...,"at":...,"cp":...,"tc":...,"ss":...,"pp":...,"ps":...,"cu":...,"hv":...,"pm":...,"v1":...,"v2":...,"pv":...,"lf":...,"cs":...,"rs":...,"pc":...,"si":...,"sl":...,"ri":...,"ff":...}
```

Telemetry delivery notes:

- `status.get` still triggers an immediate telemetry reply
- if no telemetry has been sent for about `10` seconds, the firmware emits an
  unsolicited heartbeat telemetry frame using the same schema
- pressure-sample workflows can still trigger immediate telemetry when the
  controller explicitly requests a forced pressure sample
- any telemetry emission, including `status.get` replies and forced pressure
  samples, resets the heartbeat timer

Telemetry status enums:

Sensor status:

| Code | Meaning |
| ---: | --- |
| `0` | ok |
| `1` | not_ready |
| `2` | bus_error |
| `3` | config_error |
| `4` | invalid_sample |

Pressure status:

| Code | Meaning |
| ---: | --- |
| `0` | ok |
| `1` | unavailable |
| `2` | bus_error |
| `3` | config_error |

Lift state:

| Code | Meaning |
| ---: | --- |
| `0` | down |
| `1` | moving_up |
| `2` | up |
| `3` | moving_down |

Run state:

| Code | Meaning |
| ---: | --- |
| `0` | idle |
| `1` | running |
| `2` | complete |
| `3` | stopped |
| `4` | fault |

Bare-board telemetry example:

```json
{"v":2,"k":1,"s":1,"gt":0,"cp":0.000000,"tc":0.000,"ss":2,"pp":0,"ps":1,"cu":0,"hv":0,"pm":0,"v1":0,"v2":0,"pv":1,"lf":0,"cs":0,"rs":0,"pc":0,"si":0,"sl":0,"ri":0,"ff":36}
```

Timed run telemetry example:

```json
{"v":2,"k":1,"s":42,"gt":7890,"at":3456,"cp":1.234567,"tc":20.125,"ss":0,"pp":101325,"ps":0,"cu":3210,"hv":1,"pm":0,"v1":1,"v2":0,"pv":1,"lf":2,"cs":5,"rs":1,"pc":9,"si":1,"sl":5,"ri":2,"ff":3}
```

## Parser Rules

- `v:1`
  - verbose command parser only when legacy parsing is enabled
- `v:2`
  - compact parser only
- mixed verbose and compact keys in the same message
  - invalid
- compact outbound encoding
  - always used for replies, telemetry, and events
- compact boolean-like fields
  - encoded as `0` or `1`, never `true` or `false`

## RF24 Framing

RF24 carries fragments of the exact same compact JSON body used over USB.

Fixed header fields per frame:

- protocol version
- message kind
- sequence number
- fragment index
- fragment count
- fragment payload length
- CRC16-CCITT of the full JSON body

Protection layers:

- application CRC16 across the full JSON payload
- RF24 hardware CRC16 at link level

Current compatibility rule:

- outgoing RF24 frames use header version `2`
- reassembly accepts header version `1` or `2`

## Stored EEPROM Protocol

The stored run-program schema is unchanged by compact protocol `v:2`.

EEPROM layout:

- `0x000` to `0x03F`
  - header
- `0x040` to `0x3BF`
  - bytecode program
- `0x3C0` to `0x3FF`
  - reserved

Stored header fields:

- magic and schema markers
- valid flag
- `slot_count`
- `bge1_slot`
- `bge2_slot`
- `sample_1_slot`
- `sample_count`
- `repetitions`
- `program_length`
- `program_crc16`
- `header_crc16`
- `wait_0_ms` through `wait_7_ms` stored internally in `100 ms` units

Execution properties:

- the firmware does not auto-seed EEPROM on boot
- an empty or cleared EEPROM image leaves `protocol_valid=0` until the host
  uploads or seeds a program
- one opcode is read from EEPROM at a time
- the full program is never loaded into RAM
- sample progression is metadata-driven
- all repetitions complete for the current sample before the next sample starts

Opcode map:

| Opcode | Meaning |
| ---: | --- |
| `0x00` | `END_CYCLE` |
| `0x01` | `NOP` |
| `0x10` | `LIFT_DOWN` |
| `0x11` | `LIFT_UP` |
| `0x12` | `AUTO_ZERO` |
| `0x20` | `GOTO_BGE1` |
| `0x21` | `GOTO_BGE2` |
| `0x22` | `GOTO_SAMPLE_CURRENT` |
| `0x30`..`0x3F` | `GOTO_SLOT_0`..`GOTO_SLOT_15` |
| `0x40` | `HV_ON` |
| `0x41` | `HV_OFF` |
| `0x42` | `PUMP_ON` |
| `0x43` | `PUMP_OFF` |
| `0x44` | `VALVE1_ON` |
| `0x45` | `VALVE1_OFF` |
| `0x46` | `VALVE2_ON` |
| `0x47` | `VALVE2_OFF` |
| `0x50`..`0x57` | `WAIT_0`..`WAIT_7` |
| `0x60` | `PRESSURE_SAMPLE` |
| `0x61` | `EVENT_RUN_START` |
| `0x62` | `EVENT_RUN_STOP` |
| `0x63` | `EVENT_SAMPLE_READY` |

Validation rejects:

- bad CRC
- invalid slot math
- invalid opcode
- oversized program
- missing `END_CYCLE`

## Build Note

In the default `nanoatmega328` and `nanoatmega328_usb` builds, `protocol.info`
is compiled out for flash savings and returns `e:16` (`unavailable`).

Those same Nano hardware builds also disable the legacy verbose `v:1` command
parser with `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS=0`, so hardware bring-up should
use compact `v:2` commands only.
