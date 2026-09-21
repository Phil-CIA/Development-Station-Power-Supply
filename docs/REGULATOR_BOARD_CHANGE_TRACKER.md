# DSP Regulator Board — Design Change Tracker

**Current State:** Rev 1 board built and bench-tested. First stacked power-up with HAT complete.  
**Purpose of this doc:** Design issues identified during bring-up. Logged for next PCB iteration. Workarounds provided for current development cycle.  
**Date Opened:** 2026-05-10  
**Reference:** Bench bring-up notes, stacked board power-up testing

---

## Status Legend
| Symbol | Meaning |
|--------|---------|
| 🔴 | Open — identified, needs design review and solution |
| 🟡 | In progress — workaround active, solution design in progress |
| 🟢 | Resolved — change committed to next-iter files |
| 🔵 | Won't Fix — accepted as-is for this revision |

---

## Electrical Design Issues

### RB-001 — LM2596 Feedback Path: Open Circuit When MOSFETs Off
**Status:** 🟡 In progress (workaround active)  
**Severity:** Medium (functional flaw, not data loss)  
**Found by:** Bench bring-up; 12V observed on LM2596 pin 5 (enable pin); scope trace of feedback divider  
**Board Impact:** Rev 1 built; physical hardware issue requiring workaround  
**Description:**  
The feedback divider network (sensing 5V output for regulation) is located **after** the output MOSFETs (Q1/Q2). When the MOSFETs are off (before firmware turns them on), the feedback node has no connection to the 5V output and floats to approximately the input voltage (12V) through the regulator's internal divider network.

When stacked boards first power up:
- LM2596 sees ~10.7V on feedback (12V input minus ~1.23V divider drop)
- Regulator attempts to produce output proportional to this: ~10.7V on 5V rail
- When MOSFETs finally turn on, the output MOSFETs short a ~10.7V rail through their RDS_on
- Current limiting engaged; rails collapsed to 9.3V @ 520mA bench limit

**Root Cause:** Feedback divider placement after load-switching MOSFET blocks steady-state regulation until load path is enabled.

**Current Workaround:**  
- Jumper (JP?) added to allow feedback to sense **before** the MOSFET (pre-MOSFET tap point)
- Voltage drop across MOSFET RDS_on (~tenths of a volt) accepted as calibration tuning margin
- Development continues; workaround validated in benchtop testing

**Proposed Solution (next-iter):**  
Option 1: Move feedback divider sense point before output MOSFET in schematic  
- Pro: Instant feedback, proper regulation from power-on
- Con: MOSFET voltage drop adds to calibration offset (~0.1–0.3V typical @ light load)
- Recovery: Calibration NVS stores trim values; offset easily compensated in firmware

Option 2: Add pre-charge circuit (capacitive divider + small series resistor)  
- Pro: Feedback network pre-charged to ~5V before MOSFET enables
- Con: Adds BOM, increases complexity, power-up transient still present
- Timing: Needs detailed simulation to validate ramp rate

**Recommendation:** Option 1 (move sense before MOSFET) — minimal BOM change, calibration already handles offset.

**Design Owner:** TBD (power supply redesign cycle)  
**Next Step:** Review schematics; confirm feedback tap point relocation feasible in layout; prepare next-iter netlist change

---

### RB-010 — VSENSE_5V+ Open/Floating Dominance Risk In Feedback Selector
**Status:** 🟡 In progress — mitigation implemented in Rev-C, replacement op-amp bench validation pending
**Severity:** High (regulation and safety behavior risk)  
**Found by:** SPICE selector crossover and fault-case simulations (`hardware/sim/5v_reg_selector_sweep.cir`, `hardware/sim/5v_reg_selector_fault_cases.cir`)  
**Board Impact:** Rev-C routing blocker (must close before PCB routing starts)  
**Description:**  
The 5V selector path can be pulled by a floating `VSENSE_5V+` node if leakage or injected current raises that node above the local fallback setpoint. This can force `5V_reg` to follow a false-high remote sense condition.

Simulation checkpoint from current model set:
- `VSENSE_5V+` low/off: `5V_reg` remains near local fallback (~5.043V)
- `VSENSE_5V+` disconnected with no leakage: `5V_reg` remains near local fallback
- `VSENSE_5V+` disconnected with injected leakage: selector crossover occurs once the floating node exceeds fallback threshold
- With current 10Mohm bias model: crossover near 0.504uA (`5.043V / 10Mohm`)

**Root Cause:** Floating remote-sense node is not strongly constrained during open-line conditions; selector architecture currently allows the remote path to dominate once leakage/noise lifts that node above fallback.

**Rev-C mitigation selected (2026-08-13):**
1. Add 100kΩ from `VSENSE_5V+` to GND for a deterministic open-line state.
2. Replace the 12V-overstressed LMV358 selector with TLV9352 on a 12V supply.
3. Remove the direct bring-up bypass from the production feedback path.
4. Change the diode-compensated feedback resistor to 1.8kΩ.
5. Keep a DNP service-bypass footprint for recovery only.

Rev-B evidence:
- Removing the 12V-powered LMV358 reduced total powered-system input current from the 400mA current-limit state to approximately 71mA with five LEDs lit and STM32/CrowPanel communication active.
- With U5 removed, `+5V_Reg` jumpered to `VSENSE_5V+`, and the 1.8kΩ feedback resistor installed, the rail measured 4.848V. This is expected because the temporary jumper bypasses the selector diode drop.
- Full selector validation is blocked until the TLV9352 replacement arrives.

**Required Before Rev-C Routing:**
1. Confirm the implemented 100kΩ `VSENSE_5V+` pulldown and TLV9352 selector in the final netlist.
2. Re-run the two SPICE validation decks and record pass/fail:
	- `hardware/sim/5v_reg_selector_sweep.cir`
	- `hardware/sim/5v_reg_selector_fault_cases.cir`
3. Confirm schematic/netlist/ERC consistency after hardening edits.
4. Capture one bench validation plan for forced low/open-sense scenarios.

**Recommendation:** Treat this as a pre-routing electrical gate. Do not begin Rev-C routing until the selected mitigation demonstrates deterministic fallback behavior under low/open/leakage cases.

**Design Owner:** TBD (power supply redesign cycle)
**Next Step:** Correct all U5 TLV9352 BOM metadata, install the replacement device when available, and bench-check normal remote sense plus open-sense fallback before clearing the routing blocker.

---

### RB-012 — HAT OCP Comparator Measures Absolute Rail Voltage Instead Of Shunt Differential
**Status:** 🟡 In progress — Rev-C differential architecture implemented and ERC-clean; PCB layout and bench validation pending
**Severity:** High (overcurrent threshold accuracy and repeatability risk)
**Found by:** Rev-B schematic/netlist review during range-switch bring-up
**Board Impact:** HAT Rev-B protection path; Rev-C schematic gate
**Description:**
U3 does not measure the differential voltage across either 3.3V current shunt. U3A pin 5 is fed from the upstream side of R16 through R36, while U3A pin 4 is fed from the ground-referenced POT1 threshold through R37. U3B repeats this arrangement for R17, POT2, R41, and R40. The downstream sides of R16 and R17 connect to the output rail, but that voltage is not provided to the corresponding comparator channel.

The intended current signal is:

`VSHUNT = VUPSTREAM - VDOWNSTREAM = ILOAD * RSHUNT`

The implemented comparator instead tests the absolute upstream voltage:

`VUPSTREAM = VOUT + ILOAD * RSHUNT`

This can be adjusted to trip at one rail voltage, but the trip current then moves directly with output-voltage or threshold-reference variation:

`ITRIP = (VPOT - VOUT) / RSHUNT`

Rev-B bench evidence at approximately 0.339A illustrates the scale mismatch:
- R17 = 0.200Ω: shunt signal is approximately 67.8mV, riding on the 3.3V rail.
- R16 = 0.020Ω: shunt signal is approximately 6.78mV, riding on the 3.3V rail.
- A 100mV change in `VOUT` shifts the inferred threshold by 0.5A with R17 or 5A with R16.

**Root Cause:** The analog OCP concept called for differential shunt sensing, but the implemented comparator input network uses one shunt terminal and a ground-referenced potentiometer. A comparator alone cannot subtract the rail common-mode voltage while also applying an independent ground-referenced trip threshold in this arrangement.

**Rev-C Required Direction:**
1. Route Kelvin sense pairs from both terminals of every current shunt.
2. Feed each pair into a current-sense amplifier or equivalent differential front end whose common-mode range includes the rail voltage.
3. Scale the ground-referenced amplifier output so the expected shunt full-scale voltage uses a practical comparator/reference range.
4. Compare that output against a stable reference or DAC-derived threshold; do not derive precision OCP solely from a coarse potentiometer across `+5V_Boot`.
5. Verify amplifier offset, gain error, bandwidth, input filtering, and fault response at both the 20mΩ and 200mΩ range limits.
6. Keep the INA3221 telemetry path independent; it does not replace a fast hardware OCP path.

**Rev-B Disposition:** Do not credit U3 as a calibrated fast overcurrent protection function. Continue bench work under the external PSU current limit and retain the range-switch bypass until the switching audit is complete.

**Design Owner:** TBD (HAT Rev-C redesign cycle)
**First channel design (2026-08-14):** The original 3.3V high-current baseline used R16 = 20mΩ, INA180A2 gain 50, and a nominal 2.00A trip. See `docs/REVC_3V3_HIGH_RANGE_OCP_DESIGN_2026-08-14.md`. The implemented Rev-C schematic consolidates the four channels into dual INA2180A2 current-sense amplifiers and dual TLV1702 open-collector comparators. U2/U6 operate from `+3.3V Boot`; U3/U7 remain on `+12V`. ERC and the regenerated netlist are clean as of 2026-08-14.

**Mandatory PCB layout completion gate:** Do not finalize the Rev-C PCB or release fabrication outputs until all items below are complete.

- [ ] Verify IN+ and IN- polarity for all four shunt channels against regulator-side and load-side pads.
- [ ] Route each shunt as a true Kelvin pair; sense traces must not carry load current.
- [ ] Route each Kelvin pair together and away from switch nodes, MOSFET gates, inductors, and digital/fault edges.
- [ ] Place a 100nF bypass capacitor directly across VS-GND at each INA2180 (U2 and U6) and verify its net connectivity.
- [ ] Calculate INA2180 output range and saturation margin for every 20mΩ and 200mΩ channel on the 3.3V supply.
- [ ] Lock and document the trip current, reset current, threshold tolerance, and hysteresis for all four ranges.
- [ ] Decide whether calibrated potentiometers are acceptable or replace them with fixed/reference-derived thresholds.
- [ ] Verify INA2180 and TLV1702 package pin numbering, footprints, and symbol-to-footprint mappings.
- [ ] Keep amplifier-to-comparator traces short and place threshold, filtering, and hysteresis parts at the comparator inputs.
- [ ] Provide accessible test points for each amplifier output, threshold node, and comparator/fault output.
- [ ] Rerun ERC, export a fresh netlist, run PCB DRC, and audit the resulting connectivity before fabrication release.

**Next Step:** Complete this gate during PCB layout, then perform controlled bench validation before crediting the Rev-C OCP as calibrated protection.

---

### RB-002 — USB Vbus Backfeed: HAT/ESP32 USB 5V Fights Regulator 5V Rail
**Status:** 🟡 In progress (workaround active)  
**Severity:** Medium (current limiting during stacked operation)  
**Found by:** First stacked board test; HAT connected via USB for serial debugging; bench PSU current-limited immediately  
**Board Impact:** Rev 1 HAT built with USB connector directly on ESP32-C6 module; no Vbus blocking on HAT PCB  
**Description:**  
When the HAT board (with ESP32-C6 on-module USB connector) is powered via the regulator's 5V output AND connected to a USB host, two voltage sources appear on the 5V rail:

1. **Regulator output:** ~5V nominal (feedback divider at ~10.7V pre-MOSFET, outputs ~5V under load)
2. **USB Vbus:** ~10V (typical USB wall adapter under device load)

Since USB Vbus (~10V) is higher than regulator 5V, it back-drives the 5V rail. The two sources fight via Ohmic drops in PCB traces and the regulator's output impedance. This causes:
- Voltage sag on 5V rail
- Current limiting triggered (bench PSU defensive behavior)
- 520mA @ 9.3V observed on first stacked test

**Root Cause:** No isolation between USB Vbus and regulated 5V rail on HAT PCB. Standard ESP32 module USB passthrough directly tied to board 5V plane.

**Current Workaround:**  
- Disconnect USB 5V during stacked benchtop testing
- Use data-only USB cable (no power pins) if serial debugging needed during bench test
- Result: Clean 183mA idle current at 12.06V input verified

**Proposed Solution (next-iter):**  
Option 1: Add Schottky diode (e.g., SB230) in series with USB Vbus on HAT PCB  
- Pro: Simple; Vbus can power HAT if bench supply disconnected, regulator 5V blocked from back-driving
- Con: ~0.35V forward drop in USB path (slight reduction in USB charging current if used as backup)
- BOM: 1× Schottky diode, via on HAT near USB connector

Option 2: Isolate USB connector from regulated 5V; power HAT from 3.3V or separate USB 5V regulator  
- Pro: Complete USB/5V isolation, no conflicts
- Con: Changes power distribution topology; increases complexity

Option 3: Add USB Vbus monitor firmware; disable USB Vbus charging / monitor when bench supply active  
- Pro: No hardware change to HAT
- Con: Firmware complexity; doesn't prevent back-drive if powered off

**Recommendation:** Option 1 (Schottky diode on Vbus) — minimal invasive, proven topology, no firmware changes needed.

**Design Owner:** TBD (HAT redesign cycle)  
**Next Step:** Consult HAT schematics; identify Vbus trace routing; plan diode placement near connector; update netlist and BOM

---

### RB-003 — ON/OFF Pin Switching: MOSFET Gate Drive Circuit Issue
**Status:** 🔴 Open  
**Severity:** Medium (control reliability)  
**Found by:** Initial design review; bench testing confirmed issue  
**Description:**  
The ON/OFF control pin (LM2596 pin 5) uses an N-channel MOSFET for low-side pull-down. Gate drive voltage and switching transient behavior need formal review:

- What is the minimum rise/fall time acceptable for enable transient?
- Gate charge current from 3.3V/5V GPIO — is it adequate?
- Hysteresis or debounce needed on enable pin?
- Current design appears to have gate-to-source issues (related to RB-001); once feedback divider is fixed, re-verify MOSFET behavior

**Current State:** Board powers up and runs; switch behavior not yet characterized under load transients.

**Next Step:** Scope gate voltage, drain voltage, and load response during ON → OFF → ON cycles; document switching specs; consider pull-up/pull-down resistor optimization.

---

### RB-004 — Input Voltage Terminal Block: Labeling and Polarity Clarity
**Status:** 🔴 Open  
**Severity:** Low (user experience / assembly)  
**Description:**  
The input 12V terminal block (J1?) connector needs clear polarity marking:

- Silkscreen labels must clearly indicate +12V vs GND positions
- Physical keying or polarity guard preferred (e.g., keyed connector instead of open screw terminal)
- Assembly documentation should explicitly state wire gauge and current rating for each terminal

**Current Workaround:** None; users must refer to schematic or assembly guide.

**Next Step:** Review terminal block footprint; add silkscreen arrows and labels; consider upgrading to keyed connector type if BOM allows.

---

### RB-005 — ESP32 Footprint: Compatibility and Silk Clarity
**Status:** 🔴 Open  
**Severity:** Medium (assembly accuracy)  
**Description:**  
The ESP32 footprint on the regulator board (or if this applies to HAT, clarify which board):

- Silk labels for critical pins (GPIO, GND, 3.3V, etc.) — are they visible post-assembly?
- Footprint matches current ESP32 variant in use (this was originally designed for standard ESP32; bench testing used ESP32-C6)?
- Any rework needed for pin compatibility or mechanical fit?
- Mechanical clearance around USB connector adequate for user connections?

**Current Workaround:** Manual firmware environment selection; pin mapping is GPIO5/GPIO6 per HAT schematic (not footprint dependent).

**Next Step:** Verify footprint matches deployed variant; review silk legends; confirm mechanical clearance; update BOM/assembly notes if variant changed.

---

### RB-006 — Internal Voltage Rail Protection: OCP/OVP Fuses or Monitoring
**Status:** 🔴 Open  
**Severity:** Medium (fault tolerance)  
**Description:**  
The regulator board distributes three rails (5V, 3.3V, Adjustable) to the HAT. If a rail is shorted externally or internally:

- Does current limiting protect the power distribution traces?
- Are there any pre-fuses (e.g., SMD fuses or PTC resettable devices) on the 5V/3.3V outputs?
- Should there be individual rail supervision and fault logging?

**Current Workaround:** LM2596 internal OCP (current limiting) is the only protection; relies on bench PSU to trip main fuse.

**Next Step:** Review current draw under maximum specified load for each rail; determine if SMD fuses or TVS protection diodes should be added to each output; evaluate firmware-based rail monitoring thresholds.

---

### RB-007 — Stack Height Connector: Physical Fit and Viability
**Status:** 🔴 Open  
**Severity:** Low to Medium (mechanical integration)  
**Description:**  
Current HAT stacks on regulator board via a connector (type?). Questions:

- Is the connector height (mechanical stack height) acceptable for the intended enclosure?
- Are there standoffs or alignment guides to prevent board rocking during insertion/removal?
- Pin count and contact pressure adequate for the current levels observed (183mA idle, planned higher under load)?
- Is the connector type field-replaceable, or is it soldered?

**Current Workaround:** Boards stacking works on bench with care; no mechanical feedback yet.

**Next Step:** Measure final stack height with connector; review enclosure mechanical constraints; confirm connector contact rating; evaluate alignment pins/slots.

---

### RB-008 — LED on Buck Converter Output: Status Indication (Under Consideration)
**Status:** 🟡 In progress (design consideration)  
**Severity:** Low (user feedback / debugging aid)  
**Description:**  
Proposal: Add a small LED (or RGB LED) on one of the buck converter outputs (likely 5V for visibility) to indicate:

- Power-on status (LED on when 5V present)
- Fault conditions (LED blink pattern for OCP, OVP, etc.)
- Optional: current-limit mode indication (slow blink vs. steady)

**Rationale:** Benchtop debugging and user visibility; helps diagnose power issues at a glance.

**Considerations:**
- Which output to monitor? (5V most visible; Adjustable varies)
- LED current draw (typically 1–2 mA @ 2V drop)
- Resistor sizing (standard ~1kΩ series resistor for ~2mA @ 5V)
- Firmware complexity: simple on/off vs. blink patterns for fault states
- Visual clarity: through-hole LED (visible), SMD LED (compact), or indicator on display?

**Current Workaround:** Multimeter or oscilloscope used to verify power rail status.

**Next Step:** Decision on feature desirability; if yes, choose LED type and mounting location; add firmware output pin for LED drive; update schematic and BOM.

---

### RB-009 — TS5A3157-DCKR Footprint Error
**Status:** 🔴 Open  
**Severity:** High (assembly and functional risk)  
**Description:**  
The footprint used for TS5A3157-DCKR is incorrect for the actual package pinout and/or land pattern.

- Potential for rotated/misaligned placement during assembly
- Risk of non-functional analog switch path due to incorrect pin mapping
- Rework likely required on existing prototypes if this part is populated

**Current Workaround:** Manual rework/bodge on prototype builds where needed.

**Next Step:** Verify against TI datasheet package drawing, correct footprint in KiCad library, and re-check symbol-to-footprint pin mapping before next layout spin.

---

### RB-011 — 3.3V Regulator Output Depends On R25 In Selector/Reference Path
**Status:** 🟡 In progress — redesign bench-proven, Rev-C cleanup and fallback check pending
**Severity:** High (rail setpoint accuracy and overvoltage risk on 3.3V path)  
**Found by:** Rev-B bypass bench validation (2026-08-11)  
**Board Impact:** Rev-B reworked successfully; Rev-C selector redesign implemented but not released for routing
**SPICE Model:** `hardware/sim/3v3_reg_selector_ref_v4.cir` (validated 2026-08-12)

**Description:**  
Bench measurements show a strong dependency of `+3.3V_Reg` setpoint on `R25` participation in the feedback selector/reference network around U6 and D5.

Observed behavior during no-load bring-up:
- `R25` removed/not effective: `+3.3V_Reg` rises to approximately `3.52V`
- `R25` installed/effective: `+3.3V_Reg` returns near target at approximately `3.335V`

**Corrected root cause and intended operation (2026-08-13):**
R25 was a temporary bring-up connection, not the intended production DC-feedback path. The intended selector is:

```
remote sense --> buffer --\
						   BAT54C selector --> corrected divider --> LM2596 FB
local fallback --> buffer -/
```

Two defects made the intended path fail with R25 removed:
1. `VSENSE_3V3+` floated when its HAT-side path was open.
2. The original divider did not compensate for the selector diode drop.

**Bench-proven Rev-B correction (2026-08-13):**
- Remove R25 from the active path.
- Add 100kΩ from the remote-sense input to GND.
- Change the 1kΩ upper feedback resistor to 820Ω while retaining the 680Ω series resistor and 1kΩ lower resistor.
- Increase the local-fallback attenuation so the remote branch wins during normal operation.

At approximately 300mA load:
- pre-switch rail = 3.3198V
- load-side output = 3.3203V
- selector remote anode = 3.3201V
- selector fallback anode = 3.2204V
- selector cathode = 3.0954V
- LM2596 FB = 1.2391V
- remote branch priority = 99.7mV

This demonstrates correct remote-sense regulation with R25 removed. The remaining fallback test only needs to prove a stable in-range rail; exact fallback calibration is not required.

**Current Workaround (bench):**
1. Continue with the proven 100kΩ pulldown, 820Ω divider correction, and R25 removed.
2. Treat loss of remote-branch priority or output outside the accepted 3.3V range as HOLD.

**Required Closure Work (next revision):**
1. Correct TLV9352 symbol/BOM metadata in Rev-C.
2. Confirm both op-amp channels and the selector diode belong to the same rail.
3. Keep any direct bypass footprint DNP and label it service-only.
4. Perform one open-remote-sense fallback check and confirm the rail remains stable and in range.

**New bench evidence (Rev-C continuation, 2026-09-21):**
With R11 and R35 removed as a tracked bypass state, first-power continuation shows rails out of expected range despite low input current.

Measured:
- VIN about 12.032V
- +5V rail about 4.010V
- +5V_reg about 4.08V
- +3.3V rail about 0.0633V
- +3.3V_Reg about 0V
- Input current about 25mA with about 20mA attributed to indicator paths

Interpretation for tracker status:
1. This does not satisfy RB-011 closure criteria for a stable 3.3V class output.
2. +5V_reg being below nominal while 3.3V is collapsed is consistent with a partial feed or backfeed state, not validated normal regulation.
3. Open-sense fallback verification for RB-010 remains pending by explicit bench deferral.

Action note:
Treat this as a HOLD state for Rev-C bring-up. Resolve the present +5V_reg feed path and 3.3V collapse before attempting stacked tests or RB-010/RB-011 closure.

Additional forced-enable evidence (same date):
1. With U2/U4 pin 5 manually grounded, both buck cores started and remained stable.
2. U2 readings: pin1 about 11.09V, pin2 about 5.0736V, pin4 about 1.2544V while board V_out+5 remained about 4.011V.
3. U4 readings: pin1 about 11.89V, pin2 about 3.428V, pin4 about 1.2347V while board V_out+3.3 remained about 0.063V.
4. Input current change was small (brief about +10mA jump, then about +3mA to +4mA over baseline).

Implication:
Primary buck control loops appear functional when enabled; failure remains in downstream rail distribution/select path to output nodes.

**Design Owner:** TBD (power supply redesign cycle)
**Next Step:** Finish Rev-C metadata/netlist cleanup and capture the brief fallback check when the bench is available.

**Execution Checklist (RB-011):**
- [x] Build SPICE model of 3.3V selector/reference path — `hardware/sim/3v3_reg_selector_ref_v4.cir` with bench node voltage validation
- [x] Identify R25 as a temporary bring-up bypass rather than the intended production path.
- [x] Validate model against bench: Case A V_OUT error 1.4%, D5K error 0.8%; Case B no-equilibrium prediction matches bench overvoltage behavior.
- [x] **Capture direct node voltages with bench probe (2026-08-12):** D5K, U6A_OUT, U6B_OUT, V_OUT measured for both R25 states. **R15 confirmed unpopulated** (VSENSE_3V3- floating to GND).
- [x] Bench validate remote operation with R25 removed, 100kΩ pulldown, corrected divider, and approximately 300mA load.
- [ ] Bench validate open-sense fallback remains stable and in range.
- [ ] Close RB-011 after Rev-C netlist/BOM cleanup and fallback evidence.

---

## Summary of Next-Iter Deliverables

| Issue | Next-Iter File | Action | Priority |
|-------|-----------------|--------|----------|
| RB-001 (Feedback path) | `dsp-regulator-next-iter/` | Move feedback divider before MOSFET | High |
| RB-002 (USB Vbus backfeed) | `dsp-regulator-hat-next-iter/` | Add Schottky diode on Vbus; update netlist | High |
| RB-003 (ON/OFF MOSFET) | `dsp-regulator-next-iter/` | Scope gate drive; optimize pull-up/down | Medium |
| RB-004 (Terminal labeling) | `dsp-regulator-next-iter/` | Add polarity silk + keyed connector | Medium |
| RB-005 (ESP32 footprint) | TBD (regulator or HAT) | Verify variant match; update silk legends | Medium |
| RB-006 (Rail protection) | `dsp-regulator-next-iter/` | Add SMD fuses / TVS diodes per rail | Medium |
| RB-007 (Stack connector) | TBD (mechanical review) | Measure height; confirm alignment pins | Low |
| RB-008 (Buck output LED) | `dsp-regulator-next-iter/` | Design review + firmware pin assignment | Low |
| RB-009 (TS5A3157 footprint) | `dsp-regulator-next-iter/` | Fix footprint and symbol pin mapping | High |
| RB-010 (VSENSE open/floating dominance) | `dsp-regulator-next-iter/` | Harden VSENSE_5V+ fallback behavior and validate by SPICE before routing | High (Rev-C routing blocker) |
| RB-011 (3.3V selector/reference dependency on R25) | `dsp-regulator-next-iter/` | Remove ambiguity in 3.3V feedback selector path; keep-or-redesign decision with bench revalidation | High |

---

## Revision History

| Date | Event | Author |
|------|-------|--------|
| 2026-05-10 | Tracker created; RB-001 and RB-002 opened | Bench bring-up session |
| 2026-05-10 | Added RB-003 through RB-009 from bench bring-up findings | Bench bring-up session |
| 2026-08-10 | Added RB-010 from SPICE fault-case results; marked as Rev-C routing blocker | Simulation and handoff session |
| 2026-08-11 | Added RB-011 from bypass bench evidence; conditional keep-bypass rule recorded | Bench validation session |
| 2026-08-13 | Corrected RB-011 interpretation; recorded remote-sense redesign bench pass and RB-010 TLV9352 plan | Bench validation and Rev-C schematic session |
| 2026-08-14 | Implemented dual INA2180A2/TLV1702 four-channel Rev-C OCP, corrected supply and open-collector definitions, validated ERC/netlist, and added mandatory PCB completion gate | Rev-C OCP design session |
| 2026-09-21 | Logged Rev-C first-power continuation HOLD: +5V_reg about 4.08V, +5V about 4.01V, +3.3V rails collapsed, with R11/R35 removed bypass state | Rev-C bench continuation session |
| 2026-09-21 | Added forced-enable evidence: U2 and U4 cores regulate at IC pins when pin5 is grounded, but board output nodes remain out of range | Rev-C bench continuation session |
| TBD | Design review and next-iter file creation | TBD |
