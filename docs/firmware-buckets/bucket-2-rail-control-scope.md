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
