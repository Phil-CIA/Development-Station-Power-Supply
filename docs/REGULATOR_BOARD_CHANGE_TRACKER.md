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

**2026-09-21 release check:** A fresh KiCad 10.0.5 run on the Rev-C regulator reported 326 ERC violations and 211 PCB DRC violations plus 470 unconnected items (DRC mix: 153 footprint-library, 36 clearance, 10 silk-overlap, 7 silk-over-copper, 3 silk-edge-clearance, 2 copper-edge-clearance). The commands exited 0 only because `--exit-code-violations` was not supplied. Rerun with violation exit codes before any further fabrication release.

**Design Owner:** TBD (power supply redesign cycle)
**Next Step:** Correct all U5 TLV9352 BOM metadata, install the replacement device when available, and bench-check normal remote sense plus open-sense fallback before clearing the routing blocker.

---

### RB-012 — HAT OCP Comparator Measures Absolute Rail Voltage Instead Of Shunt Differential
**Status:** 🟡 In progress — Rev-C differential architecture implemented; 2026-08-14 clean ERC superseded by 2026-09-21 findings; PCB layout and bench validation pending
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

**2026-09-21 release check:** A fresh KiCad 10.0.5 run on HAT Rev-C reported 82 ERC violations and 161 PCB DRC violations plus 129 unconnected items (DRC mix: 94 footprint-library, 48 dangling vias, 18 hole-clearance, 1 footprint mismatch). This supersedes the 2026-08-14 clean ERC checkpoint. Rerun with violation exit codes before any fabrication release.

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

**Latest bench evidence (2026-09-27):**
- U2 VIN: 11.44V
- U2 pin 5 (`Q3-D`): 0V
- `+5V_REG`: 5.0778V
- `SRC_5_HIGH`: 4.39V
- `GATE_5_HIGH`: 5.13V
- `+5V_HI_RANGE`: 4.000V
- `V_OUT +5V`: 4.000V
- Derived check: `VGS` ≈ 0.74V (`5.13V - 4.39V`)

**Interim interpretation:** The measured gate-source headroom is too small for robust enhancement at this operating point, so the high-side path may remain only weakly on (or behave as if off under load). Continue pinout/orientation and gate-drive-path validation before closing RB-003.

**Revision/source reconciliation status (2026-10-07):**
- **Operator-confirmed fault board identity (issue #62):** Regulator Rev-C physical board.
- **Authoritative source now staged in this PR:** `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net` copied from operator attachment (design source path in file header points to OneDrive Rev-C schematic; export date `2026-10-07T05:31:09`; tool `Eeschema 10.0.6`).
- **Regulator Rev-C namespace from this export (do not mix with older exports):**
  - `Q3` and `Q9` are BSS138 ON/OFF-control devices.
  - `Net-(Q3-D)` ties `Q3.3` and `Q6.3` to `U2.5`.
  - `Net-(Q12-D)` ties `Q9.3` and `Q12.3` to `U4.5`.
  - The 5V high-pass pair is `Q1/Q2` (PSMN5R2-60YL) with `U9` (LM74502DDFR) driver path.
  - `Gate _5_High` (note spacing) is Q1/Q2 gate net via `R75.2`; `SRC 5_HIGH` is Q1/Q2 source-side net including `U9.8`.
  - `+5V_reg` connects Q1 drain (`Q1.5`); Q2 drain (`Q2.5`) feeds shunt `R7` then `L1` to `V_out +5V` (`J1.2` path).
  - `+5V_Hi_Range ` (trailing space) is a filtered sense net, not the direct power-transfer node.
- Exact **installed assembly population** and probe-point correspondence to this export remain **operator-verified pending** at bench.
- **Rule for ongoing issue #62 evidence:** do not merge designators across board revisions or screenshots until the operator identifies the exact board revision and source file used for each measurement.

**Next operator bench checklist (identity-gated, no hardware modifications):**
1. **Identify source before power-on:** record board silkscreen revision and photo of the measured board area; attach the exact design source path used for designator mapping (for example one netlist file path + export timestamp).
2. **Confirm installed assembly vs export before interpretation:** visually verify that the measured area/populated parts match this netlist’s designators and part families (`Q1/Q2/U9/R75/R7/L1` path). If mismatch is found, stop and mark `IDENTITY BLOCKED`.
3. **Map probe nodes using exact exported net names:** include `+5V_Boot` (U9 VS), `ESP- GPIO 5V Hi` (U9 EN_UVLO pin 1 net), `Net-(U9-VCAP)` (U9 pin 4), `Net-(U9-OV)` (U9 pin 7 divider), `Gate _5_High`, `SRC 5_HIGH`, `+5V_reg`, `R7.2`/post-shunt node, and `V_out +5V`. Keep shorthand labels (`GATE_5_HIGH`, `SRC_5_HIGH`, `+5V_HI_RANGE`) marked **provisional mapping** until physically confirmed.
4. **Capture conservative ON/OFF table only (no rework):** with unchanged wiring and conservative PSU current limit, record the node set above in both commanded states under the same load condition.
5. **Capture control-state evidence only, not inferred polarity:** log the exact command/state readback used for each row; do not infer firmware polarity or root cause from netlist alone.
6. **Flag unresolved identity immediately:** if any measured label cannot be traced to the selected source without cross-revision inference, stop interpretation at that point and log as `IDENTITY BLOCKED`.

**Evidence package required before any closure decision:**
- Board identity photo(s) and revision marking.
- One authoritative design source reference (file path + export date/time).
- Node-to-source mapping table (including any `UNRESOLVED` rows).
- OFF/ON voltage capture table for the mapped node set.
- Raw command/state logs corresponding to each voltage capture row.
- Photo/notes confirming installed assembly points used for each probe in the `Q1/Q2/U9/R7/L1` path.

**Issue #62 closure state (user decision, 2026-10-07):** **Closed as functionally resolved for the original pass-through symptom** based on bounded operator bench evidence now captured in PR #63 docs.
- Closure basis recorded by operator includes:
  - Four commanded range paths reported working ON/OFF in operation-first bench flow.
  - CH1 HIGH loaded voltage points recorded at `5.005V` (10 ohm load) and `4.9497V` (5 ohm load).
  - CH2 HIGH loaded voltage point recorded at `3.3141V` (10 ohm load).
  - Concurrent CH1+CH2 operation recorded with panel/PSU values and confirmed return to near-zero output with final `p0=0x00` OFF state.
- Root cause and corrective-action mechanism remain **not established**; closure is functional/operational against the original symptom, not a design-root-cause proof.
- **Measurement-point clarification (operator, 2026-10-14):** the loaded voltage figures above were measured across resistors on breadboards, not verified directly at the supply output terminals. Retain the original log values, but do not treat them as terminal-regulation measurements. Issue #100 must begin by measuring directly at the output terminals and reconciling the active voltage calibration.

**Current posture update (2026-10-07, user-directed):**
- User reports hardware now **appears operational** and wants operation testing resumed first.
- This report is **not independently bench-verified in this session**.
- Active draft operation plan and execution prompt are tracked in:
  - `docs/firmware-buckets/bucket-2-rail-control-scope.md`
  - Section: **Regulator Rev-C operational acceptance plan (draft, 2026-10-07)**
- #62 is now closed by user direction; keep PR #63 open for documentation cleanup/follow-on evidence only.
- Remaining follow-up gaps (Bucket 2 scope), including current/power telemetry consistency, loaded LOW coverage, protection behavior, and transient/coldboot characterization, are tracked as separate work and do **not** block #62 closure.

**Next Step:** Execute the operation-first draft plan with operator-approved limits and capture evidence; keep troubleshooting scope exception-only and user-authorized.

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

### RB-013 — Rev-C Output Gate Drivers (U9/U14/U15/U16): Wrong LM74502 Variant Populated
**Status:** 🟡 In progress (schematic fixed; driver rework done 2026-10-06; post-rework verification pending)
**Severity:** High (output MOSFETs cannot reach full enhancement)
**Found by:** Bench measurement of Q1 gate drive on Regulator Rev-C (`Vgs≈1.76V`, `VCAP=12.43V`, `Gate-to-GND=5.13V`, `OV≈0.0044V`), traced through schematic/PCB/net cross-check

**Description:**
Q1/Q2/Q4/Q5 (and the other `PSMN5R2-60YL` output switches) on the Regulator Rev-C board are driven by four `LM74502`-family ideal-diode/high-side controllers: `U9`, `U14`, `U15`, `U16`. All four were populated/specified as the base **`LM74502DDFR`** part, which per TI's datasheet has only **60-µA peak gate drive source capacity**. The correct part for this design intent is **`LM74502HDDFR`**, which provides **11-mA** peak gate drive source capacity (~180x more).

With only 60µA available, the existing 100kΩ gate-source bleeder resistor on each channel (and other minor leakage) is enough to prevent the gate from ever charging past a low equilibrium voltage, even though the driver's charge pump (`VCAP`) is confirmed healthy and boosted (12.43V measured, well above `VS`≈+5V_Boot). This produced weak, partial FET turn-on (measured `Vgs`≈1.76V, far below the ~4.5–10V this MOSFET needs for its rated 21mΩ RDS(on)), even with the channel commanded ON and open-circuit (no load).

**Confirmed NOT the cause:**
- Charge pump / VCAP — working correctly (12.43V observed)
- OV pin — reads ≈0V, no overvoltage fault condition
- Gate-source bleeder resistor value (100kΩ) — correct for its intended purpose, just incompatible with the weak-variant driver's tiny gate current budget

**Fix applied this session:**
- Updated `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch`: `Value` property changed from `LM74502DDFR` → `LM74502HDDFR` on `U9`, `U14`, `U15`, `U16`.

**Still open:**
- [ ] Update the `LCSC` BOM field on all four (currently `C3236215`, which is the wrong-variant part) once the correct LCSC SKU for `LM74502HDDFR` is confirmed.
- [x] Physically rework all four driver ICs on the bench board (same DDF-8/TSOT-23-8 footprint; drop-in swap). Physical spares on hand marked `LM502` are the wrong (base) variant — order/verify parts marked with an `H` suffix (e.g. `L502H`) before installing.
  - 2026-10-06 bench rework: replaced U9, U14, U15, and U16; reversed D2, D5, D9, and D13. Record the installed driver marking to confirm the `H` variant.
- [ ] Re-measure `Vgs` per channel after rework to confirm full enhancement is reached.
- [ ] While reworked, confirm the gate-source zener clamp value/orientation per channel (referenced in prior HAT Rev-B notes) — this determines the practical Vgs ceiling even once gate current is no longer the bottleneck.
- [ ] Confirm no other boards/sheets in the repo carry the same `LM74502DDFR` substitution.

**Related but distinct:** RB-003 (LM2596 enable-pin low-side MOSFET drive) is a separate, unrelated gate-drive path — do not conflate the two when reviewing gate-drive issues on this board.

**Design Owner:** TBD (bench rework)
**Next Step:** Under current-limited conditions, re-measure Vgs per channel and output/load behavior after the 2026-10-06 rework; confirm the D2/D5/D9/D13 orientation against the schematic.

---

### RB-014 — Issue #100: Verify Terminal VOUT, Voltage Calibration, and Feedback Tracking
**Status:** Closed by explicit operator acceptance, 2026-10-16 — no further voltage correction or investigation required for issue #100
**Severity:** Operator accepts observed performance for intended use; no precision-regulation certification claimed
**Related issue:** [#100](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/100)

**Final operator disposition (2026-10-16):** The operator clarified that the
resistor-run point labeled no-load was accidentally taken with the load
connected and should have been omitted. Exclude that mislabeled CH1 serial
point from no-load evidence; do not use the disputed pairing to assert a
validated no-load-to-load drop. The operator accepts the observed terminal/
display performance as sufficient for this project and explicitly directed
closing issue #100 rather than pursuing smaller discrepancies or additional
calibration constants. Keep BOTH voltage gains at 1.00000 and voltage offsets
at 0.00mV. Existing current gains/offsets remain unchanged; this decision does
not reset or validate current calibration.

This disposition supersedes the investigation/recapture next steps in the
historical entries below. No more tests, coefficient writes, or hardware
changes are requested for this issue. The remaining CH2 raw/panel pairing
limitations are retained as evidence limits, not closure blockers. Closure is
operator acceptance, not proof of a hardware repair, exact loaded regulation,
current accuracy, or protection function. Both paths were last reported OFF.

**Question:** Does the physical CH1 output-terminal voltage change under load, and if so, does the INA/firmware voltage report or remote-feedback path track the same node?

**Evidence correction:** The operator clarified on 2026-10-14 that the earlier `5.005V`, `4.9497V`, and related loaded voltage figures were measured across resistors on breadboards, not confirmed at the supply terminals. Preserve those values as historical load-resistor measurements; they do not establish terminal droop. `docs/CALIBRATION_WORKFLOW_2026-09-23.md` records a single-point calibration saved on 2026-10-07 (`5V vg=0.96642, voff=0.00mV`; `3V3 vg=0.97395, voff=0.00mV`), fitted against resistor/load readings rather than verified terminal readings. The currently installed config still needs read-only confirmation; neither the historical fit nor the stored values establish terminal-voltage accuracy.

**Read-only compact-profile capture (operator, 2026-10-15):** The installed
compact F103 build accepted `HELP`, `CFGSHOW`, and `CALSHOW`. Both readback
commands reported `cfg d9=ON`, `5V vg=0.96642 voff_mV=0.00 ig=7.40741
ioff_mA=0.00`, and `3V3 vg=0.97395 voff_mV=0.00 ig=7.08889 ioff_mA=0.00`.
The operator clarified that AW9523 P1.0 / D9 drives the board-to-board
communication indicator LED; `D9ON`/`D9OFF` and `cfg d9=...` are not CH1
rail-control or output-state evidence. The firmware README maps `Q2ON` /
`Q2OFF` to the AW9523 P0.1 Q2/Q8 CH1 +5V HIGH path; these commands are
available only in the full-bench profile.
`INAPROBE` (entered twice), `INANOW`, `INARAILS`, and `INADIAG` each returned
`cmd: unsupported in compact profile`; these commands are omitted from the
compact profile by design, so this is not evidence of an INA communication
failure. Startup status samples reported `Vin=11.400 Iin=120.00` and later
`Vin=11.384 Iin=124.44` (reported by firmware; not independently verified).
The capture establishes the calibration values currently read from this
controller but does not include an INA raw reading, direct terminal DMM
reading, firmware build identifier, or explicit range/output-state record.
No load or hardware changes were reported. The existing
`bluepill_f103c8_bench` profile built locally on 2026-10-15 using a temporary
short-drive mapping after the normal-path build failed while archiving the
Adafruit BusIO objects; no firmware was flashed during that local build.
The same build is now available in VS Code as **Issue #100: Build F103
full-bench (no upload)**; it selects an unused temporary drive letter so the
Windows workspace path with spaces does not reach the archiver. The separate
upload and COM7 monitor tasks are available. A later operator-relayed agent
report states that direct invocation of `scripts\run-f103-bench.ps1 -Action
Upload` returned `Programming Finished`, `Verified OK`, and `SUCCESS`;
the literal upload log was not supplied with the serial capture below.

**Full-bench read-only capture (operator-relayed agent log, 2026-10-15):**
Direct pyserial access to COM7 succeeded at 115200 baud, 8N1, with flow control
disabled and DTR/RTS inactive before opening. The PlatformIO monitor had
previously failed because the agent terminal had non-interactive stdin, not
because COM7 was inaccessible. No text arrived during the bounded initial
receive window; commands then returned responses. `HELP` listed the full-bench
commands, confirming that the diagnostic command set is running, but not the
exact flashed source commit or image hash.

`CFGSHOW` and `CALSHOW` both returned `cfg d9=OFF` and the same voltage/current
coefficients captured above. D9 had previously read ON; no cause for this
change is established by the supplied capture. It is not rail-state evidence.
`QSTATE` returned `p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0`,
showing all decoded AW range controls inactive, not a measured terminal state.
`INAPROBE` returned `ina: 0x40=ACK 0x41=ACK`. `INANOW` and `INARAILS`
reported output sense voltages near zero and the incoming monitor at
11.424V / 117.78mA. The literal raw diagnostic response was:

```text
ina raw 0x40 CH1 shunt=0x0000(0) 0.000mV bus=0x0008 0.008V Rused=200mOhm Icalc=0mA
ina raw 0x40 CH2 shunt=0x0000(0) 0.000mV bus=0x0008 0.008V Rused=200mOhm Icalc=0mA
ina raw 0x40 CH3 shunt=0x0000(0) 0.000mV bus=0x0000 0.000V Rused=200mOhm Icalc=0mA
ina raw 0x41 CH1 shunt=0x0000(0) 0.000mV bus=0x0000 0.000V Rused=200mOhm Icalc=0mA
ina raw 0x41 CH2 shunt=0x0000(0) 0.000mV bus=0x0000 0.000V Rused=200mOhm Icalc=0mA
ina raw 0x41 CH3 shunt=0x01A8(53) 2.120mV bus=0x2CA0 11.424V Rused=18mOhm Icalc=118mA
cal 5V vg=0.96642 voff_mV=0.00 ig=7.40741 ioff_mA=0.00
cal 3V3 vg=0.97395 voff_mV=0.00 ig=7.08889 ioff_mA=0.00
```

The register reads establish working diagnostic access in the inactive-control
state; they do not establish output-terminal accuracy, enabled regulation, or
the cause of the historical load-side voltage difference. `Rused=200mOhm`
records the firmware conversion assumption, not a physical shunt measurement;
zero shunt readings cannot validate current scaling. The agent reported no
reset, gate/output commands, calibration/config writes, rewiring, or loaded
tests during capture. No direct terminal DMM or simultaneous panel reading
was supplied in that inactive-control capture.

**Approved CH1 HIGH no-external-load measurement (operator-relayed agent
capture, 2026-10-15):** Operator confirmed external loads disconnected and
source current limiting in place; source setpoints were 12.0V / 1.0A. Meter
was an OWON XDM1041, reported 5V range. Source live numeric voltage/current
and CV/CC state, meter calibration/verification status, and exact measurement
timestamps were not supplied.

Pre-enable `CFGSHOW`/`CALSHOW` retained the same coefficients and `cfg d9=OFF`.
`QSTATE` was `p0=0x00 c0=0xC0` with all decoded controls zero. `Q2ON`
returned `range: Q2/Q8 forced ON via aw95xx P0.1 (p0=0x02)` and
`awdiag Q2/Q8 want=1 out=1 in=1 dir=OUT o=0x02 i=0xC2 c=0xC0`;
`QSTATE` confirmed only Q2 active. Enabled results:

| Measurement | Reported value |
|---|---|
| CH1 output-terminal DMM | 5.0467V |
| 0x40 CH1 raw bus | `0x13B8`, 5.048V |
| 0x40 CH1 raw shunt | `0x0038(7)`, 0.280mV; `Rused=200mOhm`, `Icalc=1mA` |
| `INANOW` / `INARAILS` CH1 HIGH | 5.048V / 1.40mA (uncalibrated conversion) |
| 0x40 CH2 raw bus / shunt | `0x13B8`, 5.048V / `0x0020(4)`, 0.160mV |
| Incoming INA monitor during enabled reads | 11.384-11.392V / about 126.67mA (firmware-derived, not source readback) |
| CrowPanel CH1 | 4.88V / 0.010A / 0.05W |
| CrowPanel fault indication | "OCP trip", reported by operator as pre-existing |

The raw INA bus value is 1.3mV above the terminal DMM reading at this
reported no-load point. The saved CH1 voltage gain 0.96642 applied to 5.048V
produces 4.87848816V, matching the displayed 4.88V after rounding. Thus the
stored voltage calibration explains the observed panel under-reading
(166.7mV below the DMM), without requiring a physical terminal drop of that
size. This single point does not establish accuracy across voltage/load or
resolve the historical load-side readings. Applying the stored current gain
7.40741 to the 1.40mA conversion produces about 10.37mA, consistent with the
displayed 0.010A; no independent current measurement validates either.

The "OCP trip" indication remains unresolved: the operator's pre-existing
classification does not prove a false trip or working protection. No further
energized or loaded test is approved by this record. Leave the existing
calibration unchanged pending a separately approved correction/verification
plan; do not reset all config or alter current coefficients based on this
no-load point.

**Authorized two-rail voltage correction (operator-relayed agent capture,
2026-10-15):** The operator clarified that the load-side measurements had not
been intended as terminal-voltage calibration references and directed that
both channels' affected voltage calibration be revisited. With all decoded
Q controls inactive (`p0=0x00 c0=0xC0`) and `cfg d9=OFF`, the agent captured
the previous coefficients, then sent each command once:

```text
CALSET 5V 1.00000 0.00 7.40741 0.00
CALSET 3V3 1.00000 0.00 7.08889 0.00
```

Both command responses and final `CFGSHOW`/`CALSHOW` reported:

```text
cfg d9=OFF
cal 5V vg=1.00000 voff_mV=0.00 ig=7.40741 ioff_mA=0.00
cal 3V3 vg=1.00000 voff_mV=0.00 ig=7.08889 ioff_mA=0.00
```

This removes the load-derived voltage gains (previously 0.96642 / 0.97395)
and preserves all reported current coefficients. `CALSET` invokes persistent
save immediately; command/readback showed no reported error, but no reboot
or independent flash readback was performed to verify power-cycle retention.
Unity gain / zero offset is an uncorrected voltage-reporting baseline, not a
new precision calibration. No post-correction enabled terminal/panel reading
was taken, and CH2 still lacks the corresponding no-load terminal/raw-INA
comparison. The current coefficients remain unchanged, not newly validated.

A preceding malformed serial-framing attempt sent literal backslash+n
characters; firmware rejected the pre-write commands and no writes occurred
in that attempt. The successful run used actual LF framing via
`command.encode("ascii") + bytes([10])`; `CFGSHOW` TX hex was
`43464753484f570a`. No `CFGSAVE`, output/gate controls, reset, build/flash,
rewiring, or loads were reported during the correction. Keep outputs inactive;
any further energized verification requires a new bounded operator approval.

For the preceding no-load measurement's shutdown, `Q2OFF` returned
`range: Q2/Q8 forced OFF via aw95xx P0.1 (p0=0x00)`
and `awdiag Q2/Q8 want=0 out=0 in=0 dir=OUT o=0x00 i=0xC0 c=0xC0`;
`QSTATE` returned all decoded controls zero, and 0x40 CH1/CH2 raw bus
returned to 0.008V with zero shunts. Shutdown is confirmed by register/sense
readback, not a post-shutdown terminal DMM measurement. The agent reported no
build, flash, reset, other controls, config writes, rewiring, or loaded tests.

**Post-correction no-external-load verification, one channel at a time
(operator-relayed agent capture, 2026-10-15):** Operator confirmed loads
disconnected and readiness before each enable. Meter was reported as
"OWON 1041", Auto DCV. Baseline `CFGSHOW`/`CALSHOW` confirmed both voltage
gains 1.00000, offsets zero, and unchanged current coefficients. Initial
`QSTATE` showed `p0=0x00 c0=0xC0`, all decoded controls inactive.

| Channel / enabled state | Terminal DMM | Raw INA HIGH bus | CrowPanel | Source report |
|---|---|---|---|---|
| CH1, Q2 only, `p0=0x02` | 5.0486V | 0x40 CH1 `0x13B8`, 5.048V | 5.05V / 0.010A / 0.05W; no fault indication | 12.09V / 183mA, CV; limit reported 1.1A |
| CH2, Q5 only, `p0=0x08` | 3.3216V | 0x41 CH1 `0x0CF8`, 3.320V in INADIAG; INANOW had 3.328V | 3.33V / 0.004A / 0.01W; no fault indication | 12.09V / 178mA; literal mode/limit description "cc 1.1A", ambiguous |

CH1 raw-minus-DMM difference is -0.6mV; CH2's INADIAG HIGH bus difference
is -1.6mV. CH2 HIGH was 3.328V in the preceding INANOW and 3.320V in
INARAILS/INADIAG, a single 8mV bus-conversion increment. The panel's 3.33V
is consistent with rounding a 3.328V sample, but no simultaneous transmitted
frame was supplied to establish which sample it displayed. Do not infer
an additional calibration correction from these asynchronous, quantized
readings. The large load-derived voltage under-reading is no longer present
at these no-load points; this is not a precision accuracy certification or
loaded-regulation result.

Before the run the source current limit was reported as 2.5A; measurement
reports instead describe 1.1A. Preserve this discrepancy and clarify the
actual limit and whether CH2 was truly in CC before proposing a loaded test.
Source readings above are operator reports, not independently captured
instrument data; exact timestamps and meter verification status remain absent.
Incoming INA voltages during enabled reads were approximately 11.336-11.368V,
lower than the reported source 12.09V; different measurement nodes/timing
and measurement accuracy are not resolved here. No causal conclusion follows.

`Q2ON` and `Q5ON` each acknowledged the expected AW9523 pin (P0.1 and P0.3)
with no SR fallback; QSTATE confirmed only the intended decoded control active.
`Q2OFF` restored `p0=0x00` before CH2 testing; 5V sense then read 0.024V.
Final `Q5OFF` restored `p0=0x00`, all decoded controls inactive; 3V3 sense
read 0.016V and 5V sense 0-0.008V. These are register/sensor shutdown
observations, not post-off terminal DMM readings. No fault indication was
reported in either enabled interval; the earlier OCP indication did not
recur in this run, but its cause and protection function remain unverified.
The operator's suggestion that CH1's 10mA is board-LED current is unverified;
no independent current measurement was supplied.

No build, flash, reset/power cycle, D9/CMD:OUTPUT controls, other Q controls,
config writes, or external loads were reported. Power-cycle calibration
retention remains unverified. Leave unity voltage gains and existing current
coefficients unchanged; both paths are now inactive. Further energized or
loaded testing requires separate bounded operator approval.

**Approved resistor-load report (operator-relayed agent attachment,
2026-10-16):** Agent reported nominal 10-ohm testing one channel at a time,
actual reported resistances/ratings below, unity voltage gains and unchanged
current coefficients. Measurements across the resistor were separate enabled
intervals, not simultaneous with terminal measurements.

| Channel | Reported resistor | No-load terminal | Loaded terminal | Terminal change | Resistor voltage, separate interval | Loaded panel |
|---|---|---|---|---|---|---|
| CH1, Q2 | 10.9 ohms / 10W | 5.0458V | 5.0156V | -30.2mV, about -0.60% | 5.0193V | 5.03V / 0.486A / 2.44W |
| CH2, Q5 | 11 ohms / 10W | 3.3183V (repeat) | 3.3139V | -4.4mV, about -0.13% | 3.3141V | 3.32V / 0.332A / 1.07W |

These DMM differences are arithmetic on the operator-reported intervals,
not an independently verified regulation specification/pass. The CH1
resistor voltage exceeds its loaded terminal reading by 3.7mV and CH2 by
0.2mV, but separate intervals cannot establish the sign or size of wiring
loss. Do not use resistor voltage as a terminal-calibration reference.

The report contains material evidence conflicts:
- CH1's section labeled "no-load ON" gives raw shunt
  `0x0A40(328)`, 13.120mV / 65.60mA, essentially the loaded reading,
  and incoming INA about 10.248-10.280V / 371.11mA. Applying the saved
  current gain predicts about 0.486A, not its table's no-load panel 0.010A.
  Its reported no-load source reading was 12.09V / 181mA, versus loaded
  12.09V / 450mA CV. Thus that serial block cannot be accepted as a
  paired no-load capture without reconciliation.
- CH2's section corrected to "loaded-terminal" and its resistor interval
  both retain `0x41 CH1 shunt=0x0018(3)` / 0.60mA, the same as the
  true no-load repeat. With gain 7.08889 this predicts about 0.00425A,
  not reported loaded panel 0.332A or resistor-interval panel 0.322A.
  Incoming INA is also near the no-load value (~122-127mA) while operator
  source readings increase from 177mA no-load to 300-301mA loaded.
  This is not reconciled by the supplied serial excerpts.
- Many blocks described as literal are abbreviated with `...`, so the
  complete raw capture and timestamps are not available in this attachment.
  CH2's first interval was relabeled loaded by the operator, then a no-load
  repeat was performed; preserve that chronology instead of assuming the
  original headings establish physical load state.

No fault was reported; source CV was reported for CH1 intervals and CH2
no-load/resistor intervals. CH2 loaded-terminal source mode was not supplied;
its reported timestamp was `10/16 @0343`. All other exact measurement times
remain absent. No conclusion about the cause of the lower incoming INA
voltage or current accuracy follows from these conflicting records.

The pasted Q2/Q5 command/register excerpts show the intended single-bit
enables and `p0=0x00` after OFF; final QSTATE has all decoded controls zero.
Agent reported both outputs OFF with near-zero INA sense, no config writes,
reset, build/flash, D9/CMD:OUTPUT commands, or other Q controls. Some shutdown
sensor excerpts are abbreviated, so distinguish reported shutdown sense
results from the complete literal final QSTATE.

At the time of this report, the proposed next action was offline reconciliation of the existing complete logs and
operator measurement notes, with no new bench action: identify each actual
no-load/loaded interval and pair raw reads and panel/DMM/source observations
by time. If the original logs cannot resolve the conflicts, mark those pairs
unusable and propose a separately approved targeted recapture. Do not change
calibration or close issue #100 from this report alone. This proposed work is
superseded by the final operator disposition above.

**Static design reference:** The 2026-10-07 Rev-C netlist records the CH1 power path
as `+5V_reg` through Q1/Q2, shunt R7, and L1 to `V_out +5V` at J1.2. It maps
CH1 HIGH to INA U10 channel 1 through `+5V_Hi_Range` / `-5V_Hi_Range` across
R7 (20mΩ); `VSENSE_5V+` is a separate remote-feedback net. The active STM32
firmware applies per-rail voltage gain/offset to the INA bus-voltage conversion
before publishing telemetry. In this export, J1.2 (`V_out +5V`) also connects
to R12.1; R12.2 is on `VSENSE_5V+` with U1.3. Regulator feedback pin U2.4 is
on `Net-(U2-FB)` with R1, R5, and C2; reconcile the selector/divider connection
from the matching schematic before assigning a feedback probe point. The exact
INA bus probe side and all physical probe mappings must still be confirmed
against the installed assembly. These source mappings are not proof of the
installed board revision, populated parts, or actual sense wiring; reconcile
all of those before interpreting measurements.

**Execution plan (read-only first):**
1. Use the established Regulator Rev-C board/controller/wiring and the 2026-10-07 Rev-C netlist as the working baseline; do not repeat identity checks as a gate. Record the active firmware/build, enabled range, and output state as ordinary test context. Revisit identity only if new evidence shows the hardware or source differs from this baseline; stop only the affected probe/interpretation if a concrete mismatch is found.
2. Record the DMM model, range, calibration/verification status, lead
   placement, and exact measurement point. With no external load, compare the
   DMM directly across the output terminals against the simultaneous raw INA
   value, controller value, and CrowPanel value. Use the existing read-only
   capture sequence (`HELP`, `CFGSHOW`, `CALSHOW`, `INAPROBE`, `INANOW`,
   `INARAILS`, `INADIAG`) and preserve literal output with firmware/build
   identity. `INADIAG` is needed for raw INA bus-register readings;
   `INANOW`/`INARAILS` are derived estimates.
3. Compare the readback with the historical 2026-10-07 saved coefficients
   (`5V vg=0.96642, voff=0.00mV`; `3V3 vg=0.97395, voff=0.00mV`). Those
   values were fitted against resistor/load readings and are not terminal-
   voltage calibration evidence. Do not run `CALSET`, change coefficients, or
   save a new calibration during diagnosis. In the active STM32 firmware,
   `CALSET` writes the configuration immediately; first establish whether the
   active coefficients or measurement location explain a fixed offset. Do not
   use calibration to hide real path loss.
4. Trace the INA voltage-sense points, `VSENSE_5V+`, local fallback/selector output, regulator feedback input, and CH1 output terminals through the established Rev-C schematic/netlist. Confirm the intended feedback target and map probe pads; revisit the baseline only if measurements or new evidence reveal a concrete mismatch.
5. Only after operator approval, compare terminal voltage and resistor voltage simultaneously at no load and at individually approved, bounded CH1 loads. Record exact load resistance and temperature context, independently measured output current, raw INA data, calibrated controller/panel values, PSU voltage/current and CV/CC or limit status, and timestamps. If safe approved probe access is available, capture the mapped pre/post-switch, shunt, inductor, feedback-sense, and selector nodes to localize any drop.
6. Classify the result only from paired measurements: terminal stable but telemetry offset suggests measurement/calibration mismatch; terminal low while upstream/feedback nodes remain higher suggests path loss or sense-target mismatch; correlated movement of terminal and feedback nodes indicates a different regulation condition that needs further analysis. Do not infer root cause from the display alone.

**Safety and scope:** This is a documentation and analysis plan, not authorization to operate the bench. No rewiring, bypass changes, forced gate/feedback states, open-sense tests, overload/short tests, or firmware calibration writes without explicit operator approval. Keep source current limiting and approved load/thermal limits in force; power off and discharge before continuity checks or rewiring.

**Acceptance evidence before any pass/fail or corrective-action decision:**
- Installed board, firmware, netlist export, and measurement-node identities are reconciled.
- Direct terminal measurements are paired with load-resistor, raw INA, controller, and display readings at the same timestamps.
- Active voltage calibration is captured read-only; any proposed coefficient change is justified independently of path losses and separately approved.
- The intended feedback sense target and all relevant path drops are backed by the matched source and physical measurements.
- Numerical terminal-regulation and telemetry-to-terminal tolerances are agreed before assigning pass/fail; any untested rail/range/load coverage stays explicit.

**Next step:** Record the active compact firmware/build, range/output state,
and a direct no-load DMM reading across the CH1 output terminals with meter
details. The compact profile cannot supply INA diagnostics. If raw INA
readings are needed afterward, request authorization before flashing the
full-bench profile; no loaded test is authorized by the compact capture.

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
| RB-013 (Wrong LM74502 driver variant on U9/U14/U15/U16) | `dsp-regulator-rev-c/` (schematic done) | Rework 4x driver ICs to `LM74502HDDFR`, update BOM/LCSC, re-verify Vgs | High |
| RB-014 (Issue #100 terminal voltage / feedback tracking investigation) | Regulator Rev-C | Reconcile board identity and read-only calibration, then compare terminal/load/INA/feedback nodes under approved conditions | Unassessed |

---

## Revision History

| Date | Event | Author |
|------|-------|--------|
| 2026-05-10 | Tracker created; RB-001 and RB-002 opened | Bench bring-up session |
| 2026-05-10 | Added RB-003 through RB-009 from bench bring-up findings | Bench bring-up session |
| 2026-08-10 | Added RB-010 from SPICE fault-case results; marked as Rev-C routing blocker | Simulation and handoff session |
| 2026-09-28 | Added RB-013: Rev-C output gate drivers (U9/U14/U15/U16) populated with wrong `LM74502DDFR` (60µA) variant instead of `LM74502HDDFR` (11mA); schematic `Value` fields corrected, physical rework pending | Bench gate-drive diagnostic session |
| 2026-08-11 | Added RB-011 from bypass bench evidence; conditional keep-bypass rule recorded | Bench validation session |
| 2026-08-13 | Corrected RB-011 interpretation; recorded remote-sense redesign bench pass and RB-010 TLV9352 plan | Bench validation and Rev-C schematic session |
| 2026-08-14 | Implemented dual INA2180A2/TLV1702 four-channel Rev-C OCP, corrected supply and open-collector definitions, validated ERC/netlist, and added mandatory PCB completion gate | Rev-C OCP design session |
| 2026-09-21 | Logged Rev-C first-power continuation HOLD: +5V_reg about 4.08V, +5V about 4.01V, +3.3V rails collapsed, with R11/R35 removed bypass state | Rev-C bench continuation session |
| 2026-09-21 | Recorded fresh Rev-C ERC/DRC release check under RB-010 and RB-012 (both boards blocked; HAT 2026-08-14 clean ERC superseded) | Rev-C release validation session |
| 2026-09-21 | Added forced-enable evidence: U2 and U4 cores regulate at IC pins when pin5 is grounded, but board output nodes remain out of range | Rev-C bench continuation session |
| 2026-09-27 | Added issue #62 bench readings for RB-003 and recorded low `VGS` (0.74V) evidence on `GATE_5_HIGH` versus `SRC_5_HIGH` | Rev-C bench continuation session |
| 2026-10-06 | RB-013 bench rework: replaced U9/U14/U15/U16 and reversed D2/D5/D9/D13; post-rework Vgs verification remains open | Bench continuation session |
| 2026-10-15 | Added RB-014 issue #100 investigation plan and clarified that earlier loaded voltage figures were taken across breadboard resistors, not verified at supply terminals | Issue #100 review |
| TBD | Design review and next-iter file creation | TBD |
