# Development Station Handoff (2026-05-16)

Purpose: give another machine a clean starting point for the current Development Station Power Supply work without depending on one long chat thread.

## Current State

This repo already has an established handoff workflow.

The current baseline is:
- `HANDOFF.md` is the main top-level state summary.
- `NEW_CHAT_HANDOFF.rmd` captures a longer new-chat starter.
- `NEW_CHAT_HANDOFF_SHORT.rmd` exists for shorter resumptions.
- `docs/` contains supporting notes such as GPIO and USB hub tracking.
- The working tree currently has local KiCad-related changes that should be treated as in-progress and left alone unless explicitly requested.

## What Is Confirmed From Existing Handoff Docs

1. INA3221 dual-range current sensing is implemented.
2. Per-rail current-limit state machine and persistent NVS config are in place.
3. Two operation modes exist: `NORMAL` and `CURRENT_LIMIT`.
4. Serial commands for current-limit configuration and state are implemented.
5. The front display board bring-up work reached a known low-level baseline but the Hosyond module was judged suspect and paused.

## Priority Order From Existing Handoff

1. TFT mode button for `NORMAL` versus `CURRENT_LIMIT`
2. Incoming 12V dashboard row
3. Relay or GPIO expander for adjustable channel voltage select
4. Real rail-enable GPIO pin assignment on the next PCB revision

## Current Risks And Constraints

- The working tree is not clean because there are local KiCad-related edits and generated artifacts present.
- Those local hardware design changes were not created by this handoff task and should not be reverted automatically.
- Display-board work should not reopen broad experimentation unless explicitly requested.
- Preserve known-good commands, ports, and safety limits when resuming bench work.

## Recommended Resume Path

1. Read `HANDOFF.md` first.
2. Read `NEW_CHAT_HANDOFF.rmd` if more detail is needed.
3. Confirm the active target for the session:
   - power-supply firmware path
   - front display board path
   - PCB revision path
4. Choose one blocking task only.
5. Keep the next session focused on one validation or one code change at a time.

## Suggested Prompt For Next Chat

Continue the Development Station Power Supply work from `HANDOFF.md` and `docs/DEV_STATION_HANDOFF_2026-05-16.md`. Treat the existing handoff docs as source of truth. First summarize the current stopping point in 5 bullets or less, then give a strict 5-step plan. Keep one active step at a time, do not reopen broad exploration, and do not touch unrelated KiCad changes unless explicitly asked.

## New Reusable Templates Added

- `docs/DEV_STATION_MORNING_HANDOFF_TEMPLATE.md`
- `docs/DEV_STATION_END_OF_DAY_CLOSEOUT_TEMPLATE.md`

These are intended to replace ad hoc daily restarts with a short repeatable morning prompt and a matching end-of-day closeout block.