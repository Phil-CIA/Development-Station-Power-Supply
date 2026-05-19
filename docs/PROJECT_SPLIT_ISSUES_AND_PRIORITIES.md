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
**Status**: 🟡 Topology implemented on Rev-B and verified in netlist; bring-up validation pending for closure  
**Path**: 📍 Redesign (implemented in Rev-B) + 🔧 Bring-Up (validation and any value retune)  
**Priority**: High (functional flaw)  
**Description**: Feedback divider (measuring 5V output for regulation) is located after output MOSFETs (Q1/Q7, Q2/Q8). When MOSFETs are off at cold-start, the feedback path is open, causing the LM2596 to regulate to ~10.7V instead of 5V until the MCU boots and closes the MOSFET gates.  
**Current Workaround (Rev1 bring-up baseline)**: Jumper to sense before MOSFET; tenths of volt offset accepted as calibration margin. Less critical since +5V bootstrap regulator was added to power the MCU without waiting on the main rail.  
**Rev-B Baseline**: Use the implemented 3-channel selector topology as the active baseline; do not introduce new topology changes unless bring-up data shows continuity error that requires value retuning.  
**Implemented Rev-B Topology**: Op-amp plus Schottky selector per channel (all three channels: 5V, 3.3V, Adj):

Each channel now uses two buffered sense paths combined through a BAT54C selector node, with remote-sense intended as primary and local-sense intentionally underbiased as fallback.

**Sense points per channel:**

| Channel | Sense A node (always-live) | Sense A net (regulator) | Sense B path (HAT → regulator) | Sense B net (regulator) | FB net |
|---------|---------------------------|------------------------|-------------------------------|------------------------|--------|
| 5V | L3 pin 2 | `+5V_reg` | `V_out +5V` → R59 (10Ω) → `VSENSE_5V+` → J4 pin 3 → J1 pin 4 | `Feedback 5V` | `Net-(U4-FB)` |
| 3.3V | L1 pin 2 | `+3.3V_Reg` | `V_out +3V3` → R61 (10Ω) → `VSENSE_3V3+` → J4 pin 15 → J1 pin 9 | `Feedback 3.3` | `Net-(U3-FB)` |
| Adj | L5 pin 2 | `+V Adj Channel` | `V_out +CH3` → R63 (10Ω) → `VSENSE_ADJ+` → J6 pin 12 → J2 pin 4 | `Feedback Adj Channel` | `Net-(U2-FB)` |

**Op-amp selector cell (per channel):**
- **Remote path buffer**: Non-inverting buffer for VSENSE path into selector input
- **Local path buffer**: Non-inverting buffer for always-live local rail path into selector input
- **Selector/output node**: BAT54C common-cathode combine to LM2596 FB-divider top node
- **Intent**: Remote path wins in normal operation; local path is slightly lower to provide startup fallback without introducing large measurement handoff steps
- **Verified Rev-B values**: R31/R33/R35 = 100 ohm and R32/R34/R36 = 33k on local fallback legs

**Parts count (new additions):**
- 3 × dual op-amp (e.g., LMV358 SOT-23-8) — one per channel, 6 buffer sections total
- 6 × Schottky diode (e.g., BAT54 or similar, low Vf) — 2 per channel
- 3 × resistive divider for Sense A (2 resistors per channel matching existing Sense B ratio)
- 3 × 100pF compensation cap for Sense A divider (matching C21/C7/C11)

**Residual Impact of Bootstrap Fix**: +5V bootstrap regulator still shortens cold-start exposure, but RB-001 closure now depends on bring-up continuity data, not additional topology creation.  
**Next Action**: Preserve current topology and execute bring-up validation; retune local attenuation values only if measured continuity/step error exceeds expectation.  
**Tracker Alignment Note**: This item has moved from "topology creation" to "bring-up validation and possible value tuning."  
**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 2. USB Vbus Backfeed: HAT/ESP32 USB 5V Fights Regulator 5V
**Status**: 🟡 ✅ Workaround active | ⏳ On-hold pending board decision  
**Path**: 📍 Redesign (long-term), 🔧 Bring-Up (avoid during testing)  
**Priority**: High (current-limit during stacked test)  
**Description**: When HAT is powered by 5V regulator output AND connected to USB, USB Vbus (~10V) back-drives the 5V rail, causing voltage sag and current limiting (observed 520mA @ 9.3V during first stacked test).  
**Current Workaround**: Disconnect USB 5V or use data-only cable during bench testing  
**Redesign Solution**: Add Schottky diode on Vbus on HAT PCB to isolate USB from 5V rail  
**Board Change Note**: Potential migration to STM32F103C8T6 dev board in progress; Schottky isolation implementation deferred until board selection is finalized to avoid rework  
**Bring-Up Impact**: No USB power during load testing; use serial-only debugging if needed  
**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 3. ON/OFF Pin Switching: MOSFET Gate Drive Circuit
**Status**: 🔴 Under investigation  
**Path**: 📍 Redesign (design review)  
**Priority**: High (control reliability)  
**Description**: LM2596 pin 5 (ON/OFF) uses N-channel MOSFET for low-side pull-down. Gate drive voltage, rise/fall time, debounce behavior not yet characterized under load transients.  
**Next Step**: Scope gate voltage, drain voltage, and load response during ON → OFF → ON cycles  

**Current Rev-B netlist snapshot (for schematic review):**
- 3.3V channel control path:
	- Gate drive input: `ISET_MPU_3V3` -> R2 (100 ohm)
	- Gate pull element: R6 (100k to GND)
	- ON/OFF node pull element: R1 (10k to +5V_Boot; verified in current netlist export)
	- LM2596 ON/OFF node: U3 pin 5 via net `Net-(Q16-D)`
	- Netlist observation: Q16 source (pin 2) is currently on `GND`
- 5V channel control path:
	- Gate drive input: `ISET_MPU_5V` -> R24 (100 ohm)
	- Gate pull element: R4 (100k to GND)
	- ON/OFF node pull element: R5 (10k to +5V_Boot; verified in current netlist export)
	- LM2596 ON/OFF node: U4 pin 5 via net `Net-(Q3-D)`
	- Netlist observation: Q3 source (pin 2) is currently on `GND`
- Adj channel control path:
	- Gate drive input: `ISET_MPU_Channel_3` -> R63 (100 ohm)
	- Gate pull element: R64 (100k to GND)
	- ON/OFF node pull element: R65 (10k to +5V_Boot; verified in current netlist export)
	- LM2596 ON/OFF node: U2 pin 5 via net `Net-(Q2-D)`
	- Netlist observation: Q2 source (pin 2) is currently on `GND`

Netlist baseline verified (2026-05-19):
- ON/OFF pull values: R1/R5/R65 = 10k (not 100k as previously documented)
- Gate pull values: R4/R6/R24/R64 = 100k as documented
- Source-side bias: Q2/Q3/Q16 sources on GND (no source-specific ties to +12V present)

**RB-003 schematic checklist (execute in-order):**
1. Confirm intended ON/OFF polarity and thresholds at the LM2596 pin for each channel.
2. Verify symbol pin mapping for Q2/Q3/Q16 against the selected BSS138 footprint orientation in schematic and PCB.
3. Validate whether source-node bias is intentionally referenced to +12V for this topology.
4. Compare available gate drive high levels (ISET_MPU_3V3, ISET_MPU_5V, ISET_MPU_Channel_3) versus source-node potential to ensure expected Vgs sign and magnitude.
5. Confirm ON/OFF node default state at reset using the current 100k pull network (R1/R5/R65 and gate pulls R6/R4/R64).
6. If Vgs operating margin is invalid, update topology (device orientation and/or device type and/or drive reference) before any value tuning.
7. Re-export netlist and verify U2/U3/U4 pin-5 control nets still map one-to-one with their channel-select inputs and default pulls.

**Pass criteria for RB-003 closure:**
- Each channel has a valid control-state truth table (OFF and ON) with achievable gate drive margins at worst-case supply.
- Default power-up state is deterministic and matches intended safety behavior.
- ON/OFF transitions are explainable from schematic alone without relying on ambiguous pin orientation assumptions.

**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 4. Internal Voltage Rail Protection: OCP/OVP Fuses or Monitoring
**Status**: 🟡 Open  
**Path**: 📍 Redesign (design review)  
**Priority**: Medium (fault tolerance)  
**Description**: No per-rail fuses or TVS diodes protecting against short-circuit or overvoltage. LM2596 internal OCP is only protection; relies on bench PSU external fuse.  
**Bring-Up Strategy**: If a rail is shorted, test bench PSU defensive behavior; record fuse-trip time and power shutdown response  
**Redesign Solution**: Add SMD fuses (e.g., 2A for 5V, 1.5A for 3.3V) or TVS diodes per output; evaluate firmware rail monitoring thresholds  
**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 12. Bootstrap Rail Split Naming: Contract Cleanup Pending
**Status**: 🟡 Open (contract and baseline wording cleanup)  
**Path**: 📍 Redesign (net-contract correction)  
**Priority**: High (cross-board contract consistency)  
**Description**: Bootstrap naming and ownership language has drifted across handoffs and analysis docs. The active requirement is to keep Rev-B as the source baseline and remove ambiguous naming claims from planning documents until fully re-verified in both schematic and netlist exports.  
**Impact**: Ambiguous naming statements can misdirect routing and cross-board contract updates.  
**Redesign Solution**: Keep Rev-B as active baseline for layout, and either align all docs to a single naming contract or explicitly archive conflicting legacy wording.  
**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF_SHORT.rmd](../NEW_CHAT_HANDOFF_SHORT.rmd)

---

## PCB Layout & Mechanical Issues

### 5. TS5A3157-DCKR Footprint: Incorrect Package Match
**Status**: ✅ Resolved on Rev-B (symbol library default + instance aligned)  
**Path**: 📍 Redesign (high priority)  
**Priority**: High (assembly and functional risk)  
**Description**: Footprint used for analog switch does not match SOT-363/SC-70-6 package pinout or land pattern. Risk of rotated/misaligned placement or non-functional switch path.  
**Current Impact**: ✅ Rev-B instance footprint corrected to `Package_TO_SOT_SMD:SOT-363_SC-70-6` (LCSC C91307); symbol library default footprint also updated to prevent regressions.  
**Redesign Action**: Completed. Symbol library default is now locked to correct footprint; assembly risk eliminated for new instances in this schematic.  
**Reference**: [HANDOFF.md](../HANDOFF.md)

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
**Reference**: [HANDOFF.md](../HANDOFF.md)

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
**Reference**: [HANDOFF.md](../HANDOFF.md)

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
**Reference**: [HANDOFF.md](../HANDOFF.md)

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
**Reference**: [HANDOFF.md](../HANDOFF.md)

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

### 13. Short-Circuit Feedback Path: Protection Feedback Integration Missing
**Status**: 🟢 Implemented on Rev-B (verification complete)  
**Path**: 📍 Redesign (control/protection integration)  
**Priority**: High (layout blocker for protection/control routing)  
**Description**: Short-circuit behavior now has a dedicated fault feedback path in Rev-B. INA3221 alert outputs are summed into named nets and routed into control logic.  
**Impact**: Layout blocker cleared for fault-feedback ownership and routing on Rev-B.  
**Redesign Actions**:  
- Define explicit short-circuit feedback signal path (source, sink, polarity, threshold intent)
- Connect INA `CRITICAL`/`WARNING` outputs to named fault nets (wired-OR or per-rail scheme) and route to MCU/control domain with proper pull-up strategy
- Rev-B default proposal: use `FAULT_CRITICAL_SUM` and `FAULT_WARNING_SUM` nets and route to currently unconnected MPU pins (`U7.GPIO22` and `U7.GPIO2`) to avoid blocking layout while MPU migration is deferred
- Add named net(s) and connector contract updates where required
- Ensure feedback path is represented in both regulator Rev-B and HAT Rev-B schematics/netlists

**Verification Evidence (2026-05-18)**:
- `FAULT_CRITICAL_SUM` includes U1/U2/U3 `CRITICAL` plus U7 `GPIO2`
- `FAULT_WARNING_SUM` includes U1/U2/U3 `WARNING` plus U7 `GPIO22`
- Prior `unconnected-(U*-CRITICAL/WARNING-Pad*)` nets are absent from the regenerated Rev-B HAT netlist

---

### 14. Reference Sensing Path: Architecture Not Finalized
**Status**: 🟢 Contract verified on Rev-B netlists  
**Path**: 📍 Redesign (measurement architecture)  
**Priority**: High (layout blocker for analog routing quality)  
**Description**: Reference sensing requirements (reference node source, return path, and isolation strategy) are not finalized in the active hardware contract. Rev-B already contains `VSENSE_3V3±`, `VSENSE_5V±`, and `VSENSE_ADJ±` nets, but there is no locked reference-sensing contract defining the reference anchor and Kelvin/return strategy for final layout.  
**Impact**: Layout blocker cleared at schematic/netlist level; contract is now defined and present across regulator and HAT Rev-B netlists.  
**Redesign Actions**:  
- Define reference sensing source and return explicitly (including where analog reference ties to power ground)
- Lock Kelvin sense routing rules for `VSENSE_*` pairs (pairing, return locality, and keep-out from switching paths)
- Add/confirm net names and measurement entry points in Rev-B schematics
- Freeze a reference contract table for layout: `VSENSE_3V3±`, `VSENSE_5V±`, `VSENSE_ADJ±` with mapped measurement sink and physical test access
- Validate consistency with INA/channel sensing and feedback nets

**Reference Sensing Contract (Rev-B Lock v1)**:

| Pair | Regulator/HAT Entry | Measurement Sink | Routing Contract |
|------|----------------------|------------------|------------------|
| `VSENSE_3V3+/-` | `J4` pins 15/14 | Divider inputs via `R61/R63` and `R62/R64` | Route as paired Kelvin sense lines; keep local return with its + pair; avoid switching-node adjacency |
| `VSENSE_5V+/-` | `J4` pins 3/2 | Divider inputs via `R59/R60` | Route as paired Kelvin sense lines; maintain matched local return path |
| `VSENSE_ADJ+/-` | `J6` pins 12/11 | Sense/protection entry via `D7` (`+`) and `D1` (`-`) | Route as paired Kelvin sense lines; keep reference return tied to same channel locality |

**Verification Evidence (2026-05-18)**:
- Regulator Rev-B netlist includes all six `VSENSE_*` nets with connector entries on `J1`/`J2`
- HAT Rev-B netlist includes all six `VSENSE_*` nets with matching connector entries on `J4`/`J6`
- Pair structure is consistent with divider/protection entry points documented in this contract table

**Implementation note for layout**:
- Keep Kelvin pairing and return-locality rules enforced during PCB routing; treat this as a layout quality rule, not a remaining schematic blocker

---

### 15. MPU Change Plan: Device Migration Deferred but Must Be Tracked
**Status**: 🟡 Planned (deferred)  
**Path**: 📍 Redesign (platform transition planning)  
**Priority**: Medium (future impact, low immediate layout risk if deferred)  
**Description**: MPU change is planned, but not part of immediate pre-layout completion scope. It must remain visible as a tracked issue to avoid hidden downstream rework.  
**Current Decision**: Complete current non-MPU blocking schematic/layout prerequisites first, then execute MPU migration as a separate controlled step.  
**Redesign Actions**:  
- Capture target MPU choice and key interface deltas
- Record pin/function migration impacts on regulator/HAT connector contracts
- Schedule migration after current blocking items are closed

---

## Summary: Issues by Path

### 📍 Redesign Path Issues (PCB Next Iteration)

| Issue | ID | Priority | Action |
|-------|----|-----------|----|
| Feedback divider before MOSFET | RB-001 | High | Topology implemented on Rev-B; validate handoff continuity and retune only if needed |
| USB Vbus isolation on HAT | RB-002 | High | Add Schottky diode; update netlist/BOM |
| TS5A3157 footprint correction | RB-009 | High | ✅ Locked to Package_TO_SOT_SMD:SOT-363_SC-70-6 on Rev-B |
| MOSFET gate drive characterization | RB-003 | High | Scope transient; optimize pull-up/down resistors |
| Rail protection (fuses/TVS) | RB-006 | Medium | Add SMD fuses and/or TVS diodes per output |
| Bootstrap split net labels | 12 | High | Keep Rev-B baseline and remove conflicting naming claims until re-verified |
| Short-circuit feedback path | 13 | High | Implemented on Rev-B: INA CRITICAL/WARNING summed and routed to GPIO2/GPIO22; keep pull-up behavior documented |
| Reference sensing architecture | 14 | High | Verified on Rev-B netlists; enforce Kelvin pair/return-locality rules during layout |
| MPU change tracking | 15 | Medium | Defer migration, but keep pin/contract deltas tracked for next phase |
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
| RB-001 continuity and handoff-step validation | RB-001 | High | Measure remote/local handoff behavior across load and startup; retune local attenuation if needed |
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

- [PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md](PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md) — Detailed electrical and design issues (RB-001 through RB-009)
- [STACKED_BOARD_FIRST_POWER_BASELINE.md](STACKED_BOARD_FIRST_POWER_BASELINE.md) — Bench bring-up measurements and INA confidence assessment
- [GPIO_PINOUT.md](GPIO_PINOUT.md) — I2C bus and control signal mapping
- [power_telemetry_map.h](../../src/power_telemetry_map.h) — INA address definitions and rail mapping (to be updated based on truth table)

---

## Revision History

| Date | Event |
|------|-------|
| 2026-05-14 | Created PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md; consolidated all issues from bring-up and design review |
| 2026-05-18 | Added issue 12 for missing bootstrap split net labels and redesign-path contract alignment |
| 2026-05-18 | Added issues 13-15 for short-circuit feedback integration, reference sensing architecture, and deferred MPU migration tracking |
| 2026-05-18 | Re-validated Rev-B HAT netlist: issue 13 fault nets implemented and no INA CRITICAL/WARNING no-connect remnants; added Reference Sensing Contract (Rev-B Lock v1) for issue 14 |
| 2026-05-18 | Cross-board Rev-B VSENSE audit complete: issue 14 promoted to contract-verified at netlist level |
| 2026-05-18 | RB-009 resolved: TS5A3157 symbol library default footprint corrected to Package_TO_SOT_SMD:SOT-363_SC-70-6 on Rev-B |
| 2026-05-18 | Restart alignment: RB-001 status updated to implemented Rev-B 3-channel selector with bench continuity validation pending; issue 12 wording normalized to contract-cleanup state |
