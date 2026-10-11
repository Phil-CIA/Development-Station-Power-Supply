# Rev-C Symbol, Footprint, And Sourcing Reconciliation Handoff - 2026-08-29

## Purpose

Complete the source-to-procurement alignment pass for the active Rev-C regulator and HAT boards before any components, assembly, stencil, or PCB order is placed. This is a verification and source-data task. Do not silently substitute parts or mark a row aligned merely because LCSC recognizes its `C` number.

## Active source files

- `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch`
- `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_pcb`
- `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch`
- `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_pcb`

## Reproducible evidence

Run these after every schematic change. `kicad-cli` 10.0.5 is installed and available on the user `PATH`.

```powershell
scripts/export-revc-procurement-audit.ps1 -CartOutputPath docs/REVC_LCSC_CART_IMPORT_2026-08-29.csv
scripts/export-revc-symbol-field-completion.ps1
```

Outputs:

- `docs/REVC_PROCUREMENT_AUDIT_2026-08-29.csv`: one record per KiCad BOM reference.
- `docs/REVC_LCSC_CART_IMPORT_2026-08-29.csv`: consolidated LCSC lines derived from active schematic fields.
- `docs/REVC_SYMBOL_FIELD_COMPLETION_2026-08-29.csv`: LCSC matched MPN, manufacturer, package, description, stock state, and link for KiCad rows with incomplete identity fields.

The logged-in LCSC project successfully accepted all 66 cart lines as exact matches. Its export is available locally at `$HOME\Downloads\export_project_20260829_181958.xls` and is the input to the completion script.

## Current audit state

- 285 total references: 274 active and 11 DNP.
- 243 active references have an LCSC part number; 31 do not.
- 59 LCSC-assigned references lack an exact MPN and/or manufacturer field in KiCad.
- The LCSC completion sheet has matched product data for all 59 of those incomplete rows.
- Do not bulk-copy those 59 records into KiCad until the exceptions below are resolved.

## Confirmed change: regulator R61

The original 3.10 kOhm choice, UNI-ROYAL `0805W8F3101T5E` / `C2767486`, was backordered. R61 was changed in the regulator schematic to the in-stock standard-value choice:

| Field | Verified value |
|---|---|
| KiCad value | `3090` |
| LCSC | `C5121539` |
| Manufacturer | FOJAN |
| MPN | `FRC0805F3091TS` |
| Package / rating | 0805, 3.09 kOhm, 1%, 125 mW |
| Datasheet | `https://www.lcsc.com/datasheet/C5121539.pdf` |

The divider setpoint changes from approximately 5.04 V to approximately 5.03 V. A fresh KiCad CLI BOM export confirmed the schematic fields. Before the next PCB/BOM release, run **Update PCB from Schematic** for the regulator so the placed R61 footprint receives the new value and fields, then save and rerun DRC.

## Exceptions requiring a decision or source change

### Decisions recorded 2026-08-29

- Regulator C17 is an SMD electrolytic and is standardized to the existing 470 uF, 25 V SMD family used by the other bulk capacitors. Select `C48615124` / `JVE25V470M10X10` when its schematic metadata is completed; do not retain the through-hole `C34642` selection.
- HAT J12 and J18 are accepted as implemented. Retain their current connector symbols and vertical through-hole footprints; no further mechanical review is required for this sourcing pass.

These are not valid for automatic field completion:

| Refs | Current footprint | Current LCSC selection | Finding | Required action |
|---|---|---|---|---|
| HAT J17 | 1x3 SMD header | `C53207560`, 2x5 through-hole IDC | Pin count, mounting, and package mismatch | Select a valid 1x3 SMD header or change the hardware footprint/intended connector deliberately. |
| HAT J12, J18 | Vertical 1x6 / 1x4 through-hole headers | `C492414` / `C492412` | Resolved by user acceptance of the implemented connector arrangement | Retain current symbols, footprints, and LCSC selections. |
| Regulator C17 | SMD electrolytic, 10x10.5 mm | `C34642`, through-hole D10xL20 mm | Resolved: standardize to SMD `C48615124` / `JVE25V470M10X10` | Update the schematic value, LCSC, MPN, manufacturer, and datasheet fields. |
| HAT C38, C39; regulator C6, C22 | SMD electrolytic, 10x10 mm | `C48615124`, SMD D10xL10.5 mm | Body-height and land-pattern check incomplete | Verify the manufacturer land pattern and clearance, then accept or select a 10x10 mm alternative. |
| Regulator POT1-POT4 | Bourns 3296W footprint | `C49190453`, product reports JIERR while MPN is `3296W-1-103LF` | Manufacturer identity does not match design intent | Choose a verified Bourns source or explicitly approve the JIERR-compatible part after mechanical/electrical comparison. |

## Blank LCSC fields

Do not treat all blanks as errors. Classify them explicitly:

- Intentional non-procurement items: board logos and test points, unless the test-point hardware is intentionally installed.
- Regulator Q1/Q2/Q4/Q5/Q7/Q8/Q10/Q11: exact non-LCSC source is Nexperia `PSMN5R2-60YLX`, LFPAK56/SOT669. Do not use `HL3400` or `C22469448`.
- Still unsourced and requiring an exact source decision: regulator F1, J1, J2, J4, RT1; HAT J19, U2, and U11.

Every populated electrical or connector part must end with an exact manufacturer PN, source/distributor PN, datasheet, symbol, footprint, and assembly disposition. A blank LCSC field is acceptable only when an explicit non-LCSC source is recorded.

## Release criteria for this pass

1. Resolve every exception above with an intentional part/footprint decision.
2. Populate exact MPN and manufacturer fields only after package, value, polarity, pin count, and orientation are verified.
3. Classify each active blank-LCSC row as exact non-LCSC source or `HOLD`; do not leave unexplained blanks.
4. Sync modified schematics to PCBs, then regenerate netlists, BOMs, CPLs, ERC, and DRC.
5. Regenerate the audit, cart, and completion sheets.
6. Re-import the final cart into LCSC and confirm exact matches, availability, MOQ changes, and no automatic substitution.
7. Only then continue to board/parts ordering.

## Restart prompt

Resume from [REVC_SYMBOL_FOOTPRINT_SOURCING_HANDOFF_2026-08-29.md](REVC_SYMBOL_FOOTPRINT_SOURCING_HANDOFF_2026-08-29.md). Review the current LCSC-to-KiCad completion sheet and close the explicit J17, J12/J18, C17, capacitor-body, and potentiometer exceptions before bulk-populating any missing KiCad manufacturer fields. Preserve the R61 3.09 kOhm FOJAN `C5121539` change and sync it to the regulator PCB. Keep all orders on hold until the regenerated audit has explicit disposition for every active reference.