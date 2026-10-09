---
mode: agent
description: "Rev-D session 5 — integrate the F405 sheet into the full Rev-D HAT"
---
Work on issue #102, session 5: full HAT integration. Use branch `hw/revd-hat-integration`.

Prerequisites: session 1 (Rev-C source of truth) and session 4 (MCU sheet) must be merged.

Goal:
- In `hardware/kicad/dsp-regulator-hat-rev-d/`, bring over the Rev-C circuit blocks. Use only the blocks session 1 marked as confirmed.
- Replace the Blue Pill header with the F405 sheet.
- Wire the C3 according to the session 3 contract.
- Keep the Rev-C files unchanged.
- Run the full ERC, then the PCB layout and DRC. Check the C3 antenna keep-out, USB routing and HSE placement.

Rules:
- Record the results and the remaining fabrication and bench gates in `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`.
- Do not claim the design is validated without bench evidence.
- Commit with the Copilot co-author trailer and open a PR that references #102.
