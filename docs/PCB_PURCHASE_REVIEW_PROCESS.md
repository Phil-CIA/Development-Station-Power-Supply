# PCB Purchase Review Process

Purpose: define the final design, schematic, and PCB review process before purchasing any PCB for this project.

This is a purchase-release process, not a creativity process. It borrows the normal hardware-review pattern used in industry: review from system intent down to net-level implementation, then from board-level manufacturability down to each critical part's design requirement, selected part number, datasheet, symbol, footprint, orientation, and assembly data.

The central question is: does each important component match the engineering intent, connect correctly in the schematic and PCB, and have enough verified performance margin to deliver the expected behavior?

## Release Principle

A board is not ready to buy because it has no obvious visible mistakes. It is ready to buy only when the review package shows that the design intent, schematic implementation, layout implementation, manufacturing outputs, and known risks all agree.

Every pre-purchase review must end in one of three decisions:

| Decision | Meaning | Allowed next action |
|---|---|---|
| Release | No unresolved blockers; acceptable findings are documented | Generate/order fabrication package |
| Conditional release | Only explicitly accepted low-risk findings remain | Generate/order only if the disposition table is signed off |
| Hold | One or more blockers remain | Do not purchase PCB |

### Prototype Validation Build

Rev-C is being evaluated as a small prototype validation build, not as a production release. The first order should be limited to the minimum useful quantity, currently two boards, and should be treated as an instrumented experiment with an explicit bring-up plan. The prototype PCB is the correct way to validate board-specific behavior that a breadboard cannot reproduce: the actual module footprints and pin headers, copper and return paths, connector fit, Kelvin routing, switching noise, thermal spreading, and mechanical clearances.

Breadboard or jumper-wire work remains appropriate only for isolated low-voltage questions or a substitute test fixture. It is not a substitute for the Rev-C PCB when the question depends on the final layout. A prototype order is acceptable only when there are no unresolved safety, polarity, connectivity, or known footprint incompatibility blockers; unknown performance margins may remain as documented validation objectives.

The prototype release record must identify: the exact boards ordered, every accepted uncertainty, the measurements required at first power-up, stop conditions, and the change decision after bring-up. Passing prototype tests does not automatically authorize a larger production order.

For the current Rev-C prototype package, the generated order artifacts are:

- Regulator: `hardware/kicad/dsp-regulator-rev-c/prototype-gerbers/`, `prototype-bom.csv`, and `prototype-cpl.csv`.
- HAT: `hardware/kicad/dsp-regulator-hat-rev-c/prototype-gerbers/`, `prototype-bom.csv`, and `prototype-cpl.csv`.
- Consolidated two-board purchasing list: [REVC_PROTOTYPE_PURCHASE_BOM_2026-08-28.md](REVC_PROTOTYPE_PURCHASE_BOM_2026-08-28.md).
- Post-review LCSC optimization workflow: [LCSC_BOM_OPTIMIZATION_PROCESS.md](LCSC_BOM_OPTIMIZATION_PROCESS.md).

These outputs were generated from the current Rev-C schematic and PCB files on 2026-08-27. They are **prototype-release artifacts**. Independent Gerber/drill viewing and the JLCPCB online DFM review are now complete with no major blocking issue reported by the user on 2026-08-28. RB-012 layout closure, final connector/footprint review, and explicit prototype signoff remain required before treating the design as production-ready.

### JLCPCB DFM Screening

The generated Rev-C prototype files were screened against the published [JLCPCB rigid PCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities) and [PCB assembly capabilities](https://jlcpcb.com/capabilities/pcb-assembly-capabilities) on 2026-08-27. The boards use 1.6 mm FR-4, which is within the published standard thickness range. The measured geometry is deliberately above the small-drill risk threshold that affected an earlier design:

| Feature | Regulator Rev-C | HAT Rev-C | Prototype screening result |
|---|---:|---:|---|
| Minimum drilled feature found | 0.30 mm | 0.30 mm | No sub-0.30 mm drill found in the PCB files |
| Via size / drill | 0.60 / 0.30 mm | 0.60 / 0.30 mm | 0.15 mm radial annular copper |
| Minimum routed track width | 0.20 mm | 0.20 mm | Above the conservative 0.127 mm JLC standard trace screen |
| Board thickness | 1.6 mm | 1.6 mm | Within normal FR-4 range |
| Layer count | 4 copper layers | 2 copper layers | Standard rigid-board configurations |
| Exposed thermal-pad or via-in-pad construction | None found | None found | No tiny thermal-pad drill exception identified |

This is a file-level screen, not a JLC manufacturing guarantee. The user uploaded the exact Gerbers and drill files to JLCPCB on 2026-08-28 and reported no major issue that would prevent ordering. The online quote/DFM result is therefore **PASS for prototype fabrication review**, subject to retaining the quote configuration and checking any final JLC order-page changes. Confirm the selected stackup, copper weight, finished-hole interpretation, board outline, plated/non-plated hole classification, solder-mask slivers, silkscreen clipping, and any charge for unusual slots, edge plating, castellations, or assembly options. Do not accept an automatic drill-size upgrade without reviewing the affected footprint and net.

### Schedule Strategy: DFM First, Procurement in Parallel

Because the PCB/stencil lead time and critical-part lead times overlap, use two separate tracks:

1. Upload both Gerber packages to JLCPCB as quote/DFM jobs first. Save the quote and review the complete preview and engineering flags before payment. Uploading is not approval of an automatic geometry change.
2. If JLCPCB reports no fatal DFM issue and no unapproved drill, slot, outline, mask, or silkscreen change, place the two-board prototype PCB and stencil order. Record the quote number, selected stackup, thickness, copper weight, finish, stencil thickness, and any accepted warning.
3. In parallel, order long-lead parts and modules from the BOM. Do not wait for PCB fabrication to begin the U2/U11, connectors, protection parts, regulators, shunts, and other critical-part procurement.
4. Keep the prototype order separate from production approval. A DFM-clear PCB order proves that JLC can manufacture the files; it does not prove the design will work. First-power stop conditions, module fit, harness checks, thermal checks, and RB-012 validation remain bring-up objectives.

Do not place the PCB order if the JLC review flags an automatic drill-size change, an unmanufacturable slot or outline, a solder-mask sliver or copper-clearance violation, a missing layer, a board-size/stackup mismatch, or a footprint change that has not been reviewed against the schematic and BOM. Those are reasons to correct the source PCB and regenerate the package before ordering.

## Required Evidence Package

Create or update a dated review note before ordering. The review note must link or name the exact evidence used.

Minimum evidence:

1. Project scope and board revision under review.
2. Schematic files, PCB files, netlists, ERC reports, and DRC reports reviewed.
3. Engineering requirement source used for each reviewed circuit or component.
4. Connector contract or interface table for every board-to-board, board-to-panel, and external harness interface.
5. BOM or parts-position export used for footprint and part validation.
6. Datasheet or manufacturer product-page link for every critical selected part number.
7. SPICE model package and simulation results for circuits that depend on analog margin, startup behavior, feedback stability, thresholds, protection timing, or fault behavior.
8. Gerber/drill outputs, or a note that the design is still in pre-output review.
9. Known open loops with one of these dispositions: blocker, accepted, deferred, or not applicable.
10. Final release decision.

## Review Ladder

Run the review from broad to narrow. Do not start by checking individual footprints until the architecture and interface contract are stable.

### 1. System Intent Review

Confirm the board still solves the right problem.

- Active project direction agrees with `README.md` and the latest dated handoff.
- Board role is clear: regulator, HAT/control, front-panel display, USB hub, or support fixture.
- The board boundary is intentional: functions that should be local are not crossing harnesses unnecessarily.
- Required bring-up workarounds are understood and either preserved or intentionally removed.
- Bench-only bypasses, DNP options, and recovery paths are documented.
- Safety-critical behavior is identified before schematic signoff.

For the current Rev-C power hardware, this means the regulator board owns Kelvin sensing, telemetry front ends, fast OCP, feedback selectors, range switching, and regulator power stages. The HAT owns control, storage, USB-UART, and display/control interfaces.

### 2. Interface And Harness Contract Review

Freeze every interface before layout release.

- Connector family, part number, mate, keying, retention, current rating, voltage rating, wire gauge, and pin count are selected.
- Signal and power harnesses are separated where that is part of the architecture.
- Every pin has one owner, one direction, one voltage domain, and one return path.
- Power return current is allocated intentionally; do not rely on a single ground pin by habit.
- Cross-board nets match one-for-one between schematics, netlists, and documentation.
- Legacy orphan crossings are removed before release.
- Mechanical access, probe access, cable bend direction, latch access, and board edge clearance are checked.

Current Rev-C hold examples:

- Regulator J8/J9 symbol-to-footprint mismatch must be resolved.
- HAT-side matching coplanar connectors must be added.
- Legacy HAT `VSENSE_*` and direct range-control crossings must be removed or explicitly justified.
- Decide whether I2C polling is enough for AW9523 fault observation or whether an interrupt pin crosses the signal harness.

### 3. Electrical Architecture Review

Confirm that the schematic implements the intended electrical behavior.

- Power tree sources, sequencing, enable pins, UVLO pins, and reset states are deterministic.
- Each IC supply voltage is inside absolute maximum and recommended operating range.
- Local bypass capacitors exist, are correctly valued, and connect to the intended rails.
- Feedback paths regulate from the intended physical sense point.
- Current-sense, Kelvin, comparator, and telemetry paths measure the intended quantity.
- Open-line, floating-input, startup, reset, and brownout behavior are reviewed.
- Protection circuits fail toward the safe state.
- Test points exist for bring-up-critical nodes.

Current Rev-C hold examples:

- AW9523B power-on/reset state must be validated against LM74502 `EN_UVLO` behavior.
- INA2180, TLV1702, TLV9352, INA3221, and range-switch paths must remain regulator-local.
- Fast OCP must use true differential shunt sensing; do not credit absolute rail-voltage comparison as calibrated OCP.

### 4. Schematic Implementation Review

Review the actual schematic, not just the intended design.

- Reference designators are unique and annotated.
- Symbol pin numbers match the selected manufacturer package.
- No hidden power pins or implicit global labels create unintended connections.
- All no-connect pins are intentionally marked.
- Net labels are consistent across sheets, boards, and docs.
- ERC is current for the exact schematic state being reviewed.
- ERC errors are fixed. ERC warnings are either fixed or dispositioned with a reason.
- Netlist is regenerated after the final schematic edit.
- Text search or netlist audit confirms critical refs and nets exist where expected.

Recommended local checks:

1. Search fresh netlists for critical IC reference designators and rails.
2. Search connector pins for expected nets and unexpected legacy nets.
3. Compare schematic connector tables against generated netlist connectivity.
4. Confirm no outdated ERC report is being used as evidence.

### 5. PCB Layout Review

Review placement and routing against the electrical requirements.

- Board outline, mounting holes, keepouts, edge clearances, and panel constraints are correct.
- High-current paths are short, wide, and have planned return paths.
- Kelvin sense traces leave shunt pads independently and do not carry load current.
- Sensitive analog pairs route together and away from switch nodes, inductors, MOSFET gates, and digital edges.
- Bypass capacitors are physically close to IC supply pins with short ground return.
- Hot parts have copper, spacing, and airflow appropriate for expected dissipation.
- Connector orientation and pin 1 location match cable assembly assumptions.
- Silkscreen gives useful polarity, pin 1, connector, and test-point information.
- Probe-critical nodes are reachable after assembly.
- Wi-Fi or RF keepouts are obeyed.
- PCB DRC is current for the exact PCB state being reviewed.

### 6. Manufacturing Output Review

Review the files the board house will actually build.

- Gerbers open correctly in an independent viewer.
- Drill files are present and aligned with copper layers.
- Board outline layer is present and closed.
- Copper, solder mask, paste, and silkscreen layers are present for all required sides.
- Slot, cutout, castellated, plated-hole, and non-plated-hole intent is clear.
- Fabrication notes match the selected board house options.
- Minimum trace, clearance, annular ring, drill size, copper weight, solder mask dam, and board thickness match the board house capability.
- If assembly is ordered, BOM and CPL/position files agree with footprints and rotations.

### 7. Part Alignment Review

This is the individual-part pass. Perform it after architecture, schematic, layout, and outputs are coherent. The goal is to prove that each selected part number is the right part, is represented correctly in KiCad, is connected according to the engineering document, and has enough margin to deliver the expected performance.

For each reviewed component, answer these questions:

1. What engineering requirement or design note does this part satisfy?
2. Is the part connected correctly to satisfy that requirement?
3. Does the selected manufacturer part number meet the electrical, thermal, mechanical, tolerance, speed, voltage, current, package, sourcing, and assembly requirements?
4. Is the datasheet or manufacturer link correct for the exact selected part number or exact orderable family?
5. Does the schematic symbol match the selected part number and package pinout?
6. Does the footprint match the selected part number's package drawing, land pattern, pitch, pad count, thermal pad, height, and orientation?
7. Does the PCB implementation preserve the performance assumed by the design: placement, routing, Kelvin connections, bypassing, heat spreading, creepage/clearance, return path, and noise isolation?
8. Are alternates, substitutions, and DNP options documented well enough that they cannot silently change behavior?

For every new, changed, high-risk, or hand-soldered part, check:

- Manufacturer part number and LCSC/DigiKey/Mouser part number match the intended device.
- Engineering requirement, design calculation, or circuit note identifies why this part is needed.
- Datasheet or manufacturer product link matches the exact part under review.
- Symbol pinout matches the selected package, not just the device family.
- Footprint pad count, pitch, body outline, courtyard, and thermal pad match the datasheet.
- Pin 1 indicator is correct in symbol, footprint, silkscreen, and assembly drawing.
- Polarity markings are clear for diodes, LEDs, electrolytics, ICs, connectors, and polarized modules.
- Rotation is correct in CPL/position output if assembly is ordered.
- Alternate parts are truly footprint- and pin-compatible.
- Hand-solder access is acceptable for prototype-only parts.
- Test or rework access exists for parts likely to need bring-up changes.

### 8. Performance Conformance Review

This pass ties the component review back to expected circuit behavior. A correct footprint is not enough if the part cannot meet the job.

For each critical circuit block, document the expected operating point and the checked margin:

- Regulators and power switches: input range, output voltage, load current, dissipation, enable behavior, startup state, current limit, thermal margin, diode/FET stress, and layout-dependent heat path.
- Current sensing and OCP: shunt value and tolerance, common-mode range, amplifier gain and offset, comparator threshold, hysteresis, propagation delay, saturation margin, and fault output behavior.
- Feedback and remote sense: nominal setpoint, open-sense behavior, fallback path, input bias/leakage sensitivity, compensation impact, and physical sense location.
- Digital control and expanders: voltage domain, reset state, bus address, pull-ups, interrupt behavior, boot straps, firmware ownership, and safe default state.
- Connectors and harnesses: current per contact, total return current, wire gauge, voltage rating, mating/keying, pin order, strain relief, and service access.
- Clocks, USB, UART, I2C, SPI, and display links: voltage level, pull-ups/series resistors, bus speed, trace length, connector pinout, and termination or edge-rate assumptions.

If the performance check depends on a calculation, simulation, datasheet table, bench measurement, or earlier issue tracker note, cite that source in the review note.

### 9. SPICE Modeling And Validation Review

Use SPICE where simple inspection or DC math is not enough to trust the design before buying a PCB. The model does not need to be perfect, but it must be good enough to answer the design question it is being used for.

Model these circuit classes when they are new, changed, safety-related, or previously failed on the bench:

- Regulator feedback, enable, soft-start, compensation, load-step, startup, and shutdown behavior.
- Remote-sense selectors, fallback paths, open-sense behavior, leakage sensitivity, and diode/op-amp dominance cases.
- Current-sense amplifiers, shunts, comparator thresholds, hysteresis, OCP propagation, and fault latch or wired-OR behavior.
- MOSFET, ideal-diode, load-switch, and range-switch gates, including Vgs margin, body diode paths, startup defaults, and fault states.
- Analog filtering and noise-sensitive thresholds where tolerance or phase shift can change behavior.
- Any circuit where a wrong transient, threshold, or open/floating node could damage a board, load, or test setup.

Each SPICE validation package must include:

1. The design question being answered.
2. The schematic revision, component values, and selected PNs modeled.
3. Model source for each active device: manufacturer model, vendor macro-model, generic substitute, or hand-built approximation.
4. Known model limitations and what the simulation is not claiming.
5. Testbench files and commands or tool settings needed to rerun them.
6. Nominal-case result.
7. Worst-case or swept cases for tolerance, input voltage, load current, temperature when relevant, and likely fault states.
8. Pass/fail criteria with numeric margins.
9. Plots, measured values, or log excerpts that support the conclusion.
10. A short conclusion: pass, fail, or inconclusive.

Recommended SPICE checks for this project:

- Feedback selector sweep: local fallback versus remote sense dominance.
- Open-sense fault case: leakage or floating `VSENSE_*` node cannot drive the rail unsafe.
- OCP threshold sweep: shunt tolerance, INA2180 gain/offset, comparator threshold, hysteresis, and saturation margin.
- Range-switch gate/default-state check: MOSFET gate voltage and LM74502 enable behavior during power-up, reset, and controller high-Z states.
- Power-path stress check: diode/FET current, dissipation, reverse/backfeed state, and startup transient.

SPICE results do not replace ERC, DRC, datasheet checks, or bench validation. They answer the pre-purchase question: does the design have enough modeled margin to justify committing copper?

Critical-part minimum set for this project:

- All regulators, ideal-diode or load-switch controllers, MOSFETs, shunts, current-sense amplifiers, comparators, op amps, I2C expanders, MCUs/modules, USB-UART parts, connectors, protection diodes, crystals, and programming/debug headers.

## Blocker Rules

These findings always block PCB purchase unless the review note explicitly narrows the scope to a board where they do not apply.

- Current ERC errors.
- Current PCB DRC errors, unconnected pads, or footprint errors.
- Symbol pin count does not match footprint pin count.
- Connector pinout is provisional, undocumented, or unmatched on the mating board.
- Known unsafe startup, reset, enable, or floating-input behavior.
- Power connector current or return path is not sized for expected load.
- Selected part number does not meet the engineering requirement or has no documented margin check for a critical circuit.
- Datasheet or manufacturer link is missing or does not match the selected part number.
- Schematic symbol pinout has not been checked against the selected part number and package.
- Critical part footprint has not been checked against its datasheet.
- PCB placement/routing invalidates a performance assumption, such as Kelvin sensing, local bypassing, thermal spreading, or low-noise routing.
- Required SPICE validation is missing, stale relative to the schematic, uses undocumented substitute models, or does not include the fault/worst-case condition that motivated the model.
- SPICE result is fail or inconclusive for a circuit that affects startup safety, regulation stability, OCP, feedback fallback, or power-path protection.
- Gerbers/drills have not been opened after generation.
- Review evidence is stale relative to the latest schematic or PCB edit.

## Acceptable Findings

Low-risk findings may be accepted only when documented in the disposition table.

Usually acceptable with explanation:

- Cosmetic silkscreen warnings that do not hide polarity, pin 1, connector labels, or assembly-critical references.
- ERC warnings caused by known library pin-type limitations when the electrical connection is manually verified.
- Deferred bench measurements that do not affect PCB geometry, startup safety, or fabrication correctness for this order.

Usually not acceptable:

- Any warning that could hide a wrong footprint, wrong pinout, unintended net tie, missing return path, or inaccessible critical node.

## Disposition Table Template

| Item | Source | Risk | Decision | Required action before purchase |
|---|---|---|---|---|
|  | ERC / DRC / schematic review / layout review / BOM review / gerber review | Low / Medium / High | Fix / Accept / Defer / N/A |  |

## Final Signoff Template

| Gate | Result | Evidence |
|---|---|---|
| System intent reviewed | Pass / Fail |  |
| Interface contract frozen | Pass / Fail |  |
| Electrical architecture reviewed | Pass / Fail |  |
| Schematic/ERC current | Pass / Fail |  |
| PCB/DRC current | Pass / Fail |  |
| Engineering requirements traced to critical parts | Pass / Fail |  |
| Selected PNs checked against requirements | Pass / Fail |  |
| Datasheet links checked for critical PNs | Pass / Fail |  |
| Symbols checked against selected PNs | Pass / Fail |  |
| Footprints checked against selected PNs | Pass / Fail |  |
| Performance margins checked | Pass / Fail |  |
| SPICE models current and rerunnable | Pass / Fail / N/A |  |
| SPICE pass/fail criteria met | Pass / Fail / N/A |  |
| Gerbers/drills reviewed | Pass / Fail / N/A |  |
| Open findings dispositioned | Pass / Fail |  |
| Final decision | Release / Conditional release / Hold |  |

## How To Use This On The Current Rev-C Work

Before buying any Rev-C PCB, the review decision is automatically Hold until these are closed or explicitly dispositioned in a dated review note:

1. Resolve J8/J9 symbol-footprint mismatches.
2. Freeze signal and power harness connector families, mates, pin order, current ratings, and return allocation.
3. Add matching HAT-side coplanar connectors and verify one-for-one connectivity.
4. Remove orphaned HAT legacy analog crossings.
5. Validate AW9523B reset-safe behavior against LM74502 inputs.
6. Rerun and document required SPICE checks for feedback fallback, open-sense behavior, OCP thresholds, range-switch default states, and power-path stress where applicable.
7. Regenerate ERC reports and netlists for both boards after the connector migration.
8. Run PCB DRC after placement/routing updates.
9. Perform final connector, BOM, footprint, pin 1, gerber, and drill review.
