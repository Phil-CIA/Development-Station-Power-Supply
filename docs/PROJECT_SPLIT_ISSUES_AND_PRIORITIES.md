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
**Control Ownership Rule**: Keep the output MOSFETs as the load-connect/load-isolate layer. Use LM2596 pin 5 as the authoritative hard-disable point for faults and short-circuit shutdown. A low-side pull-down stage can assert enable, but it cannot by itself implement a fault-off override.

**Desired Rev-B control truth table**:

| Load command | Fault critical | Output MOSFET state | LM2596 pin 5 state | Result |
|--------------|----------------|---------------------|--------------------|--------|
| Disable | Inactive | Off | High / disable | Rail off, load isolated |
| Enable | Inactive | On | Low / enable | Rail on, load connected |
| Disable | Active | Off | High / disable | Fault wins, rail off |
| Enable | Active | Off | High / disable | Fault overrides command, rail off |

**Schematic edit implication**: if a shutdown path currently shares a low-side BSS138 pull-down with the enable path, it must be inverted or moved to a disable-only node so that fault assertion forces the shutdown state instead of reinforcing enable.

**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 2. USB Vbus Backfeed: HAT/ESP32 USB 5V Fights Regulator 5V
**Status**: 🟢 On hold for current MPU path | ⏳ Re-open only if MPU changes to a design with board-exposed VBUS  
**Path**: 📍 Redesign (long-term), 🔧 Bring-Up (avoid during testing)  
**Priority**: High (current-limit during stacked test)  
**Description**: When HAT is powered by 5V regulator output AND connected to USB, USB Vbus (~10V) back-drives the 5V rail, causing voltage sag and current limiting (observed 520mA @ 9.3V during first stacked test).  
**Current Workaround**: Disconnect USB 5V or use data-only cable during bench testing  
**Redesign Solution**: Add Schottky diode on Vbus on HAT PCB to isolate USB from 5V rail  
**Board Change Note**: For the current architecture, boot-regulator power to MPU and non-exposed board VBUS make this a non-blocking item. Keep this issue parked unless MPU migration re-introduces direct VBUS exposure on-board.  
**Bring-Up Impact**: No USB power during load testing; use serial-only debugging if needed  
**Next component placement package (only if MPU migration requires VBUS handling)**:
- 1 x series Schottky on USB VBUS path (recommended start: SS14 or SS24, anode at USB VBUS side, cathode at local +5V rail side).
- 1 x optional resettable fuse on USB VBUS input before the Schottky (recommended start: 500mA hold) for cable/port fault limiting.
- 1 x optional VBUS bleed resistor to GND on USB-side node (recommended start: 100k) to discharge cable-side float when unplugged.
- 2 x test points: `TP_USB_VBUS_IN` (before diode) and `TP_USB_VBUS_OUT` (after diode) for bring-up verification.
**Reference**: [HANDOFF.md](../HANDOFF.md) and [NEW_CHAT_HANDOFF.rmd](../NEW_CHAT_HANDOFF.rmd)

---

### 3. ON/OFF Pin Switching: MOSFET Gate Drive Circuit
**Status**: 🟡 Topology implemented, validation pending  
**Path**: 📍 Redesign (design review)  
**Priority**: High (control reliability)  
**Description**: LM2596 pin 5 (ON/OFF) uses N-channel MOSFET for low-side pull-down. Gate drive voltage, rise/fall time, debounce behavior not yet characterized under load transients.  
**Next Step**: Scope gate voltage, drain voltage, and load response during ON → OFF → ON cycles  

**Control contract for the Rev-B edits**:
- The MOSFET pair that touches `~{ON}/OFF` is a load-enable path, not a fault-off path.
- A fault signal must either drive a separate disable element or be inverted before it reaches the shutdown node.
- If the same low-side pull-down polarity is used for both enable and fault, fault assertion can only reinforce enable, which is the wrong polarity for shutdown.
- Use one fault-isolation diode per rail from `FAULT_CRITICAL_SUM` to each enable gate node instead of tying the three gate nets together. That preserves per-rail enable control while still letting fault pull the active gate low.

**2026-05-19 implementation snapshot (netlist)**:
- Fault fanout present: `FAULT_CRITICAL_SUM` -> `R37`, `R39`, `R45` (pin 1 on each resistor).
- Per-rail diode isolation present: `D7`, `D8`, `D10` between each fault branch and each gate net (`Q1-G`, `Q4-G`, `Q5-G`).
- Cross-coupled gate net condition is removed at the netlist level.

**Bring-up instrumentation status (2026-05-19):**
- Gate and fault test points added (3 on regulator path, 1 on HAT path).

**Fault-conditioning component status (2026-05-19 netlist):**
- Implemented: branch pull-ups (`R47`, `R48`, `R49`) to `+5V_Boot`.
- Implemented: branch RC caps (`C14`, `C17`, `C23`) at the three fault-injection branches.
- Implemented: connector entry series resistor (`R50`) between `J2 Pin_3` and `FAULT_CRITICAL_SUM`.

**Next steps (validation, not new components):**
- Scope `FAULT_CRITICAL_SUM` and each gate net (`Q1-G`, `Q4-G`, `Q5-G`) during injected fault pulses and verify no cross-coupled gate transitions.
- Measure fault assert/release timing through `R50` and branch RC networks to confirm shutdown response stays within protection targets.
- Run truth-table bench validation for all four states (Enable/Disable x Fault inactive/active) and record resulting `~ON/OFF` pin state per channel.

**Desired shutdown truth table**:

| MCU rail-enable | Fault critical | Expected `~{ON}/OFF` | Expected regulator state |
|-----------------|----------------|----------------------|--------------------------|
| 0 | 0 | High | Disabled |
| 1 | 0 | Low | Enabled |
| 0 | 1 | High | Disabled |
| 1 | 1 | High | Disabled |

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
**Status**: ✅ Resolved (Rev-B naming normalized and contract checks updated)  
**Path**: 📍 Redesign (net-contract correction)  
**Priority**: High (cross-board contract consistency)  
**Description**: Bootstrap naming and ownership language had drifted across handoffs and analysis docs. The requirement was to align Rev-B to a single naming contract and re-verify in both schematic and netlist exports.  
**Impact**: Prior ambiguity could misdirect routing and cross-board contract updates.  
**Resolution**: Rev-B exports are normalized to `+5V_Boot` only, and connector-contract verification now supports explicit baseline policy modes (`reduced-policy` default, `strict` audit mode) so redesign gating follows the active reduced-contract path while preserving strict baseline auditing when needed.  
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
**Status**: 🟡 Digital fault path implemented on Rev-B; analog instantaneous OCP path pending  
**Path**: 📍 Redesign (control/protection integration)  
**Priority**: High (layout blocker for protection/control routing)  
**Description**: Short-circuit behavior now has a dedicated digital fault feedback path in Rev-B via INA3221 alert outputs summed into named nets and routed into control logic. An analog instantaneous over-current path is still needed if sub-conversion-cycle protection response is required.  
**Impact**: Digital fault ownership/routing blocker is cleared; fast analog trip capability remains an open integration item.  
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

**Next component placement package (analog instantaneous OCP)**:
- 3 x fast current-sense front ends (one per rail, reusing existing shunt locations) feeding comparator inputs.
- 1 x multi-channel comparator stage (3 channels + optional spare/latch) to generate per-rail fast-trip outputs.
- 3 x threshold-set networks (divider/reference per channel, or shared reference with per-channel trim) for trip current setpoint.
- 3 x blanking/filter RC networks at comparator inputs to avoid switch-edge false trips.
- 3 x isolation/injection elements from comparator outputs into the existing fault-disable path so analog trip can force shutdown without cross-coupling rails.

**5V channel first (pilot implementation order)**:
- Use the existing 5V shunt path first (R13/R14 network) as the analog current source; do not use R42/R43/R44 (those are INA address-strap resistors).
- Add one open-collector comparator channel for 5V fast-trip (starter: LMV393-family channel at 3.3V logic domain).
- Add one 5V threshold network (starter: fixed divider plus trim option) to set instantaneous trip current.
- Add one input blanking RC (starter: 1k series + 1nF to GND at comparator input) to suppress switching-edge false trips.
- Add one comparator-output isolation/injection element into `FAULT_CRITICAL_SUM` so 5V analog trip can assert shutdown immediately without disturbing 3.3V/Adj enable domains.
- Add two pilot test points: `TP_OCP_5V_SENSE` (comparator sense input) and `TP_OCP_5V_TRIP` (comparator output to fault path).

**5V comparator slice wiring contract (place this one first)**:
- Sense input: take the 5V current-sense differential from the existing R13/R14 shunt network.
- Comparator channel: use only one channel of the quad comparator for this slice; leave the other three channels unplaced or DNP until the 5V slice passes.
- Sense conditioning: place the 1k series resistor and 1nF blanking capacitor right at the comparator input.
- Threshold reference: create a dedicated trip reference for this channel only; do not share the threshold node with the other rails yet.
- Output path: wire the open-collector output to the existing fault net through the planned isolation/injection element, then let the existing fault pull-up define the idle state.
- Verification: populate `TP_OCP_5V_SENSE` at the comparator input side of the blanking RC and `TP_OCP_5V_TRIP` at the comparator output side of the isolation element.

**5V slice starter component set**:
- 1 x quad comparator IC, but only channel A used for the first slice.
- 1 x 1k input series resistor.
- 1 x 1nF input blanking capacitor.
- 1 x threshold divider/trimmer network for the 5V trip point.
- 1 x output isolation element into `FAULT_CRITICAL_SUM`.
- 2 x test points (`TP_OCP_5V_SENSE`, `TP_OCP_5V_TRIP`).

**Current implementation snapshot (2026-05-20, HAT Rev-B)**:
- Comparator IC placed (`TLV1704AIPWR`) and powered from +12V/GND.
- Two comparator outputs (`OUT1`, `OUT2`) now assert `FAULT_CRITICAL_SUM` directly (global trip behavior confirmed).
- Input blanking parts are present for first two channels (1k + 1nF per channel).
- Threshold trim network parts are present for first two channels.

**Next component placement batch (continue from here)**:
- Add explicit output isolation options for comparator outputs into `FAULT_CRITICAL_SUM` (DNP-friendly):
	- Starter option: one small-signal diode per comparator output to fault net.
	- Alt option: one small series resistor per output if diode isolation is deferred.
- Add dedicated pilot test points with final names:
	- `TP_OCP_5V_SENSE` on the comparator-input side of the RC sense node.
	- `TP_OCP_5V_TRIP` on the comparator-output/fault-injection node.
- Park unused comparator channels (U8C/U8D) in a deterministic state:
	- Tie each unused `+IN` and `-IN` to a quiet reference (GND preferred for now).
	- Leave `OUT3`/`OUT4` not connected to fault net until those channels are intentionally used.

**5V pilot pass criteria before cloning to 3.3V and Adj**:
- Fast short/over-current event on 5V rail asserts shutdown path without cross-coupled gate behavior on other rails.
- No false trips during expected load-step transients after RC blanking tuning.
- Trip threshold is measurable and repeatable across bench temperature and supply variation.

**Integration rule**:
- Keep INA `CRITICAL/WARNING` as telemetry and slower supervisory path.
- Add analog comparator trip as the instantaneous protection path that can assert disable immediately.

**Protection policy decision (2026-05-20)**:
- Instantaneous analog over-current trip is treated as a system-level critical event, not a single-channel local event.
- Any rail asserting analog trip must drive `FAULT_CRITICAL_SUM` and force global shutdown.
- Rationale: once current exceeds controllable behavior, fail-safe full shutdown is preferred over selective rail isolation.

**Comparator selection shortlist (recommended order)**:
- Option A (preferred): TLV1704 (quad, open-drain outputs, wide supply range, single IC covers 3 rails + 1 spare channel).
- Option B: LM339 family (quad, open-collector outputs, common and widely available, good for wired-fault integration).
- Option C: LMV339 family (quad, low-voltage focused, suitable when comparator domain is fixed at 3.3V).

**Package decision guidance**:
- Use one quad comparator IC for the whole board (3 active channels + 1 spare/latch channel).
- If placement density is tight, choose TSSOP-14 or SOIC-14.
- If hand rework is expected during bring-up, prefer SOIC-14 for easier probing and replacement.

**Minimum electrical requirements for selected part**:
- Open-drain or open-collector outputs (needed for safe OR/injection into fault path).
- Input common-mode range that includes the sensed shunt-comparator operating range.
- Propagation delay fast enough for intended instantaneous trip behavior.
- Stable operation at chosen comparator supply rail (3.3V domain preferred for MCU/fault logic compatibility).

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

### 15. MPU Change Plan: Active Migration Closure Workflow
**Status**: 🟡 In Progress (active pre-freeze workflow)  
**Path**: 📍 Redesign (platform transition planning)  
**Priority**: High (Rev-C pin-freeze dependency)  
**Description**: MPU migration is now an active closure gate for Rev-C. Use STM32 Blue Pill as the initial target while keeping the plan non-constraining: escalate to a larger STM32 if objective closure gates fail.  
**Current Decision**: Hardware-only migration planning is in scope now. Firmware rewrite remains out of scope for this document phase.  

**Locked decisions (2026-05-24):**
- Start with STM32 Blue Pill for mapping and footprint planning.
- Keep migration non-constraining: permit scale-up to larger STM32 if required.
- Include both UART and SWD paths in hardware.
- Keep Arduino-style firmware direction for first software phase (tracked, not implemented here).

**Required closure outputs before Rev-C pin freeze:**
1. STM32 interface remap table from ESP32-C6 logical functions to candidate STM32 pins.
2. Voltage-domain compatibility table for all migrated MCU-facing nets.
3. Boot/program/debug contract with both UART and SWD connectivity.
4. MCU-critical connector/net assignment rules aligned with mixed dual-connector policy.
5. Explicit pin-freeze readiness decision: PASS, HOLD, or FAIL with rationale.

**Scale-up trigger gates (Blue Pill -> larger STM32):**
1. GPIO budget fails after assigning all must-have nets.
2. Required peripheral placement conflicts cannot be resolved without high-risk pin reuse.
3. Reset/default-safe state requirements for rail control and fault response are not met.
4. UART+SWD debug/program path cannot be completed without conflicts.

**Connector-pin assignment rules for MCU-critical nets:**
1. Keep `FAULT_CRITICAL_SUM` on a low-noise route with adjacent reference ground and no long detours.
2. Keep I2C pair and its quiet return grouped to preserve measurement integrity.
3. Keep `ISET_MPU_*` control lines away from noisy power-switching regions at connector fanout.
4. Reserve access for SWD and UART pins so they are not consumed by optional features.

**Pin-freeze readiness checklist (Issue 15 closure):**
1. All mandatory MCU logical functions are mapped and conflict-free.
2. Voltage levels and pull ownership are valid at reset and fault conditions.
3. UART and SWD paths are complete and test-accessible.
4. MCU-critical net allocation fits connector policy and quiet-ground constraints.
5. Scale-up recommendation recorded (stay Blue Pill or move up) with evidence.

**Progress snapshot (2026-05-24):**
- Closure outputs 1-4 are now documented in `docs/PCB_REDESIGN_PREP_2026-05-20.md` under Phase 10 (hardware-only migration baseline).
- Output 5 remains open with one identified blocker: **KERC-04 reset-safe startup evidence**.
- Execution checklist in Phase 10.6 is now mostly closed: `KMCU-01..07` are complete and `KERC-01/02/03/05/06` are promoted based on latest ERC evidence.
- Latest ERC evidence remains warning-only (`Errors 0`, `Warnings 50`) and no longer blocks pin-conflict, SWD, UART, fault, or I2C integrity gates.
- Remaining hold is validation-only (bench/firmware): prove rail-control defaults remain inactive through reset/early boot and assert only after controlled firmware init.
- U7 symbol identity is now normalized to local Blue Pill migrated symbol naming in the active HAT schematic; remaining hold is bench evidence only.
- Bench execution reference: `docs/KERC-04_RESET_STARTUP_VALIDATION.md`.

**Issue 15 gate result:** [HOLD - KERC-04 only] (reset-safe startup evidence pending)

---

## Summary: Issues by Path

### 📍 Redesign Path Issues (PCB Next Iteration)

| Issue | ID | Priority | Action |
|-------|----|-----------|----|
| Feedback divider before MOSFET | RB-001 | High | Topology implemented on Rev-B; validate handoff continuity and retune only if needed |
| USB Vbus isolation on HAT | RB-002 | High | On hold for current MPU path; re-open only if MPU migration exposes VBUS on-board |
| TS5A3157 footprint correction | RB-009 | High | ✅ Locked to Package_TO_SOT_SMD:SOT-363_SC-70-6 on Rev-B |
| MOSFET gate drive characterization | RB-003 | High | Scope transient; optimize pull-up/down resistors |
| Rail protection (fuses/TVS) | RB-006 | Medium | Add SMD fuses and/or TVS diodes per output |
| Bootstrap split net labels | 12 | High | ✅ Rev-B naming normalized to `+5V_Boot`; reduced-contract gate closed, strict baseline kept for audit |
| Short-circuit feedback path | 13 | High | Digital INA path implemented; add analog instantaneous OCP comparator path for fast trip |
| Reference sensing architecture | 14 | High | Verified on Rev-B netlists; enforce Kelvin pair/return-locality rules during layout |
| MPU migration closure workflow | 15 | High | Active gate: close remap, level checks, UART+SWD path, and pin-freeze PASS/HOLD/FAIL |
| Terminal block labeling | RB-004 | Medium | Add silkscreen polarity + keyed connector |
| ESP32 footprint clarity | RB-005 | Medium | Verify variant match; update silk legends |
| Stack height/connector review | RB-007 | Low | Measure height; confirm alignment pins; contact rating |
| Buck output LED (optional) | RB-008 | Low | Design LED circuit; firmware pin assignment |
| INA current mapping (architectural) | 10 | High | Review HAT schematic rail-to-channel assignment; verify shunt BOM |

---

### 🔧 Bring-Up Path Issues (Continue on Rev 1)

| Issue | ID | Priority | Action |
|-------|----|-----------|----|
| USB Vbus workaround validation | RB-002 | High | Keep current safe testing procedure; no new hardware action unless MPU migration changes VBUS exposure |
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
| 2026-05-24 | Issue 15 promoted from deferred tracking to active migration closure workflow with explicit scale-up gates, connector assignment rules, and Rev-C pin-freeze checklist |
| 2026-05-24 | Issue 15 migration-label cutover pass completed on HAT schematic: all `*_DRAFT` U7 net labels promoted to final names; ERC baseline restored to warning-only (`Errors 0`, `Warnings 50`) while symbol-library swap remains deferred |
| 2026-05-24 | Issue 12 closed: Rev-B bootstrap naming normalized to `+5V_Boot`; connector verifier updated with `-BaselineFeedbackPolicy` (`reduced-policy` default, `strict` audit) and both Rev-B netlists re-exported after leading-space label cleanup |
| 2026-05-24 | Issue 15 gate narrowed to single blocker: KERC-04 reset-safe startup evidence; other KERC gates documented as closed by latest ERC-backed evidence |
| 2026-05-24 | Issue 15 symbol migration completion: active U7 symbol/lib_id in `DSP Regulator_hat.kicad_sch` renamed to local Blue Pill migrated identity; gate remains HOLD on KERC-04 bench startup evidence only |
