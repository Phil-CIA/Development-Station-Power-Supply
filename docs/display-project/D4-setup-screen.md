# D4 — CrowPanel Setup Screen (setpoint wizard)

Scoping doc + agent kickoff for bucket **D4** of the CrowPanel screens plan
(see `docs/display-project/crowpanel-screens-tracker.md`).

- Branch: `phil-cia-crowpanel-setup-screen`
- Base:   `main` (branched from post-D3 state)
- Closes (part of): #26, tracked in #65

## Goal

Turn the existing Setup screen in `crowpanel-43-bringup/src/main.cpp`
(`create_setup_screen` / `handleSetup*` / `updateSetupBindingsFromUdi`) into a
real setpoint wizard that lets the operator edit and commit all channel
setpoints via UDI, and cancel cleanly.

## In scope

- **Setpoint fields per channel (CH1/CH2/CH3):**
  - `V_set` (mV)
  - `I_limit` (mA)
  - Operating mode: `LATCH` / `HICCUP` / `MONITOR`
- **Interaction model** (touch + encoder — both must work):
  - Select field → edit → commit or cancel
  - Coarse / fine step (long-press or on-screen toggle)
  - Field-level validation with min/max clamps and inline error hints
- **UDI protocol** (per `docs/DISPLAY_INTERFACE_STANDARD.md`):
  - Commit uses existing `CMD:VSET`, `CMD:ILIM`, `CMD:MODE` (verify exact
    tokens against the STM32 firmware and the UDI spec before wiring)
  - Wait for `ACK:` or `ERR:`; surface `ERR:` in the field-level hint
  - Never send while a prior CMD is still awaiting ACK
- **Cancel / back**: revert local edits to last-known UDI state; do not send.
- **Refresh**: pull current values via `CMD:GET STATE` on screen enter and
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
- [ ] Encoder: rotate selects, press enters edit, rotate adjusts, press commits, long-press cancels
- [ ] Touch: field tap enters edit, +/- adjusts, `Done` commits, `Cancel` reverts
- [ ] Commit sends exactly one `CMD:` per field change and updates only on `ACK:`
- [ ] `ERR:` payload shows inline on the field, doesn't crash, doesn't leave edit mode stuck
- [ ] Values persist across screen navigation (Main → Setup → Main → Setup)
- [ ] Mode cycles through `LATCH` → `HICCUP` → `MONITOR` and commits correctly
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
