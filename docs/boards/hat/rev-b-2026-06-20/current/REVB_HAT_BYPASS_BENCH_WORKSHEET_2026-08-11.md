# Rev-B HAT Jumper-Bypass Bench Worksheet (2026-08-11)

Purpose: validate Rev-B HAT electrical stability in temporary jumper-bypass mode after range-switch MOSFET removal, and produce a repeatable evidence record for keep-bypass vs reintroduce-switching decision.

Controller alignment for this worksheet:
1. Active controller is STM32F103C8T6 Blue Pill firmware in `stm32-bluepill-bringup/`.
2. The command sequence in Phase 5 uses the STM32 command shell from `stm32-bluepill-bringup/src/main.cpp`.
3. Do not substitute the root `hat_c6_i2c_probe` ESP32-C6 target for Phase 5 checks.

Current temporary hardware state:
1. Range-switch MOSFETs on affected path removed.
2. Hard jumper installed: +5V_Reg -> R19.
3. Hard jumper installed: +3.3V_Reg -> R17.

## Pass Criteria

1. No hard short, smoke, or thermal runaway.
2. VIN, +5V_Boot, +5V_Reg, and +3.3V_Reg remain stable during no-load and light-load dwell.
3. R19 and R17 destination nodes track their source rails within expected drop for jumper-wire + contact resistance.
4. No abnormal heating at jumper wires, destination pads, or adjacent components.
5. Command path remains responsive (D9ON, D9OFF, D9FLASH) with no reset loop.

## Stop Rules

1. Any rapid current rise or PSU foldback.
2. Rail collapse, oscillation, or uncontrolled drift.
3. Jumper wire or destination component heating beyond safe touch threshold.
4. Evidence of unintended backfeed or wrong-polarity node behavior.

## Bench Setup

| Item | Required State | Actual |
|---|---|---|
| Bench PSU | 12 V preset, output OFF initially | |
| PSU current limit | Conservative start (0.10 A to 0.20 A) | |
| DMM #1 | Rail voltage checks | |
| DMM #2 or scope | Node + transition checks | |
| Serial monitor | 115200 8N1, STM32 Blue Pill command shell active (USART1/CH340 path; historically COM7) | |
| Board mode | Rev-B jumper-bypass configuration installed | |

## Phase 1 - Unpowered Safety Checks

### 1.1 Visual and continuity checks

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| VIN to GND resistance | Not a hard short | | |
| +5V_Reg to GND resistance | Not a hard short | | |
| +3.3V_Reg to GND resistance | Not a hard short | | |
| Jumper +5V_Reg -> R19 continuity | Low ohms / solid | | |
| Jumper +3.3V_Reg -> R17 continuity | Low ohms / solid | | |
| No unintended short between jumper rails | Open / high resistance | | |

Gate to continue:
1. No hard-short readings.
2. Both bypass jumpers confirmed installed and solid.

## Phase 2 - Controlled First Power (Bypass Mode)

1. Set PSU to 12 V and conservative current limit.
2. Power on and monitor current immediately.
3. Hold 5 to 10 seconds, then 1 minute if stable.

| Item | Actual |
|---|---|
| Immediate current at power-on | 118 mA @ 12.02 V (bench PSU) |
| Current after 5 seconds | ~118 mA (stable) |
| Current after 60 seconds | ~118 mA (stable) |
| Any audible/visual anomaly | None reported |

## Phase 3 - Rail Baseline Table

Measure with no external load first.

| Rail / Node | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| VIN | Near PSU setpoint | PSU: 12.02 V, DMM: 11.98 V, INA: 11.432 V @ 100 mA | PASS (source), HOLD (INA calibration) | Physical VIN is stable near setpoint; INA under-reads vs DMM/PSU in this capture. |
| +5V_Boot | About 5.0 V | 4.9149 V | PASS | Within expected bring-up class. |
| +5V_Reg | About 5.0 V class | 5.0740 V (CrowPanel CH1: 5.080 V) | PASS | DMM and telemetry agree closely. |
| +3.3V_Reg | About 3.3 V class | 3.5196 V with R25 out/non-effective; 3.335 V with R25 in/effective | HOLD / CONDITIONAL PASS | RB-011: selector/reference dependency on R25. Proceed only in R25-in state. |
| R19 destination node | Tracks +5V_Reg | Measured drop across R19: 0.0682 V (R19 = 0.200 ohm) => ~0.341 A equivalent | PASS (loaded) | Drop/current aligns with `INARAILS` low-range channel current under load. |
| R17 destination node | Tracks +3.3V_Reg | Measured drop across R17: 0.0677 V (R17 = 0.200 ohm) => ~0.339 A equivalent | PASS (loaded) | Drop/current aligns with `INARAILS` low-range channel current under load. |

Bench note:
1. R17 and R19 component values confirmed as 200 mOhm each.
2. CrowPanel CH2 observed ~3.520 V in the high 3.3V state.
3. This behavior is tracked as RB-011 in docs/REGULATOR_BOARD_CHANGE_TRACKER.md.

## Phase 4 - Light-Load Stability

Apply light resistive loads one rail at a time and record drift.

Recommended starter loads:
1. 5 V rail: 100 ohm.
2. 3.3 V rail: 100 ohm.

Actual loads used in this run:
1. 5 V rail: 15 ohm.
2. 3.3 V rail: 10 ohm.

| Test | Start V | Loaded V | Delta V | Input Current | Thermal Notes | Pass/Fail |
|---|---|---|---|---|---|---|
| 5V load test (15 ohm) | 5.074 V | 4.960 V (`INARAILS` lo ch) | -0.114 V | 264.44 mA (`INARAILS` input) | Load resistor warm; rail remained stable. Additional drop attributed to current jumper/sense placement not compensating R19 + ferrite path drop. | PASS (conditional) |
| 3.3V load test (10 ohm) | 3.335 V (R25 effective baseline) | 3.320 V (`INARAILS` lo ch) | -0.015 V | 264.44 mA (`INARAILS` input) | Load resistor warm; minimal droop observed. | PASS |

Dwell checkpoint (2 to 5 minutes):

| Check | Result | Notes |
|---|---|---|
| No thermal creep at jumper wires | PASS | No abnormal jumper heating reported. |
| No drift runaway on R19 node | PASS | 5V rail held stable under applied load with expected path drop. |
| No drift runaway on R17 node | PASS | 3.3V rail stayed near 3.320 V under load. |
| Rails remain stable | PASS | No oscillation/collapse reported during loaded run. |

## Phase 5 - Command-Path Sanity In Bypass Mode

Important interpretation: in bypass mode, D9 commands verify control firmware responsiveness, not switched-path power transfer.

Commands to run:
1. HELP
2. SRTEST
3. D9OFF
4. D9ON
5. D9FLASH
6. INARAILS

| Command | Response Summary | Reset/Brownout Seen | Notes |
|---|---|---|---|
| HELP | Command menu returned expected set (`HELP ... INARAILS`) | No | Shell responsive; background `ina hb` lines continue. |
| SRTEST | Full deterministic pattern sweep completed (`0x0000, 0xFFFF, 0xAAAA, 0x5555, 0x00F0, 0x0F00`) | No | Interface-level SR path validated. |
| D9OFF | `d9: path forced OFF` acknowledged | No | Command accepted immediately. |
| D9ON | `d9: path forced ON` acknowledged | No | Command accepted immediately. |
| D9FLASH | `d9: flashing 8 cycles...` then `flash sequence complete` | No | Completed without reset; periodic status and hb remained stable. |
| INARAILS | `rail 5V: hi 5.080V 0.40mA | lo 5.080V 8.60mA` and `rail 3V3: hi 3.336V 0.20mA | lo 3.336V 4.60mA | in 11.536V 82.22mA` | No | 3.3V rail in expected class with R25 effective; switched shunt channels remain hardware-limited note acknowledged. |

## Phase 6 - Decision Gate (Keep Bypass vs Reintroduce Switching)

### A) Keep bypass for continued subsystem bring-up if all are true:
1. Rail stability and thermal behavior pass.
2. No unexpected node behavior at R19 and R17.
3. Command path remains stable.

### B) Reintroduce corrected switching path before further bring-up if any are true:
1. Persistent drift, droop, or thermal stress appears at bypass points.
2. Evidence of unintended current path/backfeed remains.
3. Measurements indicate bypass is masking a broader topology problem.

Decision:
- [x] Keep bypass for next bring-up block
- [ ] Reintroduce corrected switching hardware next

Rationale:
- Command-path sanity passed with no reset/brownout events.
- 5V path and VIN behavior remained stable in this run.
- 3.3V path is acceptable for continued bring-up only in the R25-effective state (`~3.335V`); RB-011 remains open for redesign closure.
- Phase 4 load/dwell checks passed with expected resistor warming and no instability.

## Evidence Capture Checklist

1. Photo of jumper installation (+5V_Reg -> R19 and +3.3V_Reg -> R17).
2. One no-load measurement photo.
3. One loaded measurement photo for each rail.
4. One thermal observation note after dwell.
5. Serial command log snippet for D9 and INARAILS sequence.

## Session Summary

| Item | Result | Notes |
|---|---|---|
| Unpowered checks | HOLD | Detailed Phase 1 resistance/continuity entries were not fully recorded in this worksheet capture. |
| First power behavior | PASS | 12.02V @ 118mA with stable 5s/60s trend; no anomalies reported. |
| Rail baseline | PASS (conditional) | 5V path PASS; 3.3V path acceptable only with R25 effective (`~3.335V`), high state (`~3.52V`) tracked under RB-011. R19/R17 drop checks captured and consistent under load. |
| Light-load stability | PASS (conditional) | 15 ohm (5V) and 10 ohm (3.3V) load tests stable; resistor warming observed; keep RB-011 condition in force. |
| Command-path sanity | PASS | HELP/SRTEST/D9OFF/D9ON/D9FLASH/INARAILS all responded; no reset/brownout observed. |
| Decision gate output | KEEP BYPASS | Continue bring-up in R25-effective state while RB-011 remains open. |

Next action:
- Continue subsystem bring-up in the R25-effective state and open schematic corrective action for RB-011.
