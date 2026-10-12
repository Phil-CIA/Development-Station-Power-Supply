# Rev C Board Bring-Up Plan

**Status:** Release documentation baseline — fabrication gate blocked  
**Scope:** `DSP-Regulator-RevC` and `DSP-Regulator-HAT-RevC`  
**Validation snapshot:** 2026-09-21, KiCad 10.0.5

## Current status

| Area | Status | Evidence or gate |
|---|---|---|
| Regulator Rev C selector design | Release blocked | Fresh ERC: 326 violations; fresh DRC: 211 violations and 470 unconnected items |
| HAT Rev C OCP schematic | Release blocked | Fresh ERC: 82 violations; prior 2026-08-14 clean report is superseded by the current schematic |
| HAT Rev C OCP PCB | Release blocked | Fresh DRC: 161 violations and 129 unconnected items |
| Rev C fabrication outputs | Not released | Do not order until the board-level release checklist is complete |
| Rev B bench baseline | Available with restrictions | Range-switch bypass remains installed; external PSU current limit is the active protection |
| Rev C bench results | None recorded | This plan begins with inspection and controlled first power |

The Rev C HAT OCP is not yet credited as calibrated protection. INA2180 U2/U6 must remain on `+3.3V Boot`; TLV1702 U3/U7 may remain on `+12V`.

### 2026-09-21 validation record

Fresh reports were generated from the current checked-out Rev C files with KiCad 10.0.5:

| Board | ERC | DRC | Primary report categories |
|---|---:|---:|---|
| Regulator Rev C | 326 violations | 211 violations; 470 unconnected items | DRC includes 153 footprint-library issues, 36 clearance, 10 silk-overlap, 7 silk-over-copper, 3 silk-edge-clearance, and 2 copper-edge-clearance findings |
| HAT Rev C | 82 violations | 161 violations; 129 unconnected items | DRC includes 94 footprint-library issues, 48 dangling vias, 18 hole-clearance, and 1 footprint mismatch finding |

The KiCad commands returned exit code 0 because `--exit-code-violations` was not supplied; the reports still contain violations. These results are evidence that the release gate remains blocked, not a fabrication approval.

Netlists were re-exported from both current Rev C schematics on 2026-09-21. They are current artifacts, but netlist export success does not close ERC, DRC, or PCB connectivity gates.

## Bring-up rules

1. Use the external bench-supply current limit for every first-power and fault test.
2. Never attach the HAT to a regulator board until regulator-only checks pass.
3. Start each board with unpowered visual, continuity, polarity, and resistance checks.
4. Record measured values, instrument settings, board population, jumper state, and firmware revision for every run.
5. Stop on unexpected current limiting, rail collapse, excessive heating, oscillation, smoke, or repeated resets.
6. Keep Rev B range-switch bypasses in place until the MOSFET pinout and `VGS` audit passes.
7. Do not rely on the Rev C OCP as the only safety barrier until its thresholds and fault behavior are bench-validated.

## Phase 0 — Release readiness before boards are ordered

Complete this phase against the Rev C KiCad projects.

### Regulator Rev C

- [ ] Confirm the implemented 100kΩ `VSENSE_5V+` pulldown and TLV9352 selector in the final netlist.
- [ ] Correct stale TLV9352 symbol/BOM metadata and verify all selector pin mappings.
- [ ] Re-run the 5V selector sweep and fault-case SPICE decks.
- [ ] Confirm the 3.3V selector reference path and mark service bypasses DNP.
- [ ] Run fresh ERC with violations treated as a failure, netlist export, PCB DRC with violations treated as a failure, and connectivity review.
- [ ] Confirm connector polarity, rail labels, test points, and fabrication plot settings.

### HAT Rev C

Complete every RB-012 item in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`:

- [ ] Verify `IN+`/`IN-` polarity on all four shunts.
- [ ] Route true Kelvin pairs independently of load current.
- [ ] Place and verify 100nF VS-to-GND bypass capacitors at U2 and U6.
- [ ] Calculate amplifier output and saturation margin for 20mΩ and 200mΩ channels.
- [ ] Lock trip, reset, tolerance, and hysteresis values for all four ranges.
- [ ] Decide whether potentiometer thresholds are acceptable for the first build.
- [ ] Verify INA2180 and TLV1702 footprints and pin mappings.
- [ ] Add amplifier-output, threshold, comparator-output, and fault test points.
- [ ] Run fresh ERC with violations treated as a failure, netlist export, PCB DRC with violations treated as a failure, and final connectivity audit.

**Gate:** No fabrication release until all required checks pass or an explicit exception is recorded.

## Phase 1 — Incoming-board inspection

For each assembled board, record the exact PCB revision, serial/date code, fitted parts, and any rework.

- Inspect solder bridges, tombstones, polarity, connector orientation, missing parts, and damaged pads.
- Verify the installed regulator, op-amp, current-sense amplifier, comparator, MOSFET, and MCU part numbers.
- Photograph both sides before power.
- Check for shorts from VIN, +5V, +3.3V, and each local rail to GND.
- Verify board-to-board connector orientation and pin-1 alignment.
- Confirm no USB cable or external source can backfeed the powered rail.

**Gate:** No unexplained hard short, polarity concern, or population mismatch.

## Phase 2 — Regulator Rev C standalone power

Use `docs/REGULATOR_FIRST_POWER_RUNSHEET_2026-07-11.md` as the measurement form, adapted for the Rev C population.

1. Set the bench supply to 12V with a conservative current limit.
2. Power only the regulator board.
3. Watch input current during the first seconds and hold briefly if stable.
4. Measure VIN, +5V, +3.3V, selector nodes, and feedback reference points at no load.
5. Dwell for 1–2 minutes and capture rail drift and temperature.
6. Apply only a light load, then repeat rail and current measurements.
7. Validate normal remote-sense operation.
8. Open the remote-sense input under current limit and verify the fallback remains stable and in range.

**Gate:** Stable rails, no abnormal heating, deterministic remote-sense behavior, and no unexpected current-limit event.

## Phase 3 — HAT Rev C standalone power

Use `docs/HAT_FIRST_POWER_RUNSHEET_2026-07-12.md` as the baseline, adding OCP-specific checks.

1. Power only the HAT with the external current limit enabled.
2. Measure VIN, +5V_Boot, +3.3V, and local analog supply rails.
3. Confirm MCU boot, serial output, I2C telemetry, and absence of reset/brownout loops.
4. Verify all four INA2180 outputs at zero load.
5. Confirm comparator outputs and the fault bus idle state.
6. Verify the range-switch bypass state before any load is connected.

**Gate:** Clean standalone power and communications with stable analog outputs and a known fault-bus idle state.

## Phase 4 — HAT range-switch characterization

Keep the proven bypass installed while characterizing the switching network.

- With power removed, reconcile Q1/Q2/Q7/Q8 symbol pins, footprint pads, body diodes, and fitted part numbers.
- With power applied through the current limit, run `D9OFF`, `D9ON`, and `D9FLASH`.
- Record gate, source, drain, and `VGS` for every device in every commanded state.
- Require a clearly enhanced ON state and a clearly non-conducting OFF state.
- Stop if any intermediate node hangs near the previously observed 1.96–2.1V region.

**Gate:** Do not remove the bypass or reconnect the load through the MOSFET path until the truth table and `VGS` behavior are understood.

## Phase 5 — Stacked regulator + HAT validation

Proceed only after Phases 2–4 pass.

1. Use the documented USB-isolation/data-only arrangement.
2. Connect the boards with the range bypass state still controlled.
3. Power at a conservative current limit and verify VIN, +5V_Boot, +3.3V, +5V_Reg, and +3.3V_Reg.
4. Check telemetry and command response before applying meaningful load.
5. Apply light loads one rail at a time.
6. Verify remote-sense behavior, selector transitions, and no USB VBUS backfeed.
7. Capture startup, load-step, and dwell behavior with the scope where available.

**Gate:** Stable stacked operation without current-limit events, backfeed, rail interaction, or reset loops.

## Phase 6 — OCP calibration and fault validation

Perform this only after normal stacked operation is stable and the bench PSU limit remains active.

- Record zero-current offset for all four amplifier channels.
- Apply stepped current through each range and compare measured shunt voltage, amplifier output, and threshold.
- Measure rising trip, reset current, hysteresis, response time, and temperature at least five times per channel.
- Test minimum and maximum intended rail voltage.
- Apply load steps below the trip point and check for false trips.
- Test the complete fault-bus truth table, including comparator unpowered and amplifier disconnected cases.
- Open each Kelvin connection individually and record whether behavior is safe and diagnosable.

**Gate:** Do not call the OCP calibrated until the measured thresholds, hysteresis, fault polarity, and failure behavior are documented for all four channels.

## Evidence package for each phase

Store the completed worksheet or a dated lab note with:

- board revision and serial/assembly identifier
- schematic/netlist/firmware revision
- fitted parts and jumper/bypass state
- bench supply voltage and current limit
- instrument IDs or settings where relevant
- raw measurements and pass/fail decision
- photos or scope captures for anomalies
- explicit next gate: `PASS`, `HOLD`, or `FAIL`

## First execution block

The first practical block is:

1. Resolve the current ERC/DRC and unconnected-item findings, starting with the regulator and HAT PCB connectivity.
2. Re-run ERC/DRC using violation exit codes, refresh netlists, and perform the final connectivity audit.
3. Record the release decision and generate fabrication outputs only after both boards pass.
4. Run incoming inspection and standalone regulator power before touching the HAT.
