# Bucket 3 Scope: Fault handling based on actual routed signals

Status: control document for Rev-C firmware execution scope.

Primary artifacts:
- `docs/FIRMWARE_DEVELOPMENT_PLAN.md`
- `docs/STM32_BLUEPILL_PIN_TABLE.md`
- `stm32-bluepill-bringup/src/main.cpp`
- Rev-C HAT netlist (`hardware/kicad/dsp-regulator-hat-rev-c/`)

## In scope now

- Compute `status` and `protection_flags` from runtime-observed state, not placeholder constants.
- Report fault bits only when each bit is backed by a real routed source and defined polarity.
- Keep unavailable fault sources explicit as "not observed on this revision" (no synthesized values).
- Validate asserted and cleared transitions on bench for each implemented fault source.
- Keep telemetry/event behavior aligned with current command channel (`EVT:FAULT TRIP` / `EVT:FAULT CLEAR`) when fault state changes.

## Out of scope / blocked by hardware

- Any direct STM32 GPIO-based critical fault summation path that is not routed on Rev-C (for example direct `FAULT_CRITICAL_SUM` read path).
- Fan-fault conclusions that require a dedicated tachometer net not present on the current Rev-C fan connector contract.
- Fault categories that have no routed electrical source on this board revision, including "predicted" or inferred protection states.
- New comparator/sensor hardware changes; this bucket is firmware behavior on existing routed hardware only.

## Routed-signal reality constraints

- Fault handling must be wired to the actual Rev-C routed path (AW9523 input + interrupt signaling to STM32), not to hypothetical direct MCU fault pins.
- `PIN_FAULT_CRITICAL_SUM = -1` (or equivalent explicit unavailable marker) must remain intentional until routing changes are real and documented.
- Every published protection/status bit must have:
  1. Source net/device owner
  2. Active polarity
  3. Firmware read path
  4. Bench test method
- If any one of the four items above is missing, the bit is out of scope and must be marked unavailable.

## Exit criteria (bench measurable)

1. For each in-scope fault bit, a bench action exists that reliably asserts it and a bench action exists that clears it.
2. Telemetry `status` and `protection_flags` bytes change in real time with bench-induced fault transitions.
3. Fault transition events are emitted once per state transition (trip and clear), with no persistent false assertion after clear.
4. No bit remains mapped to constant placeholders except fields explicitly marked unavailable for Rev-C.
5. A bit-source mapping table is published and matches firmware behavior under bench test.

## Evidence required

- Annotated bit-source mapping table (bit index, meaning, source path, polarity, availability on Rev-C).
- Bench capture set for each in-scope bit:
  - assertion condition
  - clear condition
  - telemetry/log excerpt showing before/after bytes
- One consolidated test log showing:
  - startup baseline (no induced faults)
  - induced fault trip
  - clear to nominal state
  - resulting `EVT:FAULT` traffic
- References to exact firmware lines and netlist/pin-table entries used to justify each mapped bit.

## Issue mapping

| Issue | Scope relationship | Notes |
|---|---|---|
| #14 Port/redesign current-limit modes to STM32 | Primary in-scope dependency | Fault semantics must remain tied to routed OCP/related sources; no software-only synthetic OCP claims. |
| #31 Milestone A fan control driver foundation | Partial dependency | Driver state can contribute to fault-context reporting, but no tach-derived fault claims on current Rev-C routing. |
| #32 Milestone B fan startup self-test | Partial dependency | Self-test can report command-path or timeout outcomes only where electrically observable signals exist. |
| #34 Milestone D fail-safe policy + telemetry/UDI integration | Primary integration path | This bucket defines which fault bits/events are legitimate for UDI/telemetry export. |
| #36 Fan-control milestone tracker | Tracking umbrella | Must carry evidence links proving bench-observed trip/clear behavior for every claimed fault. |

## Bench execution checklist

- [ ] Confirm current routed-signal assumptions against `docs/STM32_BLUEPILL_PIN_TABLE.md` and Rev-C netlist before running tests.
- [ ] Record baseline `status` and `protection_flags` with nominal operating conditions.
- [ ] Induce each in-scope fault condition one at a time; capture assertion evidence.
- [ ] Clear each induced condition; capture de-assertion evidence.
- [ ] Verify `EVT:FAULT TRIP` / `EVT:FAULT CLEAR` event behavior for each transition.
- [ ] Verify unavailable fault sources are represented as unavailable, not zeroed placeholders presented as valid measurements.
- [ ] Attach logs/captures and bit mapping table to the implementing PR description.
