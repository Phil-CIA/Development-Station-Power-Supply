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

---

## Revision History

| Date | Event | Author |
|------|-------|--------|
| 2026-05-10 | Tracker created; RB-001 and RB-002 opened | Bench bring-up session |
| 2026-05-10 | Added RB-003 through RB-009 from bench bring-up findings | Bench bring-up session |
| TBD | Design review and next-iter file creation | TBD |
