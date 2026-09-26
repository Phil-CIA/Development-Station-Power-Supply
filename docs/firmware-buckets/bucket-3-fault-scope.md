# Bucket 3 Fault-Handling Scope (Routed-Signal Evidence Only)

Status: active scoping doc for Bucket 3 execution.

This document defines the allowed fault-handling firmware scope for current Rev-C
bring-up, with explicit proof boundaries tied only to routed and observable
signals. It is a control document: issues and PRs implement this scope, but do
not replace it.

## Scope owner and branch

- Primary bucket: Bucket 3 (Fault handling based on actual routed signals)
- Planned branch: docs/firmware-bucket-3-fault-scope
- Upstream source of truth: docs/FIRMWARE_DEVELOPMENT_PLAN.md

## In scope now

- Fault-bit ownership for telemetry `status` and `protection_flags` based only on
  currently routed and firmware-observable sources.
- Explicit representation of unavailable/unobservable fault inputs as
  not-observed for this revision (not synthesized values).
- Runtime event behavior for fault transitions (`EVT:FAULT TRIP` /
  `EVT:FAULT CLEAR`) tied to the active Rev-C observation path.
- Bench evidence format and step IDs required to claim Bucket 3 closure.

## Out of scope or blocked now

- Any claim of per-channel OCP source separation unless that separation is
  physically routed and observable in current hardware.
- New virtual mappings or inferred fault sources not backed by netlist + code.
- Architecture redesign of comparator/fault topology.
- Closing fan-control milestones (#31, #32, #34, #36) as proof of generic fault
  coverage without Bucket 3-specific bit evidence.

## Fault-bit ownership policy (routed signals only)

Bucket 3 ownership is assigned only when all three are true: routed net exists,
firmware read path exists, and bit polarity is documented.

| Telemetry field | Bit(s) | Source class | Current Rev-C ownership policy | Claim status |
|---|---|---|---|---|
| `status` | CH1/CH2 enabled | Derived runtime state | Allowed when based on measured/commanded runtime state | Prove with logs + bench state context |
| `status` | CH1/CH2 CV/CC | Derived runtime state | Allowed when comparator/threshold logic is explicit in code | Prove with threshold-crossing run |
| `status` | Thermal warn | Routed sensor path | Allowed from AHT20 sampled path when sample validity is shown | Prove with captured temperature crossing |
| `protection_flags` | CH1/CH2 OVP | Routed measurement path | Allowed from bus-voltage threshold logic with documented thresholds | Prove with threshold-crossing run |
| `protection_flags` | CH1/CH2 OCP | Shared fault-summary path | Allowed only as shared assertion when fault-summary path is observable; no per-channel ownership claim | Prove shared trip/clear only |
| `protection_flags` | CH1/CH2 OTP | Routed sensor path | Allowed as global thermal event mirrored into both bits with explicit policy note | Prove with OTP threshold crossing |

Required non-claim policy:

- If a source is not routed to STM32 directly (for example direct
  `FAULT_CRITICAL_SUM` GPIO path), document it as not observed on this revision.
- If only shared fault summary is observable, do not claim channel attribution.
- If a measurement source is invalid/unavailable at runtime, mark corresponding
  proof step as not proven.

## Exit criteria

Bucket 3 is complete only when all criteria below are true for current Rev-C:

1. Every asserted fault-related telemetry bit has a documented source, polarity,
   and ownership boundary in this doc.
2. No reported bit depends on an unrouted or speculative signal mapping.
3. Assert and clear behavior is demonstrated for each in-scope fault class with
   reproducible bench steps and evidence references.
4. Any unavailable source is explicitly reported as not observed, with no
   success-shaped fallback values.
5. PR description includes a proven/not-proven report and confidence uplift path.

## Required evidence in PR description

- Bit-source mapping table (field, bit, source net/path, polarity, claim class).
- Bench worksheet results for all B3 step IDs below.
- Serial/log excerpts that show asserted and cleared transitions.
- Capture references (scope/logic/host log) for each claimed physical transition.
- Explicit list of not-proven items and why they remain open.

## Bench worksheet (stable IDs)

Use these step IDs unchanged across runs to keep evidence comparable:

| Step ID | Objective | Required observation | Evidence type | Pass rule |
|---|---|---|---|---|
| B3-S0 | Context lock | Hardware rev, firmware build ID, signal path mode recorded | Run header | All context fields present |
| B3-S1 | Ownership map freeze | Bit-source map matches this doc and current firmware | Table diff + code refs | No unmapped asserted bits |
| B3-S2 | OVP assert/clear path | OVP bit(s) assert above threshold and clear below | Log + capture | Both transitions seen |
| B3-S3 | Thermal warn assert/clear path | Warn bit asserts/clears at documented threshold crossing | Log + temp evidence | Both transitions seen |
| B3-S4 | OTP assert/clear path | OTP bit policy behavior asserts/clears as documented | Log + temp evidence | Both transitions seen |
| B3-S5 | Fault-summary assert/clear path | Shared OCP summary trip/clear reflected in flags and EVT messages | Log + capture | Trip and clear both seen |
| B3-S6 | Unavailable-source handling | Unrouted sources reported as not observed, not fabricated | Log + report | No fabricated source claims |
| B3-S7 | Claim-separation closeout | Proven vs not-proven split consistent with evidence attached | Final report | No over-claiming |

## Pass/fail matrix template

| Step ID | Verdict (Pass/Fail/Blocked) | Evidence reference | Notes |
|---|---|---|---|
| B3-S0 |  |  |  |
| B3-S1 |  |  |  |
| B3-S2 |  |  |  |
| B3-S3 |  |  |  |
| B3-S4 |  |  |  |
| B3-S5 |  |  |  |
| B3-S6 |  |  |  |
| B3-S7 |  |  |  |

## Claim-separation checklist

- [ ] Every claimed bit maps to a routed signal path or explicit derived runtime rule.
- [ ] No claim relies on unrouted or speculative nets.
- [ ] Shared-fault-only paths are reported as shared, not per-channel attributed.
- [ ] Assert and clear are both evidenced for each claimed fault class.
- [ ] Missing evidence is listed under not proven (not silently omitted).
- [ ] Confidence statement references concrete next evidence to collect.

## Reporting template (required)

Use this block in PR descriptions touching Bucket 3 behavior:

```md
Bucket: Bucket 3 (Fault handling based on actual routed signals)

Proven:
- 

Not proven:
- 

Confidence uplift path (next evidence to close gaps):
- 

Bench worksheet summary:
- B3-S0:
- B3-S1:
- B3-S2:
- B3-S3:
- B3-S4:
- B3-S5:
- B3-S6:
- B3-S7:

Bench-tested on real hardware: true/false
If false, reason:
Open follow-ups:
```

## Implementation touchpoints (primary)

- stm32-bluepill-bringup/src/main.cpp
- docs/STM32_BLUEPILL_PIN_TABLE.md
- docs/GPIO_PINOUT.md
- docs/FIRMWARE_DEVELOPMENT_PLAN.md
- hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net

## Notes

- This scope pass is docs-first and does not, by itself, close Bucket 3 evidence.
- Bucket 3 closure requires bench data from the worksheet above.
