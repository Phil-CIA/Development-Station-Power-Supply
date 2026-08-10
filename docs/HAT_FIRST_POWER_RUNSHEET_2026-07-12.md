# HAT Board Standalone First Power Runsheet (2026-07-12)

Purpose: execute the first bench power-up of the HAT board safely in isolation, validate boot and telemetry basics, and produce a clear pass/fail gate before any stacked regulator testing.

## Success Criteria

1. No smoke, no audible distress, no rapid current-limit foldback.
2. +5V_Boot and +3.3V rails present and stable at no load.
3. Board remains thermally calm during a short dwell.
4. Serial boot output is visible and stable (no reset loop/brownout spam).
5. I2C discovery path shows expected baseline behavior for current hardware state.

## Stop Rules

1. If there is a hard short or near-short from VIN to GND, +5V_Boot to GND, or +3.3V to GND, stop before power.
2. If input current rises unexpectedly fast or PSU immediately folds back, remove power and stop.
3. If +5V_Boot or +3.3V collapses/oscillates, remove power and stop.
4. If boot is unstable (continuous resets/brownouts), stop and hold before stacked tests.
5. Do not stack onto the regulator board during this runsheet.

## Reference Files

1. `docs/DEV_STATION_HANDOFF_2026-07-12.md`
2. `HANDOFF.md`
3. `docs/REGULATOR_FIRST_POWER_RUNSHEET_2026-07-11.md`
4. `platformio.ini`
5. `scripts/guarded-flash.ps1`
6. `src/i2c_scanner.cpp`
7. `src/disp_link.cpp`

## Bench Setup

| Item | Required state | Actual |
|---|---|---|
| Bench PSU | 12V preset, output OFF initially | |
| Current limit | Conservative start, e.g. 0.10A to 0.20A for first touch | |
| DMM | Ready for resistance + DC voltage | |
| Serial monitor | Ready at 115200 8N1 | |
| Scope | Optional, on +3.3V if available | |
| Regulator board | Not connected (HAT only) | |

Bring-up caution:
1. Confirm the exact Blue Pill variant matches the socket and pinout expectation before insertion.
2. A wrong Blue Pill variant can present as startup current-limit clamp and low input voltage.

## Phase 1 - Unpowered Checks

### 1.1 Visual inspection

1. Confirm no obvious solder bridges, tombstones, cracked joints, or damaged components.
2. Confirm connector polarity/orientation and bench lead polarity are clear.
3. Confirm temporary jumper/workaround state required by this build is physically present and unchanged.

### 1.2 Meter checks

Record approximate readings before power.

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| VIN to GND resistance | Not a hard short | | |
| +5V_Boot to GND resistance | Not a hard short | | |
| +3.3V to GND resistance | Not a hard short | | |
| Input polarity marking clear | Yes | | |
| Temporary jumper/workaround confirmed | Yes | | |

Gate to continue:
1. No hard-short reading.
2. Temporary jumper/workaround state confirmed.

## Phase 2 - First Controlled Power-On (HAT only)

### 2.1 Initial energization

1. Set PSU to 12V.
2. Set current limit to conservative start value.
3. Connect only the HAT board.
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

## Phase 3 - No-Load Rail Measurements

Measure directly on the HAT board.

| Rail | Expected target | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| VIN | About bench setting | | | |
| +5V_Boot | About 5.0V | | | |
| +3.3V | About 3.3V | | | |

Gate to continue:
1. +5V_Boot and +3.3V are present and stable.
2. No current-limit event.
3. No abnormal heating during short dwell.

## Phase 4 - Short Dwell

1. Leave board powered for 1 to 2 minutes at no load.
2. Recheck +5V_Boot and +3.3V near end of dwell.
3. If using scope, capture +3.3V startup behavior and note overshoot/collapse.

| Check | Result | Notes |
|---|---|---|
| 1 to 2 minute dwell stable | | |
| +5V_Boot drift acceptable | | |
| +3.3V drift acceptable | | |
| Startup waveform captured (optional) | | |

## Phase 5 - Serial Boot Validation

1. Connect serial monitor to expected HAT USB-UART path.
2. Verify expected target identity before any upload action.
3. Power-cycle HAT and capture boot log at 115200 8N1.
4. Confirm runtime remains stable (no looped resets).

| Check | Result | Notes |
|---|---|---|
| Boot log appears after power cycle | | |
| No brownout/reset loop | | |
| Console remains responsive | | |

Fallback only if serial boot is missing/unstable:
1. Run guarded build for HAT target.
2. Run guarded upload for HAT target.
3. Re-run Phase 5.

## Phase 6 - I2C/Telemetry Baseline

1. Observe scanner/telemetry output from active firmware path.
2. Record discovered device addresses and any bus errors.
3. Treat known near-zero shunt current behavior as expected for current temporary hardware bypass.

| Check | Result | Notes |
|---|---|---|
| Expected I2C devices discovered | | |
| No persistent bus error spam | | |
| Telemetry output updates | | |

## Results Summary

| Item | Result | Notes |
|---|---|---|
| HAT standalone first power | [PASS/FAIL/HOLD] | |
| No-load rail check | [PASS/FAIL/HOLD] | |
| Serial boot validation | [PASS/FAIL/HOLD] | |
| I2C/telemetry baseline | [PASS/FAIL/HOLD] | |
| Ready for stacked-entry planning | [YES/NO] | |

### Field Note - 2026-07-13

1. Incorrect Blue Pill variant insertion attempt caused startup clamp behavior.
2. Correct Blue Pill variant installed: input current about 70mA at 12V with 6 LEDs on.
3. State acceptable for proceeding to serial boot validation.

## Next Action

1. If PASS: create a separate stacked-entry runsheet and execute with USB power routing made explicit.
2. If HOLD/FAIL: log exact failing observation and isolate domain (power, rails, boot, serial, I2C) before any stacking.
