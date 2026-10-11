# Rev-C Prototype Purchase BOM - 2026-08-28

## Purpose

This is the consolidated purchasing list for two Rev-C prototype sets. It is derived from the current generated KiCad BOMs and is intended to accelerate procurement while the PCB/stencil order and long-lead parts proceed in parallel.

Use the reusable [LCSC BOM optimization process](LCSC_BOM_OPTIMIZATION_PROCESS.md) immediately after each design review. This document is the Rev-C implementation of that process and should be updated for later revisions rather than recreated.

The raw board BOMs remain the source evidence:

- `hardware/kicad/dsp-regulator-rev-c/prototype-bom.csv`
- `hardware/kicad/dsp-regulator-hat-rev-c/prototype-bom.csv`

The raw BOM includes DNP, logo, test-point, custom-library, and hand-installed items. This document separates those categories; it does not silently substitute parts.

## Prototype quantity model

| Board | Prototype boards | Per-board active grouped lines | Two-board requirement |
|---|---:|---:|---:|
| Regulator Rev-C | 2 | 62 | Multiply active quantity by 2 |
| HAT Rev-C | 2 | 26 | Multiply active quantity by 2 |

Order at least the calculated quantity plus normal assembly overage for small passives. For unique modules, connectors, regulators, shunts, and protection devices, buy one spare where practical.

## Review result: exact identities and two-board quantities

The following assignments were checked against the LCSC product records on 2026-08-28. `Prototype qty` is the requirement for two boards before optional spare/overage. These rows are procurement records, not hardware changes.

| Board refs | KiCad value | Exact manufacturer PN | Manufacturer | LCSC PN | Footprint / path | Prototype qty | Status |
|---|---|---|---|---|---|---:|---|
| HAT C32,C36,C37,C40,C41,C42,C57 | 100nF | `CL21B104KCFNNNE` or `CC0805KRX7R9BB104` | Samsung Electro-Mechanics or YAGEO | `C28233` or `C49678` | 0805 / LCSC SMT candidate | 14 | Conditional: current export permits two exact choices; freeze one before JLC |
| HAT C35 | 100nF | `CC0805KRX7R9BB104` | YAGEO | `C49678` | 0805 / LCSC SMT candidate | 2 | Conditional: verify voltage requirement and quote |
| HAT R3,R67,R68 | 10k ohm | `0805W8F1002T5E` | UNI-ROYAL | `C17414` | 0805 / LCSC SMT candidate | 6 | Pass for identity; stock/assembly quote still required |
| HAT R1 | 470 ohm | `0805W8F4700T5E` | UNI-ROYAL | `C17710` | 0805 / LCSC SMT candidate | 2 | Conditional: verify tolerance/power against circuit |
| HAT R2 | 150 ohm | `0805W8F1500T5E` | UNI-ROYAL | `C17471` | 0805 / LCSC SMT candidate | 2 | Conditional: verify tolerance/power against circuit |
| HAT R69,R70 | 4.7k ohm | `0805W8F4701T5E` | UNI-ROYAL | `C17673` | 0805 / LCSC SMT candidate | 4 | Conditional |
| HAT R77,R79 | 33 ohm | `0805W8F330JT5E` | UNI-ROYAL | `C17634` | 0805 / LCSC SMT candidate | 4 | Conditional |
| HAT R82,R83 | 5.1k ohm | `0805W8F5101T5E` | UNI-ROYAL | `C27834` | 0805 / LCSC SMT candidate | 4 | Conditional |
| HAT R63 | 100 ohm | `0805W8F1000T5E` | UNI-ROYAL | `C17408` | 0805 / LCSC SMT candidate | 2 | Conditional |
| HAT R64 | 100k ohm | `0805W8F1003T5E` | UNI-ROYAL | `C149504` | 0805 / LCSC SMT candidate | 2 | Conditional |
| HAT C34 | 100uF | `CL31A107MQHNNNE` | Samsung Electro-Mechanics | `C15008` | 1206 / LCSC SMT candidate | 2 | Conditional: active only if DNP is intentionally cleared |
| HAT C38,C39 | C_Small_US | `JVE25V470M10X10` | JIERR | `C48615124` | 10 x 10.5 mm / hand-install | 4 | Conditional: verify polarity, height, and ripple |
| HAT D1,D2 | Green | `KT-0805G` | Hubei KENTO Elec | `C2297` | 0805 / LCSC SMT candidate | 4 | Conditional |
| HAT D12 | WS2812B | `XL-1615RGBC-2812B` | XINGLIGHT | `C5349954` | 1.6 x 1.5 mm / LCSC SMT candidate | 2 | Conditional: verify LED pinout against footprint |
| HAT D14 | USBLC6-2SC6 | `USBLC6-2SC6` | STMicroelectronics | `C7519` | SOT-23-6L / LCSC SMT candidate | 2 | Conditional |
| HAT J7 | Fan | `2510-3A` | BOOMELE (Boom Precision Elec) | `C29275` | 1x3 2.54 mm through-hole / hand-install | 2 + spares | Conditional: mating termination, pin 1, and fan lead order open |
| HAT J9,J10 | Display Uart / USB Hub I2C | `B4B-XH-A(LF)(SN)` | JST | `C144395` | Current custom `JST_B4B-XH-A` footprint; catalog part is 4-pin, 2.5 mm | 4 | Conditional: verify custom footprint dimensions, pin 1, and mating housing |
| HAT J18 | Coplanar signal harness | `PZ254R-11-04P` | XFCN | `C492412` | Horizontal/right-angle 1x4 2.54 mm through-hole interface as physically observed on HAT; existing KiCad footprint name says `Vertical`, but pad geometry is compatible and the remaining difference is silkscreen | 2 + spares | Conditional: verify body/height, pin 1, mating cable, board clearance, and silkscreen orientation |
| HAT J12 | Coplanar power harness | `PZ254R-11-06P` | XFCN | `C492414` | Horizontal/right-angle 1x6 2.54 mm through-hole interface as physically observed on HAT; existing KiCad footprint name says `Vertical`, but pad geometry is compatible and the remaining difference is silkscreen | 2 + spares | Conditional: verify body/height, pin 1, parallel ground contacts, current, mating cable, board clearance, and silkscreen orientation |
| Regulator J6 | Coplanar signal harness | `HX PZ2.54-1x4P TP-YQ` | hanxia | `C41417361` | Vertical SMD 1x4 2.54 mm header with staggered pins; matches the regulator-board side as physically observed | 2 + spares | Conditional: pad pattern aligns; verify body/height, pin 1, mating cable, and board clearance |
| Regulator J7 | Coplanar power harness | `HX PZ2.54-1x6P TP-YQ` | hanxia | `C41417363` | Vertical SMD 1x6 2.54 mm header with staggered pins; matches the regulator-board side as physically observed | 2 + spares | Conditional: pad pattern aligns; verify body/height, power-contact suitability, mating cable, and board clearance |
| HAT J8 | USB-TYPE-C-16P | `USB-TYPE-C-019` | DEALON | `C2927039` | SMD right-angle / LCSC SMT candidate | 2 | Conditional: verify land pattern and assembly library |
| HAT Q9 | AO3400A | `AO3400A` | AOS | `C20917` | SOT-23 / LCSC SMT candidate | 2 | Conditional: source BOM now active; verify final assembly preview |
| HAT U12 | W25Q128 | `W25Q128JVSIQ` | Winbond | `C97521` | SOIC-8 / DNP | 0 | DNP; exclude from active quantity |
| HAT U13 | AHT20 | `AHT20` | Aosong | `C2757850` | custom SMD / hand-install pending footprint review | 2 | Conditional |
| HAT U15 | AMS1117-3.3 | `AMS1117-3.3` | Advanced Monolithic Systems | `C6186` | SOT-223 / LCSC SMT candidate | 2 | Conditional: thermal and output-capacitor review |
| HAT U16 | CH340C | `CH340C` | WCH | `C84681` | SOIC-16 / LCSC SMT candidate | 2 | Conditional: source BOM corrected and regenerated; verify final assembly preview |

The HAT rows not listed above remain Hold or hand-install until their exact PN, footprint, and disposition are closed. The regulator rows likewise remain Hold because the available prototype export does not provide controlled manufacturer/LCSC identity fields. Do not infer a PN from a generic value.

The consolidated two-board active quantities remain provisional until the final assembly preview is checked. The HAT prototype BOM and netlist now include active Q9 and corrected U16 `C84681`; the custom-field assembly CSV has been aligned, but JLC must still confirm the selected parts and placements.

## Regulator exact-part audit

The regulator netlist, rather than the simplified prototype CSV, is the current source for these assignments. Quantities below are for two boards before spares.

| Refs | KiCad value | Exact manufacturer PN | Manufacturer | LCSC PN | Two-board qty | Path/status |
|---|---|---|---|---|---:|---|
| U1,U3 | TLV9352 | `TLV9352IDR` | Texas Instruments | `C4366962` | 4 | LCSC SMT candidate; verify SOIC-8 pinout and stock |
| U5 | AW9523B | `AW9523BTQR` | AWINIC | `C148077` | 2 | LCSC SMT candidate; verify QFN thermal pad and pinout |
| U6,U12 | INA2180A2IDGKR | `INA2180A2IDGKR` | Texas Instruments | `C2058508` | 4 | LCSC SMT candidate; verify VSSOP-8 pinout |
| U10,U11 | INA3221 | `INA3221AIRGVR` | Texas Instruments | `C181255` | 4 | LCSC SMT candidate; verify VQFN-16 exposed pad |
| U9,U14,U15,U16 | LM74502DDFR | `LM74502DDFR` | Texas Instruments | `C3236215` | 8 | LCSC SMT candidate; verify TSOT-23-8 pinout and reset behavior |
| Q1,Q2,Q4,Q5,Q7,Q8,Q10,Q11 | PSMN5R2-60YL | `PSMN5R2-60YLX` | Nexperia | LCSC not verified; Digi-Key `1727-2595-1-ND` (cut tape) | 16 | Conditional sourcing, electrical/package identity Pass: 60 V, 5.2 mOhm LFPAK56/SOT669; do not use the invalid R+O HL3400 `C22469448` |
| R7,R37 | 20m ohm | `JER2512F2R020` | JIERR | `C49164694` | 4 | Hand-install or SMT candidate; verify Kelvin land pattern and power |
| R19,R45 | 200m ohm | `FRM252WJR200TK` | FOJAN | `C7420022` | 4 | Hand-install or SMT candidate; verify tolerance, power, and Kelvin land pattern |
| C17 | 470uF 50V | `KM477M050G20RR0VH2FP0` | Chengx | `C34642` | 2 | Hand-install; verify through-hole lead spacing and height |
| C29 | 1000uF 25V | `EEEFT1E102UP` | Panasonic | `C401766` | 2 | Hand-install; verify 10 x 10.2 mm footprint and ripple |
| C8,C19,C32 | 330uF 25V | `RVT1V331M1010` | ROQANG | `C72510` | 6 | Hand-install/SMT candidate; verify height and ripple |
| L1,L4 | ferrite bead | `BLM31PG121SN1L` | Murata | `C85841` | 4 | LCSC SMT candidate; verify current and impedance |

The remaining regulator exact records are present in the generated netlist for many resistors, small capacitors, diodes, and connectors, but the full two-board purchasing table still needs to be generated from those fields. Do not treat the simplified `prototype-bom.csv` as proof that those fields are absent.

## Hand-install / buy separately

These parts should be procured separately from the JLC SMT assembly BOM unless the assembly quote explicitly supports the required process:

| Board refs | Part/value | Quantity for two boards | Procurement status |
|---|---|---:|---|
| HAT U2 | Teyleten Robot ESP32-C3 Supermini, Amazon ASIN `B0D47G24W3` | 2 + spare recommended | Exact received-board dimensions, pinout, and current still require verification |
| HAT U11 | ALMOCN STM32F103C8T6 Blue Pill, Amazon ASIN `B0C9PKJF2T` | 2 + spare recommended | Prototype source; exact board revision and dimensions require verification |
| HAT J7 | BOOMELE `2510-3A`, LCSC `C29275` | 2 + spares | Through-hole hand-install. Fan pin order is J7.1 switched return, J7.2 positive, J7.3 tach |
| HAT J12/J18/J17 | 2.54 mm pin headers | 2 each + spares | Confirm mating cable, pin 1, and orientation |
| HAT J9/J10 | JST XH/display and USB-hub connectors | 2 each + spares | Exact connector PN/mate remains to be confirmed |
| Regulator J1/J2/J4 | Terminal blocks and board headers | 2 each + spares | Confirm mating and current/voltage ratings |
| Regulator J6/J7 | hanxia `HX PZ2.54-1x4P TP-YQ` / `HX PZ2.54-1x6P TP-YQ`, LCSC `C41417361`/`C41417363` | 2 each + spares | Hand-install pending body/height, mating cable, and harness-current validation; placed pad pattern aligns with catalog staggered-pin description |
| Regulator shunts | R7/R37 20 mOhm; R19/R45 200 mOhm | 2 each + spare | Verify resistance, power rating, package, and Kelvin land pattern |
| Regulator POT1-POT4 | Bourns 3296W-1-103LF | 8 total + spares | Confirm final threshold strategy and availability |
| Regulator F1 | MDL 5 fuse | 2 + spare | Confirm exact fuse voltage/current and holder fit |

## JLC SMT assembly candidates

These are suitable candidates for a separate JLC assembly BOM after exact orderable PNs and stock are confirmed:

- 0805 passives, LEDs, ferrite/inductor parts, and small signal diodes.
- `BSS138`, `AO3400A`, and other SOT-23 transistors.
- `LM74502DDFR`, `INA2180A2IDGKR`, `INA3221`, `TLV1702AIDGKR`, and `AW9523B`.
- `AMS1117-3.3` in SOT-223 if thermal and assembly process are accepted.
- `CH340C`, `W25Q128`, `AHT20`, `USBLC6-2SC6`, and `WS2812B` where the exact assembly part is selected.
- USB-C connector and other surface-mount connectors only after the JLC library part is checked against the PCB footprint.

Do not hand the raw KiCad BOM to JLC as a final assembly BOM without removing DNP rows and confirming every assembly part has an exact manufacturer or distributor PN.

## Corrections already reflected

- HAT U2 is ESP32-C3 Supermini, not ESP32-C6.
- HAT J7 is the 3-pin fan connector, LCSC `C29275`.
- `Fan_Tach` is J7 pin 3 to R3 and STM32 U11 PA0.
- HAT U16 is CH340C, corrected LCSC `C84681` (the previous `C2609442` assignment is invalid).
- Regulator U9/U14/U15/U16 use `LM74502DDFR`.
- DNP rows remain excluded from active quantities; do not populate them unless the prototype plan explicitly calls for them.

## Open procurement blockers

1. Confirm JLC assembly availability, exact PNs, package/orientation, and quote status for every SMT candidate.
2. Verify the HAT J12/J18 right-angle `C492412/C492414` records and datasheets against the physically observed horizontal connectors and placed footprints; treat the `Vertical` library name as a naming issue, not a pad-geometry mismatch.
3. Resolve J12/J18 mating cable, body height, pin 1, board clearance, silkscreen orientation, and power-return allocation.
4. Confirm connector mates and footprint contracts, especially J7's three-wire fan termination and J9/J10's corrected four-pin identity.
5. Confirm custom ESP32-C3 and Blue Pill physical fit against received boards.
6. Complete the regulator MOSFET sourcing record: use Nexperia `PSMN5R2-60YLX` / Digi-Key `1727-2595-1-ND` or another traceable source for the exact LFPAK56 device. Do not use R+O `HL3400`/`C22469448` in the current LFPAK56 footprint.
7. Confirm remaining regulator exact orderable PNs, shunt power ratings, package/Kelvin land pattern, and hand-install sourcing.
8. Confirm whether the current design needs all four potentiometers populated for prototype bring-up.
9. Confirm fan tach output type, pull-up behavior, connector pin order, and PA0 input limits before connecting the fan.
10. Re-run the final HAT JLC quote/DFM and compare BOM, CPL, PCB references, and tach routing after the source corrections.

## Recommended ordering sequence

1. Order the long-lead modules, shunts, connectors, pots, fuse, and other hand-installed parts now.
2. Use the regenerated board ZIPs for the PCB/stencil quote after the tach revision passes JLC DFM.
3. Purchase JLC SMT assembly only after the assembly BOM has exact PNs, stock, and DNP disposition.
4. Buy overage for small passives and at least one spare of each unique module/connector.
5. Record distributor, order date, exact received marking, and substitutions in the bring-up log.

## Parts-order queue while PCB release stays on hold

The following procurement is allowed to proceed in parallel with the final board-release gate, provided the exact manufacturer/distributor identities above are retained and no board-order commitment is made:

- Regulator power MOSFETs: Nexperia `PSMN5R2-60YLX` through the verified distributor path; do not substitute `HL3400`/`C22469448`.
- Long-lead modules and passives: shunts, pots, fuse, connector mates, harness terminals, and the custom board modules that are already identified in this worksheet.
- Fan connector and harness materials: J7 3-pin fan header, mating connector, and fan-tach wiring parts.
- Hand-install items and spares: XH/JST mates, 2.54 mm headers, and any unique board-to-board cable assemblies.
- Exact SMT candidates only after the JLC assembly quote confirms package, stock, and DNP status for each part.

The board-order gate remains `HOLD` until the final symbol-to-footprint and exact-PN review is signed off for the current schematic/PCB package. Procurement may proceed, but fabrication release does not.

## Status

**Sourcing consolidation is incomplete; do not use this narrative worksheet as a vendor upload.** Regenerate `docs/REVC_PROCUREMENT_AUDIT_2026-08-29.csv` and `docs/REVC_LCSC_CART_IMPORT_2026-08-28.csv` with `scripts/export-revc-procurement-audit.ps1` after each fresh BOM/netlist export. The audit exposes every `HOLD` reference; the LCSC cart contains only active `SOURCED` lines. Parts may be ordered only after every active `HOLD` has an exact LCSC record or an explicitly documented non-LCSC path, while the PCB fabrication order remains on hold until the Rev-C gate is closed. The fan-tach implementation remains preserved: J7.1 switched return, J7.2 selected fan positive, J7.3 `Fan_Tach` to U11 PA0 through the 3.3 V pull-up, with Q9 as the low-side switch.
