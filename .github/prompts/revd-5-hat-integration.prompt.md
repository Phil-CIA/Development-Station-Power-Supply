---
mode: agent
description: "Rev-D session 5 — integrate the F405 sheet into the full Rev-D HAT"
---
Work on issue #102, session 5: full HAT integration. Use branch `hw/revd-hat-integration`.

Starting point: use the separate editable integration starter
`hardware/kicad/dsp-regulator-hat-rev-d-full/DSP-Regulator-HAT-RevD-Full.kicad_pro`
and its same-named root schematic, local `MCU.kicad_sch` and `HAT.kicad_sch`.
The first source-backed pass connects ISET, flash, CH340, UDI, I2C, fan and
SWD interfaces to the frozen F405 pins. It is still partial, not a complete
HAT or bare C3 implementation. Read its README for the exact retained source,
deliberate R64/D12 changes and current 0-error / 80-warning ERC evidence.
Resolve rather than suppress the warnings; source-absent AW9523,
shift-register and measurement/protection blocks require actual circuits,
not invented net mappings. PA4/PC4/PC13 remain NC until their endpoints exist.

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
