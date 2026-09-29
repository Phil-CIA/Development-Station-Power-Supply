# D4 — CrowPanel Setup Screen (setpoint wizard)

> **Scope gate (2026-09-29):** During bring-up the STM32 UDI command handler
> (`stm32-bluepill-bringup/src/main.cpp`) was verified. It supports only
> `OUTPUT ON/OFF`, `GET OUTPUT`, `GET STATE`, `GET ILIM CH1|CH2`, and
> `ILIM CH1|CH2 <mA>`. There is **no `VSET` and no `MODE`** (CH1/CH2 are fixed
> 5.00 V / 3.30 V rails), and **CH3 has no command set at all** — it is the
> fixed monitor / 5 V bootstrap rail, not an adjustable output. Per the
> guardrails below, the STM32 protocol was **not** extended. V_set and mode are
> deferred to [#73](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/73);
> CH3 setpoint control is not a gap to fill and is intentionally excluded.
>
> **PR #71 delivers the achievable UI-only subset:** an edit → commit →
> cancel/revert wizard state machine for the supported fields (Output enable +
> CH1/CH2 I_limit) with await-ACK gating, inline `ERR:` display, UI-side
> clamping, `GET`-driven refresh, and touch + encoder parity. Coarse/fine step
> is deferred (no spare input on the current 4-button + encoder layout).

## ▶ Session handoff (2026-09-29) — next agent resume here

**State: code implemented + flashed to the physical CrowPanel. Bench validation
NOT yet done. PR #71 stays DRAFT until it passes and screenshots are attached.**

Done this session (branch `phil-cia-crowpanel-setup-screen`, PR #71):
- `43ec6de` — D4 setup wizard UI subset in `crowpanel-43-bringup/src/main.cpp`
  (edit buffer + cancel/revert, await-ACK gate w/ 1.5 s timeout, inline `ERR:`,
  contextual Edit/Apply + Done/Cancel labels, per-field clamps, touch+encoder).
- `08556aa` — dropped stale CH3 setpoint framing from this doc.
- Issue [#73](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/73)
  filed for the missing STM32 `VSET` / `MODE` commands (CH1/CH2 only; CH3 excluded).
- Issue [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74)
  filed to track encoderless operation for the current hardware revision
  (this rev has no rotary encoder populated).
- Built clean for `crowpanel43` and **flashed to COM12** (MAC 80:B5:4E:E2:E4:08,
  hash verified).

To do (pick up here):
1. Bench-test against the **Bench acceptance** checklist below on the physical panel.
2. Capture screenshots (view / edit / error) → attach to PR #71.
3. Flip the tracker D4 row to 🟢 and take PR #71 out of draft.

Environment gotchas (verified this session, also in repo memory):
- Flash the **worktree** build, not the main workspace: run
  `scripts/guarded-flash.ps1 -Target crowpanel -Action upload` from
  `copilot-worktrees/.../d4-setup-screen/` so PlatformIO uses the worktree project.
- Uploads hang unless Python is forced to UTF-8: set `$env:PYTHONUTF8='1'` and
  `$env:PYTHONIOENCODING='utf-8'` before the upload (cp1252 console codepage
  crashes PlatformIO's echo thread and stalls esptool → blank display).
- Redirect upload output with `*> file.log`; don't pipe through `Select-Object`.

Unrelated loose end (do NOT mix into PR #71): an uncommitted STM32 `main.cpp`
Q6/Q12 relabel is parked in `git stash@{0}` on `main`. Left untouched.

Scoping doc + agent kickoff for bucket **D4** of the CrowPanel screens plan
(see `docs/display-project/crowpanel-screens-tracker.md`).

- Branch: `phil-cia-crowpanel-setup-screen`
- Base:   `main` (branched from post-D3 state)
- Closes (part of): #26, tracked in #65

## Goal

Turn the existing Setup screen in `crowpanel-43-bringup/src/main.cpp`
(`create_setup_screen` / `handleSetup*` / `updateSetupBindingsFromUdi`) into a
real setpoint wizard that lets the operator edit and commit the UDI-supported
channel setpoints (Output enable + CH1/CH2 I_limit) via UDI, and cancel cleanly.

## In scope

- **Setpoint fields (only what the STM32 UDI protocol actually supports):**
  - `Output` enable (`OUTPUT ON|OFF`)
  - `I_limit` for **CH1** and **CH2** (`ILIM CH1|CH2 <mA>`)
  - CH3 is **out of scope by design** — it is the fixed monitor / 5 V bootstrap
    rail and has **no command set** (not an adjustable output). Do not add CH3
    setpoint rows.
  - `V_set` and operating mode (`LATCH` / `HICCUP` / `MONITOR`) are **not in the
    protocol** (CH1/CH2 are fixed 5.00 V / 3.30 V rails) — deferred to
    [#73](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/73).
- **Interaction model** (touch-first for this hardware rev; encoder optional if present):
  - Select field → edit → commit or cancel
  - Field-level validation with min/max clamps and inline error hints
  - Coarse / fine step is deferred (no spare input on the 4-button + encoder layout)
- **UDI protocol** (per `docs/DISPLAY_INTERFACE_STANDARD.md`):
  - Commit uses the existing `CMD:OUTPUT` and `CMD:ILIM` tokens (verified
    against the STM32 firmware). There is no `CMD:VSET` / `CMD:MODE`.
  - Wait for `ACK:` or `ERR:`; surface `ERR:` in the field-level hint
  - Never send while a prior CMD is still awaiting ACK
- **Cancel / back**: revert local edits to last-known UDI state; do not send.
- **Refresh**: pull current values via `CMD:GET` on screen enter and
  after any successful commit.

## Out of scope (do NOT expand into these)

- Fault modal — that's D7.
- Settings submenu behavior — D6.
- Graph windowing — D5.
- Boot self-test — D8.
- Any UDI protocol changes on the STM32 side. If a needed command is
  missing, stop and open an issue instead of extending the protocol here.

## Files expected to change

- `crowpanel-43-bringup/src/main.cpp`
  - `create_setup_screen(...)`
  - `refreshSetupScreenLabels()`
  - `applySetupAck` / `applySetupEvent` / `applySetupError`
  - `setupSelectDelta` / `setupAdjustDelta` / `setupRequestRefresh`
  - `handleSetupEncoderRotate` / `Press` / `LongPress`
  - `setup_prev_btn_event_cb` / `setup_next_btn_event_cb` /
    `setup_edit_btn_event_cb` / `setup_done_btn_event_cb`
- `docs/display-project/crowpanel-screens-tracker.md` — flip D4 to 🟢 with PR link
- `docs/display-project/crowpanel-ui-audit.md` — mark the resolved D4 items

Do not touch STM32 firmware in this PR.

## Bench acceptance (must pass before opening the PR non-draft)

- [ ] Enter Setup from Main; all fields populate from live UDI within 1 s
- [ ] Encoder path parity (only if encoder is physically present): rotate selects, press enters edit, rotate adjusts, press commits, long-press cancels
- [ ] Touch parity: `Prev/Next` selects or adjusts (while editing), `Edit/Apply` enters + commits, `Done/Cancel` exits + reverts
- [ ] Commit sends exactly one `CMD:` per field change and updates only on `ACK:`
- [ ] `ERR:` payload shows inline on the field, doesn't crash, doesn't leave edit mode stuck
- [ ] Values persist across screen navigation (Main → Setup → Main → Setup)
- [ ] Setup scope is limited to Output + CH1/CH2 I_limit (no `VSET`/`MODE` controls shown in this PR; deferred to #73)
- [ ] Clamps: values below min / above max are rejected in the UI before send
- [ ] Screenshots of Setup in view / edit / error states attached to the PR

## Non-goals for review

- Refactoring the whole screen state machine — keep changes local to Setup.
- Restyling — D1 already handled layout; only add styles the new states need.

---

## Agent kickoff prompt (copy into VSCode)

> You are the VSCode Copilot agent working D4 on branch
> `phil-cia-crowpanel-setup-screen` (already created off `main`, post-D3).
>
> **Read first:**
> - This file (`docs/display-project/D4-setup-screen.md`) — your scope
> - `docs/display-project/crowpanel-screens-tracker.md`
> - `docs/display-project/crowpanel-ui-audit.md` — D4-tagged rows
> - `docs/DISPLAY_INTERFACE_STANDARD.md` — UDI framing
> - `crowpanel-43-bringup/src/main.cpp` — search for `create_setup_screen`,
>   `handleSetup`, `applySetup`, `setup_*_event_cb`
>
> **Rules:**
> - Do not commit to `main`. Work only on `phil-cia-crowpanel-setup-screen`.
> - No STM32 firmware changes. If UDI commands you need don't exist, stop
>   and file an issue instead of extending the protocol.
> - Small, reviewable commits. Bench-test on the physical CrowPanel before
>   opening non-draft.
> - Attach screenshots (view / edit / error) to the PR body.
> - Update the tracker row for D4 to 🟢 with the PR link in the same PR.
> - Update `crowpanel-ui-audit.md` to check off the D4-deferred items.
>
> **When done:** report back to the coordinating session (or update
> issue #65) so we can plan D5.

---

## Model recommendation

Escalate to **Claude Opus 4.7** or **Opus 4.8** for this bucket. The setup
wizard has real state-machine complexity (view / edit / awaiting-ACK / error /
revert) and must interleave encoder + touch + async UDI responses without
deadlocks. Sonnet 5 is a fine fallback if Opus is unavailable, but not
the mini / flash tiers.

## Bench results (2026-09-29)

Status key: Pass / Fail / N/A

| # | Scenario | Expected | Result | Evidence | Notes |
|---|---|---|---|---|---|
| 1 | Cold boot → Splash → Main → Setup | Setup populates ILIM CH1/CH2 within 1 s of entering | Pass | Not captured (session policy) | Operator-confirmed on physical panel; artifact screenshots intentionally skipped unless needed for debugging. |
| 2 | Tap ILIM CH1 → tap Edit → adjust value → Apply | Field shows pending marker; on ACK the value updates; pending clears | N/A | Pending | Requires on-device touch interaction + screenshot capture. |
| 3 | Same as #2 for ILIM CH2 | Same behavior | N/A | Pending | Requires on-device touch interaction + screenshot capture. |
| 4 | Tap Edit → adjust → Cancel | Value reverts to last committed; no CMD sent | N/A | Pending | Requires on-device touch interaction + serial/behavior observation. |
| 5 | Tap Edit → adjust → Apply, then before ACK, tap another field | Second input blocked/queued until ACK or 1.5 s timeout | N/A | Pending | Requires timing-sensitive touch test on hardware. |
| 6 | Force an ERR (out-of-range if defeatable, or unplug STM32 UDI mid-commit) | Inline ERR appears; edit state not stuck; retry works | N/A | Pending | Requires controlled fault injection on bench. |
| 7 | Setup → Main → Setup navigation | Values persist and re-fetch cleanly | N/A | Pending | Requires repeated navigation on device. |
| 8 | Output ON/OFF toggle still works from Main | Unaffected by D4 changes | N/A | Pending | Requires Main-screen interaction against live STM32 link. |

Screenshot target folder: `docs/display-project/screenshots/D4/`
