# Rev-B Pre-Fab Disposition (2026-06-19)

Scope: Regulator Rev-B and HAT Rev-B only.
Policy: KERC-04 is deferred for this order cycle and does not block fabrication release.

## Evidence Used

- DRC report: hardware/kicad/dsp-regulator-hat-rev-b/DRC.rpt (created 2026-06-19T03:14:49-0500, post U10 fix)
- ERC report: hardware/kicad/dsp-regulator-hat-rev-b/ERC.rpt (created 2026-06-19T03:15:13-0500, post U10 fix)
- DRC report: hardware/kicad/dsp-regulator-rev-b/DRC.rpt (created 2026-06-19T02:16:21-0500)
- ERC baseline and handoff policy from HANDOFF.md, NEW_CHAT_HANDOFF.rmd, and README.md
- Open issue tracker context from docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md and docs/PCB_REDESIGN_PREP_2026-05-20.md

## DRC Snapshot

- HAT Rev-B DRC (post U10 fix): **0 violations, 0 unconnected pads, 0 footprint errors — CLEAN**
- HAT Rev-B ERC (post U10 fix): 0 errors, 5 warnings (all pin_to_pin and lib_symbol_issues for U5 AHT20 from easyeda2kicad library — non-blocking library noise)
- Regulator Rev-B DRC: 2 warnings, 0 unconnected pads, 0 footprint errors

## Disposition Table

| Board | Finding | Type | Risk | Decision | Owner Action |
|---|---|---|---|---|---|
| HAT Rev-B | U10 footprint mismatch: local Maple_Mini copy vs library Module copy | lib_footprint_mismatch (warning, local override) | Medium: assembly/fit risk if footprint source of truth is ambiguous | ~~Must-Fix~~ **RESOLVED** | Footprint fixed 2026-06-19. DRC now reports 0 violations. |
| Regulator Rev-B | Silkscreen clipped by solder mask at approx (140.57, 104.34) mm | silk_over_copper (warning, local override) | Low: cosmetic/readability only if polarity and reference readability remain clear | Acceptable | Keep as-is for this release unless the marking obscures polarity or critical assembly legend. |
| Regulator Rev-B | Silkscreen clipped by solder mask at approx (194.43, 104.48) mm | silk_over_copper (warning, local override) | Low: cosmetic/readability only if polarity and reference readability remain clear | Acceptable | Keep as-is for this release unless the marking obscures polarity or critical assembly legend. |
| HAT Rev-B | ERC U5 AHT20: 5 warnings (pin_to_pin Unspecified pin types + lib_symbol_issues easyeda2kicad not in config) | ERC library noise only | Low: no electrical implication; symbol works correctly; purely a library portability issue | Acceptable | No action for this order; if library portability matters for later handoff, add easyeda2kicad to sym-lib-table. |

## Non-DRC Open Loops (Pre-Order Review)

| Issue | Decision | Reason | Owner Action |
|---|---|---|---|
| RB-007 stack-height closure | Defer | Mechanical fit is not a fab-file electrical blocker for current board order | Capture measured stack height versus enclosure limit after board receipt/assembly check. |
| KERC-04 reset-safe startup bench evidence | Defer (explicit policy) | Bench validation gate, not PCB fab geometry gate for this cycle | Keep HOLD status documented; execute startup capture plan in docs/KERC-04_RESET_STARTUP_VALIDATION.md. |
| J21 display power current budget | Defer | Depends on measured display load profile; no immediate PCB geometry blocker | Keep J21 pin 1 net unchanged until measured current data is recorded. |

## Release Gate Decision (Current)

- Blockers to clear before fabrication output generation:
  - **None.** U10 footprint fix is confirmed — DRC now clean on HAT Rev-B.
- Acceptable findings for this order:
  1. Two Regulator silk clipping warnings — cosmetic only; no assembly-critical labels obscured per current review.
  2. HAT ERC U5 (AHT20): 5 warnings — library noise (easyeda2kicad not in config); no electrical implication.
- Deferred by policy:
  1. KERC-04 bench evidence.
  2. RB-007 stack-height closure.
  3. J21 display power budget decision.

## Final Pre-Fab Checklist

1. ~~Confirm U10 footprint authority~~ **DONE** — DRC clean 2026-06-19T03:14:49.
2. Confirm connector contract script still passes in both modes:
   - ./scripts/verify-connector-contract.ps1 -Mode reduced
   - ./scripts/verify-connector-contract.ps1 -Mode baseline
3. Confirm DRC reports are current for both boards and unchanged from this disposition.
4. If all above are satisfied, generate gerber + drill outputs for both Rev-B boards.
5. Record artifact paths and timestamps in handoff after output generation.
