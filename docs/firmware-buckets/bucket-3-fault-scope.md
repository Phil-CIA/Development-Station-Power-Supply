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

## Bench execution runbook (operator)

Use this runbook to execute one Bucket 3 evidence pass with reproducible logs.
It does not redefine pass/fail criteria; it provides the capture sequence for the
`B3-S0..B3-S7` worksheet.

### Pre-run capture setup

1. Start two log captures:
   - STM32 debug serial console (command/response log).
   - Display-link UART capture (to record `EVT:FAULT TRIP/CLEAR`, `ACK:*`, `ERR:*`).
2. Record run header fields before commands:
   - Date/time, hardware revision, firmware commit SHA, operator initials,
     board wiring notes, induced-fault method.
3. Confirm startup path:
   - Boot log includes `fault path: AW9523 INT PB7`.

### Command baseline (before inducing any condition)

Run and capture:

```text
HELP
DIAG
INANOW
AHTNOW
CFGSHOW
```

For display-link/UDI fault-state baseline, capture:

```text
CMD:GET STATE
```

Expected baseline rule:
- Fault state may be `NA` before first valid AW9523 fault sample; do not claim
  this as clear/assert proof. Continue after a valid sampled state is observed.

### Step-oriented execution mapping

| Worksheet step | Execute | Capture expectation |
|---|---|---|
| B3-S0 | Record run header + startup line | Full context lock is present |
| B3-S1 | Snapshot current bit-source map from this doc + firmware refs | No asserted bit without mapping |
| B3-S2 | Induce/remove OVP condition per bench method | Assert then clear in logs/capture |
| B3-S3 | Raise/lower temperature across warn threshold | Warn assert then clear |
| B3-S4 | Raise/lower temperature across OTP threshold | OTP assert then clear |
| B3-S5 | Induce/remove shared fault-summary condition | `EVT:FAULT TRIP` then `EVT:FAULT CLEAR`; shared OCP flags transition |
| B3-S6 | Validate unrouted-source handling | No fabricated direct GPIO fault-source claim |
| B3-S7 | Complete proven/not-proven report | Claim separation matches attached evidence |

### Minimum artifacts to attach

- Debug console log text file.
- UDI/display-link UART log showing at least one `EVT:FAULT TRIP` and one
  `EVT:FAULT CLEAR` for any claimed B3-S5 pass.
- Capture images/files referenced by step ID (`B3-S2`..`B3-S5` as applicable).
- Completed pass/fail matrix with one evidence reference per row.

## Pass/fail matrix template

| Step ID | Verdict (Pass/Fail/Blocked) | Evidence reference | Notes |
|---|---|---|---|
| B3-S0 | Pass | 2026-09-26 run header below + branch/worktree verification | Context lock captured: Rev-C serial path on COM7, branch `phil-cia-bucket-3-execution`, commit `afa2410`. |
| B3-S1 | Pass (code-map freeze) | Firmware refs listed in "Bench evidence run (2026-09-26)" | Bit ownership boundaries are mapped from current source; no bench assert/clear claimed here. |
| B3-S2 | Blocked | Baseline command transcript below | OVP assert/clear induction not executed in this pass; no threshold crossing evidence captured. |
| B3-S3 | Blocked | `AHTNOW` sample only (`aht20: T=27.02C RH=48.93% status=0x18`) | Thermal warn threshold crossing was not induced; no assert/clear evidence. |
| B3-S4 | Blocked | `AHTNOW` sample only (`aht20: T=27.02C RH=48.93% status=0x18`) | OTP threshold crossing was not induced; no assert/clear evidence. |
| B3-S5 | Blocked | COM12 monitor had no `EVT:FAULT TRIP/CLEAR`; COM7 image rejected runbook UDI state commands | Shared fault-summary trip/clear not observed in logs during this pass. |
| B3-S6 | Pass (policy evidence) | Source refs in "Bench evidence run (2026-09-26)" (`PIN_FAULT_CRITICAL_SUM = -1`, `fault path: AW9523 INT PB7`) | Unavailable direct GPIO source is explicitly disabled/not claimed; no fabricated direct source evidence found in current code mapping. |
| B3-S7 | Pass with bounded claim | Proven/not-proven report below | Claim separation maintained: only context/code-map policy proven; fault assert/clear classes remain not proven. |

## Bench evidence run (2026-09-26, runbook compatibility gap)

Run context:

- Date/time: 2026-09-26 (live bench session)
- Hardware revision: Rev-C (operator context)
- Branch/worktree: `phil-cia-bucket-3-execution` at `afa2410`
- Serial paths used: COM7 (STM32 CLI/log), COM12 (display-link UART monitor)
- Operator: Copilot App
- Induced-fault method: not executed in this pass (compatibility gate)

Branch/worktree verification captured:

```text
git branch --show-current
phil-cia-bucket-3-execution

git log --oneline -n 5
afa2410 (HEAD -> phil-cia-bucket-3-execution) Add Bucket 3 bench runbook
5d9dbcc Add Bucket 3 fault scope doc
6886a8d (origin/main, origin/HEAD, main) inital save filled out
...
```

Runbook baseline command transcript (COM7):

```text
HELP
cmd: HELP/FTEST/AHT*/SR*/D9*/INA*/CAL*/CFG*/AW*/SIMFAIL*
udi: CMD:OUTPUT|ILIM|GET*

DIAG
cmd: unknown 'DIAG'

INANOW
ina 5V 0x40: CH1 4.000V 1.00mA | CH2 4.000V 0.60mA | CH3 0.000V 0.00mA
ina 3V3 0x41: CH1 0.000V 0.00mA | CH2 0.000V 0.00mA | CH3 11.456V 102.22mA

AHTNOW
aht20: T=27.02C RH=48.93% status=0x18

CFGSHOW
cfg d9=OFF 5V[   ] 3V3[   ]

CMD:GET STATE
cmd: unknown 'CMD:GET STATE'
udi: CMD:OUTPUT|ILIM|GET*

GET STATE
cmd: unknown 'GET STATE'
```

Display-link UART capture status (COM12):

- Monitor opened successfully at 115200.
- No `EVT:FAULT TRIP` / `EVT:FAULT CLEAR` observed in this run.

Firmware ownership/policy mapping refs used for B3-S1/B3-S6:

- `stm32-bluepill-bringup/src/main.cpp`: direct fault GPIO intentionally unavailable (`PIN_FAULT_CRITICAL_SUM = -1`).
- `stm32-bluepill-bringup/src/main.cpp`: startup path prints `fault path: AW9523 INT PB7`.
- `stm32-bluepill-bringup/src/main.cpp`: shared OCP summary mirrors to both OCP bits when observable.
- `stm32-bluepill-bringup/src/main.cpp`: thermal warn/OTP and OVP bits are runtime-derived before telemetry publish.

### B3-S5 follow-up attempt (2026-09-26, targeted)

Objective:

- Close B3-S5 by capturing shared fault-summary trip/clear with display-link
  `EVT:FAULT TRIP` and `EVT:FAULT CLEAR`.

What was executed:

```text
COM7 monitor opened @115200
COM12 monitor opened @115200

COM7> HELP
cmd: HELP/FTEST/AHT*/SR*/D9*/INA*/CAL*/CFG*/AW*/SIMFAIL*

COM7> SIMFAIL
cmd: unknown 'SIMFAIL'

COM7> SIMFAIL ON
simfail: usage SIMFAIL <AHT|INA|FLASH|ALL> <ON|OFF>

COM7> SIMFAIL OFF
simfail: usage SIMFAIL <AHT|INA|FLASH|ALL> <ON|OFF>
```

Observed result:

- Active image exposes only simulated subsystem failure controls
  (`AHT|INA|FLASH|ALL`), not a fault-summary trip/clear injection command.
- COM12 capture still contained no `EVT:FAULT TRIP` / `EVT:FAULT CLEAR`.

Disposition update:

- B3-S5 remains **Blocked** for this run because no valid induced shared-fault
  transition was available through the exposed command set, and no physical
  induced trip/clear sequence was executed in this session.

### B3-S5 follow-up attempt (2026-09-26, compile-unblocked rerun)

Objective:

- Remove STM32 image mismatch risk, then re-run B3-S5 event capture.

Execution summary:

```text
1) Fixed STM32 serial declaration compatibility in stm32-bluepill-bringup/src/main.cpp
  - switched to HardwareSerial pin-pair constructors for SerialDbg/SerialU3

2) Build and upload from phil-cia-bucket-3-execution
  - platformio run -d stm32-bluepill-bringup -e bluepill_f103c8  => SUCCESS
  - platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload => SUCCESS

3) Baseline verification after flash (COM7)
  HELP -> cmd: HELP/DIAG/FTEST/AHT*/SR*/D9*/INA*/CAL*/CFG*/AW*
  DIAG -> diag:fault-awint

4) Fault-path stimulation attempts (COM7)
  AWPROBE -> aw95xx: candidate ACK at 0x58
  AWMODE  -> aw95xx: forced GPIO + push-pull mode ...
  Q39ON/Q39OFF toggles executed

5) Display-link monitor (COM12)
  no EVT:FAULT TRIP or EVT:FAULT CLEAR observed during this rerun
```

Disposition update:

- Compile/upload blocker is closed for this branch run.
- B3-S5 remains **Blocked** because required shared-fault trip/clear evidence
  (`EVT:FAULT TRIP` and `EVT:FAULT CLEAR`) was still not observed on COM12 during
  this rerun.
- No synthetic shared-fault injection command is exposed in this flashed image;
  physical induced-fault method remains required for closure evidence.

## Claim-separation checklist

- [x] Every claimed bit maps to a routed signal path or explicit derived runtime rule.
- [x] No claim relies on unrouted or speculative nets.
- [x] Shared-fault-only paths are reported as shared, not per-channel attributed.
- [ ] Assert and clear are both evidenced for each claimed fault class.
- [x] Missing evidence is listed under not proven (not silently omitted).
- [x] Confidence statement references concrete next evidence to collect.

## Reporting template (required)

Use this block in PR descriptions touching Bucket 3 behavior:

```md
Bucket: Bucket 3 (Fault handling based on actual routed signals)

Proven:
- Branch/worktree and doc scope lock for `phil-cia-bucket-3-execution` (`afa2410`, `5d9dbcc`) verified during live bench session.
- Code-level ownership map freeze completed against current firmware: fault source path is AW9523 interrupt based (`fault path: AW9523 INT PB7`), with direct `FAULT_CRITICAL_SUM` GPIO path intentionally unavailable (`PIN_FAULT_CRITICAL_SUM = -1`).
- Claim separation maintained for this pass: no per-channel OCP attribution claim beyond shared summary behavior.

Not proven:
- B3-S2 OVP assert/clear transitions above/below threshold.
- B3-S3 thermal warn assert/clear threshold crossing.
- B3-S4 OTP assert/clear threshold crossing.
- B3-S5 shared fault-summary trip/clear with required `EVT:FAULT TRIP` and `EVT:FAULT CLEAR` capture.
- Host-visible UDI event capture path remains unproven in current wiring/port mapping; COM12 has not produced `ACK:`/`ERR:`/`EVT:` traffic in-session.

Confidence uplift path (next evidence to close gaps):
- Flash/boot the exact STM32 image that includes runbook baseline commands (`DIAG`, `CMD:GET STATE` handling) and confirm startup line `fault path: AW9523 INT PB7` in the captured boot log.
 - Use the now-validated branch image (build/upload succeeded on 2026-09-26 rerun) as the baseline for next B3-S5 capture.
- Re-run B3-S2 with controlled OVP threshold crossing and attach paired assert/clear timestamps plus capture reference.
- Re-run B3-S3 and B3-S4 with controlled temperature ramps across warn/OTP thresholds and attach assert/clear logs plus temperature evidence.
- Re-run B3-S5 with induced shared fault-summary trip/clear and attach UDI log lines containing both `EVT:FAULT TRIP` and `EVT:FAULT CLEAR`.
- If synthetic trigger support remains limited to `SIMFAIL <AHT|INA|FLASH|ALL>`, execute a physical shared-fault induction method and capture both COM7 context and COM12 EVT transitions in the same time window.
- Keep all unresolved rows explicitly marked Not proven until the above artifacts are attached.

Bench worksheet summary:
- B3-S0: Pass (context lock complete).
- B3-S1: Pass (code-map freeze complete).
- B3-S2: Blocked (no OVP induced transition evidence).
- B3-S3: Blocked (no thermal warn threshold crossing evidence).
- B3-S4: Blocked (no OTP threshold crossing evidence).
- B3-S5: Blocked (no `EVT:FAULT TRIP/CLEAR` evidence captured).
- B3-S6: Pass (unavailable-source policy validated in code mapping).
- B3-S7: Pass with bounded claim (strict proven/not-proven separation applied).

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

## Agent handoff update (2026-09-26)

Use this handoff block as the current status for the next agent session.

- Branch: `phil-cia-bucket-3-execution`
- Latest commits on this branch include:
  - `4b00d57` Bucket 3: unblock STM32 build and rerun B3-S5 capture
  - `4fa7bdc` Bucket 3: add B3-S5 follow-up evidence status
  - `fc383b2` Bucket 3: record bench evidence matrix with bounded claims

Current proven state:

- STM32 build+upload blocker is closed for current branch image.
- COM7 is confirmed STM32 debug CLI path (`HELP`, `DIAG`, status lines).
- Fault ownership mapping and unavailable-source policy remain documented and bounded.

Current blocked state:

- B3-S5 is still Blocked: required `EVT:FAULT TRIP` and `EVT:FAULT CLEAR` are not yet captured in host-visible logs.
- COM12 currently behaves as CrowPanel flash/power USB and has not shown UDI event traffic in capture attempts.

Bench wiring/port reality to carry forward:

- STM32 UDI link is USART3 on PB10/PB11.
- If CrowPanel USB (COM12) does not bridge UDI traffic, use a passive USB-UART tap for evidence capture:
  - tap RX -> PB10 (STM32 TX)
  - tap GND -> STM32 GND
  - leave tap TX disconnected for listen-only capture

Exact closure condition for Bucket 3 evidence:

- Capture one trip+clear cycle with log lines containing both:
  - `EVT:FAULT TRIP`
  - `EVT:FAULT CLEAR`
- Update B3-S5 row to Pass only when both lines are attached as evidence.

## Bucket 3 takeover execution audit (2026-09-27)

Scope lock used for this takeover:

- Scope: Bucket 3 fault-claim evidence closure only (B3-S0..B3-S7).
- Method: runbook conformance audit against existing captured artifacts in this repo.
- Branch state: target branch `phil-cia-bucket-3-execution` is currently attached to a
  separate linked worktree; this workspace takeover is executing from
  `phil-cia-bucket-3-execution-takeover` created at the same target branch commit
  (`23bfb25`) to avoid cross-worktree mutation.

Artifacts audited in this takeover:

- `docs/firmware-buckets/bucket-3-fault-scope.md` (all prior B3 runs and matrix rows).
- `docs/FIRMWARE_DEVELOPMENT_PLAN.md` (Bucket 3 evidence contract alignment).
- Workspace text/log artifacts (`map_capture*.txt`, docs logs/handoffs) for direct
  assert/clear proof lines.

Audit result:

- No new repository artifact was found that directly proves B3-S2/B3-S3/B3-S4
  assert+clear transitions.
- No new repository artifact was found that contains both required B3-S5 lines:
  `EVT:FAULT TRIP` and `EVT:FAULT CLEAR` in one validated trip/clear evidence cycle.
- Existing B3-S0/B3-S1/B3-S6/B3-S7 evidence remains bounded and valid.

### Takeover reporting block (filled)

Proven:

- B3-S0 context lock exists with run header + branch/worktree evidence in this doc.
- B3-S1 ownership-map freeze exists with code references and bounded claim policy.
- B3-S6 unavailable-source handling is explicitly documented and evidenced (`PIN_FAULT_CRITICAL_SUM = -1`, AW9523 interrupt path).
- B3-S7 claim separation remains bounded: no speculative channel-attribution or virtual-source claims added.

Not proven:

- B3-S2 OVP assert and clear transitions (both edges) are not yet evidenced.
- B3-S3 thermal warn assert and clear transitions (both edges) are not yet evidenced.
- B3-S4 OTP assert and clear transitions (both edges) are not yet evidenced.
- B3-S5 shared fault summary assert/clear is not yet evidenced with both
  `EVT:FAULT TRIP` and `EVT:FAULT CLEAR`.

Confidence uplift path:

- Capture one controlled threshold-crossing run for B3-S2 and attach paired
  assert/clear timestamps plus capture reference.
- Capture one controlled thermal ramp run that crosses warn and OTP thresholds for
  B3-S3/B3-S4 and attach paired assert/clear + temperature evidence.
- Capture one shared-fault trip/clear run for B3-S5 with UDI log evidence that
  includes both `EVT:FAULT TRIP` and `EVT:FAULT CLEAR` in-sequence.
- Keep B3-S2..B3-S5 as Blocked/Not proven until the above direct artifacts are
  attached and referenced in the matrix.

Bucket 3 closure state after takeover audit:

- **Evidence-pending** (not closed): required assert+clear paths for B3-S2..B3-S5
  are still missing from attached artifacts.
