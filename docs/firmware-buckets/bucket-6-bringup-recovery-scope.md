# Bucket 6: Bring-up diagnostics and recovery paths (Rev-C scope control)

Status: active control document; diagnostic coverage and recovery remain
bench-evidence requirements.

Recovered from PR #54, which merged into `phil-cia-firmware-scope-buckets-alignment`
after base PR #50 merged to main.

## Scope intent

Let the operator distinguish firmware, source/contract, and wiring/peripheral
failures from logs, then recover through explicit authorized actions rather than
silent fallback.

## In scope now

- Startup build/target identity, hardware assumptions, and unavailable-path declarations.
- Failure classification for transport, peripheral initialization, config load,
  sensor/expander reads, and host/display link startup.
- Actionable cause and recovery guidance, including retry versus stop-test decisions.
- Repeatable startup-test evidence capture linked to the owning issue/bucket.

## Out of scope / blocked by hardware

- Diagnostic coverage claims for unrouted or unobservable signals.
- Silent emulation of unavailable hardware as healthy.
- Autonomous recovery that hides the root cause.
- Production fault coverage beyond identified hardware and measured behavior.
- A mandatory full Rev-C requalification or Rev-D integration campaign. The
  pin table's current user-directed carry-forward plan remains controlling.

## Startup/failure diagnostics and recovery intent

- Prioritize operator clarity: expected source/target, unavailable paths, cause,
  and a concrete next action.
- `DIAG` and existing startup health snapshots provide current diagnostics;
  richer recovery guidance is still a criterion, not newly implemented here.
- Firmware's AW9523/interrupt assumptions conflict with the current HAT
  schematic/netlist. Report the mismatch rather than calling it verified.
  Use `docs/STM32_BLUEPILL_PIN_TABLE.md` for source identity and limitations.
- Distinguish wiring/contract, peripheral initialization, config recovery, and
  runtime failures. Receipt of telemetry does not resolve a source mismatch.
- F405 compilation does not establish hardware readiness or bench validation.

## Exit criteria (bench measurable)

1. Cold-boot logs identify target/revision assumptions and unavailable paths.
2. Each tested major failure produces a concrete operator action.
3. Logs alone distinguish at least three failure classes.
4. Documented recovery steps produce repeatable expected startup outcomes.
5. Source mismatches remain explicit and are not silently classified as healthy.

## Evidence required

- Normal startup log.
- Authorized failure captures: one wiring/contract mismatch, one peripheral
  failure, and one config/persistence anomaly.
- Recovery steps and observed outcomes for each induced failure.
- Hardware/source identity and firmware revisions, serial logs, procedure
  transcript, and pass/fail matrix tied to criteria.
- Explicit untested/blocked entries rather than inferred passes.

## Issue mapping

| Issue | Role | Disposition |
|---|---|---|
| #27 Bootup log/testing | Startup diagnostics and evidence | Primary |
| #33 AHT20 fan curve | Sensor/path diagnostics | Supporting |
| #35 Fan bench evidence | Recovery evidence sink | Supporting |
| #37 ESP32 startup | Startup context | Supporting; does not authorize testing a replaced module |
| #39 I2C startup | Bus-init/failure evidence | Supporting |

## Bench execution checklist

- [ ] Record exact build/target, assembly, and source assumptions.
- [ ] Capture normal boot and current unavailable-path warnings.
- [ ] Define authorized failure injection and recovery before powered testing.
- [ ] Capture wiring/contract, peripheral, and config failure classes.
- [ ] Verify logs contain actionable retry/stop guidance.
- [ ] Execute recovery and record repeated outcomes.
- [ ] Attach the evidence matrix; keep untested cases open.
