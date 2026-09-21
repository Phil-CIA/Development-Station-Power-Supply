# Regulator Board Rev-C First Power Runsheet (2026-09-21)

Purpose: continue the Rev-C regulator board bring-up from the bypass state already applied on the bench, using the Rev-B runsheet format and the Rev-C design-state facts from `docs/REGULATOR_BOARD_CHANGE_TRACKER.md` as the gate criteria.

This is a continuation runsheet, not a cold start: pre-power checks and initial power-on already happened this session. See "Starting Bench State" below before running any new step.

## Success Criteria

1. No smoke, no audible distress, no rapid current-limit foldback.
2. 5V rail present and stable at no load.
3. 3.3V rail present and stable at no load.
4. RB-011 check: `+3.3V_Reg` lands near 3.3V class (not the ~3.52V overvoltage state) with R25 populated.
5. RB-010 check: `5V_reg` follows `VSENSE_5V+` in normal remote-sense operation, and falls back deterministically (~5.04V class) when `VSENSE_5V+` is opened, via the 100kΩ pulldown.
6. Board remains thermally calm during a short dwell.
7. Optional: light-load checks pass before any stacked HAT testing.

## Stop Rules

1. Do not trust R11/R35 function/value from the repo schematic until Phase 0 file reconciliation is complete — repo schematic and net currently disagree on R11's value/footprint.
2. If there is a hard short or near-short from 12V to GND, 5V to GND, or 3.3V to GND, stop before power.
3. If input current rises unexpectedly fast or the PSU immediately folds back, remove power and stop.
4. If 5V or 3.3V exceeds expected range, or rails collapse/oscillate, remove power and stop.
5. If `VSENSE_5V+` open-line fallback is not deterministic (rail follows a floating/false-high condition instead of falling back), stop and treat RB-010 as unresolved before continuing.
6. Do not attach the HAT until regulator-only no-load and light-load checks pass.

## Reference Files

1. `docs/REGULATOR_BOARD_CHANGE_TRACKER.md` — RB-010 (VSENSE_5V+ selector), RB-011 (R25 dependency), RB-012 (HAT OCP, not this board)
2. `STACKED_BOARD_FIRST_POWER_BASELINE.md`
3. `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch`
4. `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net`
5. `C:\Users\user\OneDrive\JLCPCB files\Development station supply\Regulator board\Rev C\` — as-manufactured source of truth, use to resolve R11/R35 discrepancy

## Bench Setup

| Item | Required state | Actual |
|---|---|---|
| Bench PSU | 12V preset | 12V applied |
| Current limit | Conservative start, 0.10A to 0.20A for first touch | 200mA (OCL) |
| DMM | Ready for resistance + DC voltage | |
| Scope | Ready on 5V rail if available | |
| Electronic load | Disconnected for initial power | Disconnected |
| HAT board | Not connected for first power | Not connected |

## Starting Bench State (carried in from this session, before any new step below)

| Item | Value |
|---|---|
| Input | 12V |
| PSU current limit (OCL) | 200mA |
| Bypass state | R11 and R35 removed |
| Input current draw | 25mA |
| Visual indicators | 3 LEDs lit |
| Anomalies observed | None (no fault/foldback) |

R11 and R35 are confirmed 0Ω link resistors (1206) in both the repo schematic and the OneDrive as-manufactured schematic. Additional cross-check now confirms D4 is a Green 0805 LED in the active schematic files. The repo netlist component/designator mapping is stale for multiple references (including at least R11/R35 and D4), so do not use the netlist component table for value/designator decisions until regenerated from the current schematic.

## Phase 0 - Reconcile As-Manufactured Files (do before trusting any schematic reference below)

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| Repo Rev-C KiCad files compared against OneDrive Rev-C as-built files | Match, or differences understood | Schematic agrees on R11/R35 as 0Ω/1206; discrepancy isolated to stale repo netlist component table | PASS |
| R11 value/footprint discrepancy (repo schematic 0Ω/1206 vs repo net 1kΩ/0805) resolved against OneDrive schematic | Resolved | Resolved in favor of schematic/OneDrive: R11 is 0Ω, 1206 (same pattern for R35) | PASS |
| R11/R35 net function identified (what removing them opens) | Identified | R11 bridges GND to Net-(D3-Pad1); R35 bridges +5V_reg to Net-(R35-Pad2) | PASS |
| Gerbers/BOM CSV synced from OneDrive into repo Rev-C folder if newer | Synced or explicitly deferred | Deferred: repo KiCad files are newer than OneDrive set; OneDrive BOM/gerbers retained as manufacturing baseline reference | PASS |

Gate to continue:
1. R11/R35 function is known before deciding whether to reinstall them or continue bring-up without them.

Phase 0 gate status: PASS.

## Phase 1 - Unpowered Checks

### 1.1 Visual inspection

1. Confirm regulator IC, diode, inductor, electrolytics, and connector polarity all look correctly oriented.
2. Confirm no obvious solder bridges, tombstones, cracked joints, or loose bodge wires.
3. Confirm R25 (10Ω) is populated (RB-011 requirement).
4. Confirm the 100kΩ `VSENSE_5V+` pulldown is populated (RB-010 mitigation).

### 1.2 Meter checks

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| 12V input to GND resistance | Not a hard short | Not a hard short (session ohm checks) | PASS |
| 5V rail to GND resistance | Not a hard short | Not a hard short (session ohm checks) | PASS |
| 3.3V rail to GND resistance | Not a hard short | Not a hard short (session ohm checks) | PASS |
| Input polarity marking clear | Yes | Initial connection was reversed, then corrected before current readings | HOLD |
| R25 populated | Yes | Installed | PASS |
| 100kΩ VSENSE_5V+ pulldown populated | Yes | Installed (reported as R10 installed) | PASS |

Gate to continue:
1. No hard-short reading. (Already satisfied this session.)
2. R25 and the VSENSE_5V+ pulldown confirmed populated.

## Phase 2 - First Controlled Power-On

### 2.1 Initial energization

Already performed this session (see Starting Bench State). Record any continuation below if current limit or bypass state changes.

| Item | Actual |
|---|---|
| PSU voltage setting | 12V |
| PSU current limit | 200mA |
| Immediate current at power-on | 25mA |
| Current after 5 seconds | 25mA (stable, per session report) |
| Any audible/thermal/visual anomaly | None; 3 LEDs lit |

Continuation check (same session):
1. R11 and R35 were reinstalled and board repowered.
2. Input current again settled at approximately 25mA (same class as prior state).
3. Voltage measurements remained in the same class as previous reading set (rails still out of expected regulation range).
4. U2 pin 5 and U4 pin 5 measured at +5V_Boot level, indicating both channels observed in OFF state.

Forced-enable diagnostic (same session, pin 5 manually grounded):
1. Pin 5 was jumpered to GND for both regulators; both cores began operating.
2. Input current rose by about 10mA briefly, then settled about 3mA to 4mA above the 25mA baseline.
3. Bench supply during capture: 12.05V, 28mA, 0.337W.
4. 5V regulator (U2) node measurements with pin 5 grounded:
	- Pin 1 = 11.09V
	- Pin 2 = 5.0736V
	- Pin 3 = 0V
	- Pin 4 = 1.2544V
	- Measured V_out+5 = 4.011V
5. 3.3V regulator (U4) node measurements:
	- Pin 1 = 11.89V
	- Pin 2 = 3.428V
	- Pin 3 = 0V
	- Pin 4 = 1.2347V
	- Measured V_out+3.3 = 0.063V

Interpretation from this diagnostic:
1. Buck regulator cores are active and regulating near target at their own output/feedback pins when enabled.
2. The observed rail failure is downstream of the regulator cores (distribution/switch/select path), not a primary buck start-up failure.

### 2.2 No-load rail measurements

| Rail | Expected target | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| 12V input | About bench setting | 12.032V | PASS | |
| 5V rail | About 5.0V | 4.0100V | FAIL | Undervoltage relative to expected 5V class |
| 3.3V rail | About 3.3V | 0.0633V | FAIL | Rail effectively down |

### 2.3 RB-011 check (R25 / TLV9352 selector path)

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| +3.3V_Reg with R25 populated | ~3.335V class | 0V | FAIL |

### 2.4 RB-010 check (VSENSE_5V+ selector fallback)

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| 5V_reg with VSENSE_5V+ connected normally | Follows remote sense | 4.08V | FAIL |
| 5V_reg with VSENSE_5V+ opened/disconnected | Deterministic fallback, ~5.04V class | Not tested (deferred this session) | HOLD |

Gate to continue:
1. 5V and 3.3V are present and stable.
2. No current-limit event beyond the 200mA start point.
3. No abnormal heating during short dwell.
4. RB-011 and RB-010 checks pass.

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

Only run this phase if Phases 0 to 3 pass cleanly.

### 4.1 5V light-load check

| Test | Load | Rail voltage | Input current | Pass/Fail | Notes |
|---|---|---|---|---|---|
| 5V light load | 100 ohm nominal | | | | |

### 4.2 3.3V light-load check

| Test | Load | Rail voltage | Input current | Pass/Fail | Notes |
|---|---|---|---|---|---|
| 3.3V light load | 100 ohm nominal | | | | |

## Phase 5 - Optional Stacked HAT Follow-Up

Only proceed if regulator-only phases pass. See the companion HAT Rev-C runsheet for the standalone HAT phase before this step.

1. Keep USB Vbus isolated or use a data-only cable as documented in RB-002.
2. Attach the HAT.
3. Re-run controlled power-on.
4. Confirm basic telemetry discovery only after raw rails remain healthy.

## Results Summary

| Item | Result | Notes |
|---|---|---|
| Phase 0 file reconciliation | [PASS/FAIL/HOLD] | |
| Regulator-only first power | [PASS/FAIL/HOLD] | Bypass state applied: R11/R35 removed, 200mA OCL, 25mA draw, 3 LEDs lit |
| No-load rail check | [PASS/FAIL/HOLD] | |
| RB-011 check | [PASS/FAIL/HOLD] | |
| RB-010 check | [PASS/FAIL/HOLD] | |
| Light-load rail checks | [PASS/FAIL/HOLD] | |
| Ready for stacked HAT test | [YES/NO] | |

Current observation notes (2026-09-21 bench report):
1. Input polarity was initially reversed, then corrected.
2. With R11/R35 removed: input current is about 25mA, with about 20mA attributed to LEDs (D16 about 10mA, D19 about 5mA, D4 about 5mA).
3. 5V channel appears to be backfed somewhere in the present bypass state.

Updated interim status from current bench data:
1. Phase 0 file reconciliation: PASS.
2. Regulator-only first power: HOLD (board powers without foldback, but rails are out of spec at board output nodes).
3. No-load rail check: FAIL.
4. RB-011 check: FAIL.
5. RB-010 check: FAIL/HOLD (normal-mode reading FAIL; open-sense fallback HOLD not yet tested).
6. Ready for stacked HAT test: NO.

## Next Action

1. If PASS: move to the HAT Rev-C standalone runsheet, then stacked bring-up.
2. If HOLD: capture the exact failing observation and pivot to schematic/net tracing, prioritizing the Phase 0 R11/R35 reconciliation if that is the open blocker.

Session pivot note (2026-09-21):
1. Operator requested immediate switch to standalone HAT bring-up to use MCU/telemetry as diagnostic aid.
2. Regulator runsheet remains HOLD; stacked testing remains blocked.
3. Active companion procedure: docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md.
