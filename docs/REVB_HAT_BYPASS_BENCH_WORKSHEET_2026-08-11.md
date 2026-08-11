# Rev-B HAT Jumper-Bypass Bench Worksheet (2026-08-11)

Purpose: validate Rev-B HAT electrical stability in temporary jumper-bypass mode after range-switch MOSFET removal, and produce a repeatable evidence record for keep-bypass vs reintroduce-switching decision.

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
| Serial monitor | 115200 8N1, STM32 command shell active | |
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
| Immediate current at power-on | |
| Current after 5 seconds | |
| Current after 60 seconds | |
| Any audible/visual anomaly | |

## Phase 3 - Rail Baseline Table

Measure with no external load first.

| Rail / Node | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| VIN | Near PSU setpoint | | | |
| +5V_Boot | About 5.0 V | | | |
| +5V_Reg | About 5.0 V class | | | |
| +3.3V_Reg | About 3.3 V class | | | |
| R19 destination node | Tracks +5V_Reg | | | |
| R17 destination node | Tracks +3.3V_Reg | | | |

## Phase 4 - Light-Load Stability

Apply light resistive loads one rail at a time and record drift.

Recommended starter loads:
1. 5 V rail: 100 ohm.
2. 3.3 V rail: 100 ohm.

| Test | Start V | Loaded V | Delta V | Input Current | Thermal Notes | Pass/Fail |
|---|---|---|---|---|---|---|
| 5V light load | | | | | | |
| 3.3V light load | | | | | | |

Dwell checkpoint (2 to 5 minutes):

| Check | Result | Notes |
|---|---|---|
| No thermal creep at jumper wires | | |
| No drift runaway on R19 node | | |
| No drift runaway on R17 node | | |
| Rails remain stable | | |

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
| HELP | | | |
| SRTEST | | | |
| D9OFF | | | |
| D9ON | | | |
| D9FLASH | | | |
| INARAILS | | | |

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
- [ ] Keep bypass for next bring-up block
- [ ] Reintroduce corrected switching hardware next

Rationale:
- 

## Evidence Capture Checklist

1. Photo of jumper installation (+5V_Reg -> R19 and +3.3V_Reg -> R17).
2. One no-load measurement photo.
3. One loaded measurement photo for each rail.
4. One thermal observation note after dwell.
5. Serial command log snippet for D9 and INARAILS sequence.

## Session Summary

| Item | Result | Notes |
|---|---|---|
| Unpowered checks | [PASS/FAIL/HOLD] | |
| First power behavior | [PASS/FAIL/HOLD] | |
| Rail baseline | [PASS/FAIL/HOLD] | |
| Light-load stability | [PASS/FAIL/HOLD] | |
| Command-path sanity | [PASS/FAIL/HOLD] | |
| Decision gate output | [KEEP BYPASS / REINTRODUCE SWITCHING] | |

Next action:
- 
