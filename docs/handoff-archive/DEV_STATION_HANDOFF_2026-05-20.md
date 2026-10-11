# Development Station Handoff (2026-05-20)

Purpose: carry forward the OCP comparator stage completion and test point rename work from today's schematic session.

## Session Intent And Stop Call

- Review and complete the analog instantaneous OCP comparator circuit on DSP Regulator HAT Rev-B.
- Confirm protection policy (global vs per-channel shutdown).
- Expand from single comparator slice to all three rails.
- Clean up test point labels before bench bring-up.

---

## What Was Completed

1. **Verified all three comparator ICs (U8/U9/U10) fully wired:**
   - All OUT1/OUT2 outputs from U8, U9, U10 land on `FAULT_CRITICAL_SUM` (global trip bus).
   - Spare channels OUT3/OUT4 on each IC are no-connect (DNP).
   - Confirmed in netlist export dated 2026-05-20T04:24:42-0500.

2. **Confirmed protection policy — global OCP trip:**
   - Any rail instant overcurrent asserts `FAULT_CRITICAL_SUM` → full system shutdown.
   - Rationale: current is beyond controllable range; fail-safe full shutdown preferred over selective isolation.
   - Documented in `docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md`.

3. **Confirmed 12V comparator supply / 3.3V fault logic is not a conflict:**
   - TLV1704 outputs are open-drain. The fault bus high level is set only by R65 pull-up to 3V3_ESP.
   - No level shifting or voltage attenuation needed.

4. **Input blanking RC networks placed for all 6 active comparator channels:**
   - C31–C36 (blanking caps), R67–R79 series sense resistors.

5. **Threshold trimmers placed:**
   - POT1–POT6, one per comparator input channel.

6. **Isolation diodes D9/D10/D11 added (1N5819WS Schottky, SOD-323):**
   - One per comparator IC output pair.
   - **DNP status for bench bring-up.** Footprints will be on board; populate after per-channel testing is complete.

7. **Weak pull-up added for bench testing** (fault bus held high during isolated comparator tests).

8. **Test point rename table established** (schematic edit still pending in KiCad at session close):

| Ref  | Old Value         | New Value              | Net                         |
|------|-------------------|------------------------|-----------------------------|
| TP2  | TP_OCP_5V_SENSE   | TP_OCP_5V_CH1_SENSE    | Net-(U8A-+IN1) — 5V shunt forward side |
| TP5  | TP_OCP_5V_SENSE   | TP_OCP_5V_CH2_SENSE    | Net-(U8B-+IN2) — 5V shunt return side  |
| TP6  | TP_OCP_5V_SENSE   | TP_OCP_3V3_CH1_SENSE   | Net-(U9A-+IN1) — 3.3V shunt forward    |
| TP8  | TP_OCP_5V_SENSE   | TP_OCP_3V3_CH2_SENSE   | Net-(U9B-+IN2) — 3.3V shunt return     |
| TP10 | TP_OCP_5V_SENSE   | TP_OCP_ADJ_CH1_SENSE   | Net-(U10A-+IN1) — Adj shunt forward    |
| TP9  | TP_OCP_5V_SENSE   | TP_OCP_ADJ_CH2_SENSE   | Net-(U10B-+IN2) — Adj shunt return     |
| TP3  | TP_OCP_5V_TRIP    | TP_OCP_TRIP            | FAULT_CRITICAL_SUM          |
| TP7  | TP_OCP_5V_TRIP    | TP_OCP_3V3_TRIP        | FAULT_CRITICAL_SUM (same node as TP3/TP11) |
| TP11 | TP_OCP_5V_TRIP    | TP_OCP_ADJ_TRIP        | FAULT_CRITICAL_SUM (same node) |

> Note: TP3, TP7, TP11 are electrically identical — all on `FAULT_CRITICAL_SUM`.
> Keeping three separate points is valid for PCB layout convenience (one near each IC).
> Alternatively, delete TP7 and TP11 and keep only TP3. User's call.

---

## Current State At Stop

- **2026-05-20 follow-up decision:** Fresh netlist export dated `2026-05-20T06:08:26-0500` is accepted for bench work. Test point value variants using `High`/`Low` wording are close enough to the intended channel naming and are **not** a blocking schematic item.
- **Active target:** DSP Regulator HAT Rev-B schematic — OCP comparator stage
- **Schematic file:** `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch`
- **Netlist file:** `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net` (accepted fresh export for bench baseline)
- **Safe resume point:** Begin bench bring-up of the 5V OCP channel using the accepted fresh netlist baseline.
- **Hardware left connected:** No bench changes this session — schematic work only.
- **Highest-risk unresolved item:** Trip threshold calibration (POT1–POT6 values not yet set).

---

## Known-Good Checks (Netlist-Verified)

1. U8, U9, U10 OUT1/OUT2 all on `FAULT_CRITICAL_SUM` — confirmed net code 29.
2. U8, U9, U10 supply pins on 12V/GND — confirmed.
3. POT1–POT6 each drive a threshold divider input to the correct comparator -IN pin.
4. Sense TPs (TP2/TP5/TP6/TP8/TP9/TP10) all on the +IN (signal) side, not the -IN threshold side.
5. D9/D10/D11 present in netlist with 1N5819WS Schottky values and SOD-323 footprint.
6. R65 pull-up (10kΩ) on `FAULT_CRITICAL_SUM` to 3V3_ESP — confirmed net membership.

---

## Still Blocked By

1. **POT threshold tuning** — set POT1–POT6 trim values for correct per-channel OCP current trip point. Do this at bench with each rail loaded.
2. **Isolation diode population** — D9/D10/D11 DNP until all channels bench-verified individually.

---

## Comparator-to-Rail Mapping (Reference)

| IC  | Channel | Sense TP  | Threshold Pot | Rail        | Shunt Side     |
|-----|---------|-----------|---------------|-------------|----------------|
| U8  | ch1 A   | TP2       | POT1          | 5V          | Forward        |
| U8  | ch2 B   | TP5       | POT2          | 5V          | Return         |
| U9  | ch1 A   | TP6       | POT3          | 3.3V        | Forward        |
| U9  | ch2 B   | TP8       | POT4          | 3.3V        | Return         |
| U10 | ch1 A   | TP10      | POT6          | Adj         | Forward        |
| U10 | ch2 B   | TP9       | POT5          | Adj         | Return         |

---

## Next Session Priority Order

1. Begin bench bring-up of 5V OCP channel (U8 ch1/ch2, POT1/POT2, probe TP2/TP5 + TP3).
2. Set POT1/POT2 threshold with a known resistive load on the 5V rail; confirm FAULT_CRITICAL_SUM trips at target current.
3. Repeat for 3.3V (U9) and Adj (U10) channels.
4. After all channels verified: populate D9/D10/D11 isolation diodes.

## First Action Next Session

- **2026-05-20 Session Pivot**: Bench work paused. Active priority is PCB redesign prep for Rev-C order.
- Start with [docs/PCB_REDESIGN_PREP_2026-05-20.md](../docs/PCB_REDESIGN_PREP_2026-05-20.md) — captures signal inventory, board-split decision matrix, and pending schematic edits.
- Next session will confirm three schematic decisions (see below) and then gather board measurements to resolve single vs split board trade-off.

---

## PCB Redesign Prep: Schematic Decision Lock (2026-05-20)

The following policy decisions are now locked for Rev-C prep:

1. **OCP Isolation Diodes (D9/D10/D11) Strategy**:
   - Decision: Keep **DNP** for first Rev-C build while preserving footprints.
   - Rationale: Keeps optional isolation path available without forcing comparator output shaping before full bench data.

2. **Unused Comparator Channels (U8C/U8D, U9C/U9D, U10C/U10D)**:
   - Decision: Tie unused `-IN` and `+IN` to GND; keep unused outputs disconnected from fault bus.
   - Rationale: Deterministic idle state and lower floating/noise risk.

3. **Bootstrap Net Naming (`+5V_Boot`)**:
   - Decision: Use single cross-board net name `+5V_Boot` and document source ownership on regulator side.
   - Rationale: Avoid connector-contract drift and keep netlist review simple.

Remaining order blockers are now mechanical fit/stack-height confirmation, footprint measurement verification, and continuity verification items captured in [docs/PCB_REDESIGN_PREP_2026-05-20.md](../docs/PCB_REDESIGN_PREP_2026-05-20.md).

Before board order, run the Footprint Measurement Gate in [docs/PCB_REDESIGN_PREP_2026-05-20.md](../docs/PCB_REDESIGN_PREP_2026-05-20.md) and log measured pitch/body/pad values for the changed footprint(s).

---

## 5V OCP Bench Run Card (Ready To Execute)

Use this exact sequence so trip-point data is comparable across sessions.

1. Pre-check power-off continuity:
   - Confirm TP2 and TP5 are on the 5V shunt sense nodes.
   - Confirm TP3 is on `FAULT_CRITICAL_SUM`.
   - Confirm D9/D10/D11 remain DNP during initial threshold tuning.
2. Probe setup:
   - CH1: TP2 (5V sense high).
   - CH2: TP5 (5V sense low).
   - CH3: TP3 (`FAULT_CRITICAL_SUM`).
   - Optional CH4: 5V rail output node for collapse timing.
3. Load setup:
   - Start with a known resistive or electronic load at a safe current below expected trip.
   - Increase load in small steps until first trip event.
4. Threshold tuning:
   - Adjust POT1/POT2 in small increments only.
   - After each trim step, repeat one controlled load ramp and record trip current.
5. False-trip check:
   - Run at least three load-step profiles below trip target.
   - Confirm no spurious `FAULT_CRITICAL_SUM` assertions.
6. Record outcome:
   - Trip current (A), repeatability over 3 runs, and fault response behavior.
   - Note whether recovery is deterministic after fault clear.

### 5V Pass/Fail Criteria

- PASS: Trip occurs within target band and repeats consistently across 3 runs.
- PASS: No false trips during expected non-fault load steps.
- PASS: `FAULT_CRITICAL_SUM` assertion causes expected shutdown behavior.
- FAIL: Trip threshold drifts significantly between runs or false trips occur.

### Bench Log Entry Stub (Paste Into bench-log.md)

- Date: [YYYY-MM-DD]
- Rail under test: 5V
- Load setup: [resistive/e-load details]
- Target trip current: [A]
- Measured trip current: [run1/run2/run3]
- False trips observed: [Y/N + condition]
- FAULT_CRITICAL_SUM behavior: [assert/clear timing notes]
- Recovery behavior: [deterministic/not deterministic]
- Scope captures: [file path or screenshot reference]

---

## Source Of Truth Files

- `docs/DEV_STATION_HANDOFF_2026-05-20.md` (this file)
- `docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md` (issue tracker, OCP policy documented)
- `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch` (schematic source)
- `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net` (accepted fresh export for bench baseline)

---

## Instructions For Next Chat

1. Read `docs/DEV_STATION_HANDOFF_2026-05-20.md` first.
2. The fresh HAT Rev-B netlist is accepted for bench work; TP naming variants are not a blocking task.
3. Bench bring-up order: 5V rail first, then 3.3V, then Adj.
4. Do not populate D9/D10/D11 until per-channel bench verification is complete.
5. Trip threshold (POT values) must be set empirically at bench — no calculated value is committed yet.
