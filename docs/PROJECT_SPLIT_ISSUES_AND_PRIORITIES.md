# Project Split: Issues and Priorities

**Date**: May 14, 2026  
**Scope**: Complete issue inventory for both redesign path and bring-up continuation path  

---

## Overview

The WorkStation power-supply project is splitting into two parallel efforts:

1. **Redesign Path**: PCB revision with architectural improvements; designs for next-generation boards
2. **Bring-Up Path**: Continue developing on Rev 1 hardware; learn failure modes and system behavior

This document tracks all known issues, their priority, and which path owns each.

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| 🔴 | High priority — blocks system validation or creates risk |
| 🟡 | Medium priority — design flaw or missing feature |
| 🟢 | Low priority — quality-of-life, optimization, or future expansion |
| 📍 | Redesign Path — owned by PCB redesign cycle |
| 🔧 | Bring-Up Path — addressed while developing on Rev 1 |
| ✅ | Resolved — change committed or accepted workaround in place |

---

## Electrical & Power Issues

### 1. LM2596 Feedback Path: Open Circuit When MOSFETs Off
**Status**: 🟡 ✅ Workaround active  
**Path**: 📍 Redesign (long-term)  
**Priority**: High (functional flaw)  
**Description**: Feedback divider (measuring 5V output for regulation) is located after output MOSFETs. When MOSFETs are off, feedback floats to ~10.7V causing regulator to produce ~10.7V instead of 5V on power-up.  
**Current Workaround**: Jumper to sense before MOSFET; tenths of volt offset accepted as calibration margin  
**Redesign Solution**: Move feedback divider sense point before MOSFET in schematic  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-001)

---

### 2. USB Vbus Backfeed: HAT/ESP32 USB 5V Fights Regulator 5V
**Status**: 🟡 ✅ Workaround active  
**Path**: 📍 Redesign (long-term), 🔧 Bring-Up (avoid during testing)  
**Priority**: High (current-limit during stacked test)  
**Description**: When HAT is powered by 5V regulator output AND connected to USB, USB Vbus (~10V) back-drives the 5V rail, causing voltage sag and current limiting (observed 520mA @ 9.3V during first stacked test).  
**Current Workaround**: Disconnect USB 5V or use data-only cable during bench testing  
**Redesign Solution**: Add Schottky diode on Vbus on HAT PCB to isolate USB from 5V rail  
**Bring-Up Impact**: No USB power during load testing; use serial-only debugging if needed  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-002)

---

### 3. ON/OFF Pin Switching: MOSFET Gate Drive Circuit
**Status**: 🔴 Under investigation  
**Path**: 📍 Redesign (design review)  
**Priority**: High (control reliability)  
**Description**: LM2596 pin 5 (ON/OFF) uses N-channel MOSFET for low-side pull-down. Gate drive voltage, rise/fall time, debounce behavior not yet characterized under load transients.  
**Next Step**: Scope gate voltage, drain voltage, and load response during ON → OFF → ON cycles  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-003)

---

### 4. Internal Voltage Rail Protection: OCP/OVP Fuses or Monitoring
**Status**: 🟡 Open  
**Path**: 📍 Redesign (design review)  
**Priority**: Medium (fault tolerance)  
**Description**: No per-rail fuses or TVS diodes protecting against short-circuit or overvoltage. LM2596 internal OCP is only protection; relies on bench PSU external fuse.  
**Bring-Up Strategy**: If a rail is shorted, test bench PSU defensive behavior; record fuse-trip time and power shutdown response  
**Redesign Solution**: Add SMD fuses (e.g., 2A for 5V, 1.5A for 3.3V) or TVS diodes per output; evaluate firmware rail monitoring thresholds  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-006)

---

## PCB Layout & Mechanical Issues

### 5. TS5A3157-DCKR Footprint: Incorrect Package Match
**Status**: 🔴 Confirmed  
**Path**: 📍 Redesign (high priority)  
**Priority**: High (assembly and functional risk)  
**Description**: Footprint used for analog switch does not match SOT-363/SC-70-6 package pinout or land pattern. Risk of rotated/misaligned placement or non-functional switch path.  
**Current Impact**: May require manual rework on prototypes if this part is populated  
**Redesign Action**: Verify datasheet package drawing, correct footprint in KiCad library, re-check symbol-to-footprint pin mapping  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-009)

---

### 6. Input Voltage Terminal Block: Labeling and Polarity Clarity
**Status**: 🟡 Open  
**Path**: 📍 Redesign (assembly QA)  
**Priority**: Medium (user experience / assembly error prevention)  
**Description**: Input 12V terminal block needs clear polarity marking in silkscreen; physical keying or polarity guard preferred.  
**Redesign Actions**:  
- Add silkscreen arrows and +12V / GND labels
- Consider upgrading to keyed connector type
- Document wire gauge and current rating for each terminal  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-004)

---

### 7. Stack Height Connector: Physical Fit and Viability
**Status**: 🟡 Open  
**Path**: 📍 Redesign (mechanical review)  
**Priority**: Low to Medium (mechanical integration)  
**Description**: HAT stacks on regulator via connector; stack height may not fit intended enclosure; alignment/contact pressure not yet validated under load.  
**Bring-Up Observations**: Manual stacking works on bench; no mechanical feedback issues observed during light load  
**Redesign Actions**:  
- Measure final stack height with connector
- Review enclosure mechanical constraints
- Confirm connector contact rating for observed current levels
- Evaluate alignment pins/slots for removal/insertion robustness  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-007)

---

### 8. ESP32 Footprint: Compatibility and Silk Clarity
**Status**: 🟡 Under investigation  
**Path**: 📍 Redesign (assembly clarity)  
**Priority**: Medium (assembly accuracy)  
**Description**: ESP32 footprint may not match variant in use (originally designed for standard ESP32; bench testing used ESP32-C6). Silk labels for critical pins unclear post-assembly.  
**Bring-Up Finding**: HAT discovered with ESP32-C6, not standard ESP32; pin mapping is GPIO5/GPIO6 per HAT schematic, not footprint dependent  
**Redesign Actions**:  
- Verify footprint matches deployed variant
- Review/clarify silk legends for critical pins
- Confirm mechanical clearance around USB connector  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-005)

---

### 9. Buck Output Status LED: Design Consideration
**Status**: 🟢 Design proposal  
**Path**: 📍 Redesign (nice-to-have)  
**Priority**: Low (user feedback / debugging aid)  
**Description**: Proposal to add LED on 5V buck output to indicate power-on, fault, or current-limit mode visually.  
**Bring-Up Strategy**: Not critical for bringup; skip on Rev 1  
**Redesign Considerations**:  
- LED current draw (~1–2mA @ 2V drop, standard ~1kΩ series resistor)
- Blink pattern for fault states (OCP, OVP, current-limit cycle)
- Firmware output pin assignment required  
**Reference**: [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md#rb-008)

---

## Measurement & Control Issues

### 10. INA3221 Current Mapping: Channel-to-Rail Assignment Incorrect
**Status**: 🔴 Confirmed  
**Path**: 🔧 Bring-Up (debug), 📍 Redesign (schematic review)  
**Priority**: High (telemetry accuracy)  
**Description**: INA3221 current readings do not match known load behavior. With 10 ohm load on 3.3V drawing ~327mA, INA reports only 18–20mA on loaded channel. Likely cause: wrong channel-to-shunt mapping, incorrect shunt values assumed, or sense routing does not place load current through the expected shunt.  
**Observed Data** (as of 2026-05-11):
- Loaded 3.3V: measured ~3.27V, expected ~327mA, INA reported 18mA on CH1/CH2
- Voltage telemetry appears directionally correct (3.304V INA vs ~3.273V measured)
- Input shunt (CH3 @ 0x41) also suspect: 11.544V and 252mA reported vs ~12.02V measured

**Bring-Up Debug Plan**:  
1. Build channel-to-shunt truth table from HAT schematic
2. Meter actual shunt resistor values on board (20mΩ vs 200mΩ assumed)
3. Load one rail at a time and trace INA reading for the corresponding channel
4. Identify which channels are correctly routed vs. misassigned

**Redesign Review**:  
- Verify HAT schematic rail-to-INA-channel assignment
- Confirm shunt value assumptions with actual BOM
- Update firmware mappings based on truth table  
**Reference**: [STACKED_BOARD_FIRST_POWER_BASELINE.md](STACKED_BOARD_FIRST_POWER_BASELINE.md) INA Confidence Check section

---

### 11. Adjustable Rail Feedback: Disconnected or Default State
**Status**: 🟡 Open  
**Path**: 🔧 Bring-Up (verify), 📍 Redesign (schematic check)  
**Priority**: Medium (rail control)  
**Description**: Adjustable rail (channel 3) reading same voltage as 3.3V reference, suggesting feedback divider not connected or in default state. Rail should be independently adjustable via ISET potentiometer or digital trim (MCP4231).  
**Bring-Up Investigation**:  
- Check feedback divider continuity (from adjustable output to feedback network)
- Verify potentiometer (ISET) range and orientation
- Measure feedback network voltage vs potentiometer output
- If applicable, check LM2596 feedback pin (pin 4) for proper divider scaling  
**Redesign**: Ensure feedback network is present and properly routed if adjustable rail intended to be independent

---

## Summary: Issues by Path

### 📍 Redesign Path Issues (PCB Next Iteration)

| Issue | ID | Priority | Action |
|-------|----|-----------|----|
| Feedback divider before MOSFET | RB-001 | High | Relocate sense point in schematic; validate feedback transient |
| USB Vbus isolation on HAT | RB-002 | High | Add Schottky diode; update netlist/BOM |
| TS5A3157 footprint correction | RB-009 | High | Fix SOT-363/SC-70-6 mapping; KiCad library update |
| MOSFET gate drive characterization | RB-003 | High | Scope transient; optimize pull-up/down resistors |
| Rail protection (fuses/TVS) | RB-006 | Medium | Add SMD fuses and/or TVS diodes per output |
| Terminal block labeling | RB-004 | Medium | Add silkscreen polarity + keyed connector |
| ESP32 footprint clarity | RB-005 | Medium | Verify variant match; update silk legends |
| Stack height/connector review | RB-007 | Low | Measure height; confirm alignment pins; contact rating |
| Buck output LED (optional) | RB-008 | Low | Design LED circuit; firmware pin assignment |
| INA current mapping (architectural) | 10 | High | Review HAT schematic rail-to-channel assignment; verify shunt BOM |

---

### 🔧 Bring-Up Path Issues (Continue on Rev 1)

| Issue | ID | Priority | Action |
|-------|----|-----------|----|
| USB Vbus workaround validation | RB-002 | High | Document safe testing procedure (no USB Vbus during bench) |
| Feedback jumper stability testing | RB-001 | High | Long-duration load test; confirm no mode-hopping |
| INA current debug & mapping | 10 | High | Build truth table; meter shunt values; correlate channels to rails |
| Adjustable rail feedback investigation | 11 | Medium | Check continuity; verify potentiometer; characterize output range |
| Rail protection behavior | RB-006 | Medium | Intentional short-circuit test (safe conditions); record fuse trip time |
| MOSFET switch transient logging | RB-003 | Medium | Scope gate/drain during control transitions; document rise/fall times |
| System load-testing at higher currents | Various | Medium | Test with multiple loads; verify no unexpected behavior or resets |

---

## Implementation Strategy

**Redesign Path** (High-level process):
1. Create `dsp-regulator-next-iter/` and `dsp-regulator-hat-next-iter/` folders in `hardware/kicad/`
2. Copy current schematics as starting points
3. Apply fixes from highest-priority items first
4. Perform ERC/DRC validation on each change
5. Tag each fix with issue ID for traceability

**Bring-Up Path** (High-level process):
1. Execute debugging plan for INA current mapping (build truth table, meter values)
2. Document safe procedures for USB isolation, load testing, protection validation
3. Log any new issues discovered during testing
4. Keep Rev 1 hardware stable as testbed for system discovery

---

## Reference Documents

- [REGULATOR_BOARD_CHANGE_TRACKER.md](REGULATOR_BOARD_CHANGE_TRACKER.md) — Detailed electrical and design issues (RB-001 through RB-009)
- [STACKED_BOARD_FIRST_POWER_BASELINE.md](STACKED_BOARD_FIRST_POWER_BASELINE.md) — Bench bring-up measurements and INA confidence assessment
- [GPIO_PINOUT.md](GPIO_PINOUT.md) — I2C bus and control signal mapping
- [power_telemetry_map.h](../../src/power_telemetry_map.h) — INA address definitions and rail mapping (to be updated based on truth table)

---

## Revision History

| Date | Event |
|------|-------|
| 2026-05-14 | Created PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md; consolidated all issues from bring-up and design review |
