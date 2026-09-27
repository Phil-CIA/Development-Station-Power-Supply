# Bucket 5 Config/Persistence Scope (Versioned Recovery Evidence)

Status: active scoping doc for Bucket 5 execution.

This document defines the allowed config/persistence firmware scope for current
Rev-C bring-up and the exact bench evidence required before claiming Bucket 5
closure. It is a control document: implementation issues/PRs are artifacts under
this scope, not replacements for it.

## Scope owner and branch

- Primary bucket: Bucket 5 (Config/persistence/calibration)
- Planned branch: docs/firmware-bucket-5-config-persistence-scope
- Upstream source of truth: docs/FIRMWARE_DEVELOPMENT_PLAN.md

## In scope now

- Versioned load/save/reset/erase config behavior on STM32 persistent storage.
- Deterministic defaults when config is missing, invalid, or unreadable.
- Explicit recovery-reason observability in logs/commands.
- Persistence of calibration/config values across cold boots.
- Evidence workflow and stable step IDs for reproducible closure.

## Out of scope or blocked now

- Data-model redesign without migration semantics.
- Silent fallback behavior that hides corruption or recovery causes.
- Unversioned format changes with no compatibility handling.
- Protocol-family redesign unrelated to existing config/recovery controls.

## Persistence ownership policy (Rev-C)

Bucket 5 claims are allowed only when each behavior is both implemented and
bench-observed with reproducible evidence.

| Behavior class | Ownership policy | Claim rule |
|---|---|---|
| Save/load persistence | Use explicit save/load flow and verify round-trip across reboot | Claim only with before/after evidence |
| Reset defaults | Reset path must produce deterministic default state | Claim only with reset logs and post-reset state |
| Corruption/mismatch recovery | Recovery must be explicit, safe, and reason-coded | Claim only with induced-failure evidence |
| Recovery reason surfacing | Reason code/log must match observed failure mode | Claim only with matched log + command output |

## Exit criteria

Bucket 5 is complete only when all criteria below are true on bench hardware:

1. Cold boot restores persisted config/calibration values as expected.
2. Reset-to-defaults path is deterministic and verifiable.
3. At least one intentional invalid/missing/corrupt config run recovers safely.
4. Recovery reason is surfaced explicitly in logs/command output.
5. Proven/not-proven split is documented with no silent or implied claims.

## Required evidence in PR description

- Before/after state table for persistence operations.
- Boot logs for restore path and recovery path.
- Command transcript for config operations and recovery-reason readback.
- Explicit pass/fail verdict for each worksheet step.
- List of unresolved gaps (if any) and confidence uplift path.

## Bench worksheet (stable IDs)

Use these IDs unchanged across runs:

| Step ID | Objective | Required observation | Evidence type | Pass rule |
|---|---|---|---|---|
| B5-S0 | Context lock | Hardware rev, firmware build ID, storage path mode recorded | Run header | All context fields present |
| B5-S1 | Baseline state capture | Initial config + recovery-reason state captured | Log + command transcript | Baseline captured before mutation |
| B5-S2 | Save/reload round-trip | Mutated value persists across save/load cycle | Command transcript + state table | Value restored exactly |
| B5-S3 | Cold-boot persistence | Saved value survives power cycle | Boot log + state readback | Value preserved |
| B5-S4 | Reset-to-defaults determinism | Reset command returns deterministic defaults | Log + readback | Defaults match policy |
| B5-S5 | Corrupt/missing config recovery | Induced invalid state triggers safe recovery | Boot log + readback | Recovery succeeds safely |
| B5-S6 | Recovery reason observability | Recovery reason code/log aligns with induced fault | Log + command output | Reason is explicit and correct |
| B5-S7 | Claim-separation closeout | Proven/not-proven split matches attached evidence | Final report | No over-claiming |

## Bench execution runbook (operator)

This runbook provides a narrow capture sequence for `B5-S0..B5-S7`.

### Pre-run setup

1. Start serial capture for STM32 debug output.
2. Record run header: date/time, hardware revision, firmware SHA, operator.
3. Capture startup lines and baseline config state.

### Baseline commands

Run and capture:

```text
HELP
CFGSHOW
CFGSAVE
CFGLOAD
```

If available in this image, also capture recovery state readback:

```text
CMD:GET CFGREC
```

### Step-oriented execution mapping

| Worksheet step | Execute | Capture expectation |
|---|---|---|
| B5-S0 | Record run header + firmware identity | Full context lock |
| B5-S1 | Capture baseline config/recovery state | Reference baseline before changes |
| B5-S2 | Mutate config, save, load, read back | Exact round-trip persistence |
| B5-S3 | Power cycle and re-read config | Persisted values survive cold boot |
| B5-S4 | Reset defaults and re-read | Deterministic default state |
| B5-S5 | Induce invalid/missing config case | Safe recovery path engages |
| B5-S6 | Read/report recovery reason | Reason code/log matches induced case |
| B5-S7 | Complete proven/not-proven report | Bounded claim closeout |

## Pass/fail matrix template

| Step ID | Verdict (Pass/Fail/Blocked) | Evidence reference | Notes |
|---|---|---|---|
| B5-S0 |  |  |  |
| B5-S1 |  |  |  |
| B5-S2 |  |  |  |
| B5-S3 |  |  |  |
| B5-S4 |  |  |  |
| B5-S5 |  |  |  |
| B5-S6 |  |  |  |
| B5-S7 |  |  |  |

## Claim-separation checklist

- [ ] Every claimed persistence behavior is evidenced with logs/readback.
- [ ] Recovery behavior is not claimed without induced-fault evidence.
- [ ] Recovery reason visibility is explicit (not inferred).
- [ ] Missing evidence is listed under not proven.
- [ ] Confidence uplift path identifies exact next capture needed.

## Reporting template (required)

Use this block in PR descriptions touching Bucket 5 behavior:

```md
Bucket: Bucket 5 (Config/persistence/calibration)

Proven:
- 

Not proven:
- 

Confidence uplift path (next evidence to close gaps):
- 

Bench worksheet summary:
- B5-S0:
- B5-S1:
- B5-S2:
- B5-S3:
- B5-S4:
- B5-S5:
- B5-S6:
- B5-S7:

Bench-tested on real hardware: true/false
If false, reason:
Open follow-ups:
```

## Implementation touchpoints (primary)

- stm32-bluepill-bringup/src/main.cpp
- docs/FIRMWARE_DEVELOPMENT_PLAN.md
- docs/STM32_BLUEPILL_PIN_TABLE.md

## Notes

- This scope pass is docs-first and does not, by itself, close Bucket 5 evidence.
- Bucket 5 closure requires bench data across all required worksheet steps.
