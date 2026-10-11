# LCSC BOM Optimization Process

## Purpose

Run this process immediately after schematic, PCB, netlist, DRC, and manufacturing-review work is complete. It converts the raw KiCad BOM into a verified LCSC/JLCPCB procurement and assembly BOM without silently changing the design.

The process is reusable across revisions. Copy the current revision's BOM worksheet, update its revision/date/source files, and preserve the previous revision as an audit record. Do not recreate the workflow from scratch.

## Source hierarchy

Use these sources in order:

1. Current schematic and generated netlist for reference designators, values, footprints, DNP state, and connectivity.
2. Current PCB and CPL for placed footprint, side, rotation, and assembly eligibility.
3. Exact manufacturer datasheet for electrical and mechanical requirements.
4. LCSC product page and downloadable datasheet for the selected orderable item.
5. JLCPCB assembly library/availability and quote result for assembly eligibility and stock.
6. Distributor or marketplace listing only for parts that are intentionally hand-installed or unavailable through LCSC. Record the source and its limitations.

A marketplace listing is not an exact manufacturer PN unless the listing provides a controlled, traceable manufacturer part number.

## Required worksheet fields

Maintain one row per selected orderable part, with these fields:

| Field | Requirement |
|---|---|
| Board / refs | All board references covered by the row |
| Function | What requirement the part satisfies |
| KiCad value | Current schematic value |
| Exact manufacturer PN | Manufacturer's orderable PN, not a generic family name |
| LCSC PN | `C######` where LCSC is the source |
| Manufacturer | Exact manufacturer name |
| Datasheet URL | Must match the selected PN |
| LCSC product URL | Direct product page |
| Footprint | Exact placed footprint |
| Qty per board | Active populated quantity |
| Prototype qty | Number of boards plus overage/spares |
| Assembly path | LCSC SMT, hand-install, through-hole, module, or do-not-populate |
| Stock/lead time | Capture date and availability |
| Approved alternate | Only if electrically/mechanically equivalent and documented |
| Verification status | Pass, conditional, hold, DNP, or N/A |
| Notes/evidence | Pinout, voltage, current, package, thermal, mating, or risk notes |

## Procedure

### 1. Freeze the design source

- Export a fresh netlist, BOM, CPL, ERC, and DRC from the exact schematic/PCB revision.
- Record the source filenames and export date.
- Do not optimize parts while the schematic or PCB is still changing.
- Treat DNP parts, logos, fiducials, and mechanical-only references separately from populated electrical parts.

### 2. Normalize and group

- Group identical value, package/footprint, voltage rating, tolerance, and assembly function.
- Do not group parts only because their displayed value is the same. For example, `100 nF` parts with different package, dielectric, voltage, or footprint remain separate rows.
- Preserve reference designators for traceability.
- Calculate quantities for the actual prototype count, plus small-passive overage and recommended unique-part spares.
- Keep power shunts, fuses, connectors, modules, and safety/protection parts as individually reviewed rows even when values repeat.

### 3. Assign assembly disposition

Use one of these dispositions:

- **LCSC SMT:** exact LCSC part selected, stocked/quotable, footprint verified, and suitable for JLC assembly.
- **Hand-install:** module, through-hole, connector, potentiometer, large capacitor, fuse, fan interface, or part requiring a separate mechanical operation.
- **Buy separately:** source outside LCSC or marketplace procurement is required.
- **DNP:** excluded from the active prototype build and from the assembly quantity.
- **Hold:** missing exact PN, stock, datasheet, footprint, mating part, or requirement evidence.

Do not put a generic KiCad value or a marketplace description into the LCSC assembly BOM as though it were an exact orderable part.

### 4. Verify each exact part

For every critical part, check:

- Manufacturer PN and LCSC PN refer to the same device.
- Datasheet matches the selected PN and package.
- Symbol pin numbers and functions match the datasheet.
- Footprint pad count, pitch, exposed pad, body outline, height, and orientation match.
- Electrical ratings meet voltage, current, tolerance, thermal, and transient requirements.
- Part is stocked or has a realistic lead time for the build.
- Any alternate preserves pinout, footprint, ratings, and function.
- Connector mating part, polarity, keying, retention, and wire size are documented.

### 5. Optimize without redesigning silently

Permitted optimization:

- Consolidate identical verified parts into one purchasing line.
- Select a stocked LCSC part with the same electrical and mechanical specification.
- Replace a generic placeholder with the exact part already intended by the schematic.
- Buy practical overage for passives and spares for unique parts.
- Separate JLC assembly parts from hand-installed parts.

Requires schematic/PCB review and a new export:

- Changing package, footprint, pinout, voltage rating, current rating, tolerance, thermal pad, connector pitch, mating family, or protection behavior.
- Substituting a regulator, MOSFET, shunt, connector, MCU/module, USB part, or sensor.
- Changing a DNP decision that affects the prototype test plan.

Never substitute a part solely because it is cheaper or in stock.

### 6. Check LCSC/JLC assembly constraints

Before assembly ordering:

- Confirm the part appears in the selected JLCPCB assembly library or is accepted as a customer-supplied part.
- Confirm basic/extended-part fees and stock status.
- Confirm package size, polarity marking, feeder/assembly orientation, and both-side placement support.
- Confirm through-hole, module, connector, large capacitor, potentiometer, fuse, and hand-solder items are excluded or explicitly supported.
- Confirm DNP parts are removed from the active assembly BOM.
- Compare the assembly BOM, CPL, PCB references, and final JLC preview.

### 7. Produce the purchasing outputs

Create and retain:

- Raw KiCad BOM for engineering evidence.
- Consolidated procurement BOM for all prototype boards.
- LCSC/JLC SMT assembly BOM containing only exact assembly parts.
- Hand-install/buy-separately list.
- DNP list.
- CPL/position export.
- Source netlist, ERC, DRC, Gerbers, and drill files.
- Quote/stock snapshot with date.
- Open-issue and substitution log.

### 8. Final review gate

The LCSC BOM is ready for ordering only when:

- Every populated critical part has an exact PN, datasheet, footprint, and assembly disposition.
- All repeated passives are consolidated without mixing incompatible specifications.
- All DNP items are excluded from active quantities.
- Modules, connectors, shunts, fuses, and other hand-installed parts have separate sourcing records.
- Quantities cover the prototype count and planned spares.
- LCSC/JLC stock and lead times are recorded.
- Any alternate or substitution is explicitly approved.
- BOM, CPL, PCB, and schematic reference designators agree.
- The final JLC assembly preview has been reviewed.
- Remaining unknowns are marked Hold or Conditional and assigned an owner.

## Current Rev-C implementation

The first implementation is [REVC_PROTOTYPE_PURCHASE_BOM_2026-08-28.md](REVC_PROTOTYPE_PURCHASE_BOM_2026-08-28.md). Its raw sources are:

- `hardware/kicad/dsp-regulator-rev-c/prototype-bom.csv`
- `hardware/kicad/dsp-regulator-hat-rev-c/prototype-bom.csv`

Run `scripts/export-revc-procurement-audit.ps1` after each fresh BOM/netlist export. It produces two linked outputs:

- `docs/REVC_PROCUREMENT_AUDIT_2026-08-29.csv`: one row for every raw BOM reference, including `SOURCED`, `HOLD`, and `DNP` status.
- `docs/REVC_LCSC_CART_IMPORT_2026-08-28.csv`: consolidated quantities for only the `SOURCED` LCSC records.

The cart is not approval to substitute or omit `HOLD` rows. The audit is the required gap list: every populated reference must be `SOURCED` or have an explicit documented non-LCSC procurement path before the purchase review can pass.

Current engineering consolidation:

- Regulator: 229 active BOM rows grouped into 62 purchasing lines.
- HAT: 40 active BOM rows grouped into 26 purchasing lines.
- HAT U2: ESP32-C3 Supermini, Amazon ASIN `B0D47G24W3`, hand-installed.
- HAT U11: STM32F103C8T6 Blue Pill, Amazon ASIN `B0C9PKJF2T`, hand-installed.
- HAT J7: 3-pin 2.54 mm header, LCSC `C29275`.
- HAT U16: CH340C, LCSC `C84681`.
- Fan tach: J7.3 `Fan_Tach` to STM32 U11 PA0 through the 3.3 V pull-up.

Verification snapshot (2026-08-28):

- The HAT generated assembly BOM contains usable LCSC assignments for its resistor/capacitor groups, LEDs, protection parts, connectors, AHT20, flash, regulator, and Q9. The export omits many manufacturer PNs; those rows must be completed from the matching LCSC product records before they are submitted to JLC.
- HAT U16 was misassigned in the prior BOM: `C2609442` is a KOA `RN73R2ETTD1061B50` resistor, not CH340C. The schematic and regenerated BOM now use WCH `CH340C`, LCSC `C84681`, datasheet `https://www.lcsc.com/datasheet/C84681.pdf`. Recheck the final JLC quote against this identity.
- HAT Q9 was marked DNP in the prior prototype BOM. The schematic and regenerated BOM now mark wired Q9 `AO3400A` active, as required by the fan-tach handoff. Recheck the final assembly preview includes it.
- The regulator generated netlist contains many exact LCSC assignments even though `prototype-bom.csv` is a simplified value/footprint export. Verified examples include TI `TLV9352IDR`/`C4366962`, AWINIC `AW9523BTQR`/`C148077`, TI `INA2180A2IDGKR`/`C2058508`, TI `INA3221AIRGVR`/`C181255`, TI `LM74502DDFR`/`C3236215`, and the listed passive, diode, inductor, connector, and shunt records.
- The regulator power MOSFET rows are now source-identified but not LCSC-verified: the intended Nexperia orderable is `PSMN5R2-60YLX` (base type `PSMN5R2-60YL`, 60 V, 5.2 mOhm, LFPAK56/SOT669), available through Digi-Key as `1727-2595-1-ND` cut tape. The assigned LCSC record was invalid R+O `HL3400`/`C22469448` (30 V, SOT-23). This was both an electrical substitution and a footprint/package mismatch. The schematic and PCB source metadata are now corrected to Nexperia; keep LCSC procurement on Hold as `TBD` until an exact LCSC record is verified, and do not use HL3400. Intended datasheet: `https://assets.nexperia.com/documents/data-sheet/PSMN5R2-60YL.pdf`.
- Regulator rows without a controlled exact record remain Hold, including the LM2596S-ADJ stages, TLV1702 stages, shunts where rating/PN is unresolved, fuse, terminal blocks, and any custom or mechanical-only items.

The current list is therefore an incomplete engineering consolidation, not an order-ready LCSC/JLC assembly BOM. Exact assignments and stock snapshots must be rechecked in the quote; do not request JLC assembly or alter hardware until the Holds are closed.
