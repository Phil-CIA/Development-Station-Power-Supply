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
- INADIAG

Use this path to:
- Hold known-safe gate states (all OFF baseline unless a step needs ON)
- Verify AW9523 control is alive
- Capture directional INA behavior while stepping ranges/loads

### 2) Legacy ESP32 calibration reference (not the active STM32 controller)

File: src/main.cpp
File: src/power_telemetry_map.h

These commands belong to the legacy ESP32 firmware, not
`stm32-bluepill-bringup/src/main.cpp`. Do not use this command set to infer or
change the active STM32 controller's calibration or physical shunt mapping.

Legacy command set:
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

## Current-measurement qualification gate (before calibration)

**Status:** Bounded CH1/CH2 HIGH correlation captures and a direct CH1 current
capture recorded below. No calibration values changed; no protection testing
performed.
Operator confirms a 10Ω / 5W resistor load and an OWON XDM1041 DMM. The
disconnected, cold load measured 10.68Ω (operator report). The published
XDM1041 specification is ±(0.05% of reading + 5 counts) on the 5V/50V DC
voltage ranges and ±(0.15% + 10 counts) on the 500Ω resistance range. DMM
calibration status and resistor tolerance remain unknown, so these are
instrument specifications, not a traceable uncertainty statement.
Specification source: https://in.owon.com/products_owon_4_1%7C2_digits_xdm1000_series_bench-type_digital_multimeter
Operator-approved provisional capture bounds: test one channel at a time with
the 10Ω / 5W load, 800mA maximum input-supply limit, and no more than 10 seconds
at load; stop if CH1 leaves 4.75–5.25V or CH2 leaves 3.135–3.465V. Apply only
after unpowered range-to-INA mapping is identified. These bounds do not
authorize protection testing, overloads, or persistent calibration changes.

### Static path findings

- PR #63's current Rev-C regulator export is
  `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net` (export timestamp
  `2026-10-07T05:31:09`). The operator confirms these values are installed:
  R7/R37 = 20mΩ, R19/R45 = 200mΩ, and R64 = 10mΩ. The export confirms the
  first four values and routes the sense nets to INA3221s as follows:

  | Rail/range | INA in Rev-C netlist | INA channel | Range sense nets | Shunt |
  |---|---|---:|---|---|
  | CH1 +5V HIGH | U10 | 1 | `+5V_Hi_Range` / `-5V_Hi_Range` | R7, 20mΩ |
  | CH1 +5V LOW | U10 | 2 | `+5V_low_Range` / `-5V_low_Range` | R19, 200mΩ |
  | CH2 +3.3V HIGH | U11 | 1 | `+3v3V_Hi_Range` / `-3v3V_Hi_Range` | R37, 20mΩ |
  | CH2 +3.3V LOW | U11 | 2 | `+3v3V_low_Range` / `-3v3V_low_Range` | R45, 200mΩ |
  | Incoming monitor | U11 | 3 | `Incoming (+)` / `Incoming (-)` | R64, 10mΩ (operator-confirmed) |

  Firmware addresses U10/U11 as `0x40`/`0x41` and `INARAILS` labels channel 1
  HIGH and channel 2 LOW. This confirms the intended design mapping. Installed
  board revision/population is operator-reported, not independently inspected.
  The export labels R64 `10mohm`, but its component description / linked part
  metadata says 0Ω; the physical 10mΩ value is operator-confirmed, so this is
  a netlist/BOM metadata discrepancy, not a reason to substitute another value.
- The STM32 controller reads INA3221s at `0x40` and `0x41`. Its conversion is
  signed INA shunt-register value × 0.04mV, then shunt mV divided by
  `inaShuntOhmsFor()`. That helper returns 0.200Ω for every output channel and
  0.018Ω for the `0x41` channel-3 incoming monitor. Against the mapped shunts,
  the output formula is nominally correct for LOW (200mΩ) but 10× too small
  for HIGH (20mΩ); the incoming-monitor value also differs from the confirmed
  10mΩ R64. Do not use calibration to mask this range-dependent conversion
  mismatch.
- The published CH1 and CH2 telemetry both use INA channel 1 (index 0), then
  apply one rail-level current coefficient: `I_cal = I_raw * current_gain +
  current_offset_mA`. Defaults are gain 1 and offset 0, but boot can restore
  other persistent flash values. The active STM32 has `CFGSHOW`/`CALSHOW` for
  readback; `CALSET` writes the persistent config immediately. There are no
  per-range calibration coefficients in this path. `INARAILS` can show both
  INA channels, but the published frame always uses channel 1 (HIGH), even
  when the LOW hardware path is active; the CrowPanel therefore cannot provide
  a LOW-range current reading with the current firmware.
- The CrowPanel parses the transmitted integer-mA fields and displays them to
  0.001A precision. It receives the controller's calibrated/rounded frame
  values, not raw INA data, and does not show the calibration coefficients.
  If an INA read fails, the controller currently publishes fallback values
  (500mA for CH1, 320mA for CH2); do not treat those as measurements.
- The operator clarified that #63's loaded readings were incidental, not a
  controlled current-accuracy test. A later reported 10.68Ω cold resistance
  can be used only for an illustrative calculation if it was the same load:
  CH1 `5.005V / 10.68Ω = 468.6mA` and CH2
  `3.3141V / 10.68Ω = 310.3mA`. The CrowPanel values (66mA and 45mA) are
  approximately 7.10× and 6.90× lower than those illustrative estimates.
  Load identity, measurement timing, DMM accuracy/calibration status, and
  resistor temperature/tolerance are not established; this is a discrepancy
  clue only, not evidence that verifies current accuracy or a precise scaling
  error. It is directionally consistent with the now-traced HIGH-range
  formula mismatch but does not independently verify that explanation. CH2's
  recorded 1.1W conflicts with its displayed
  `3.32V × 0.045A ≈ 0.149W`; preserve the discrepancy as unresolved rather
  than selecting one value.
- **Bounded CH1 HIGH correlation capture (operator log, 2026-10-07):** With
  the 10.68Ω cold resistor on CH1, measured voltage across the load was
  4.9203V, giving an illustrative `I = V/R = 460.7mA` and `P = V²/R = 2.267W`.
  CrowPanel reported CH1 `5.02V, 0.066A, 0.33W`; CH2 was zero/off. The serial
  log records `Q2ON`, `QSTATE p0=0x02 ... Q2=1`, and `INARAILS`:
  `rail 5V: hi 5.024V 65.60mA | lo 5.024V 20.60mA`; the 3V3 output channels
  were zero, incoming monitor about 11.088V / 320mA. Controller HIGH current
  and display current agree closely (65.60 vs 66mA), while the load-based
  estimate is about 7.02× higher. The load voltage (4.9203V) is also about
  0.104V below the reported INA/panel voltage (~5.02V). The capture is bounded
  correlation evidence, not a traceable current measurement: DMM calibration,
  resistor tolerance/temperature, and the exact `CFGSHOW` coefficient values
  were not established (`cfg d9=ON 5V[   ] 3V3[   ]` was the recorded output).
  The known 20mΩ HIGH shunt plus the firmware's 200mΩ conversion predicts a
  nominal 10× under-reading, not the observed 7.02×; therefore the conversion
  mismatch is real in source, but does not alone explain the captured value.
  Output was returned OFF after the capture.
- **Bounded CH2 HIGH correlation capture (operator log, 2026-10-07):** With
  the same 10.68Ω cold resistor, voltage across the load was 3.2756V, giving
  illustrative `I = V/R = 306.7mA` and `P = V²/R = 1.005W`. CrowPanel reported
  CH2 `3.32V, 0.046A, 0.15W`; CH1 was zero/off. The serial log records
  `QSTATE p0=0x08 ... Q5=1` and `INARAILS`:
  `rail 3V3: hi 3.320V 45.60mA | lo 3.320V 13.80mA`, with the 5V channels
  zero. Controller HIGH and display current agree (45.60 vs 46mA), while the
  load-based estimate is about 6.73× higher. Load voltage is about 44mV below
  the reported INA/panel voltage. The panel's 0.15W is consistent with
  `3.32V × 0.046A ≈ 0.153W`, but far below the load-based 1.005W estimate.
  The same uncertainty limitations as CH1 apply. The 200mΩ firmware
  denominator with a 20mΩ HIGH shunt predicts about 30.7mA for this load,
  below the observed 45.6mA; the known range conversion mismatch alone does
  not fully explain the capture. No coefficients were changed.
- **CH1 HIGH direct-current capture (operator log, 2026-10-07):** Fluke 87 in
  series reported 482mA; OWON XDM1041 across the resistor reported 4.8617V.
  QSTATE was `p0=0x02 ... Q2=1`; `INARAILS` reported HIGH 65.00mA, LOW
  20.40mA; CrowPanel was clarified as 5.02V, 0.065A, 0.33W (not 0.65A).
  Direct Fluke current is about 7.42× the controller HIGH value. The load
  voltage and direct current imply about 10.09Ω at that instant, versus
  10.68Ω operator-reported cold resistance; this is consistent with a hot-load
  change but remains unverified. Load voltage is 4.8617V while INA/panel
  voltage is about 5.024/5.02V, a 0.16V discrepancy whose cause (measurement
  location, wiring/contact drop, or timing) is not established. Panel power is
  internally consistent with panel V×I (`5.02V × 0.065A ≈ 0.326W`) but not
  load-side V×direct-I (`4.8617V × 0.482A ≈ 2.34W`). The fixed 200mΩ
  denominator with a 20mΩ HIGH shunt predicts nominal 10× scaling; multiplying
  65mA by ten gives 650mA, still about 35% above the direct Fluke reading.
  Thus the range conversion mismatch is confirmed in source, but does not
  alone explain the measured chain. The Fluke exact variant/range, calibration
  status, and burden-voltage contribution were not recorded; treat this as the
  best available direct reference, not a traceable uncertainty-qualified
  measurement. This remains one spot capture, not a repeatability or
  calibration dataset. No coefficients were changed.
- `INANOW`/`INARAILS` report derived, uncalibrated estimates, not the raw INA
  register words or shunt millivolts. `INADIAG` is a read-only controller
  command that prints each INA register word, signed shunt count, converted
  shunt millivolts and bus voltage, the firmware's currently used denominator,
  derived current, and active rail calibration coefficients. `Rused` is the
  denominator currently compiled into firmware, not a verification of the
  installed physical shunt. It reads INA registers without changing INA
  configuration or persistent calibration. Read failures are reported per
  device; do not interpret failed reads as zero.
- **Bounded read-only INADIAG capture after authorized STM32 flash (operator
  session, 2026-10-07):** The STM32 helper controller was flashed on COM7 with
  the branch build that includes `INADIAG`, then queried with read-only
  commands only: `HELP`, `CFGSHOW`, `INAPROBE`, `INANOW`, `INARAILS`, and
  `INADIAG`. `CFGSHOW` reported default coefficients for both rails
  (vg=1.00000, voff_mV=0.00, ig=1.00000, ioff_mA=0.00). `INAPROBE` reported
  0x40/0x41 ACK. `INANOW` and `INARAILS` showed output channels at 0V/0mA and
  incoming monitor near 11.74V / 91.11mA. `INADIAG` reported raw register words
  and derived values for all channels, including `0x41 CH3 shunt=0x0148 (41),
  1.640mV, bus=0x2DD0 (11.728V), Rused=18mOhm, Icalc=91mA`; all output channels
  reported zero raw/current in that snapshot. No `CALSET`, `CFGSAVE`,
  `CFGRESET`, or `CFGERASE` command was issued in this session. This capture is
  command-path and firmware-state evidence only; no load-step, range-switch,
  protection, or current-accuracy measurement was performed in this step.
- **Bounded calibration apply + single-point validation (operator session,
  2026-10-07):** After explicit operator approval, coefficients were applied
  and saved on the STM32 helper using `CALSET` and `CFGSAVE`:
  `5V vg=0.96642 voff_mV=0.00 ig=7.40741 ioff_mA=0.00`,
  `3V3 vg=0.97395 voff_mV=0.00 ig=7.08889 ioff_mA=0.00`. `CFGSHOW` and
  `INADIAG` confirmed persistence. Validation was run one channel at a time
  with the other channel forced off and readback captured using
  `QSTATE`/`INARAILS`/`INADIAG`:
  CH2 (Q5 ON, CH1 OFF): controller raw `3.320V, 45.00mA` with applied 3V3
  coefficients gives `Vcal=3.2335V`, `Ical=319.0mA`; operator reference was
  `3.23V`, `319mA` (errors: `+0.0035V` / `+0.11%`, `0.0mA` / `0.00%`).
  CH1 (Q2 ON, CH2 OFF): controller raw `5.024V, 64.80mA` with applied 5V
  coefficients gives `Vcal=4.8553V`, `Ical=480.0mA`; operator reference was
  `4.86V`, `479mA` (errors: `-0.0047V` / `-0.10%`, `+1.0mA` / `+0.21%`).
  Both channels were returned OFF after captures. Scope limits: single-point
  validation per channel, Dupont/breadboard wiring, and no repeatability sweep,
  multi-point fit, or protection testing in this closeout step.
- The older `src/main.cpp` / `src/power_telemetry_map.h` `RCAL` and `SHUNT`
  commands belong to the legacy ESP32 implementation; they do not establish
  the active STM32 controller's range mapping.

### Bounded procedure

1. **Identify before probing.** Record regulator and HAT silkscreen revisions,
   installed-controller identity/firmware build, and shunt/range designators.
   Current operator-reported installed values are R7/R37 = 20mΩ,
   R19/R45 = 200mΩ, and R64 = 10mΩ; retain them as operator evidence, not
   independent measurement. Select the exact design/netlist export for those
   boards. If board identity or component identity cannot be matched, stop as
   `IDENTITY BLOCKED`.
2. **Map with power removed.** Disconnect input power and loads, confirm the
   rails are discharged, and record each installed shunt's designator,
   marking/value evidence, channel, and both terminals. Trace each shunt to
   the load path, INA inputs/channel, and HIGH/LOW selector state using the
   selected Rev-C source and unpowered continuity checks. An in-circuit
   resistance reading alone is not proof of shunt value. Stop on any
   designator, population, or mapping mismatch; do not switch ranges live to
   resolve an identity question.
3. **Capture read-only firmware state.** With output off, save the complete
   `CFGSHOW` (or `CALSHOW`), `INAPROBE`, `INANOW`, `INARAILS`, and `INADIAG`
   outputs, firmware/build identity, and CrowPanel `STATUS`/telemetry output. Do not
   issue `CALSET`, `CFGSAVE`, `CFGRESET`, `CFGERASE`, or legacy `RCAL`/
   `SHUNT` commands. Record sensor-read failures and config-recovery status;
   a fallback frame is not a valid channel reading.
4. **Set limits before energizing.** Use only the operator-approved supply
   current limit, output-current ceiling, load, test duration, and acceptable
   rail-voltage band. Test one output at a time with a stable, known load.
   Once the unpowered mapping is confirmed, capture HIGH and LOW separately
   only where the installed hardware and active firmware provide an identified
   path for each range. Do not repeat switching characterization, combine
   channels, change loads/ranges while energized, or approach an overload.
5. **Measure and compare together.** Preferred reference is a calibrated DC
   ammeter in series with the load (record its make/model, mode/range,
   calibration status, resolution, accuracy/uncertainty, and burden-voltage
   limitation). With the confirmed OWON XDM1041 and 10Ω / 5W load, measure
   voltage directly across the resistor and calculate
   `I_reference = V_load / 10.68Ω`; the cold resistance is operator-reported.
   Include DMM accuracy, unknown calibration status, resistor tolerance,
   self-heating, and contact/lead effects as uncertainty limitations. The
   resistor's nominal value alone is not a current reference. For each
   stable capture, log channel/range, load value/rating, terminal voltage,
   resistor voltage, resistance and uncertainty, calculated or directly
   measured reference current, controller `INARAILS` uncalibrated estimate,
   active calibration coefficients, telemetry-frame integer mA, CrowPanel
   displayed A, timestamp, and uncertainty/limitations. Record DMM model,
   mode/range, resolution, accuracy/calibration status, and any current-mode
   burden voltage if using direct current measurement. Do not infer current
   from the PSU input display.
6. **Stop on an anomaly.** Remove output power and stop if the physical map
   disagrees with the selected netlist/firmware, the reference range or load
   rating is exceeded, a reading is unstable, the measured voltage leaves the
   approved band, or controller/display values do not correspond to a valid
   sensor read. Mark untested ranges/channels `NOT MEASURED`.
7. **Calibration is a separate approval gate.** Only after the physical
   shunts/mapping, reference uncertainty, firmware conversion path, and
   controller-versus-display comparison are verified may calibration be
   proposed. Present the evidence and proposed coefficients first; obtain
   explicit operator approval before any persistent write.

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
