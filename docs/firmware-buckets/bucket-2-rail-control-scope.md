# Bucket 2 — Rail control behavior (5V / 3V3 / CH3)

Status: active (documentation-first control scope).  
Primary issue owner: #28.

## Scope statement (Rev-C hardware reality)

This bucket defines only the rail-control behavior that can be proven on the current Rev-C bring-up path: boot-safe output states, deterministic enable/disable control, and per-rail ISET path behavior through real routed control paths.  
It is bring-up-first: no scope expansion beyond what current hardware routing and bench instrumentation can verify.

## In scope now

- Deterministic power-on safe inactive state before firmware enables any output path.
- Explicit rail control behavior for +5V, +3V3, and CH3 control path handling in current firmware targets.
- Validation that enable/disable actions affect only the intended rail/control path.
- Validation that ISET/output-control transitions map to expected behavior on actual bench hardware.
- Documentation of known Rev-C control-path constraints that affect rail behavior expectations.

## Out of scope / blocked by hardware

- New rail-architecture redesign or feature expansion unrelated to current Rev-C bring-up validation.
- Claims of rail-control behavior for unrouted or unobservable signals in this hardware revision.
- Any control-path behavior that cannot be measured on bench with current access points and instrumentation.
- Display/UI feature work that does not change or verify underlying rail-control behavior.

## Exit criteria (bench measurable)

1. **Boot-safe default state proven:** on power-up, outputs remain in safe inactive state until firmware explicitly enables control paths.
2. **Rail-selective control proven:** enable/disable commands transition only the intended rail path (no unintended cross-rail toggles).
3. **Control-path transitions proven:** expected gate/control transitions are observed for each applicable rail path (+5V, +3V3, CH3 path handling) during enable and disable sequences.
4. **Repeatability proven:** results are repeatable across at least three cold-boot test cycles with consistent outcomes recorded.

## Evidence required (logs/captures/steps)

- Bench procedure with explicit setup: power source, probe points, firmware revision, and command sequence.
- Rail state table per test step (initial, enable, disable, fault/recovery if applicable).
- Scope captures or equivalent captures for control-path transitions at enable/disable boundaries.
- Firmware/serial logs capturing command issuance and observed state acknowledgments.
- Short operator notes for any divergence between expected and observed behavior, including whether it is firmware-limited or hardware-limited.

## Issue mapping (Bucket 2 ownership and dependencies)

### Primary owner

- **#28 — Develop the cooling controls** (primary Bucket 2 execution issue in the current firmware plan mapping).

### Supporting evidence dependencies (startup/control-path relevance)

- **#27 — Bootup log and testing**: startup-log evidence source for boot-safe default-state claims.
- **#37 — ESP32 startup test**: startup behavior evidence feed where control-path startup sequencing is observed.
- **#39 — I2C startup test**: startup dependency evidence when rail-control path observations rely on initialized peripheral/control plumbing.

### Control-path context dependencies

- **#30 — Reconcile full STM32 pin contract across hardware, firmware, and docs**: pin/net correctness dependency for trustworthy rail-control interpretation.

## PR execution checklist (bench work)

- [ ] Confirm current firmware commit/branch and hardware revision under test are recorded in PR notes.
- [ ] Run and record at least three cold-boot cycles for boot-safe default-state validation.
- [ ] Execute per-rail enable/disable sequence and log outcomes in a rail state table.
- [ ] Capture at least one transition artifact per applicable rail/control path (+5V, +3V3, CH3 handling).
- [ ] Attach firmware/serial logs that align with bench timing and transitions.
- [ ] Classify any mismatch as firmware defect, hardware-routing limitation, or unresolved; avoid silent pass/fail claims.
- [ ] Link evidence back to #28 in PR description and reference supporting startup/control-path issues where used.

## Bring-up-first guardrails

- Keep this bucket scoped to proving current hardware behavior, not redesigning architecture.
- If evidence shows hardware-route blockers, document and carry as explicit blockers rather than adding speculative firmware-only workarounds.
- Update this scope only when `docs/FIRMWARE_DEVELOPMENT_PLAN.md` changes the control definition or issue ownership.
