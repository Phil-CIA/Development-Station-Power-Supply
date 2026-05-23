# Hardware Issue Analysis - 2026-05-18 (Archived Snapshot)

Purpose: preserve historical analysis context while avoiding conflicts with current restarted handoff guidance.

## Source Of Truth For Active Work

Use these files first for current direction and stop-state:
- HANDOFF.md
- NEW_CHAT_HANDOFF.rmd
- NEW_CHAT_HANDOFF_SHORT.rmd
- docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md

## Archived Summary

This analysis was produced during an earlier in-session verification pass and contained mixed interim findings. The active baseline has since been restarted and normalized.

Current normalized baseline:
- Rev-B regulator/HAT netlists are the active hardware baseline.
- RB-001 3-channel selector implementation is present in Rev-B (LMV358 plus BAT54C with local attenuation).
- Sense-B origins are confirmed on HAT Rev-B (R59/R60, R61/R62, R63/R64).
- Bench continuity validation remains pending for final RB-001 closure.
- Bootstrap naming and ownership statements should be treated as contract-cleanup items unless explicitly re-verified in latest exports.

## How To Use This File

- Treat this file as historical context only.
- Do not use old "pending/implemented" claims here to override HANDOFF.md.
- If any statement here conflicts with handoff docs, handoff docs win.
