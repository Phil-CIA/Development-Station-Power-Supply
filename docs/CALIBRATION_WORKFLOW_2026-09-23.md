# Development Station Calibration Workflow (Recovered + Updated)

Date: 2026-09-23
Status: Active working procedure for next calibration session

## Purpose

Recover and update the existing calibration approach already present in firmware and handoff notes, without changing the current CrowPanel UART0 shared transport setup.

## Key Decision

Keep CrowPanel on the current UART0 shared mode for now.

- CrowPanel is confirmed booting and running on COM12.
- In UART0 shared mode, CrowPanel console command interaction is intentionally limited by firmware design.
- Calibration command/control should therefore run from the Blue Pill helper console and/or the main controller console where calibration commands already exist.

## What already exists in repo

### 1) Blue Pill helper path (bring-up and rail-state verification)

File: stm32-bluepill-bringup/src/main.cpp

Useful commands:
- HELP
- AWPROBE
- AWMODE
- Q1ON/Q1OFF, Q2ON/Q2OFF, Q3ON/Q3OFF, Q4ON/Q4OFF, Q5ON/Q5OFF, Q9ON/Q9OFF
- QSTATE
- INAPROBE
- INANOW
- INARAILS

Use this path to:
- Hold known-safe gate states (all OFF baseline unless a step needs ON)
- Verify AW9523 control is alive
- Capture directional INA behavior while stepping ranges/loads

### 2) Main controller calibration path (actual coefficient + shunt config)

File: src/main.cpp
File: src/power_telemetry_map.h

Recovered calibration command set:
- RCALSHOW
- RCAL <rail> <range> <vgain> <voff> <igain> <ioff>
- SHUNTSHOW
- SHUNT <rail> <range> <ohms>
- AUTORANGE <ON|OFF>
- RTHR <low_mA> <high_mA>
- CFGSHOW
- CFGSAVE
- CFGLOAD
- CFGRESET
- CALRAMP / CALRAMP0 / CALRAMP1
- RAILSNAP

Supported rail/range tokens from command handling:
- rails: 5V, 3V3, ADJ, IN12
- ranges: HIGH, LOW, SINGLE

Calibration is applied as:
- V_cal = V_raw * vgain + voff
- I_cal = I_raw * igain + ioff

## Safe preflight before any calibration action

1. Verify Blue Pill path is alive:
- AWPROBE
- QSTATE
- INARAILS

2. Ensure range MOSFET baseline is safe:
- Set all controlled paths OFF
- Confirm with QSTATE

3. Confirm bench instrumentation:
- DMM connected to rail under test
- Known load attached (or electronic load with controlled current)
- External PSU current limit set conservatively

4. Keep a capture log open:
- Timestamp
- Rail
- Range
- DMM voltage/current
- INA raw reading
- Command used
- Result/pass-fail

## Practical calibration flow (recommended)

### Phase A: Baseline capture

1. On Blue Pill console:
- AWPROBE
- QSTATE
- INARAILS

2. Record baseline readings at near-no-load and one light-load point for each rail.

### Phase B: Shunt sanity check

1. On main controller console:
- SHUNTSHOW

2. If known hardware shunt values differ from config, correct first:
- SHUNT <rail> <range> <ohms>
- Repeat SHUNTSHOW
- CFGSAVE

### Phase C: Two-point coefficient extraction per rail/range

Use two load points per rail/range for both voltage and current where practical.

Given points (raw1, ref1) and (raw2, ref2):

$$
\text{gain} = \frac{ref2 - ref1}{raw2 - raw1}
$$

$$
\text{offset} = ref1 - (\text{gain} \cdot raw1)
$$

Then apply:
- RCAL <rail> <range> <vgain> <voff> <igain> <ioff>
- RCALSHOW
- CFGSAVE

### Phase D: Validation pass

1. Re-measure at 2-3 additional points not used for fitting.
2. Compare calibrated telemetry vs DMM/load reference.
3. If errors are outside acceptance, re-fit using cleaner points and re-apply RCAL.
4. Freeze config with CFGSAVE.

## Suggested acceptance gates

Per rail/range, after calibration:
- Voltage error within target band you set for this session (example: <= 1 to 2 percent)
- Current error within target band for intended operating region
- No unstable jumps in readings across repeated captures
- QSTATE and INARAILS still consistent with commanded path state

## Session output template (copy into handoff)

- Rail:
- Range:
- Shunt configured:
- Fit points used (raw/ref):
- Applied RCAL:
- Validation points:
- Worst-case voltage error:
- Worst-case current error:
- Pass/Fail:
- Notes:

## Important current-session note

CrowPanel on UART0 shared mode is considered operational for display bring-up. For calibration progress right now, use Blue Pill + main-controller console workflows above and avoid transport changes until calibration docs and results are captured.
