# CE-CUBE Protocol v1 To v2 Migration

Documented by: Codex (OpenAI GPT-5 coding agent)
Timestamp: 2026-06-25 10:06:32 +03:00

## Scope

This document maps the previous verbose `v:1` command schema to the compact
`v:2` wire contract.

Transition policy:

- firmware still accepts incoming `v:1` commands only when
  `CE_CUBE_ENABLE_LEGACY_V1_COMMANDS=1`
- firmware accepts incoming compact `v:2` commands
- firmware no longer emits verbose `v:1` replies, telemetry, or events

## Envelope Mapping

| v1 | v2 | Notes |
| --- | --- | --- |
| `v` | `v` | `1` becomes `2` for compact messages |
| `kind:"command"` | `k:2` | numeric message kind |
| `kind:"telemetry"` | `k:1` | outbound only |
| `kind:"ack"` | `k:3` | outbound only |
| `kind:"error"` | `k:4` | outbound only |
| `kind:"event"` | `k:5` | outbound only |
| `seq` | `s` | same sequence meaning |
| `cmd:"..."` | `c:<id>` | string command name becomes numeric id |
| `code:"..."` | `a:<id>` or `e:<id>` | split by reply type |
| `event:"..."` | `ev:<id>` | string event name becomes numeric id |

## Command Name Mapping

| v1 Command | v2 Id |
| --- | ---: |
| `sensor.set_rate` | `1` |
| `sensor.set_excitation` | `2` |
| `sensor.temp_comp` | `3` |
| `sensor.auto_zero` | `4` |
| `lift.move` | `5` |
| `carousel.step` | `6` |
| `carousel.goto_slot` | `7` |
| `carousel.adjust` | `8` |
| `hv.set` | `9` |
| `pump.set` | `10` |
| `valve.set` | `11` |
| `injection.configure` | `12` |
| `run.start` | `13` |
| `run.stop` | `14` |
| `run.status` | `15` |
| `protocol.begin` | `16` |
| `protocol.chunk` | `17` |
| `protocol.commit` | `18` |
| `protocol.info` | `19` |
| `protocol.clear` | `20` |
| `status.get` | `21` |

## Field Mapping

### Command And Setup Fields

| v1 Field | v2 Field | Value Mapping |
| --- | --- | --- |
| `rate_hz` | `rh` | same numeric rate values |
| `freq_hz` | `fh` | same numeric frequency values |
| `level` | `xl` | `vdd_8->0`, `vdd_4->1`, `vdd_3_8->2`, `vdd_2->3` |
| `enable` | `en` | `true/false` or `1/0` becomes `1/0` only |
| `clear` | `cl` | `1/0` |
| `position` | `lp` | `down->0`, `up->1` |
| `steps` | `st` | same signed integer |
| `slot` | `sl` | same numeric value |
| `dir` | `ad` | `cw` or `clock -> 1`, `ccw` or `counter -> -1` |
| `id` | `vi` | same numeric value |
| `mode` | `im` | `hydro->0`, `electro->1` |
| `duration_ms` | `du` | same numeric value |

### Protocol Upload Fields

| v1 Field | v2 Field |
| --- | --- |
| `slot_count` | `sc` |
| `bge1_slot` | `b1` |
| `bge2_slot` | `b2` |
| `sample_1_slot` | `s1` |
| `sample_count` | `sn` |
| `repetitions` | `rr` |
| `injection_mode` | `im` |
| `collection_duration_ms` | `cd` |
| `injection_duration_ms` | `jd` |
| `droplet_duration_ms` | `dd` |
| `wait_0_ms` | `w0` |
| `wait_1_ms` | `w1` |
| `wait_2_ms` | `w2` |
| `wait_3_ms` | `w3` |
| `wait_4_ms` | `w4` |
| `wait_5_ms` | `w5` |
| `wait_6_ms` | `w6` |
| `wait_7_ms` | `w7` |
| `off` | `of` |
| `data` | `dt` |
| `len` | `ln` |
| `crc` | `cr` |

### Telemetry And Event Fields

| v1 Field | v2 Field |
| --- | --- |
| `cap_pf` | `cp` |
| `temp_c` | `tc` |
| `sensor_status` | `ss` |
| `pressure_pa` | `pp` |
| `pressure_status` | `ps` |
| `pump` | `pm` |
| `valve1` | `v1` |
| `valve2` | `v2` |
| `protocol_valid` | `pv` |
| `lift` | `lf` |
| `carousel_pos` | `cs` |
| `run_state` | `rs` |
| `run_pc` | `pc` |
| `sample_index` | `si` |
| `sample_slot` | `sl` |
| `repetition` | `ri` |
| `faults` | `ff` |
| `event` | `ev` |

### New v2-Only Timing Fields

These fields did not exist in the verbose `v:1` schema.

| Canonical Name | v2 Field | Notes |
| --- | --- | --- |
| `global_time_ms` | `gt` | outbound `event` uptime in milliseconds since firmware boot |
| `analysis_time_ms` | `at` | outbound `telemetry` or `event` milliseconds since the most recent `EVENT_RUN_START`; omitted until valid |

## Reply Code Mapping

### Ack

| v1 | v2 |
| --- | ---: |
| `accepted` | `0` |
| `status` | `1` |

### Error

| v1 | v2 |
| --- | ---: |
| `invalid_json` | `1` |
| `invalid_field` | `2` |
| `invalid_command` | `3` |
| `busy` | `4` |
| `overflow` | `5` |
| `rf_crc` | `6` |
| `rf_fragment` | `7` |
| `sensor_io` | `8` |
| `sensor_config` | `9` |
| `not_ready` | `10` |
| `protocol_invalid` | `11` |
| `protocol_crc` | `12` |
| `protocol_bounds` | `13` |
| `protocol_opcode` | `14` |
| `storage` | `15` |
| `unavailable` | `16` |

### Event

| v1 | v2 |
| --- | ---: |
| `run_start` | `1` |
| `run_stop` | `2` |
| `sample_ready` | `3` |
| `protocol_info` | `4` |

## Example Conversion

Verbose `v:1` status request:

```json
{"v":1,"kind":"command","seq":1,"cmd":"status.get"}
```

Compact `v:2` equivalent:

```json
{"v":2,"k":2,"s":1,"c":21}
```

Verbose `protocol.begin`:

```json
{"v":1,"kind":"command","seq":40,"cmd":"protocol.begin","slot_count":12,"bge1_slot":0,"bge2_slot":1,"sample_1_slot":5,"sample_count":1,"repetitions":1,"injection_mode":"electro","collection_duration_ms":1000,"injection_duration_ms":1000,"droplet_duration_ms":1000,"wait_0_ms":1000,"wait_1_ms":0,"wait_2_ms":0,"wait_3_ms":0,"wait_4_ms":0,"wait_5_ms":0,"wait_6_ms":0,"wait_7_ms":0}
```

Compact `v:2` equivalent:

```json
{"v":2,"k":2,"s":40,"c":16,"sc":12,"b1":0,"b2":1,"s1":5,"sn":1,"rr":1,"im":1,"cd":1000,"jd":1000,"dd":1000,"w0":1000,"w1":0,"w2":0,"w3":0,"w4":0,"w5":0,"w6":0,"w7":0}
```

## GUI Guidance

Recommended GUI flow:

1. parse the compact `v:2` JSON object
2. translate keys and numeric ids through `protocol-v2-dictionary.json`
3. normalize into readable internal names such as `cap_pf`, `run_state`,
   `protocol_valid`, `global_time_ms`, and `analysis_time_ms`
4. keep compact aliases at the transport boundary only
