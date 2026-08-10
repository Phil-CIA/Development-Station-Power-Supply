# Regulator Board First Power Runsheet (2026-07-11)

Purpose: execute the first bench power-up of the regulator board safely, using the known repo hazards and previous baseline as the gate criteria.

## Success Criteria

1. No smoke, no audible distress, no rapid current-limit foldback.
2. 5V rail present and stable at no load.
3. 3.3V rail present and stable at no load.
4. Board remains thermally calm during a short dwell.
5. Optional: light-load checks pass before any stacked HAT testing.

## Stop Rules

1. If the RB-001 feedback workaround cannot be physically confirmed, stop before applying 12V.
2. If there is a hard short or near-short from 12V to GND or from 5V to GND, stop before power.
3. If input current rises unexpectedly fast or the PSU immediately folds back, remove power and stop.
4. If 5V exceeds expected range or rails collapse/oscillate, remove power and stop.
5. Do not attach the HAT until regulator-only no-load checks pass.

## Reference Files

1. `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`
2. `STACKED_BOARD_FIRST_POWER_BASELINE.md`
3. `hardware/kicad/dsp-regulator-rev-b/DSP-Regulator-RevB.kicad_sch`
4. `hardware/kicad/dsp-regulator-rev-b/DSP-Regulator-RevB.net`

## Bench Setup

| Item | Required state | Actual |
|---|---|---|
| Bench PSU | 12V preset, output OFF initially | |
| Current limit | Conservative start, e.g. 0.10A to 0.20A for first touch | |
| DMM | Ready for resistance + DC voltage | |
| Scope | Ready on 5V rail if available | |
| Electronic load | Disconnected for initial power | |
| HAT board | Not connected for first power | |

## Phase 1 - Unpowered Checks

### 1.1 Visual inspection

1. Confirm regulator IC, diode, inductor, electrolytics, and connector polarity all look correctly oriented.
2. Confirm no obvious solder bridges, tombstones, cracked joints, or loose bodge wires.
3. Confirm the RB-001 feedback workaround is physically present if this build requires it.

### 1.2 Meter checks

Record the approximate readings before power.

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| 12V input to GND resistance | Not a hard short | | |
| 5V rail to GND resistance | Not a hard short | | |
| 3.3V rail to GND resistance | Not a hard short | | |
| Input polarity marking clear | Yes | | |
| Feedback workaround confirmed | Yes | | |

Gate to continue:
1. No hard-short reading.
2. Feedback workaround confirmed.

## Phase 2 - First Controlled Power-On

### 2.1 Initial energization

1. Set PSU to 12V.
2. Set current limit to the conservative start value chosen above.
3. Connect only the regulator board.
4. Power on while watching PSU current immediately.
5. If current behavior is calm, hold for 3 to 5 seconds.

Record:

| Item | Actual |
|---|---|
| PSU voltage setting | |
| PSU current limit | |
| Immediate current at power-on | |
| Current after 5 seconds | |
| Any audible/thermal/visual anomaly | |

### 2.2 No-load rail measurements

Measure directly on the board.

| Rail | Expected target | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| 12V input | About bench setting | | | |
| 5V rail | About 5.0V | | | |
| 3.3V rail | About 3.3V | | | |
| Adjustable rail | Informational only for this first pass | | | |

Gate to continue:
1. 5V and 3.3V are present and stable.
2. No current-limit event.
3. No abnormal heating during short dwell.

## Phase 3 - Short Dwell and Startup Capture

1. Leave the board powered for 1 to 2 minutes at no load.
2. Recheck 5V and 3.3V near the end of the dwell.
3. If using the scope, capture startup on 5V and note any overshoot or collapse.

| Check | Result | Notes |
|---|---|---|
| 1 to 2 minute dwell stable | | |
| 5V drift acceptable | | |
| 3.3V drift acceptable | | |
| Startup waveform captured | | |

## Phase 4 - Light-Load Checks

Only run this phase if Phases 1 to 3 pass cleanly.

### 4.1 5V light-load check

1. Apply a light resistive load first.
2. Measure rail voltage and input current.

| Test | Load | Rail voltage | Input current | Pass/Fail | Notes |
|---|---|---|---|---|---|
| 5V light load | 100 ohm nominal | | | | |

### 4.2 3.3V light-load check

1. Remove the 5V load.
2. Apply a light resistive load on 3.3V.
3. Measure rail voltage and input current.

| Test | Load | Rail voltage | Input current | Pass/Fail | Notes |
|---|---|---|---|---|---|
| 3.3V light load | 100 ohm nominal | | | | |

## Phase 5 - Optional Stacked HAT Follow-Up

Only proceed if regulator-only phases pass.

1. Keep USB Vbus isolated or use a data-only cable as documented in RB-002.
2. Attach the HAT.
3. Re-run controlled power-on.
4. Confirm basic telemetry discovery only after raw rails remain healthy.

## Results Summary

| Item | Result | Notes |
|---|---|---|
| Regulator-only first power | [PASS/FAIL/HOLD] | |
| No-load rail check | [PASS/FAIL/HOLD] | |
| Light-load rail checks | [PASS/FAIL/HOLD] | |
| Ready for stacked HAT test | [YES/NO] | |

### 2026-07-11 First-Power Pass

| Item | Result | Notes |
|---|---|---|
| Regulator-only first power | PASS | +5V_Boot = 5.02V, 5V channel = 5.04V, +3.3V = 3.314V |
| No-load rail check | PASS | Vin = 11.98V, input current = 27mA, about 323mW |
| Light-load rail checks | HOLD | Not run yet in this session |
| Ready for stacked HAT test | NO | Hold until light-load confirmation is complete |

## Next Action

1. If PASS: move to stacked HAT validation with USB isolation.
2. If HOLD: capture exact failing observation and pivot to schematic/net tracing around RB-001 or the affected rail.