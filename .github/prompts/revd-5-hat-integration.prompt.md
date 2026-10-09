---
mode: agent
description: "Rev-D session 5 — integrate the F405 sheet into the full Rev-D HAT"
---
Work on issue #102, session 5: full HAT integration. Use branch `hw/revd-hat-integration`.

Prerequisites: session 1 (Rev-C source of truth) and session 4 (MCU sheet) must be merged.
Wait to start until the user confirms the memory-free-up session is complete.

User direction (2026-10-09):
- The supplied 2026-08-28 production HAT PCB and matching Gerber/drill set are
  the working manufacturing reference. The repository Rev-C project differs;
  this is non-blocking for integration, but exact physical-board and
  schematic-to-release correlation remain unresolved. Do not claim a match.
- Do not wait for a full Rev-C bench requalification. Treat existing circuits
  as reuse candidates and reuse or replace them as integration review
  requires. Do not present unresolved ISET/fault/shift-register safe states as
  verified.
- The existing Rev-C ESP32-C3 is replaced and need not be tested for reuse.
  The Rev-C fan path is untested; its bench check is deferred and does not
  block starting integration. Validate the new Rev-D C3 independently.

Goal:
- In `hardware/kicad/dsp-regulator-hat-rev-d/`, integrate suitable Rev-C blocks using session 1 and the production reference, documenting reuse/replacement decisions and unresolved source limits.
- Replace the Blue Pill header with the F405 sheet.
- Wire the C3 according to the session 3 contract.
- Keep the Rev-C files unchanged.
- Run the full ERC, then the PCB layout and DRC. Check the C3 antenna keep-out, USB routing and HSE placement.

Rules:
- Record the results and the remaining fabrication and bench gates in `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`.
- Do not claim the design is validated without bench evidence.
- Commit with the Copilot co-author trailer and open a PR that references #102.
