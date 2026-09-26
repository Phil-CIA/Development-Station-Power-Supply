# Bucket 6: Bring-up diagnostics and recovery paths (Rev-C scope control)

Status: active control document for Bucket 6 execution on Rev-C bring-up firmware.

## Scope intent

Define startup and failure-path diagnostics so a bench operator can identify whether a problem is firmware logic, contract/routing mismatch, or external hardware/wiring issue, and recover using explicit operator actions instead of silent fallback behavior.

## In scope now

- Startup diagnostics in boot logs for:
  - target firmware build/revision identity
  - board/revision assumptions used by runtime checks
  - explicit declaration of unavailable or unrouted fault/telemetry paths
- Failure taxonomy and logging for bring-up critical initialization paths:
  - transport/peripheral init failures
  - configuration/persistence load failures
  - sensor/expander read-path failures
  - display/host link startup failures (where applicable)
- Operator recovery guidance embedded in failure messages:
  - immediate suggested actions (power-cycle sequence, wiring checks, connector checks, fallback mode expectations)
  - "retry vs stop test" guidance for bench execution
- Consistent startup-test evidence capture flow used by bring-up runs and issue-linked validation.

## Out of scope / blocked by hardware

- Claiming diagnostics for signals/nets not routed or not observable on current Rev-C hardware.
- Silent emulation of unavailable hardware fault sources to appear "healthy."
- Full autonomous recovery state machines that mask root cause during bring-up.
- Production-grade fault coverage beyond what current routed nets and bench instrumentation can validate.

## Startup/failure diagnostics and operator recovery intent

Diagnostics must prioritize **operator clarity over autonomy** during bring-up:

- On startup, firmware reports what it expects from hardware and what is unavailable on this revision.
- On failure, firmware emits actionable, human-readable cause + next-step guidance, not only numeric codes.
- Recovery messaging must let the operator distinguish:
  - contract/wiring mismatch
  - peripheral initialization failure
  - runtime firmware fault
- Logs must be sufficient to classify failure class without attaching a debugger.

## Exit criteria (bench measurable)

1. **Startup contract visibility:** cold boot log includes revision assumptions and explicit unavailable-path declarations.
2. **Failure-path actionability:** each induced major initialization failure emits a message with a concrete operator recovery action.
3. **Operator diagnosis fidelity:** using logs alone, a bench operator can correctly classify at least three failure classes (wiring/contract, peripheral init, firmware/runtime path).
4. **Recovery repeatability:** documented recovery steps produce the expected post-recovery startup outcome in repeated runs.

## Evidence required

- Normal startup log capture (clean boot path).
- Induced failure captures for at least:
  - one wiring/contract mismatch scenario
  - one peripheral-init failure scenario
  - one persistence/config anomaly scenario
- Recovery execution notes with observed outcomes for each induced failure.
- Bench artifact set attached to the related PR/issue updates:
  - serial logs
  - concise test-step transcript
  - pass/fail summary table tied to exit criteria.

## Issue mapping

| Issue | Bucket 6 role | Scope disposition |
|---|---|---|
| #27 Bootup log and testing | Primary implementation issue for startup diagnostics and evidence capture | In scope now |
| #33 Milestone C AHT20 fan curve | Diagnostics/recovery validation dependency where sensor/path failures must be operator-visible | In scope now |
| #35 Milestone E bench fan-validation evidence capture | Primary evidence sink for bench proof of diagnostics and recovery outcomes | In scope now |
| #37 ESP32 startup test | Supporting startup-test context feeding Bucket 6 evidence model and failure taxonomy alignment | In scope now (support context) |
| #39 I2C startup test | Supporting startup-test context for bus-init/failure-path validation evidence | In scope now (support context) |

## Bench execution checklist

- [ ] Capture clean cold-boot startup log with revision/contract declarations.
- [ ] Induce and capture one wiring/contract mismatch startup failure.
- [ ] Induce and capture one peripheral initialization failure.
- [ ] Induce and capture one config/persistence anomaly path.
- [ ] Verify each failure log includes explicit operator recovery instructions.
- [ ] Execute documented recovery step(s) for each failure and capture outcomes.
- [ ] Produce a compact pass/fail matrix against all Bucket 6 exit criteria.
- [ ] Attach logs, matrix, and bench notes to the implementation PR as Bucket 6 evidence.
