# Issue 101 - STM32 Flash Headroom / VS Code Agent Execution Plan

Issue: https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/101
Branch: phil-cia-stm32-flash-headroom
Primary owner: Bucket 6 - bring-up diagnostics and recovery paths.
Dependencies: Bucket 4 UDI, Bucket 5 persistence/calibration, Bucket 3 faults.
Status: Steps 1-3 and 6 complete and reviewed; Steps 4/5 skipped. Independent review findings are addressed; Step 7 remains partial with open criteria.
Next authorized handoff: Step 7a planning-only investigation using issue-101-agent-plan.txt. No further hardware testing is authorized unless the user separately approves it.

## How we work

This Copilot app session is the coordinator. The user chooses the VS Code
agent/model for each step and pastes its report back here. The coordinator
reviews the actual diff and evidence before authorizing the next step.
Do not execute this entire file as one assignment.

The durable plan lives in this Markdown file. At each handoff, the
coordinator provides one standalone plain-text prompt in a copyable code
block, incorporating the common instructions, assigned step and report
format below. `issue-101-agent-plan.txt` contains only the current copy/paste
handoff, not a second execution plan.
Later step prompts are prepared, NOT authorized.
After each review, update this file's execution record and next handoff.
Changing models does not change scope or bypass an approval gate.
Only one agent writes to the branch at a time.

Use the checkout for this branch, NOT the project's main checkout.
At plan creation, its Windows path is:
```text
C:\Users\user\Esp32 projects VScode\copilot-worktrees\Development-Station-Power-Supply\phil-cia-fictional-adventure
```
The folder retains its generated name although the branch was renamed.
If the path changes, obtain the current session path from the coordinator.
Before editing, verify git branch --show-current and git status --short.
If the branch is wrong or another agent's changes conflict, stop and report.

## Confirmed direction / non-negotiables

Native USB CDC is unused on the current HAT and can be removed. Future USB,
Wi-Fi and a more capable MCU belong to a later revision, not this PR.
CDC removal must consolidate all console paths onto ONE USART1 object;
a flag-only removal leaves duplicate USART1 objects and is not acceptable.
USART1: CH340 console, PA10 RX / PA9 TX, 115200 8N1.
USART3: separate UDI, PB11 RX / PB10 TX, 115200 8N1.
Confirm these against docs/STM32_BLUEPILL_PIN_TABLE.md before changes.

Bench-only commands may be gated behind a compile-time bring-up flag.
Keep the full bench profile available; do not delete commands in this PR
just because a compact profile excludes them. Deletion requires captured
bucket evidence and separate approval.

Every maintained profile retains safe startup, AW9523 fault IRQ/EVT,
telemetry, UDI ACK/ERR/EVT, CALSET/CFG provisioning, runtime device drivers,
and actionable boot/config/failure/recovery messages.
Do not remove runtime functions merely because a debug command uses them.
Do not alter persistence layout, hardware pins, rail architecture, CrowPanel
behavior, board flash limits, or assume undocumented 128 KiB capacity.
External W25Q128 data flash is not additional executable internal flash.

The user approved a compact complete-image ceiling of 52,428 bytes at Gate C.
The full-bench image must fit the documented 65,536-byte capacity.
CI enforces these limits using the programmed `firmware.bin` size.
Static RAM totals do not establish heap/stack or timing safety.

## Common agent instructions

Work only on issue #101 in branch phil-cia-stm32-flash-headroom.
Read the current issue body AND comments, this plan, README.md,
docs/SYSTEM_DEVELOPMENT_WORKFLOW.md, docs/FIRMWARE_DEVELOPMENT_PLAN.md,
docs/display-project/README.md, stm32-bluepill-bringup/README.md,
stm32-bluepill-bringup/platformio.ini, docs/STM32_BLUEPILL_PIN_TABLE.md,
docs/DISPLAY_INTERFACE_STANDARD.md, and .github/workflows/platformio-build.yml.
Repo code/docs are the source of truth; inspect callers before removing code.

Execute ONLY the assigned step. Preserve unrelated edits. No broad cleanup,
new controller design, deep driver rewrite, or String rewrite.
No uploads, output toggles, induced faults, flash erase, or physical tests
without separate user authorization. Build-only work is the default.
Never claim bench verification from a successful build.
Do not create another branch/PR, merge, edit main, push, or amend commits.
Leave changes uncommitted for coordinator review. The coordinator commits
accepted steps and updates the draft PR.
Do not modify this plan's approval/status fields yourself.

Build with:
python -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8
Record failures honestly. Do not change flash capacity to make a build pass.
If blocked, report evidence and stop; do not start the next step.
Use existing testing/build conventions and add focused checks for new logic.
Do not silently omit failures or emit success-shaped fallback responses.

## Required agent report

STEP / selected model (if known):
Branch, checkout path, starting commit:
Files changed and why:
Exact commands run and exit results:
Resolved platform/core/toolchain/direct/transitive library versions:
Before/after flash bytes, free bytes, static RAM, and delta:
ELF/map/section/symbol evidence paths:
Behavior preserved / intentional behavior changes:
Tests and regressions covered:
Bench evidence: NOT RUN, or user-authorized procedure and observed results:
Risks, blockers, unresolved decisions:
Suggested next action (do not execute):

For documentation-only or measurement-only steps, state no firmware change.
Distinguish measured savings from estimates. Compare identical dependency
versions and profiles, and count .data initializers in flash usage.
Do not commit ELF/map/build products or raw verbose logs; use session artifacts
or PR attachments and put concise durable results in relevant repo docs.

## Step 1 - Reproduce and freeze the baseline

Execute Step 1 only. Inspect the current environment and reproduce the
unmodified STM32 build at the current commit. The issue baseline is commit
92699b9055c234a5c3ab6e01942a42af69880efa: flash 64,892 B, free 644 B,
static RAM 5,536 B. Those figures are prior evidence, not a guaranteed result.

Capture the ELF, linker map (generate one if absent), sections, and largest
symbols. Identify resolved dependencies including AW9523 and BusIO and
compare tool versions with the issue. Explain any baseline discrepancy.
Pin the currently resolved direct/transitive library versions in the STM32
configuration without upgrading them, then clean-build and verify the same
baseline. Record tooling needed for reproducible later comparisons; do not
pin unrelated projects or refactor the CI installation yet.

Document the baseline and evidence location in the STM32 README. Inspect all
console initialization/logging/polling paths and report the intended single
USART1 consolidation; do NOT remove CDC or alter firmware behavior yet.
Report the command inventory with runtime-shared helpers and a proposed
bench-only allowlist, including commands not explicitly listed in the issue.
Stop for Gate A.

**Gate A:** coordinator reviews baseline, pins, console ownership and scope.
Authorize Step 2 only after accepting this report/diff.

## Step 2 - Remove CDC and consolidate the console

Execute Step 2 only after Gate A approval. Remove native USB CDC from the
maintained STM32 configuration. Inspect Arduino core 2.12 behavior and choose
one USART1 object with explicit ownership; avoid both the core Serial1 and
an application SerialDbg initializing/polling the same peripheral.
Consolidate every boot log, heartbeat, error, helper and command polling path
onto that single console, retaining the CH340 pin/baud contract.
Keep USART3 UDI separate and unchanged. Do not add LTO or gate debug commands
in this step. Rename logging helpers only where their old name becomes
misleading; avoid unrelated source cleanup.

Build cleanly, compare against Step 1, inspect symbols for CDC removal and
single USART1 ownership, and explain RAM changes. Update the STM32 README
console description and firmware plan's flash status with measured results,
explicitly NOT bench-tested. Include safe, user-run bench checks for boot
messages, heartbeat, HELP/DIAG/CFGSHOW and UDI GET/ACK/ERR; do not run hardware.
Stop for Gate B.

**Gate B:** coordinator reviews console diff and build evidence.
User bench-checks console/UDI when hardware is available. Missing bench
evidence remains explicit; it is not permission to claim runtime success.

## Step 3 - Add compact and full bench profiles

Execute Step 3 only after authorization. Based on the approved inventory,
introduce one explicit bring-up flag and a maintained full-bench environment.
Use bluepill_f103c8 as the compact/default environment, with the flag off,
and bluepill_f103c8_bench as the full bench environment with the flag on.
Share platform/dependency/common flags through PlatformIO inheritance; no
duplicated drifting config. Neither profile re-enables native USB CDC.

Gate bench-only command dispatch, their exclusive implementations/strings,
and HELP entries consistently. Candidates approved in the issue include
Q-path/range-pair tests, AW9523 P1.0/mode pokes, D9FLASH, SRTEST, INADIAG,
QSEQ and Q9DIAG. Other commands require inventory review, not assumptions.
Preserve shared boot/fault/device/control functions, CFG/CAL provisioning
and actionable diagnostics in both profiles. Excluded commands must report
an explicit unsupported/unknown error, never appear to succeed.

Safeguard note: some helpers are shared between bench-only command handlers
and startup/runtime flows. In particular, `runFlashBringupTest()` and
`aw95xxEnsureGpioPushPull()` are used by startup/runtime code and must not
be removed or gated when bench commands are gated; gate only the bench
command entry points, not these shared helpers.

Build both profiles cleanly. Provide a command availability table and
before/after flash/RAM deltas for like-for-like configurations. Verify
full-bench command/help parity and compact retained behavior. Update the
STM32 README with profile usage and warnings. Do not delete bench code,
add LTO, change parsing or touch hardware. Stop for Gate C.

**Gate C:** accepted. The compact profile ceiling is 52,428 B. The user
chose to skip optional LTO and parser optimization; Steps 4 and 5 are not
authorized for this PR.

## Step 4 - Conditional LTO trial

Run only if explicitly authorized at Gate C. Trial -flto in both maintained
profiles with identical dependencies. Compare each profile before/after;
record linker warnings, flash, static RAM and relevant code retention.
Do not retain LTO just because it compiles. Document runtime risks and a
user-run bench regression checklist covering boot outputs, IRQ/fault EVT,
I2C/SPI devices, telemetry, console/UDI and persistence/recovery.
Retain it only if the coordinator accepts the tradeoff; do not silently
roll forward despite warnings or absent evidence. No parser changes.
Stop and report. Bench verification remains a separate gate.

## Step 5 - Conditional bounded ILIM parser

Run only if explicitly authorized after reviewing achieved headroom.
Replace only ILIM's sscanf dependency with a bounded, overflow-safe parser.
Inspect normalization and existing parser helpers first. Preserve valid
CH1/CH2 values, range endpoints, command responses and state updates.
Test zero, positive limits, upper endpoints, one-above, negative/sign input,
huge integers/overflow, bad channel, missing tokens, whitespace, malformed
numbers and trailing input. Capture current behavior before changes.
Document any intentional stricter treatment of trailing/malformed input and
obtain coordinator approval rather than silently changing compatibility.

Provide deterministic host-side or repository-standard logic tests and
build both profiles; compare incremental measured savings. Invalid input
must leave both limits unchanged and produce an explicit protocol error.
Do not rewrite String-based command buffering or unrelated command parsing.
Update directly affected protocol docs only if behavior changes. Stop.

## Step 6 - Enforce the agreed budget in existing CI

Gate C approved the numeric ceiling. Step 6 extends
.github/workflows/platformio-build.yml; CI already exists.
Build compact and full-bench STM32 environments without disturbing other
targets. Enforce the agreed compact ceiling and full bench's 65,536-byte
capacity. Measure flash from the actual linked artifact including .data
load bytes and required vector/initialization sections, not .text alone.
Report used/free flash and static RAM. Fail explicitly for missing artifacts,
malformed measurements and over-budget results; never skip into success.

Add focused tests for exactly-at-budget, one-byte-over, missing/malformed
artifact input, and correct flash-vs-RAM attribution. Keep checks portable
between Windows developer builds and Linux CI. Upload size/map evidence as
CI artifacts and document the local equivalent. Avoid adding global size
limits to unrelated targets. Record remaining-feature headroom assumptions
from the real feature inventory, not "80% complete" arithmetic.
Stop with the report; do not merge or close the issue.

## Step 7 - Bench evidence and PR closeout

Execute documentation/review preparation only unless the user separately
authorizes specific hardware actions. Prepare a safe evidence checklist
covering compact and bench boot, safe PA0/PA1/PA2 output states, console,
USART3 telemetry and ACK/ERR round trips, AW9523 fault IRQ/EVT, I2C/SPI,
config/calibration cold-boot persistence and induced recovery.
Fault injection, output enabling and config erase/reset need explicit user
approval and a known-safe bench setup. Capture runtime heap/stack/timing
evidence where feasible; report gaps rather than treating static RAM as proof.

Incorporate only actual user-provided or authorized observations. Update
docs/FIRMWARE_DEVELOPMENT_PLAN.md and the STM32 README with final measured
sizes, profile contracts, preserved functionality and open evidence.
Report acceptance criteria individually as met, open or blocked. The
coordinator reviews the diff, CI and evidence and updates the draft PR.
No automatic merge, issue closure, or unsupported "bench passed" claims.

### Step 7a - Plan the remaining bench checks without operating hardware

User selected this planning-only handoff on 2026-10-09. Read the current
firmware and existing evidence to determine how to reach STM32 USART3 for
`GET STATE`, `GET CFGREC`, and a non-mutating unknown-command error case.
Confirm actual-board applicability before specifying PA0/PA1/PA2 test
points, expected inactive levels, and measurement tolerances.

Prepare a procedure with source references, prerequisites, exact proposed
commands, expected responses, evidence fields, and stop conditions.
Account for automatic CrowPanel OUTPUT/ILIM writes, competing UART
transmitters, mixed binary/text traffic, and automatic startup flash writes.
No serial-port access, uploads, resets, power cycles, wiring changes,
measurements, firmware edits, or hardware operations are authorized.
Report feasibility and unresolved setup questions to the coordinator.
Stop before execution; the user must separately approve a specific procedure.

## Execution record - coordinator owned

Planning: complete; Markdown plan with per-step plain-text handoffs.
Step 1: complete; coordinator has accepted Gate A and verified the baseline.
	- Baseline measured: flash 64,892 B (free 644 B), static RAM 5,536 B.
	- Before/after dependency-pin ELF hashes: identical (no code/behavior churn).
Step 2: implementation complete; Gate B accepted by coordinator.
	- PlatformIO metric: flash 51,980 B used / 13,556 B free; static RAM 2,000 B.
	- Complete loadable image: flash 52,300 B used / 13,236 B free.
	- CDC absent in maintained STM32 config; one USART1 console owner; USART3 UDI remains separate.
	- Bench status for Step 2: NOT RUN (build/diff/documentation evidence only).
Step 3: implementation complete; Gate C accepted by coordinator and user.
	- Environments: `bluepill_f103c8` (compact/default) and `bluepill_f103c8_bench` (full bench), with shared settings in `stm32_common` and `default_envs = bluepill_f103c8`.
	- Compact build result: PlatformIO flash 44,956 B used / 20,580 B free, static RAM 2,000 B; complete image 45,276 B used / 20,260 B free; `firmware.bin` 45,276 B.
	- Full-bench build result: PlatformIO flash 52,256 B used / 13,280 B free, static RAM 2,000 B; complete image 52,576 B used / 12,960 B free; `firmware.bin` 52,576 B.
	- Delta vs Step 2 baseline (PlatformIO 51,980 B / complete image 52,300 B): compact saves 7,024 B; full-bench is +276 B.
	- Build status: both profiles passed clean builds; bench profile remains available for bring-up.
Step 4: skipped by user decision; not authorized.
Step 5: skipped by user decision; not authorized.
Step 6: implementation complete; user-approved budgets enforced in CI.
	- Compact complete-image limit: 52,428 B; full-bench capacity/limit: 65,536 B.
	- Both local profile builds and size checks passed; the earlier four-job Actions run 37918855171 on 6fe846c passed before the unit-test step was added.
	- Added the STM32 size-checker unit-test step to CI; all five tests pass locally and the compact job's CI test step passed in Actions run 37927216963 on commit `1fe8155` (all four jobs succeeded).
Step 7: partial bench evidence captured 2026-10-09; further hardware testing stopped at user direction.
	- Compact and full-bench uploads/boot logs passed; authorized automatic startup test erased/programmed only FLASH_TEST_ADDR and passed.
	- Compact unsupported bench-command responses, full-bench read-only diagnostics and CrowPanel-observed USART3 telemetry/available GET ACKs passed.
	- CrowPanel remained connected under user-authorized output isolation and automatically sent OUTPUT ON and ILIM CH1/CH2 traffic; the agent did not manually issue these writes.
	- Direct GET STATE/GET CFGREC and STM32 ERR injection through CrowPanel CLI: BLOCKED. PA0/PA1/PA2 electrical measurement: NOT MEASURED.
	- No further bench work is authorized unless the user separately approves it.
Bench status: PARTIAL; blocked and unmeasured criteria remain open.
Flash ceiling approval: compact complete-image ceiling 52,428 B APPROVED; full-bench limit is 65,536 B.
Independent review: record inconsistencies corrected; checker unit tests now run in CI.
Step 7a: planning-only handoff prepared at user request; investigation results pending. Hardware execution remains unauthorized.
PR #103: remains draft pending disposition of the remaining open acceptance criteria.
