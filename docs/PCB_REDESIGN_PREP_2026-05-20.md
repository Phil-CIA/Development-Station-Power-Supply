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

### 2026-05-22 Implementation Kickoff: Verified Cross-Board Contract

Netlist verification from both Rev-B exports confirms the active inter-board contract below.

| Group | Nets (current) | Count |
|------|-----------------|-------|
| Kelvin sense pairs | `VSENSE_5V+`, `VSENSE_5V-`, `VSENSE_3V3+`, `VSENSE_3V3-`, `VSENSE_ADJ+`, `VSENSE_ADJ-` | 6 |
| Feedback return paths | `Feedback 5V`, `Feedback 3.3`, `Feedback Adj Channel` | 3 |
| Rail-enable control | `ISET_MPU_5V`, ` ISET_MPU_3V3`, ` ISET_MPU_Channel_3` | 3 |
| Fault | `FAULT_CRITICAL_SUM` | 1 |
| Bootstrap naming (needs normalization) | `+5V_Boot`, `+5V_Bootstrap` | 2 names in use |

Implementation note:
- The signed-off naming policy is a single bootstrap contract, but Rev-B netlists still show both `+5V_Boot` and `+5V_Bootstrap` variants. Normalize this before final netlist release.

### 2026-05-22 Target: Moderate Redistribution (Pin-Count Reduction First)

Goal: keep two boards for now, but move enough tightly coupled analog/protection logic to reduce connector dependency.

| Candidate move | Current owner | Proposed owner | Expected connector impact |
|----------------|---------------|----------------|---------------------------|
| OCP comparator stage (`U8/U9/U10`, trim network) | HAT | Regulator | Can remove/avoid cross-board comparator-driving dependencies and simplify fault routing ownership |
| Fault pull-up ownership (`FAULT_CRITICAL_SUM` pull-up strategy) | HAT side policy | Regulator-side ownership | Simplifies one-direction fault contract and reduces ambiguity at connector |
| Sense conditioning used only for split handoff | HAT | Regulator | Reduces duplicated analog handoff nodes and may allow fewer analog contract lines |

### Reduced-Connector Contract Candidates

Use this as the implementation screen before deciding one-board consolidation.

| Contract Option | Cross-board analog nets | Cross-board control/fault nets | Relative pin count | Decision signal |
|-----------------|-------------------------|--------------------------------|--------------------|-----------------|
| Current split (baseline) | 6 VSENSE + 3 Feedback | 3 ISET + 1 FAULT + power/ground | Highest | Keep only if redistribution gains are small |
| Reduced split (target) | Prefer only one analog scheme (either VSENSE pairs or feedback returns, not both) | Keep 3 ISET + 1 FAULT + required rails | Medium | Preferred if it materially reduces connector complexity |
| Single board | None | None | Lowest | Choose if reduced split still leaves a heavy connector contract |

Implementation rule for this revision:
1. Remove contract duplication first (do not carry both remote VSENSE pairs and duplicate feedback-return crossings unless required by measured behavior).
2. Normalize bootstrap naming to one net contract before any connector repin decisions.
3. If reduced split cannot drop connector complexity enough after ownership moves, promote one-board consolidation immediately instead of preserving a weak partition.

### 2026-05-23 Pivot: Production Connector Architecture (Two-Board Path)

Decision update:
- Pin-count-only optimization is no longer the primary design driver.
- Keep two boards, but replace hobby-style 2.54 mm breakaway headers with a production interconnect architecture.

Connector strategy (recommended):
1. **Power connector**: dedicated high-current board-to-board interface for rail transfer and power returns.
2. **Signal connector**: separate mezzanine interface for Kelvin sense, control, fault, and low-current references.

Why this pivot:
- Current interface relies on pin-ganging for rail current and leaves too few return/common paths.
- This is electrically workable but poor production practice for current density, grounding, and contact reliability.
- Splitting power and signal connectors allows cleaner return-path engineering and preserves two-board modularity.

### Connector Requirements (Must Be Defined Before Part Selection)

| Requirement | Target | Status |
|-------------|--------|--------|
| Per-rail max current (`+12V`, `+5V`, `+3.3V`, `+V Adj`) | Numeric values required | Open |
| Contact current derating rule | Use <= 70% of vendor rating at worst-case ambient | Open |
| Return path capacity | Total return contacts sized >= total outbound current capacity | Open |
| Minimum dedicated quiet grounds on signal connector | >= 3 (sense/control reference) | Open |
| Connector retention/keying | Positive latch or equivalent anti-mis-mate feature | Open |
| Mating cycle requirement | Define service-life target | Open |
| Stack-height compatibility | Must satisfy mechanical envelope and spacing constraints | Open |

### Proposed Connector Partition (Draft, Mixed Distribution)

| Connector | Carries | Notes |
|-----------|---------|-------|
| Connector A (`CLP/FW` pair #1) | Mixed set: power rails, returns, and subset of control/sense nets | Not a strict power-only connector |
| Connector B (`CLP/FW` pair #2) | Mixed set: remaining power returns plus control/sense/ground references | Not a strict signal-only connector |

Connector selection lock (2026-05-23):
- Use two identical Samtec CLP/FW pairs: `2x` (`CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A`).
- Total inter-board positions available: `40` (20 per connector x 2 connectors).
- Allocation policy: mixed distribution across both connectors (do not assume clean power/control separation).

### Current Budget Worksheet (To Fill Before Final Connector Choice)

Working assumptions for v0 screening:
- Contact derating rule: use 70% of nominal rating.
- Active connector assumption from selected pair: 3.1 A/contact nominal, 2.17 A/contact derated.
- Rail-current assumptions are provisional design targets until bench characterization is logged.

| Rail / Group | Max current (A) | Candidate contact rating (A/contact) | Derated usable (A/contact) | Contacts required (N) | Current plan (N) | Margin |
|--------------|------------------|--------------------------------------|----------------------------|-----------------------|------------------|--------|
| `+12V` feed | 1.0 (provisional) | 3.1 | 2.17 | 1 | 1 | 1.17 A |
| `+5V_reg` | 2.0 (provisional target) | 3.1 | 2.17 | 1 | 1 | 0.17 A |
| `+3.3V_Reg` | 1.5 (provisional target) | 3.1 | 2.17 | 1 | 1 | 0.67 A |
| `+V Adj` | 2.0 (provisional) | 3.1 | 2.17 | 1 | 1 | 0.17 A |
| Power return sum | 6.5 (sum of above) | 3.1 | 2.17 | 3 | 4 | 2.18 A |

Provisional source notes:
- `+5V_reg` and `+3.3V_Reg` starter targets are aligned to protection-sizing guidance captured in issue tracking (`2A` and `1.5A` examples).
- `+V Adj` and `+12V` values are temporary screening assumptions for connector-family down-select only.
- Replace all provisional values with measured/locked limits before freezing connector part numbers.

### Active Allocation Snapshot (2026-05-23 User Update)

Current working allocation:
- Signal pins used: 18
- Power rails: 3 rails at 3.0 A each
- Connector set: two identical CLP/FW pairs (40 total positions)

Capacity check using active contact assumption (3.1 A nominal, 2.17 A derated at 70%):
1. Outbound contacts per 3.0 A rail: `ceil(3.0 / 2.17) = 2`
2. Outbound power contacts for 3 rails: `2 x 3 = 6`
3. Required return contacts for 9.0 A total return: `ceil(9.0 / 2.17) = 5`
4. Total power-group contacts (outbound + return): `6 + 5 = 11`
5. Total used positions with current allocation: `18 (signal) + 11 (power) = 29`
6. Remaining spare positions: `40 - 29 = 11`

Result:
- Provisional PASS for pin-count capacity under the current 18-signal / 3x3 A allocation.
- Still HOLD for final freeze until exact net-to-pin assignment and return-path layout are checked.

### Draft 40-Pin Allocation Table (Dual CLP/FW, Mixed Pre-Layout Baseline)

Scope note:
- This table is the electrical allocation baseline (what each connector position class is reserved for).
- Distribution is intentionally mixed across both connectors.
- Physical connector placement, orientation, and trace routing in KiCad are separate layout tasks.

| Connector | Allocation group | Nets / function | Positions used |
|-----------|------------------|-----------------|----------------|
| Connector A (`CLP/FW` pair #1) | Outbound power rails | Subset of rail contacts (`+12V`, `+5V_reg`, `+3.3V_Reg`) | 3 |
| Connector A (`CLP/FW` pair #1) | Power returns | `GND_power_return` | 3 |
| Connector A (`CLP/FW` pair #1) | Signal/control/fault/bootstrap/quiet grounds | Mixed subset of the 18-signal contract | 9 |
| Connector A (`CLP/FW` pair #1) | Spare / growth | Reserved margin | 5 |
| Connector B (`CLP/FW` pair #2) | Outbound power rails | Remaining rail contacts (`+12V`, `+5V_reg`, `+3.3V_Reg`) | 3 |
| Connector B (`CLP/FW` pair #2) | Power returns | `GND_power_return` | 2 |
| Connector B (`CLP/FW` pair #2) | Signal/control/fault/bootstrap/quiet grounds | Remaining subset of the 18-signal contract | 9 |
| Connector B (`CLP/FW` pair #2) | Spare / growth | Reserved margin | 6 |

Allocation totals check:
1. Connector A used: `3 + 3 + 9 + 5 = 20`
2. Connector B used: `3 + 2 + 9 + 6 = 20`
3. Combined positions: `20 + 20 = 40`
4. Matches active allocation baseline: `18 signal positions`, `11 power-group positions`, `11 spare positions`

### SMT Placement Study Gate (Board-Level, Before Pin Freeze)

Why this gate exists:
- Electrical allocation can pass on paper but still fail in practical placement/routing.
- SMT board-to-board connectors can help routing density and stack profile, but can hurt service robustness and assembly tolerance if placement is poor.

SMT help vs hurt checklist:
1. Helps when: short escape paths, clean return stitching, compact stack height, and no connector overhang conflicts.
2. Hurts when: weak solder-joint leverage under insertion force, poor probe/tool access, or dense fanout that forces long detours and reference breaks.

Placement study workflow (KiCad):
1. Create three placement variants using the same two CLP/FW connectors.
2. For each variant, do quick escape routing for critical groups (`VSENSE_*`, `FAULT_CRITICAL_SUM`, `ISET_*`, rails, and returns).
3. Run DRC and inspect courtyard overlap, keepout conflicts, and accessible rework/probe paths.
4. Score each variant against the matrix below.
5. Freeze only the best-scoring variant that passes hard gates.

Suggested variants:
- Variant A: connectors on one board edge, parallel orientation.
- Variant B: connectors near board centerline, parallel orientation.
- Variant C: staggered spacing (mixed orientation only if routing improves and assembly still passes).

Hard gates (must pass):
1. No DRC errors around connector fanout/courtyards.
2. Kelvin pair routing keeps stable reference adjacency and no forced long loops.
3. Power and return escape can meet copper width/via count targets without choke points.
4. Probe access remains for bring-up nets and fault/debug points.
5. Assembly/rework access supports iron/hot-air reach without shadowing critical parts.

Variant scoring matrix (quick decision tool):

| Variant | Escape routing complexity (1-5, higher is easier) | Return-path continuity (1-5) | Kelvin/control integrity (1-5) | Assembly/rework access (1-5) | Mechanical robustness (1-5) | Total (max 25) | Gate result |
|---------|----------------------------------------------------|------------------------------|---------------------------------|-------------------------------|-----------------------------|----------------|-------------|
| A | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [PASS/HOLD/FAIL] |
| B | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [PASS/HOLD/FAIL] |
| C | [fill] | [fill] | [fill] | [fill] | [fill] | [fill] | [PASS/HOLD/FAIL] |

Placement freeze output:
1. Selected variant ID.
2. Final connector coordinates/orientation committed in PCB layout.
3. Pin-number mapping updated to match real placement escape success.

### Connector Selection Gate (Pass/Fail)

Do not freeze connector part numbers until all checks pass:
1. All rails and returns meet derated current capacity with margin.
2. Across both connectors, quiet-ground allocation and net adjacency support Kelvin/control integrity.
3. Mechanical stack height and placement are compatible with enclosure constraints.
4. Selected families provide keying/retention suitable for repeated assembly/service.
5. PCB footprint availability and manufacturability are verified for both board sides.

### Initial Connector Shortlist (For Evaluation)

Use this as a sourcing/layout starting point. Final selection requires the current-budget worksheet and derating checks above.

| Role | Candidate family class | Typical strengths | Typical risks | Fit for this project |
|------|------------------------|-------------------|---------------|----------------------|
| Power connector | High-current board-to-board power mezzanine | High current per contact, robust wipe force, good for grouped rails/returns | Larger footprint and stack-height impact | High |
| Power connector | Power blade or busbar-style board interconnect | Excellent current density and thermal margin | Mechanical integration complexity, cost | Medium-High |
| Power connector | Rugged wire-to-board power pair (board-stack via short harness) | High current, easy sourcing, robust crimp ecosystem | Loses pure board-stacking simplicity | Medium |
| Signal connector | Fine-pitch mezzanine board-to-board | High pin density for VSENSE/ISET/FAULT/grounds, compact | Lower current per pin, tighter assembly tolerance | High |
| Signal connector | Rugged micro mezzanine (latching) | Better retention and service robustness | Lower pin density and larger area | Medium-High |

Selection rule:
1. Choose power connector first from current-capacity/return-budget constraints.
2. Choose signal connector second from pin density, quiet-ground allocation, and assembly tolerance.
3. Do not collapse power and signal back into one connector unless current and grounding budgets still pass with margin.

### Candidate Series Matrix (v0 Starting Point)

| Role | Candidate series | Type | Why consider | Primary risk/check |
|------|------------------|------|--------------|--------------------|
| Power connector | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | Board-to-board mezzanine pair | User-selected Samtec pair; significant upgrade over prior hobby header approach | Confirm exact mating compatibility, current/contact limits, and mated-height window from datasheet |
| Signal connector | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | Board-to-board mezzanine pair | Same qualified part family as power connector; simplifies sourcing and assembly | Validate mixed allocation map (adjacency, return distribution, and crosstalk risk) |

Initial screening gate for candidate series:
1. Keep only options that meet derated current and return capacity from the worksheet.
2. Keep only options with available mating-height choices inside the target screening band (`H_mated = 8.0 .. 10.0 mm`, with 9.0 mm ideal).
3. Keep only options with positive retention or anti-mis-mate features for service handling.
4. De-prioritize options with long procurement lead-time unless second-source exists.

### Target-Height Filter (8.0 .. 10.0 mm Screening Band)

Purpose:
- Down-select connector families before part-number lock by checking whether each family offers viable mated-height options near the 9.0 mm target.

| Role | Candidate series | Mated-height options available in family | 8.0 .. 10.0 mm available? | Closest option to 9.0 mm | Height filter result |
|------|------------------|-------------------------------------------|---------------------------|--------------------------|----------------------|
| Power connector | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | User-selected pair (datasheet confirmation pending) | [pending] | [pending] | HOLD (selected; verify height/current) |
| Signal connector | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | Same as selected power connector pair | [pending] | [pending] | HOLD (selected; verify height/current + signal allocation) |

Filter interpretation:
1. PASS: family has at least one practical option in 8.0 .. 10.0 mm, and one option close to 9.0 mm.
2. HOLD: incomplete datasheet confirmation or architecture variant (for example harness path).
3. FAIL: no practical options in 8.0 .. 10.0 mm.

### Candidate Scoring Matrix (v0, Pre-Measurement)

Scoring method:
- 5 = strong fit, 3 = acceptable with caveat, 1 = poor fit.
- Total score is pre-measurement only and must be re-scored after stack-height and sourcing checks.

#### Power Connector Candidates

| Candidate series | Derated current capacity fit | Return-path capacity fit | Mechanical stack/footprint risk | Service/retention fit | Sourcing risk | Total (max 25) | Provisional status |
|------------------|------------------------------|--------------------------|----------------------------------|-----------------------|--------------|----------------|--------------------|
| Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | 5 | 5 | 3 | 4 | 3 | 20 | **User-selected power candidate (verify datasheet details)** |

Power selection interpretation:
1. Power connector path is locked to Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` per current user direction.
2. Remaining action is datasheet validation (mating fit, current/contact limits, and mated-height target window).

#### Signal Connector Candidates

| Candidate series | Pin-density fit (VSENSE/ISET/FAULT/grounds) | Quiet-ground allocation fit | Mechanical tolerance risk | Service/retention fit | Sourcing risk | Total (max 25) | Provisional status |
|------------------|-----------------------------------------------|-----------------------------|---------------------------|-----------------------|--------------|----------------|--------------------|
| Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | 4 | 4 | 4 | 4 | 4 | 20 | **User-selected signal connector (identical CLP/FW pair; verify allocation + datasheet details)** |

Signal selection interpretation:
1. Signal connector path is locked to a second identical Samtec CLP/FW pair per current user direction.
2. Remaining action is pin-allocation validation (quiet grounds and sensitive nets) plus datasheet/source confirmation.

### Provisional Finalist Pair (Subject To Gate Closure)

Recommended pair for current direction:
1. Power: Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (user-selected, only active power path).
2. Signal: Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector, user-selected).

Do not freeze part numbers yet:
- Final selection still depends on verified datasheet mating details (mated height and current/contact limits), confirmed rail limits, and procurement lead-time checks.

### Datasheet Validation Ledger (Selected PNs)

Purpose:
- Track exact evidence required to move selected connectors from HOLD to frozen.

Automated source-access status (2026-05-23):
- Attempted manufacturer/distributor extraction for selected PNs returned anti-bot or parse failures in this environment (HTTP 403/500 or no extractable content).
- Validation is therefore in structured HOLD state until manual datasheet values are copied into the table below.

| Item | Selected PN(s) | Required evidence | Current value | Source status | Gate status |
|------|----------------|-------------------|---------------|---------------|-------------|
| Power mating compatibility | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | Explicit mating statement in series datasheet/configurator | [pending] | Blocked via automated fetch (Samtec/Octopart/DigiKey mirror) | HOLD |
| M5 power mated height | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | Nominal mated height + tolerance | [pending] | Blocked via automated fetch | HOLD |
| Power current/contact | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` | Current/contact rating + derated usable value | 3.1 A nominal (user-provided), 2.17 A derated @70% | User-provided from datasheet screenshot; automated fetch blocked | CONDITIONAL (await direct datasheet citation) |
| Signal mating compatibility | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | Explicit mating pair confirmation for second identical connector usage | [pending] | Same selected pair as power connector; automated fetch blocked | HOLD |
| M6 signal mated height | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | Nominal mated height + tolerance for second connector usage | [pending] | Same selected pair as power connector; automated fetch blocked | HOLD |
| Signal current/contact | Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (second identical connector) | Current/contact rating for signal connector | 3.1 A nominal (user-provided), 2.17 A derated @70% | User-provided from datasheet screenshot; automated fetch blocked | CONDITIONAL (await direct datasheet citation) |
| Procurement readiness | Both selected pairs | In-stock/lead-time snapshot from preferred distributors | [pending] | Blocked via automated fetch | HOLD |

Extraction completion rule:
1. Fill all `Current value` cells with numeric datasheet values and tolerance where applicable.
2. Copy exact source reference (datasheet title/revision/page) into session handoff notes.
3. Recompute RB-007 final pair numbers after M5/M6 are populated.
4. Move selected pair status from HOLD to PASS only after current and height checks both pass.

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

### Mechanical Stack-Height and Fit Closure Pack (RB-007)

Purpose:
- Close RB-007 with measured mechanical data that is reusable even if connector series changes.

#### A) Measurement Inputs (Capture First)

| Input ID | Description | Method | Unit | Status |
|----------|-------------|--------|------|--------|
| M1 | Regulator PCB thickness | Caliper at board edge (no copper burr) | mm | 1.60 (measured 2026-05-23) |
| M2 | HAT PCB thickness | Caliper at board edge | mm | 1.60 (measured 2026-05-23) |
| M3 | Tallest regulator top-side component above PCB | Height gauge/caliper from PCB top plane | mm | 11.00 (measured 2026-05-23) |
| M4 | Tallest HAT bottom-side component below PCB | Height gauge/caliper from PCB bottom plane | mm | 2.54 (measured 2026-05-23) |
| M5 | Candidate connector mated stack height (power connector) | Datasheet nominal + tolerance | mm | [open] |
| M6 | Candidate connector mated stack height (signal connector) | Datasheet nominal + tolerance | mm | [open] |
| M7 | Assembly tolerance allowance (board warp + placement + spacer tolerance) | Process allowance | mm | 1.00 (provisional planning default; replace when process spec is known) |
| M8 | Enclosure internal Z budget at stack location | Enclosure measurement/CAD | mm | N/A (non-limiting per current enclosure) |
| M9 | Keepout near external interfaces (USB, terminal block, headers) | CAD/physical measurement | mm | N/A (no known global constraints; check only for local placement conflicts) |

Measured baseline constants from current data:
- `M3 + M4 = 13.54 mm` (fixed term before connector mated height and assembly allowance).
- With current known measurements, `H_required = 13.54 + H_mated + M7`.
- Spacing policy (2026-05-23 user direction):
   - Minimum acceptable inter-board spacing envelope: `13.54 mm`.
   - Preferred working spacing envelope: `23.54 mm` (minimum + 10.00 mm).
- Target-driven connector sizing:
   - `H_mated_target = 23.54 - 13.54 - M7 = 10.00 - M7`.
   - With provisional `M7 = 1.00`, target `H_mated` is `~9.00 mm`.
   - Practical starting band for connector search: `H_mated = 8.0 .. 10.0 mm`.

#### B) Stack-Height Calculation (Connector-Agnostic)

For each connector option pair, compute:

1. Functional mated height:
   - `H_mated = max(M5, M6)` for two-connector architecture if boards are constrained by the taller mated pair.

2. Required internal envelope:
   - `H_required = M3 + H_mated + M4 + M7`

3. Spacing margins:
   - `H_margin_min = H_required - 13.54`
   - `H_margin_pref = 23.54 - H_required`

4. Target fit error (optional but useful):
   - `H_error_target = H_required - 23.54`

Pass rule:
- PASS if `|H_error_target| <= 1.0 mm` and keepout/service checks are PASS or N/A-documented.
- CONDITIONAL if `1.0 mm < |H_error_target| <= 2.0 mm` and tradeoff is documented.
- FAIL if `|H_error_target| > 2.0 mm` or keepout/service checks fail.

#### C) Mechanical Fit Worksheet (Per Candidate Pair)

Scenario note:
- Pair-A/B/C below are mated-height screening scenarios (not locked part selections).
- Assumes `M7 = 1.00 mm` provisional default and no known global keepout constraints.

| Candidate pair ID | Power connector class/series | Signal connector class/series | H_mated (mm) | H_required (mm) | H_margin_min (mm) | H_margin_pref (mm) | H_error_target (mm) | Interface keepout pass (Y/N/N-A) | Service access pass (Y/N/N-A) | Result |
|-------------------|------------------------------|-------------------------------|--------------|-----------------|-------------------|--------------------|---------------------|-----------------------------------|--------------------------------|--------|
| Pair-A | Scenario: low stack target | Scenario: low stack target | 8.00 | 22.54 | 9.00 | 1.00 | -1.00 | N-A (screening stage) | N-A (screening stage) | PASS (screening) |
| Pair-B | Scenario: nominal target | Scenario: nominal target | 9.00 | 23.54 | 10.00 | 0.00 | 0.00 | N-A (screening stage) | N-A (screening stage) | PASS (screening, ideal target) |
| Pair-C | Scenario: high stack target | Scenario: high stack target | 10.00 | 24.54 | 11.00 | -1.00 | 1.00 | N-A (screening stage) | N-A (screening stage) | PASS (screening) |

#### D) XY Keepout / Collision Check (Required)

Record at minimum:
1. Clearance to USB connector envelope on HAT and regulator.
2. Clearance to terminal block and wiring bend radius region.
3. Clearance to test points needed during bring-up.
4. Clearance for latch engagement/disengagement tool/finger access.

| Check ID | Region | Minimum required clearance (mm) | Measured clearance (mm) | Result |
|----------|--------|----------------------------------|-------------------------|--------|
| K1 | USB envelope | [fill] | [fill] | [PASS/FAIL] |
| K2 | Terminal/wire bend envelope | [fill] | [fill] | [PASS/FAIL] |
| K3 | Test-point probe access | [fill] | [fill] | [PASS/FAIL] |
| K4 | Latch/service access | [fill] | [fill] | [PASS/FAIL] |

#### E) RB-007 Closure Gate

Do not close RB-007 until all are true:
1. At least one candidate pair lands within target-fit PASS or accepted CONDITIONAL band.
2. Keepout checks K1-K4 are either PASS or explicitly marked N/A with rationale.
3. No connector blocks required bench access points for bring-up.
4. Measurement source (physical vs CAD vs datasheet) is logged for each value.

If no pair passes, trigger one of these paths immediately:
1. Lower-profile connector pair re-selection.
2. Board-spacing/mechanical architecture change.
3. Single-board concept escalation.

### Measurement Requirements (Needed to Decide)

1. **Current Regulator Board Stack Footprint**: 
   - Measure PCB size (mm²)
   - Measure with all components populated
   
2. **HAT Footprint**:
   - Measure PCB size (mm²)
   - Measure with all components populated

3. **Stack Height**:
   - Current: Regulator + HAT + connector thickness
   - Enclosure height limit: non-limiting for current build direction
   - Enforce spacing policy: minimum 13.54 mm, preferred 23.54 mm
   - Use the RB-007 closure worksheet above for final PASS/FAIL

4. **Cost Impact**:
   - Connector cost (both male/female sides): [USER INPUT REQUIRED]
   - Assembly time per connector interface: [USER INPUT REQUIRED]

5. **Thermal Analysis**:
   - Peak power dissipation on regulator (worst-case load): [CALCULATE FROM LM2596 + MOSFET]
   - Available thermal budget on single board: [USER INPUT REQUIRED]

### Preliminary Recommendation (Pending Measurements)

**Keep split board IF:**
- Mechanical spacing policy is satisfied and keepout/service checks pass
- Thermal budget on single board insufficient for regulator dissipation
- Modularity/serviceability is a design goal

**Consolidate to single board IF:**
- Mechanical spacing policy cannot be satisfied with acceptable connector architecture
- Thermal design can be solved with local heatsinking
- Cost savings justify redesign effort

### Mechanical Decision Output (Record Here)

| Item | Value |
|------|-------|
| Selected candidate pair ID | [fill] |
| Measured H_required (mm) | [fill] |
| Enclosure budget M8 (mm) | N/A (non-limiting) |
| Final H_margin_min (mm) | [fill] |
| Final H_margin_pref (mm) | [fill] |
| RB-007 result | [PASS/FAIL] |
| Decision | [Keep split / Escalate single-board / Rework connector selection] |

---

## Phase 6: Next Actions (Ordered)

1. **Schematic Finalization**:
   - [x] Confirm D9/D10/D11 placement strategy (lock first Rev-C build as DNP with footprints retained)
   - [x] Confirm unused comparator channels parking (tie unused +IN/-IN to GND; keep unused outputs disconnected)
   - [x] Clarify `+5V_Boot` net naming across both boards (single net contract)
   - [ ] Complete Footprint Measurement Gate table for all changed footprints
   - [ ] Re-export both netlists after above edits

2. **Measurement Gathering**:
   - [x] Measure Regulator Rev-B PCB thickness and tallest top-side component height (M1, M3) (2026-05-23)
   - [x] Measure HAT Rev-B PCB thickness and tallest bottom-side component depth (M2, M4) (2026-05-23)
   - [ ] Measure stack height with current connector
   - [x] Set provisional assembly allowance M7 = 1.00 mm for target-driven connector screening (2026-05-23)
   - [x] Mark enclosure height as non-limiting for current mechanical decision path (2026-05-23)
   - [x] Add RB-007 mechanical stack-height closure worksheet and pass/fail gate (2026-05-23)
   - [x] Mark M9 as no known global constraints (evaluate local conflicts only) (2026-05-23)
   - [x] Run scenario-based stack sensitivity (H_mated = 8/9/10 mm) to bracket target-fit behavior (2026-05-23)
   - [ ] Populate remaining M5/M6 values from candidate connector datasheets
   - [ ] Compute candidate-pair `H_required`, `H_margin_pref`, and `H_error_target` results
   - [ ] Calculate regulator thermal dissipation (worst-case load + ambient)

3. **Board-Split Decision**:
   - [x] Start connector-contract implementation pass (2026-05-22)
   - [ ] Build reduced split pin-map proposal after ownership moves (comparators/fault/sense conditioning)
   - [x] Pivot to production connector architecture discussion (2026-05-23)
   - [x] Define provisional per-rail current budget and return-path budget for connector sizing (2026-05-23)
   - [ ] Replace provisional budget with bench-validated current limits
   - [ ] Select candidate power + signal connector families and score against requirements
   - [ ] Evaluate single vs split based on RB-007 gate output and measured thermal data
   - [ ] If split board chosen: finalize connector pinout and create layout rules
   - [ ] If single board chosen: start mechanical/thermal integration study

4. **PCB Order Prep**:
   - [ ] Create Rev-C board split strategy doc (if split) or single-board outline (if merged)
   - [ ] Finalize component placement rules and signal routing constraints
   - [ ] Ready for manufacturing quotes and component sourcing

---

## Phase 7: Implementation Worklist (Started 2026-05-22)

1. **Connector Contract Reduction**
   - [x] Verify active cross-board net groups from both Rev-B netlists
   - [x] Identify duplicated contract paths (VSENSE + Feedback overlap) per rail
   - [x] Propose one contract per rail (primary analog crossing only)
   - [x] Add automated connector-contract verification script (`scripts/verify-connector-contract.ps1`)
   - [x] Run baseline verification (current status captured)
   - [x] Run reduced-split verification (current fail baseline captured)

2. **Ownership Moves (Moderate Redistribution)**
   - [ ] Draft move plan for OCP comparator stage from HAT to Regulator
   - [ ] Draft fault pull-up single-owner contract and connector directionality
   - [ ] Recompute connector pin requirement after ownership changes

3. **Architecture Gate (2 boards vs 1 board)**
   - [ ] If reduced contract still high-complexity, trigger single-board concept freeze
   - [ ] If reduced contract is clean, freeze two-board reduced connector map

4. **Production Connector Track (2026-05-23 pivot)**
   - [x] Adopt two-connector architecture (dedicated power connector + dedicated signal connector)
   - [x] Complete provisional current budget worksheet for all rails and return capacity (2026-05-23)
   - [ ] Replace worksheet provisional values with measured/locked current limits
   - [x] Create v0 connector candidate series shortlist (2026-05-23)
   - [x] Add v0 candidate scoring matrix and provisional finalist pair (2026-05-23)
   - [x] Add target-height filter table for 8.0 .. 10.0 mm screening band (2026-05-23)
   - [x] Record user-selected connector candidates: Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (power) and second identical Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` (signal)
   - [x] Lock power connector direction to Samtec pair only (2026-05-23)
   - [x] Lock signal connector direction to second identical Samtec CLP/FW pair (2026-05-23)
   - [x] Record dual-connector assumption: 2 x 20 positions = 40 total positions, 3.1 A/contact nominal user-provided (2026-05-23)
   - [x] Record active allocation snapshot: 18 signal pins + 3 rails at 3.0 A each; computed 29 used / 11 spare positions (2026-05-23)
   - [x] Create draft 40-pin dual-connector allocation table (mixed electrical allocation across both connectors, pre-layout) (2026-05-23)
   - [ ] Run SMT placement study in KiCad (variants A/B/C), score matrix, and freeze connector placement variant
   - [ ] Validate shortlisted families against derated current/contact margins, stack constraints, and lead-time checks
   - [ ] Freeze connector choice and update pin-map around selected footprints

### Rev-C Reduced Split Pin-Map Proposal (Draft v0)

This is the first implementation candidate for connector reduction. It assumes moderate redistribution (move comparator ownership and remove duplicated analog crossings).

| Net / Function | Baseline state | Draft v0 action | Why |
|----------------|----------------|-----------------|-----|
| `VSENSE_5V+/-` | Cross-board | Keep | Required for Kelvin sense integrity (5V rail) |
| `VSENSE_3V3+/-` | Cross-board | Keep | Required for Kelvin sense integrity (3.3V rail) |
| `VSENSE_ADJ+/-` | Cross-board | Keep | Required for Kelvin sense integrity (adjustable rail) |
| `Feedback 5V` | Cross-board | Remove crossing (localize to regulator) | Duplicates analog contract when VSENSE pair already crosses |
| `Feedback 3.3` | Cross-board | Remove crossing (localize to regulator) | Same duplicate-path reduction |
| `Feedback Adj Channel` | Cross-board | Remove crossing (localize to regulator) | Same duplicate-path reduction |
| `ISET_MPU_5V` | Cross-board | Keep | Needed for per-rail enable control if split remains |
| ` ISET_MPU_3V3` | Cross-board | Keep | Needed for per-rail enable control if split remains |
| ` ISET_MPU_Channel_3` | Cross-board | Keep | Needed for per-rail enable control if split remains |
| `FAULT_CRITICAL_SUM` | Cross-board | Keep (single direction, regulator-owned pull-up policy) | Keeps global fault contract explicit |
| `+5V_Boot` / `+5V_Bootstrap` naming | Mixed variant | Normalize to one name before repin | Prevents contract drift and connector errors |

Estimated impact of Draft v0:
- Removes 3 analog connector crossings (`Feedback 5V`, `Feedback 3.3`, `Feedback Adj Channel`).
- Keeps 6 Kelvin-sense lines + 3 control + 1 fault as the core split contract.
- If this still feels too heavy after placement study, trigger one-board architecture path.

### Rev-C Connector Contract (Draft v1 Pin-Level)

This table is mapped from current Rev-B netlists and is the implementation baseline for connector repin edits.

| Function / Net | Regulator side pin | HAT side pin | Draft v1 action |
|----------------|--------------------|--------------|-----------------|
| `VSENSE_5V+` | `J1-3` | `J4-3` | Keep |
| `VSENSE_5V-` | `J1-2` | `J4-2` | Keep |
| `VSENSE_3V3+` | `J1-15` | `J4-15` | Keep |
| `VSENSE_3V3-` | `J1-14` | `J4-14` | Keep |
| `VSENSE_ADJ+` | `J2-12` | `J6-12` | Keep |
| `VSENSE_ADJ-` | `J2-11` | `J6-11` | Keep |
| `ISET_MPU_5V` | `J1-5` | `J4-5` | Keep |
| ` ISET_MPU_3V3` | `J1-10` | `J4-10` | Keep |
| ` ISET_MPU_Channel_3` | `J2-5` | `J6-5` | Keep |
| `Feedback 5V` | `J1-4` | `J4-4` | Remove crossing in reduced split |
| `Feedback 3.3` | `J1-9` | `J4-9` (currently NC on HAT export) | Remove crossing in reduced split |
| `Feedback Adj Channel` | `J2-4` | `J6-4` | Remove crossing in reduced split |
| `FAULT_CRITICAL_SUM` | `J2-3` via `Net-(J2-Pin_3)` into `R50` | `J6-3` | Keep, but make direction and pull-up ownership explicit |
| `+5V_reg` distribution | `J1-6/7/8` | `J4-6/7/8` | Keep |
| `+3.3V_Reg` distribution | `J1-11/12/13` | `J4-11/12/13` | Keep |
| `+V Adj Channel` distribution | `J2-13/14/15` | `J6-13/14/15` | Keep |
| `+12V` feed | `J2-8` | `J6-10` | Keep |
| Bootstrap rail naming | `J2-7` currently `+5V_Bootstrap` and internal `+5V_Boot` also present | `J6-7` currently `+5V_Bootstrap` | Normalize to one net name |
| Ground returns | `J1-1`, `J2-1`, `J2-2` | `J4-1`, `J6-1`, `J6-2` | Keep |

Draft v1 connector impact:
- Baseline control/sense/fault contract: 13 core signal crossings (6 VSENSE + 3 ISET + 1 FAULT + 3 rail-control dependencies) plus rail/ground distribution pins.
- Reduced split edit target: drop all three `Feedback *` crossings and keep only VSENSE pairs as analog contract.

### Immediate KiCad Migration Actions (Next Implementation Pass)

1. Remove connector attachment for `Feedback 5V`, `Feedback 3.3`, `Feedback Adj Channel` on both board schematics and localize feedback processing to regulator side.
2. Normalize bootstrap naming (`+5V_Boot` vs `+5V_Bootstrap`) to one net contract before connector repin freeze.
3. On regulator schematic, rename `Net-(J2-Pin_3)` into an explicit connector-facing fault name to avoid hidden net semantics at the board boundary.
4. Re-export both netlists and re-run this pin-map table as a verification checklist.

### Phase 8: Automated Verification Gate

Use the script below after every netlist export:

- `powershell -ExecutionPolicy Bypass -File scripts/verify-connector-contract.ps1 -Mode baseline`
- `powershell -ExecutionPolicy Bypass -File scripts/verify-connector-contract.ps1 -Mode reduced`

Gate behavior:
- Exit code is non-zero on any failed check.
- `baseline` mode validates current-contract integrity.
- `reduced` mode enforces the target reduced contract (no connector crossing on `Feedback *` nets and single bootstrap naming).

Current status snapshot (2026-05-22):
- Baseline mode: 1 failure (`+5V_Boot` and `+5V_Bootstrap` naming both present).
- Reduced mode: 4 failures (three `Feedback *` crossings still present + bootstrap naming duplication).

Re-validation snapshot (2026-05-23):
- Baseline mode: 1 failure (bootstrap naming duplication still present).
- Reduced mode: 4 failures (same three `Feedback *` crossings plus bootstrap naming duplication).

Success criteria for next KiCad pass:
1. Reduced mode fails only on items not yet intentionally migrated.
2. After migration actions are complete, reduced mode returns all PASS.

### Phase 9: Connector Documentation Gap Closure

Current gap:
- Connector electrical/mechanical limits are not yet documented to production level.

Required documentation outputs:
1. Final connector current-capacity table with derating method used.
2. Final return-path allocation table (power returns vs quiet/signal grounds).
3. Connector part selection rationale (why selected, why alternatives rejected).
4. Assembly/service notes (mating order, latch engagement, inspection points).
5. Validation checklist for bring-up: contact resistance check, thermal rise check, and load-path verification.

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
| 2026-05-22 | Started implementation with verified cross-board contract, reduced-split draft, and pin-level connector contract (Draft v1) |
| 2026-05-22 | Added automated connector-contract verification gate and recorded baseline/reduced check status |
| 2026-05-23 | Pivoted to production connector architecture for two-board design (dedicated power connector + dedicated signal connector) |
| 2026-05-23 | Filled provisional connector current/return budget worksheet and marked follow-up to replace assumptions with bench-validated limits |
| 2026-05-23 | Added v0 candidate connector series matrix and screening gate for power + signal interconnect down-select |
| 2026-05-23 | Added RB-007 mechanical stack-height closure pack (measurement inputs, calculation worksheet, keepout checks, and pass/fail gate) |
| 2026-05-23 | Updated RB-007 policy: enclosure height non-limiting; spacing rule set to 13.54 mm minimum with 23.54 mm preferred target |
| 2026-05-23 | Converted RB-007 to target-driven connector sizing (`H_mated_target`) and added provisional defaults for unknown M7/M9 |
| 2026-05-23 | Added scenario-based stack sensitivity table (H_mated 8/9/10 mm) showing target-fit behavior before final connector part selection |
| 2026-05-23 | Added connector-family target-height filter matrix (8.0 .. 10.0 mm band, 9.0 mm ideal) to down-select families before part-number lock |
| 2026-05-23 | Recorded user-selected connector candidates: Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` for both power and signal connectors (two identical pairs) |
| 2026-05-23 | Power connector direction locked to Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` only |
| 2026-05-23 | Signal connector direction locked to second identical Samtec `CLP-110-02-L-D` + `FW-10-05-L-D-400-150-A` |
| 2026-05-23 | Added dual-connector capacity assumption: 40 total positions and 3.1 A/contact nominal (user-provided), with 70% derated planning value 2.17 A/contact |
| 2026-05-23 | Recorded active allocation snapshot from user: 18 signal pins plus 3 rails at 3.0 A each; provisional pin-count result 29 used / 11 spare |
| 2026-05-23 | Updated draft 40-pin dual-connector allocation to mixed distribution across both connectors (no strict power-vs-control split) and kept it as pre-layout electrical baseline |
| 2026-05-23 | Added SMT placement study gate (A/B/C variant workflow, hard gates, and scoring matrix) before final pin-map freeze |
| 2026-05-23 | Added v0 candidate scoring matrix and provisional finalist connector pair pending mechanical/current/procurement closure |
| 2026-05-23 | Added selected-PN datasheet validation ledger and recorded automated source-access blockers (HTTP 403/500) pending manual extraction |
| 2026-05-23 | Re-ran connector-contract verification gate: baseline still 1 fail (bootstrap naming), reduced still 4 fails (3 feedback crossings + bootstrap naming) |
