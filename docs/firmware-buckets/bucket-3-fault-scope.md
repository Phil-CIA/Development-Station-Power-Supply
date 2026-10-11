# Bucket 3 Scope: Fault handling based on actual routed signals

Status: control document for Rev-C firmware execution scope; not a claim of
bench-verified protection coverage.

Recovered from PR #55, which merged into `phil-cia-firmware-scope-buckets-alignment`
after its base PR #50 had already merged to main. Updated for the current
source-reconciliation constraints.

Primary artifacts:
- `docs/FIRMWARE_DEVELOPMENT_PLAN.md`
- `docs/STM32_BLUEPILL_PIN_TABLE.md`
- `stm32-bluepill-bringup/src/main.cpp`
- Rev-C HAT schematic/netlist (`hardware/kicad/dsp-regulator-hat-rev-c/`)

## In scope now

- Compute `status` and `protection_flags` from runtime-observed state, not placeholder constants.
- Claim hardware fault coverage only when each bit is backed by an identified routed source and defined polarity.
- Keep unavailable fault sources explicit as "not observed on this revision"; distinguish software-derived warnings from hardware trips.
- Validate asserted and cleared transitions on bench for each implemented and source-backed fault source.
- Keep telemetry/event behavior aligned with the current command channel (`EVT:FAULT TRIP` / `EVT:FAULT CLEAR`).

## Out of scope / blocked by hardware

- Any direct STM32 critical-fault GPIO path not routed on Rev-C.
- Fan-fault conclusions requiring a dedicated tachometer net absent from the current Rev-C fan contract.
- Protection claims based only on software thresholds or unverified pin/device assumptions.
- New comparator/sensor hardware changes; this bucket validates firmware against existing identified hardware.
- Rev-D integration or a full Rev-C requalification campaign; neither is authorized by recovering this scope.

## Routed-signal reality constraints

- Firmware currently assumes AW9523 input/interrupt signaling on PB7. The current
  repository HAT schematic/netlist contains no AW9523 or interrupt path. Treat
  this as a source/firmware mismatch, not a verified routed fault source.
- Keep `PIN_FAULT_CRITICAL_SUM = -1`; do not enable a speculative direct GPIO.
- Use `docs/STM32_BLUEPILL_PIN_TABLE.md` for source identity and mismatches.
  Do not use the separate Regulator Rev-C designators to justify HAT routing.
- Every published protection/status bit must have:
  1. Source net/device owner, or an explicitly identified software-derived origin
  2. Active polarity and meaning
  3. Firmware read/computation path
  4. Bench test method and availability on the identified assembly
- Missing evidence blocks a coverage claim; it must not be replaced by a
  healthy-looking zero. This document does not change the existing wire schema.
- Automatic CV/CC indication remains gated by #77 and the fixed-rail display
  plan. A comparison against the RAM ILIM value does not prove regulation.

## Exit criteria (bench measurable)

1. Each in-scope, source-backed fault bit has repeatable assertion and clearing actions.
2. Telemetry bytes change with observed bench transitions; software-derived states remain labeled as such.
3. Fault transition events occur once per transition, without persistent false assertion after clear.
4. No constant placeholder is presented as valid hardware protection coverage.
5. A bit-source mapping table matches firmware, the identified hardware source, and captured behavior.

## Evidence required

- Bit-source mapping table: bit, meaning, source, polarity, firmware path, and availability.
- Assertion/clear captures for each source-backed bit, with before/after telemetry bytes.
- Consolidated log: startup baseline, induced trip, return to nominal, and `EVT:FAULT` traffic.
- Exact firmware references and matching netlist/pin-table entries.
- Explicit unresolved/source-mismatch entries for any bit lacking physical evidence.

## Issue mapping

| Issue | Scope relationship | Notes |
|---|---|---|
| #14 Current-limit recovery modes | Primary dependency | Tie semantics to real OCP sources, not software-only OCP claims. |
| #31 Fan driver foundation | Partial dependency | No tach-derived fault claims on the current routing. |
| #32 Fan startup self-test | Partial dependency | Report only electrically observable outcomes or explicit unavailability. |
| #34 Fail-safe policy + telemetry/UDI | Integration path | Export only accurately classified fault bits/events. |
| #36 Fan-control tracker | Evidence umbrella | Link observed behavior for each protection claim. |
| #77 Fixed-rail CV/CC validation | Coverage gate | Command acceptance and a software threshold do not prove current regulation. |

## Bench execution checklist

- [ ] Identify the assembly and reconcile only the source paths needed for the test.
- [ ] Record baseline status/protection bytes and classify unavailable sources.
- [ ] Capture each authorized source-backed assertion and clear action.
- [ ] Verify transition events and their polarity against observations.
- [ ] Distinguish software warnings from verified hardware protection.
- [ ] Attach logs, source references, and the bit mapping to the implementing PR.
