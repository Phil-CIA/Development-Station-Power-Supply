# Bucket 2 Rail-Control Scope (5V / 3V3 / CH3)

Status: active scoping doc for Bucket 2 execution.

This document defines the allowed rail-control firmware scope for Rev-C bring-up,
the non-goals, and the exact bench evidence required before claiming bucket closure.
It is a control document: implementation issues/PRs are artifacts under this scope,
not replacements for it.

## Scope owner and branch

- Primary bucket: Bucket 2 (Rail-control behavior)
- Planned branch: docs/firmware-bucket-2-rail-control-scope
- Upstream source of truth: docs/FIRMWARE_DEVELOPMENT_PLAN.md

## In scope now

- Deterministic boot-safe default output states on STM32 startup.
- Correct rail control-path ownership for +5V, +3V3, and CH3 control outputs.
- Correct ISET output control behavior on PA0, PA1, and PA2.
- Commanded rail enable and disable behavior validated against actual control-path
  hardware, not assumed from software state alone.
- Verification that enabling one rail does not unintentionally toggle another rail.

## Out of scope or blocked now

- New rail architecture redesign.
- Regulator board feature expansion unrelated to current Rev-C rail-control path.
- Inventing behavior contracts for nets not routed in current Rev-C hardware.
- Reclassifying hardware OCP topology as a software-only current-limit redesign.
- Any protocol-family changes not required for rail on/off and associated feedback.

## Rail-control contract (Rev-C)

The firmware must preserve a strict boot-safe sequence:

1. Power-on and reset state is safe/inactive for all controlled output paths.
2. Firmware initialization must not create brief unintended rail-enable pulses.
3. A rail-enable command may only affect the intended rail control path.
4. A rail-disable command must return the target control path to inactive state.
5. The observed control transitions must match the command and selected rail.

## Exit criteria

Bucket 2 is complete only when all criteria below are true on bench hardware:

1. Power-on defaults leave outputs inactive until firmware explicitly enables.
2. Rail enable/disable actions are isolated to the intended rail.
3. Measured gate/control-path transitions per rail match expected behavior.
4. At least one cold boot and one warm reset run produce identical safe defaults.
5. Results are captured in a reproducible state table and linked in the PR.

## Required evidence in PR description

- Bench procedure used, with hardware setup details.
- Rail state table with pre-command and post-command observed states.
- Transition evidence for each rail (scope capture, logic capture, or equivalent).
- Serial log excerpts showing command intent and response timing.
- Explicit pass/fail verdict for each exit criterion.
- Any deviations, with cause hypothesis and follow-up issue link.

## Bench evidence matrix (minimum)

| Test ID | Condition | Command | Expected observation | Evidence |
|---|---|---|---|---|
| B2-T01 | Cold boot | none | All controlled rails inactive by default | Rail state table + startup log |
| B2-T02 | Warm reset | none | Same inactive defaults as cold boot | Rail state table + reset log |
| B2-T03 | Idle safe state | Enable +5V | +5V control path toggles active; others unchanged | Transition capture + log |
| B2-T04 | +5V active | Disable +5V | +5V returns inactive; others unchanged | Transition capture + log |
| B2-T05 | Idle safe state | Enable +3V3 | +3V3 control path toggles active; others unchanged | Transition capture + log |
| B2-T06 | +3V3 active | Disable +3V3 | +3V3 returns inactive; others unchanged | Transition capture + log |
| B2-T07 | Idle safe state | Enable CH3 | CH3 control path toggles active; others unchanged | Transition capture + log |
| B2-T08 | CH3 active | Disable CH3 | CH3 returns inactive; others unchanged | Transition capture + log |
| B2-T09 | Rapid toggling | Enable/disable each rail x5 | No cross-rail glitches or unintended latching | Capture + summary table |

## Implementation touchpoints (primary)

- stm32-bluepill-bringup/src/main.cpp
- src/rev1/main.cpp (reference behavior only)
- hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net
- docs/STM32_BLUEPILL_PIN_TABLE.md
- docs/GPIO_PINOUT.md

## Guardrails

- Do not mark bucket complete based only on software variable state.
- Do not claim rail isolation without capture-backed evidence.
- Do not merge rail-control behavior changes without updating any stale docs.
- If a net/rail contract is ambiguous, resolve with netlist and pin-table updates
  before expanding firmware behavior.

## PR template block for Bucket 2

Use this block in PR descriptions touching Bucket 2 behavior:

```md
Bucket: Bucket 2 (Rail-control behavior)

Exit criteria check:
- [ ] Safe inactive power-on defaults verified
- [ ] Per-rail enable/disable isolation verified
- [ ] Control-path transitions captured per rail
- [ ] Cold-boot and warm-reset default parity verified
- [ ] Bench state table attached

Evidence links:
- Rail state table:
- Transition captures:
- Serial logs:

Bench-tested on real hardware: true/false
If false, reason:
Open follow-ups:
```

## Notes

- This scope doc is documentation-first by design. Firmware behavior expansion
  should follow in dedicated implementation PRs under the same bucket.
- If Rev-C routing changes, update pin/net references first, then re-baseline this
  bucket's rail-control expectations.

## Bench evidence run (2026-09-26, Rev-C, CSV-only)

Run context:

- Hardware revision: Rev C
- Rail path mode: switched
- Command path used: STM32 serial CLI on COM7
- Known bench constraints: none reported
- Firmware target/build id: unknown (traceability gap)
- Capture evidence type: none (no scope/logic attached)

### B2-S0..B2-S7 evidence table (this run)

| Step | Objective | Evidence summary | Verdict | Claim class |
|---|---|---|---|---|
| B2-S0 | Context lock | Rev C, switched mode, serial CLI command path, no constraints; build id unknown | Pass with traceability gap | SPP-capable context |
| B2-S1 | Cold boot default safety | Boot log showed AW policy-latched outputs low, D9 forced OFF, cfg default OFF state before commands | Pass | CSV |
| B2-S2 | Warm reset parity | Warm reset reproduced same default OFF behavior before commands | Pass | CSV |
| B2-S3 | 5V command/state enable | `D9ON` -> `d9: aw95xx P1.0 forced ON`; `CFGSHOW` -> `cfg d9=ON 5V[   ] 3V3[   ]` | Pass | CSV |
| B2-S4 | 5V command/state disable | `D9OFF` -> `d9: aw95xx P1.0 forced OFF`; `CFGSHOW` -> `cfg d9=OFF 5V[   ] 3V3[   ]` | Pass | CSV |
| B2-S5 | Non-target unchanged (serial/state) | During toggles, `CFGSHOW` continued to report `3V3[   ]` unchanged | Pass at serial/state level | CSV |
| B2-S6 | Rapid toggling stability | 5 ON/OFF cycles completed with consistent command/state reflections and no parser/runtime errors | Pass | CSV |
| B2-S7 | Claim separation closeout | No scope/logic captures provided; switched-path proof intentionally not claimed | Pass with bounded claim | CSV-only |

### Key serial excerpts captured

```text
=== TX: D9ON ===
d9: aw95xx P1.0 forced ON
=== TX: CFGSHOW ===
cfg d9=ON 5V[   ] 3V3[   ]

=== TX: D9OFF ===
d9: aw95xx P1.0 forced OFF
=== TX: CFGSHOW ===
cfg d9=OFF 5V[   ] 3V3[   ]

=== TX: Q39ON ===
range: Q3/Q9 forced ON via aw95xx P0.0+P0.5 (p0=0x21)
=== TX: QSTATE ===
range: p0=0x21 c0=0xC0 Q1=0 Q2=0 Q3=1 Q4=0 Q5=0 Q9=1 Q6=0

=== TX: Q612ON ===
range: Q6/Q12 forced ON via aw95xx P0.1 (p0=0x02)
=== TX: QSTATE ===
range: p0=0x02 c0=0xC0 Q1=0 Q2=1 Q3=0 Q4=0 Q5=0 Q9=0 Q6=1
```

### Claim-separation checklist (completed)

- [x] Run mode and command path were explicitly recorded.
- [x] Command/state validation was tracked independently of switched-path proof.
- [x] No switched-path claim was made from serial-only logs.
- [x] Non-target unchanged conclusion was bounded to serial/state visibility.
- [x] Rapid-toggle conclusion was bounded to command/state behavior.
- [ ] Scope/logic capture evidence attached for physical switched-path proof.

### Bounded conclusion for this run

Proven in this run:

- Deterministic command/state behavior for tested rail-control primitives over serial.
- 5V D9 command/state enable/disable reflects correctly and repeatedly.
- No serial-state indication of unintended 3V3 config-state changes during tested 5V toggles.

Not proven in this run:

- Physical switched-path electrical transitions per rail under real control-path measurement.
- Capture-backed rail-isolation proof at the hardware node level.

### Handoff block for next agent

```md
Bucket 2 bench evidence handoff (2026-09-26):

- CSV-only evidence run completed for B2-S0..B2-S7 on Rev-C.
- 5V command/state path validated using D9ON/D9OFF and CFGSHOW on COM7.
- Rapid toggle x5 completed with consistent ON/OFF command-state behavior.
- Additional parser-visible path toggles validated: Q3/Q39/Q612 with QSTATE readback.
- No scope/logic captures attached; do not claim switched-path electrical proof.
- Remaining gap: attach capture evidence before claiming B2 switched-path closure.
```

---

## Regulator Rev-C operational acceptance plan (draft, 2026-10-07)

**Status:** Draft for operator approval before live execution.  
**Posture:** Operation-first acceptance testing (not MOSFET fault-campaign-first).  
**User-reported state:** Hardware appears to be working.  
**Verification state:** Not independently bench-verified in this session.  
**Issue state:** #62 remains pending formal acceptance evidence.

### Governing source and boundaries

- Board under test: **Regulator Rev-C**.
- Authoritative staged design export: `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net` (export header date `2026-10-07T05:31:09`, tool `Eeschema 10.0.6`).
- Scope limited to **CH1 fixed +5V** and **CH2 fixed +3.3V** only.
- Out of scope for this draft run:
  - CH3/adjustable rail
  - Redesign actions
  - Firmware edits/reflash by default
  - Calibration writes or destructive persistence tests
  - Overload/OCP injection, short-circuit tests, CV/CC claims
  - Automatic range sweeps or make-before-break switching
- OUTPUT is shared user-facing control. Do **not** invent per-channel UI behavior.
- "Isolation" in this plan means one physical rail/range is intentionally under test while the other is unloaded/known-safe; if hardware/firmware cannot provide true physical isolation, mark **LIMITED/BLOCKED** explicitly.

### Command/bit/net table to verify before powered steps

Use this table to prevent alias/designator confusion. These command names are software aliases and are **not** literal regulator Q-reference ownership claims.

| CLI command | AW9523 P0 bit | Netlist signal (Rev-C export) | Hardware path intent | Bench rule |
|---|---:|---|---|---|
| `Q2ON/Q2OFF` | P0.1 | `ESP- GPIO 5V Hi` (`U5 P0_1_6` -> `U9 EN_UVLO_1`) | CH1 high-range driver enable input path | Verify electrical effect with measurements; do not trust command ACK alone |
| `Q1ON/Q1OFF` | P0.2 | `ESP- GPIO 5V Low` (`U5 P0_2_7` -> `U14 EN_UVLO_1`) | CH1 low-range control path | Same |
| `Q5ON/Q5OFF` | P0.3 | `ESP- GPIO 3.3V High` (`U5 P0_3_8` -> `U15 EN_UVLO_1`) | CH2 high-range control path | Same |
| `Q4ON/Q4OFF` | P0.4 | `ESP- GPIO 3.3V Low` (`U5 P0_4_10` -> `U16 EN_UVLO_1`) | CH2 low-range control path | Same |
| `Q3ON/Q3OFF` | P0.0 | `ISET_MPU_5V` (`U5 P0_0_5`) -> BSS138 control (`Q3`) | Buck ON/OFF control branch (not high-range pass FET) | Treat as control-path-only unless measured at U2.5 and rail nodes |
| `Q9ON/Q9OFF` | P0.5 | `ISET_MPU_3V3` (`U5 P0_5_11`) -> BSS138 control (`Q9`) | Buck ON/OFF control branch (not high-range pass FET) | Same |
| `QSTATE` | Readback | Alias print (`Q1/Q2/Q3/Q4/Q5/Q9/Q6`) from P0 register bits | Software state visibility | Not proof of electrical pass/fail |

**Important polarity note (must be resolved before live pass/fail claims):**
- The buck ON/OFF branch can be reverse-sense at the functional level (for example, a logic-high control could disable a buck path depending on regulator ON/OFF polarity and transistor stage).  
- Do **not** infer final ON/OFF polarity from netlist labels or command names alone; validate by paired bit-state + node-voltage evidence.
- Avoid legacy combined recipes (`QSEQ`, `Q39*`, `Q612*`) for acceptance gating unless explicitly re-authorized for a bounded purpose.

### P0 — preflight record (required before power-cycling)

Record all items before live execution:
1. Hardware revision markings and board photos.
2. Firmware repo commit/build ID and dirty/clean state.
3. Controller serial device path and instrument IDs.
4. Bench PSU set voltage and current limit.
5. Feedback/sense/jumper/bypass physical state.
6. Baseline command/log record:
   - `HELP`
   - `AWPROBE` (if supported)
   - `AWMODE` (if supported)
   - `QSTATE`
   - `INARAILS`
7. Confirm active link is **live hardware**, not demo/simulated path.

### P1 — cold boot and warm reset default-state check

1. Power cycle with no rail-control command; record:
   - Bit/readback state (`QSTATE` + any available mode info)
   - Terminal voltages (both rails)
   - Observation timestamps
2. Perform warm reset; repeat same captures.
3. Separate residual discharge observations from steady-state OFF expectations.
4. If safe inactive defaults cannot be demonstrated, mark **HOLD** and do not continue unattended.

### P2 — CH1 high-range operation sequence (+5V path)

1. Controlled sequence: OFF -> ON -> steady -> OFF under one agreed light load.
2. Capture for each step:
   - Reference DMM terminal voltage/current
   - Independent load/ref current reading
   - INA/controller/display reported values
   - Actual command and bit state
3. After first complete pass, repeat cycle **x5** under same conditions.

### P3 — CH1 low-range operation sequence

1. Run only within operator-approved low-range load.
2. Ensure load disabled and output off before selecting range.
3. Confirm safe OFF/discharge before switching path.
4. Never enable simultaneous opposing paths.
5. Capture same evidence set as P2.

### P4/P5 — repeat for CH2 (+3.3V)

- P4: CH2 high-range sequence with CH1 monitored as non-target.
- P5: CH2 low-range sequence with CH1 monitored as non-target.
- Record any cross-coupling/non-target movement explicitly.

### P6 — both channels selected ranges, shared OUTPUT behavior

1. Use verified ranges from earlier steps.
2. Apply light loads incrementally (within approved limits only).
3. Run shared OUTPUT cycle **x5** and capture rail stability/cross-coupling.
4. Include:
   - Initial settle observation target: ~60 s
   - Proposed light-load soak target: ~5 min (operator-approved)
5. End with confirmed physical OFF and safe load removal.

### Draft gates (require operator approval before PASS)

These are **proposed functional acceptance gates**, not final product limits:
- CH1 nominal voltage band: **4.75 V to 5.25 V** (+/-5%)
- CH2 nominal voltage band: **3.135 V to 3.465 V** (+/-5%)

Operator must approve before run:
1. Input current limit.
2. High/low range test currents by hardware rating.
3. Settling/discharge/OFF thresholds.
4. Instrument-based telemetry error tolerance.

If thresholds are unresolved, mark results **NOT ASSESSED** (not PASS).

### Stop/HOLD conditions

Stop and mark **HOLD** immediately on any of:
- Input current-limit hit
- Unexpected heating/smell/noise
- Out-of-approved-band terminal voltage
- Fault/reset events
- Non-target path activation

If safe, set OUTPUT off and capture logs. If not safe, bench input off.  
Do not auto-expand troubleshooting; seek operator authorization for any exception diagnostics.

### Evidence table schema (use for all run rows)

| Test ID | Rail | Range | Load/current | Command + actual bits | Ref V | Ref I | INA/display V/I | Other rail impact | Settling/soak | Capture path | Criterion | Verdict |
|---|---|---|---|---|---:|---:|---|---|---|---|---|---|

Allowed verdict values: `PASS`, `FAIL`, `HOLD`, `NOT RUN`, `NOT ASSESSED`.

### Operation evidence log (Regulator Rev-C, 2026-10-07)

Initial P0 baseline entry captured from live operator measurements.  
This entry is **observation-only** and does not satisfy formal acceptance criteria yet.

| Test ID | Rail | Range | Load/current | Command + actual bits | Ref V | Ref I | INA/display V/I | Other rail impact | Settling/soak | Capture path | Criterion | Verdict |
|---|---|---|---|---|---:|---:|---|---|---|---|---|---|
| P0-OBS-001 | CH1 +5V / CH2 +3.3V | N/A (baseline, no range command) | No external load; PSU set 12.09 VDC; PSU current limit 800 mA; idle input current 138 mA | `QSTATE`: `range: p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0` | CH1 terminal: 0.00034 V; CH2 terminal: 0.00034 V | Input current observed: 138 mA (idle) | Not captured in this step | Not assessed in this step | Steady unloaded snapshot only | User-reported live bench record (2026-10-07) | Baseline observation only; no cold boot/reset/transient/load criterion exercised | NOT ASSESSED |
| P0-OBS-002 | CH1 +5V / CH2 +3.3V | N/A (baseline, no range command) | Controller (Blue Pill) powered only from bench-powered board per operator report; no independent USB/supply power reported | `INARAILS` response captured (verbatim; see log snippet below) | PSU display reference unchanged from P0-OBS-001 (12.09 V, 138 mA idle) | INA incoming monitor reports 11.488 V, 95.56 mA (`rail 3V3 ... in`) | `rail 5V: hi 0.008V 0.00mA \| lo 0.000V 0.00mA`; `rail 3V3: hi 0.000V 0.00mA \| lo 0.000V 0.00mA \| in 11.488V 95.56mA`; `rail note: 0x41 CH3 is incoming rail monitor on this rev.` | No enable/load/reset action in this step | Steady unloaded snapshot only | User-reported live bench record (2026-10-07) | Observation-only logging; no formal accuracy/tolerance criterion executed | NOT ASSESSED |
| P1-OBS-001 | CH1 +5V / CH2 +3.3V | N/A (warm-reset observation only) | No channel enable/load step run in this record; coldboot capture unavailable because bench power OFF drops serial | Warm-reset boot log captured (verbatim; see snippet below). Key lines include `aw95xx boot: p0 forced low (0x00)` and later config restore `d9: aw95xx P1.0 forced ON`, `cfg d9=ON 5V[   ] 3V3[   ]` | Post-reset terminal readings reported by operator as unchanged near-zero relative to baseline (qualitative only; no new numeric values provided) | Warm-reset status lines report incoming monitor samples `Vin=11.480 Iin=95.56` then `Vin=11.480 Iin=97.78` | Warm-reset status stream captured; no post-reset `INARAILS`/final `QSTATE` row yet in this step | No explicit non-target activation observed in supplied text, but no transient capture provided | Warm-reset steady-state observation only | User-reported live bench reset log + qualitative post-reset terminal report (2026-10-07) | Warm-reset evidence only; default-OFF policy acceptance not proven without coldboot + transient capture + final post-reset state capture | NOT ASSESSED |
| P2-OBS-001 | CH1 +5V primary check / CH2 non-target observation | CH1 HIGH path command check only (no load); CH2 held non-target | User-approved CH1 HIGH no-load check scope: `Q2ON` path, CH2/LOW off, PSU still 12.09 V / 800 mA; provisional CH1 band 4.75..5.25 V | Post-reset QSTATE pasted as `ange: p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0` (prefix truncated in paste). Final OFF QSTATE later supplied exactly as `range: p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0`. Operator reported repeated ON/OFF cycling completed (five or more), but exact per-cycle command/QSTATE logs were not provided. | CH1 terminal reported 5.05 V during ON; operator also reported OFF response after OFF command (no separate numeric OFF voltage supplied) | Not supplied for this step (no measured load current reported; no PSU active-current value supplied) | CrowPanel displayed 5.05 V during CH1 ON; no additional telemetry values supplied in this step | CH2 explicitly observed/measured by operator and reported as remaining near-zero during CH1 ON (qualitative) | Repeated no-load ON/OFF behavior operator-reported (>=5), exact run count and per-cycle records not supplied; no light-load or soak in this step | User-reported live bench operation note (2026-10-07) | Functional no-load observation only: CH1 ON point in provisional band + qualitative repeated response; full P2/light-load/transient/protection acceptance still pending complete evidence set | NOT ASSESSED |
| P3-OBS-001 | CH1 +5V primary check / CH2 non-target observation | CH1 LOW path command check only (no load) | From confirmed OFF, operator instructed to run `Q1ON` then `Q1OFF`; proposed CH1 band 4.75..5.25 V; CH2 monitored as non-target | Operator response: `worked fine` (qualitative only). No new command log lines, QSTATE rows, or OFF-state numeric capture supplied in this step. | No new numeric CH1 voltage supplied in this step | Not supplied in this step | Not supplied in this step | No non-target anomaly reported in this step | Qualitative no-load functional check only | User-reported live bench note (2026-10-07) | Qualitative functional observation only; insufficient data for formal P3 acceptance metrics | NOT ASSESSED |
| P4-OBS-001 | CH2 +3.3V primary check / CH1 non-target observation | CH2 HIGH path command check only (no load) | From p0-zero check context, operator instructed `Q5ON` then `Q5OFF`; other paths off; proposed CH2 band 3.135..3.465 V | Operator reported CH2 `3.3234V` with ON/OFF success and no other-channel change. No exact ON/OFF QSTATE lines or OFF numeric terminal reading supplied. | CH2 active reading reported: 3.3234 V (do not interpret as OFF value) | Not supplied in this step | CrowPanel/extra telemetry numeric not supplied separately in this step | CH1 reported unchanged qualitatively while switching CH2 HIGH | No-load functional cycle only | User-reported live bench note (2026-10-07) | Point-in-time CH2 active value is inside provisional band; full P4 acceptance still pending complete ON/OFF state and telemetry/current evidence | NOT ASSESSED |
| P5-OBS-001 | CH2 +3.3V primary check / CH1 non-target observation | CH2 LOW path command check only (no load) | From p0-zero context, operator instructed `Q4ON` then `Q4OFF`; other paths off | Operator reported `3.3236 volts`, no influence on other channel, and CrowPanel responding with correct voltage + active indication. No exact ON/OFF QSTATE rows, OFF numeric terminal reading, or separate display-number capture supplied. | CH2 active reading reported: 3.3236 V | Not supplied in this step | CrowPanel qualitative response: correct voltage/active indication (no separate exact display number supplied) | CH1 reported unaffected qualitatively | No-load functional cycle only | User-reported live bench note (2026-10-07) | Point-in-time CH2 active value is inside provisional band; full P5 acceptance still pending complete ON/OFF state and telemetry/current evidence | NOT ASSESSED |
| P2-LOAD-001 | CH1 +5V loaded check / CH2 non-target observation | CH1 HIGH loaded spot-check only (CH1 LOW loaded deferred) | Operator proposed and approved 10 ohm 5W resistor for CH1 HIGH (~500 mA/~2.5 W nominal), PSU input limit 800 mA, provisional CH1 band 4.75..5.25 V. Pre-fit state confirmed: p0 zero and both terminals near zero. Bench input OFF used to fit resistor across CH1 terminals; CH2 left unloaded; sense wiring preserved. | Command sequence reported: verify p0 zero, `Q2ON` only, observe about 10 s, then `Q2OFF`. Exact ON-state QSTATE and dwell timestamp not supplied. Post-OFF return near-zero reported. | CH1 terminal measured 5.005 V while loaded (inside approved provisional band) | Independent CH1 load current not directly measured in this step; from nominal 10 ohm and 5.005 V, expected ~0.5005 A/~2.505 W is calculated only (not verified resistor value/current measurement) | Reported telemetry/indicators during loaded ON: CrowPanel 5.03 V, 0.066 A, 0.33 W; PSU reported 411 mA and 4.952 W input | CH2 concurrent loaded value not supplied; non-target disturbance not reported in this loaded step | Single loaded spot-check cycle only; no soak/repeat matrix in this step | User-reported live bench loaded check (2026-10-07) | Loaded voltage point + safe return OFF observed; telemetry/current-accuracy acceptance unresolved; not a full P2/protection/transient acceptance | NOT ASSESSED |
| P4-LOAD-001 | CH2 +3.3V loaded check / CH1 non-target observation | CH2 HIGH loaded spot-check only (CH2 LOW loaded deferred) | Same 10 ohm 5W resistor explicitly approved for CH2 HIGH (~330 mA/~1.1 W nominal), PSU input 12.09 V with 800 mA limit, provisional CH2 band 3.135..3.465 V. Bench power OFF used to move resistor CH1->CH2, allow cool-down, keep CH1 unloaded, preserve sense wiring. | Sequence reported: restore power, verify p0 zero, `Q5ON` (~10 s capture intent), then `Q5OFF` and confirm OFF. Exact ON/OFF QSTATE and dwell timestamp not supplied. | CH2 terminal measured 3.3141 V while loaded (inside approved provisional band) | Independent CH2 load current not directly measured; from nominal 10 ohm and 3.3141 V, expected ~0.33141 A/~1.098 W is calculated only (not verified resistor value/current measurement) | Reported telemetry/indicators during loaded ON: CrowPanel 3.32 V, 0.045 A, 1.1 W; PSU 12.09 V, 262 mA, 3.15 W input | Operator reported no other-channel influence and qualitative return-to-zero for outputs when OFF; exact concurrent CH1 numeric during ON not supplied | Single loaded spot-check cycle only; no soak/repeat matrix in this step | User-reported live bench loaded check (2026-10-07) | Loaded CH2 voltage point + qualitative OFF return observed; telemetry V/I/P consistency and current-accuracy acceptance unresolved; not a full P4/protection/transient acceptance | NOT ASSESSED |

Notes for this entry:
- No cold-boot/reset/transient/load test executed in this record.
- No build/assembly confirmation bundle captured in this record.
- Do **not** infer all internal buck rails OFF from latch bits alone (Q3/Q9 low is not equivalent to proven buck-disable state).
- The recorded 800 mA value is the configured **input PSU current limit only**; it is not an approved output load-test current/protection threshold.
- Input monitor mismatch (PSU display vs INA incoming monitor) is logged as an **unassessed reference-location/accuracy observation only**; it is not recorded as a failure diagnosis or calibration claim.
- No channel enable/load/reset tests are included in these two P0 observation rows.
- Coldboot capture remains pending/unavailable in current setup; warm-reset evidence is not a substitute for coldboot acceptance.
- Do **not** label default-OFF policy proven from this warm-reset sample alone (`P0 low at boot` + later `D9 restored ON`; no transient capture; no final post-reset `QSTATE` in this step).
- Operation testing can continue with bounded read-only post-reset state collection next.
- For P2-OBS-001, OFF `QSTATE` is now supplied; ON `QSTATE`, OFF numeric voltage, PSU active-current, and per-cycle logs/count are still missing and must remain unfilled until operator supplies them.
- No external-load, protection, transient, reset-acceptance, or full sequence verdict is claimed from P2-OBS-001.
- Four range paths are now operator-reported functional at **no load** (CH1 HIGH numeric, CH1 LOW qualitative, CH2 HIGH numeric, CH2 LOW numeric/qualitative) with bounded evidence limitations retained.
- Do not infer calibrated accuracy, loaded regulation, transient behavior, protection behavior, or coldboot acceptance from these no-load observations.
- Next phase is load-equipment capture and operator approval of bounded currents before any light-load operation evidence is logged.
- First loaded CH1 HIGH spot-check is now logged (P2-LOAD-001): voltage point passed provisional band and OFF return was reported; this is not full loaded acceptance.
- Telemetry discrepancy vs nominal resistor expectation is tracked as **NOT ASSESSED**; no auto-calibration, diagnosis, or regulation-fault conclusion is made from this single spot-check.
- Next requested step: operator approval and execution of CH2 HIGH loaded check with same resistor target (~330 mA/~1.1 W nominal); loaded LOW checks remain deferred.
- Loaded CH2 HIGH spot-check is now logged (P4-LOAD-001): terminal voltage point passed provisional band and qualitative OFF return was reported.
- CrowPanel CH2 telemetry internally conflicts (`3.32 V * 0.045 A ~= 0.1494 W` vs reported `1.1 W`), and current is below nominal 10 ohm expectation; this remains **NOT ASSESSED** with no automatic calibration/diagnosis inference.
- Both rails now have individual HIGH loaded voltage-point observations plus qualitative OFF return. Loaded LOW/protection/soak/transient/coldboot acceptance remain pending.
- Next phase (before combined operation) requires available second-rated resistor and explicit bounded-current approval.

Verbatim `INARAILS` capture for P0-OBS-002:

```text
rail 5V: hi 0.008V 0.00mA | lo 0.000V 0.00mA
rail 3V3: hi 0.000V 0.00mA | lo 0.000V 0.00mA | in 11.488V 95.56mA
rail note: 0x41 CH3 is incoming rail monitor on this rev.
```

Verbatim warm-reset boot log for P1-OBS-001:

```text
stm32-bluepill bringup: boot
fault path: AW9523 INT PB7
aw95xx boot: p0 forced low (0x00)
awmode gcr=0x10(pp) c0=0xC0 c1=0xFE m0=0xFF m1=0xFF
stm32-bluepill usart3: ready
cmd: HELP/DIAG/FTEST/AHT*/SR*/D9*/INA*/CAL*/CFG*/AW*
udi: CMD:OUTPUT|ILIM|GET*
flash: begin bring-up test
flash: JEDEC ID mfg=0xEF type=0x40 cap=0x18
flash: SR1 before=0x00
flash: erase/program/readback PASS
[CFG] Loaded
[CFG] REC r=O d=0 s=0
d9: aw95xx P1.0 forced ON
cfg d9=ON 5V[   ] 3V3[   ]
aht20: startup probe PASS
ina: 0x40=ACK 0x41=ACK
ina: incoming rail sample PASS
st f=PASS aw=PASS aht=PASS Vin=11.480 Iin=95.56 T=25.69 RH=50.60 n=1
st f=PASS aw=PASS aht=PASS Vin=11.480 Iin=97.78 T=25.73 RH=50.46 n=1
```

Additional verbatim/near-verbatim operator text for P2-OBS-001:

```text
ange: p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0
ok looks good , 5.05 as displayed on the crowpanel and verified by the volt meter and it responds to the command and also verifed off after off command sent
range: p0=0x00 c0=0xC0 Q1=0 Q2=0 Q3=0 Q4=0 Q5=0 Q9=0
actuall i did that and more , it only took 5 revs and 6 months to do that
worked fine
That worked too , 3.3234V off and on . no change inthe other channel while swithing
that work prefactly 3.3236 volts and no influnance on the other channel , the cropanel responds too with the correct voltage and it knows the channel is activelt on
CrowPanel5.03V,0.066A,0.33W; PSU411mA,4.952W; CH1 terminal5.005V
```

### Copy-paste VS Code Copilot execution prompt (single sequential bench session)

```md
Execute a single-session, operation-first bench acceptance run for Regulator Rev-C using PR #63 latest docs/netlist state. Do not split into parallel bench agents.

Read first (authoritative context):
1) README.md
2) docs/SYSTEM_DEVELOPMENT_WORKFLOW.md
3) docs/FIRMWARE_DEVELOPMENT_PLAN.md
4) docs/firmware-buckets/bucket-2-rail-control-scope.md (including "Regulator Rev-C operational acceptance plan (draft, 2026-10-07)")
5) docs/REGULATOR_BOARD_CHANGE_TRACKER.md (RB-003 posture)
6) docs/DISPLAY_INTERFACE_STANDARD.md
7) hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net (fresh export in PR #63)
8) stm32-bluepill-bringup/src/main.cpp (actual command/bit behavior)

Guardrails:
- Verify this checkout includes PR #63 latest netlist/plan updates before running.
- Never assume main branch has these updates.
- If checkout is dirty, report it and ask before touching unrelated files.
- Never switch/reset branches blindly.
- Never commit to main.
- No reflashing, power changes, or port-shopping unattended.
- Use existing guarded target/port tasks only.
- No firmware/schematic/PCB edits in this run.
- Keep #62/#63 open; do not close/merge or claim fix.

Direction settled by user:
- Hardware appears to be working; prioritize OPERATION TESTING.
- Test order: boot/reset defaults -> each rail and range path -> light-load regulation -> telemetry correlation -> both channels together.
- Current-limit/protection remains gated (no overload/short/OCP injection).
- Scope only CH1 fixed +5V and CH2 fixed +3.3V.
- No CH3/adjustable rail, no auto range sweep, no make-before-break claims.
- Shared OUTPUT is shared; do not invent per-channel UI.

Before powered steps, present for operator approval:
1) Verified command/bit/net mapping table from source + netlist (including alias caveats and reverse-sense possibility on buck control paths).
2) Proposed load/current/tolerance gates (explicit numbers) and stop conditions.
3) Any unresolved instrument/load/threshold inputs needed from operator.

Execution behavior:
- Perform one measurement step at a time.
- Wait for actual operator-provided measurements at each step.
- Never invent measurements or infer electrical pass from command ACK alone.
- Treat historical D9/CSV evidence as historical only; bounded claims only.
- If limits/thresholds are unresolved, use NOT ASSESSED rather than PASS.
- On anomaly, mark HOLD and stop for operator decision before scope expansion.

Evidence handling:
- Record results in existing tracker docs (no root-level dated handoff files).
- Keep timestamps aligned across command logs and measurements.
- Distinguish observation-only notes from formal criterion pass/fail.
- End by reporting completed checks, incomplete checks, and explicit remaining evidence gates.
```
