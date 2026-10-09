---
mode: agent
description: "Rev-D session 5 — integrate the F405 sheet into the full Rev-D HAT"
---
Work on issue #102, session 5: full HAT integration. Use branch `hw/revd-hat-integration`.

Starting point: use the separate editable integration starter
`hardware/kicad/dsp-regulator-hat-rev-d-full/DSP-Regulator-HAT-RevD-Full.kicad_pro`
and its same-named root schematic and local `MCU.kicad_sch`. It currently
contains only the reviewed F405 support, Type-C/USB circuit and logical C3
interface, not the full HAT or a bare C3 module.

Preserve `hardware/kicad/dsp-regulator-hat-rev-d/` (the reviewed session-4
support-only project) and all Rev-C KiCad files byte-identically. Do not
reference the support project's child sheet from the full-HAT project.

User direction (2026-10-09): prior HAT circuits are working reuse candidates
and may be replaced as needed. Full Rev-C bench requalification is not a
prerequisite; the untested fan's later bench check does not block integration.
The supplied 2026-08-28 production Rev-C PCB/Gerber/drill set is the working
manufacturing reference, but exact physical-board/schematic correlation is
unresolved. Do not claim a match or copy/modify external OneDrive production
files. PR #111 records the mismatch but is not a merged prerequisite; use
the current contract's source findings and keep remaining uncertainty explicit.

Goal:
- In `hardware/kicad/dsp-regulator-hat-rev-d-full/`, integrate the required
  prior HAT circuit blocks or their replacements. Record each source and
  reconcile block-level connectivity and safe states; do not assume the
  production PCB is correlated to the committed schematic.
- Connect the existing local F405 sheet instead of the Blue Pill header.
  Remove its no-connect markers only as the frozen interfaces are wired;
  resolve imported reference-designator collisions.
- Replace the old C3 implementation with the bare ESP32-C3-MINI-1U
  external-antenna module and support/recovery circuits from the session 3
  contract. J4 alone is not that implementation.
- Keep the Rev-C files unchanged.
- Run full ERC after schematic integration. PCB layout and DRC are later
  release gates, not starter validation. Check the C3 antenna/RF constraints,
  USB routing and HSE placement when layout exists.

Rules:
- Record the results and the remaining fabrication and bench gates in `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`.
- Do not claim the design is validated without bench evidence.
- Commit with the Copilot co-author trailer and open a PR that references #102.
