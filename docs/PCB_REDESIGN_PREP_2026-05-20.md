# PCB Redesign Prep (2026-05-20)

Goal: Capture all known schematic edits and board-split decision points so Rev-C can be ordered with confidence.

**Current State**: Two-board split (Regulator + HAT) with increased signal traffic. Decision pending: keep split or merge to single board?

---

## Phase 1: Signal Inventory (What Crosses Between Boards?)

### Regulator Board → HAT Board Signals

**Power & Measurement** (Feedback/Sense Pairs):
- `VSENSE_5V+` / `VSENSE_5V-` (5V output sensing, 2 lines)
- `VSENSE_3V3+` / `VSENSE_3V3-` (3.3V output sensing, 2 lines)
- `VSENSE_ADJ+` / `VSENSE_ADJ-` (Adjustable output sensing, 2 lines)

**Fault/Protection**:
- `FAULT_CRITICAL_SUM` (global OCP/fault line)

**Power Rails** (if not integrating):
- `+12V_IN` (input supply)
- `+5V_Reg` / `GND` (primary 5V distribution)
- `+3.3V_Reg` / `GND` (primary 3.3V distribution)
- `+V_Adj` / `GND` (adjustable channel output)
- `+5V_Boot` (bootstrap supply to control circuits)

**Total Current Connector Count**: 13 signals (power/fault/sense) + GND/Vcc repeats

---

## Phase 2: Component Distribution Decision Matrix

### Current Two-Board Split (HAT + Regulator)

| Component | Board | Rationale |
|-----------|-------|-----------|
| **Regulators** (U2/U3/U4: LM2596) | Regulator | Large heat-dissipation; requires thermal management separate from digital |
| **Output MOSFETs** (Q1–Q8) | Regulator | Power switching; heat + current path management |
| **Shunt resistors** (R13–R16, R20–R23) | Regulator | Current measurement points; should stay close to load rail for accuracy |
| **INA3221** (U1) | HAT | Digital telemetry; I2C to ESP32 |
| **OCP Comparators** (U8/U9/U10) | HAT | Fast fault detection; easier integration with fault bus |
| **Threshold trimmers** (POT1–POT6) | HAT | User adjustable; on same board as comparators for signal integrity |
| **ESP32 MCU** (U7) | HAT | Digital control/telemetry host |
| **Power entry / terminal block** | Regulator | Input 12V supply management |
| **Feedback op-amps** (Selector circuit) | Regulator | Analog measurement path; close to sense points improves PSRR |

---

## Phase 3: Known Pending Schematic Edits (All Boards)

### Regulator Board (DSP-Regulator Rev-B)

| Issue ID | Status | Description | Components Affected | Action |
|----------|--------|-------------|---------------------|--------|
| RB-001 | 🟡 Implemented | 3-channel Kelvin selector for feedback | U5/U6 (op-amp), C7/C11/C21 (feedback caps), R31–R36, BAT54C diodes | Bench validation pending; no additional edits needed |
| RB-003 | 🟡 Implemented | MOSFET gate-drive control topology | Q2/Q3/Q16 (BSS138), R1/R5/R65 (pull-ups), diode isolation D7/D8/D10 | Gate drive transient scoping needed (no schematic change) |
| 13 | 🟡 Implemented | Analog OCP comparator path | None (integration happens on HAT board) | Pending HAT finalization |

### HAT Board (DSP-Regulator-HAT Rev-B)

| Issue ID | Status | Description | Components Affected | Action |
|----------|--------|-------------|---------------------|--------|
| 13 | 🟢 Policy locked | Analog OCP comparator stage (all 3 rails) | U8/U9/U10 (TLV1704), C31–C36, R67–R79, POT1–POT6, D9–D11 | Keep D9–D11 footprints but default DNP for first Rev-C build |
| —— | 🟢 Policy locked | Comparator unused-channel parking | U8C/U8D, U9C/U9D, U10C/U10D outputs | Tie unused +IN/-IN to GND; keep unused outputs off fault bus |
| —— | 🟡 Open | FAULT output pull-up strategy | R65 (10k to 3V3_ESP) | **Verify**: Confirm fault-bus pull-up value and source rail |

### Cross-Board Connector Contract

| Issue ID | Status | Description | Signals Affected | Action |
|----------|--------|-------------|------------------|--------|
| 14 | ✅ Verified | Reference sensing contract (Kelvin pairs) | `VSENSE_*±` nets | **Locked**: Routing rule enforcement during layout only |
| 12 | 🟢 Policy locked | Bootstrap split naming consistency | `+5V_Boot` net naming across both boards | Keep single `+5V_Boot` across boards; document source ownership on regulator side |

---

## Pre-Order Issue Triage (Decision Pass)

Purpose: decide what must be resolved before Rev-C order versus what is explicitly deferred.

| Issue ID | Current State | Pre-Order Decision | Order Block? | Owner/Timing |
|----------|---------------|--------------------|--------------|--------------|
| RB-001 | Implemented on Rev-B, validation pending | Keep current topology; no new schematic edits unless bench shows continuity/handoff error | No | Bring-up validation after board order |
| RB-002 | On hold for current MPU path | Keep parked; do not add VBUS isolation parts in this Rev-C pass unless MPU plan changes | No | Re-open only with MPU migration |
| RB-003 | Implemented, transient validation pending | Keep netlist implementation; perform bench truth-table and transient capture later | No | Bring-up validation |
| RB-004 | Input connector polarity/silk clarity open | Add silk polarity markers and terminal labeling in PCB layout pass | No | Layout pass before fab release |
| RB-005 | ESP32 footprint/silk clarity open | Confirm deployed module footprint, then record measured footprint dimensions before Gerber release | **Yes** | Schematic + footprint measurement now |
| RB-006 | Rail fuse/TVS policy open | Defer protection add-ons to post-Rev-C data unless enclosure/safety target requires immediate add | No | Next hardware iteration decision |
| RB-007 | Stack height/mechanical fit open | Measure board stack and enclosure budget before locking split-vs-single architecture | **Yes** | Mechanical check now |
| RB-008 | Buck status LED proposal | Defer; optional feature not required for Rev-C readiness | No | Future QoL update |
| RB-009 | TS5A3157 footprint corrected | Closed | No | None |
| 10 | INA channel mapping mismatch | Treat as bring-up + firmware mapping closure; verify shunt/channel contract but do not block PCB unless routing mismatch is found | No* | Bring-up now, schematic check quick-pass |
| 11 | Adjustable rail behavior uncertain | Perform continuity/feedback check; block only if schematic net is actually open | Conditional | Quick schematic continuity check now |
| 12 | Bootstrap naming drift | Lock one naming contract across both schematics/docs | **Yes** | Decide now, then netlist re-export |
| 13 | Analog OCP path implemented on HAT | Keep global-trip policy and comparator topology; finalize output isolation population policy | **Yes** | Decide D9/D10/D11 policy now |
| 14 | VSENSE contract verified | Keep as locked routing contract | No | Layout rule enforcement |
| 15 | MPU migration deferred | Keep deferred and tracked | No | Separate migration phase |

\* If INA mismatch is traced to schematic routing or incorrect shunt value assumptions in BOM, promote to blocking immediately.

### Blocking Items To Close Before Rev-C Order

1. RB-005: Confirm ESP32 module footprint and silk callouts, then log footprint measurements.
2. RB-007: Capture stack height + enclosure fit data and finalize split vs single decision.
3. Issue 12: Lock bootstrap naming contract.
4. Issue 13: Lock D9/D10/D11 population policy.
5. Issue 11 (conditional): Confirm adjustable rail feedback continuity is not open.

### Footprint Measurement Gate (Required Before Order)

Use this gate for every newly changed footprint in Rev-C scope.

1. Open footprint in KiCad Footprint Editor and measure key geometry.
2. Compare against datasheet package drawing.
3. Record measured values in the table below.
4. Mark pass/fail and block Gerber release on any mismatch outside tolerance.

Recommended tolerance for this gate: +/-0.10 mm on pitch and body edges unless the datasheet requires tighter.

| Footprint Ref | Package | Datasheet Pitch (mm) | Measured Pitch (mm) | Datasheet Body LxW (mm) | Measured Body LxW (mm) | Pad Width x Length (mm) | Courtyard Margin (mm) | Result |
|---------------|---------|----------------------|---------------------|--------------------------|------------------------|-------------------------|-----------------------|--------|
| U7 (ESP32 module) | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [PASS/FAIL] |
| Ux (changed part) | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [PASS/FAIL] |

---

## Phase 4: Single-Board vs Split-Board Decision

### Split Board (Current) — Pros / Cons

**Pros**:
- ✅ Thermal separation: regulator heat stays isolated; easier thermal design
- ✅ Component density: each board smaller, easier PCB routing
- ✅ Modularity: HAT can be swapped/upgraded independently
- ✅ Test isolation: can bench-test regulator without HAT mounted

**Cons**:
- ❌ Connector complexity: 13-pin interface adds cost + assembly risk
- ❌ Signal integrity: longer sense traces (VSENSE_5V+/- crosses via connector)
- ❌ Stack height: combined PCB height may exceed enclosure budget
- ❌ Connector contact resistance adds to feedback path noise

### Single Board (Proposed Consolidation) — Pros / Cons

**Pros**:
- ✅ No connectors: direct PCB traces, lower noise feedback
- ✅ Simpler assembly: one PCB order, one stocking SKU
- ✅ Lower cost: no connector BOM or interface labor
- ✅ Easier troubleshooting: all signals local, no interface debugging

**Cons**:
- ❌ Thermal challenge: regulator + ESP32 on same board = heat management complexity
- ❌ Size: single board larger, may exceed target footprint
- ❌ Design coupling: feedback routing must avoid switching noise from MOSFET switching
- ❌ Loss of modularity: regulator and control always together

---

## Phase 5: Decision Criteria & Recommendations

### Measurement Requirements (Needed to Decide)

1. **Current Regulator Board Stack Footprint**: 
   - Measure PCB size (mm²)
   - Measure with all components populated
   
2. **HAT Footprint**:
   - Measure PCB size (mm²)
   - Measure with all components populated

3. **Stack Height**:
   - Current: Regulator + HAT + connector thickness
   - Target enclosure height limit: [USER INPUT REQUIRED]

4. **Cost Impact**:
   - Connector cost (both male/female sides): [USER INPUT REQUIRED]
   - Assembly time per connector interface: [USER INPUT REQUIRED]

5. **Thermal Analysis**:
   - Peak power dissipation on regulator (worst-case load): [CALCULATE FROM LM2596 + MOSFET]
   - Available thermal budget on single board: [USER INPUT REQUIRED]

### Preliminary Recommendation (Pending Measurements)

**Keep split board IF:**
- Stack height + connector < 20mm and fits enclosure
- Thermal budget on single board insufficient for regulator dissipation
- Modularity/serviceability is a design goal

**Consolidate to single board IF:**
- Stack height + connector > 20mm OR does not fit enclosure
- Thermal design can be solved with local heatsinking
- Cost savings justify redesign effort

---

## Phase 6: Next Actions (Ordered)

1. **Schematic Finalization**:
   - [x] Confirm D9/D10/D11 placement strategy (lock first Rev-C build as DNP with footprints retained)
   - [x] Confirm unused comparator channels parking (tie unused +IN/-IN to GND; keep unused outputs disconnected)
   - [x] Clarify `+5V_Boot` net naming across both boards (single net contract)
   - [ ] Complete Footprint Measurement Gate table for all changed footprints
   - [ ] Re-export both netlists after above edits

2. **Measurement Gathering**:
   - [ ] Measure Regulator Rev-B PCB size + height (populated)
   - [ ] Measure HAT Rev-B PCB size + height (populated)
   - [ ] Measure stack height with current connector
   - [ ] Get enclosure dimensions and height budget
   - [ ] Calculate regulator thermal dissipation (worst-case load + ambient)

3. **Board-Split Decision**:
   - [ ] Evaluate single vs split based on measurements
   - [ ] If split board chosen: finalize connector pinout and create layout rules
   - [ ] If single board chosen: start mechanical/thermal integration study

4. **PCB Order Prep**:
   - [ ] Create Rev-C board split strategy doc (if split) or single-board outline (if merged)
   - [ ] Finalize component placement rules and signal routing constraints
   - [ ] Ready for manufacturing quotes and component sourcing

---

## Source Of Truth Files

- `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch` — HAT schematic
- `hardware/kicad/dsp-regulator/DSP-Regulator.kicad_sch` — Regulator schematic (if exists)
- `docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md` — Detailed issue tracking
- `docs/DEV_STATION_HANDOFF_2026-05-20.md` — Current session handoff

---

## Revision History

| Date | Event |
|------|-------|
| 2026-05-20 | Created PCB_REDESIGN_PREP_2026-05-20.md; captured signal inventory, board-split decision matrix, and pending edits |
| 2026-05-21 | Locked schematic policy decisions in pre-order triage and added mandatory Footprint Measurement Gate before order release |
