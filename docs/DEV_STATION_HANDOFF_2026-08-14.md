# Development Station Power Supply Handoff - 2026-08-14

## Session outcome

The HAT Rev-C overcurrent-protection redesign moved from the Rev-B root-cause finding to an implemented, ERC-clean four-channel differential architecture.

The Rev-B OCP comparator arrangement is still not accepted as calibrated protection. Continue using the external bench-supply current limit for Rev-B work.

## Rev-C HAT OCP implementation

The active design is under `hardware/kicad/dsp-regulator-hat-rev-c/`.

Implemented architecture:

- U2 and U6: INA2180A2 dual current-sense amplifiers, gain 50.
- U2/U6 VS: `+3.3V Boot`, not `+12V`.
- U3 and U7: TLV1702 dual open-collector comparators powered from `+12V`.
- TLV1702 output pins 1 and 7 are correctly typed `open_collector` in the embedded KiCad symbol.
- Each comparator pair may therefore use the intended wired-OR connection without an output-to-output ERC conflict.
- Four current-sense channels are present for the 3.3V and 5V high/low ranges.

The INA2180 supply correction is mandatory: its recommended supply range is 2.7V to 5.5V and its absolute maximum is 6V. The prior 12V connection was unsafe.

## Validation checkpoint

Fresh KiCad artifacts generated on 2026-08-14:

- `hardware/kicad/dsp-regulator-hat-rev-c/ERC.rpt`: 0 errors, 0 warnings.
- `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`: U2/U6 pin 8 are on `+3.3V Boot`.
- The same netlist keeps U3/U7 pin 8 on `+12V`.
- U3/U7 pins 1 and 7 export as `open_collector`.

The netlist export timestamp is 2026-08-14T06:09:57. Regenerate ERC and the netlist after any subsequent schematic change.

## PCB completion gate

RB-012 in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md` now contains a mandatory checklist. Do not finalize the Rev-C PCB or release fabrication outputs until it is complete.

The gate requires:

1. Verify regulator-side/load-side IN+ and IN- polarity on all four shunts.
2. Use true Kelvin routing from every shunt; sense traces must carry no load current.
3. Keep Kelvin pairs together and away from switching and digital edges.
4. Place and verify one local 100nF VS-GND bypass capacitor at each INA2180.
5. Calculate output and saturation margin for every 20mOhm and 200mOhm channel on 3.3V.
6. Lock trip current, reset current, threshold tolerance, and hysteresis for every range.
7. Decide whether calibrated pots remain acceptable or fixed/reference-derived thresholds are required.
8. Verify INA2180/TLV1702 footprints and symbol-to-footprint pin mappings.
9. Keep comparator input networks local and provide amplifier, threshold, and fault test points.
10. Rerun ERC, netlist export, PCB DRC, and a final connectivity audit before release.

## Documentation state

- RB-012 is the active issue and PCB release gate in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`.
- `docs/REVC_3V3_HIGH_RANGE_OCP_DESIGN_2026-08-14.md` records the original single-channel INA180/fixed-reference design calculations. It is useful analysis, but it does not yet describe the implemented dual INA2180/pot-threshold architecture and must not be treated as the final four-channel BOM.
- The 2.00A high-range ceiling remains provisional.

## Other active hardware loops

These remain open from the previous handoff:

- Regulator Rev-C selector: finish TLV9352 metadata/netlist cleanup and bench-check 5V operation plus open-sense fallback when parts are available.
- Regulator 3.3V selector: normal remote-sense operation is bench-proven; capture the remaining open-sense fallback stability check.
- HAT range switching: retain the bypass and audit Q1/Q2/Q7/Q8 pinout, body-diode orientation, and gate/source/drain/Vgs during `D9OFF`, `D9ON`, and `D9FLASH` before selecting the final Rev-C topology.
- Do not remove the working range bypass until the switch path passes that audit.

## Working-tree state

The repository is intentionally dirty. Rev-C regulator and HAT project folders, today’s OCP design note, and several simulation artifacts are currently untracked. Rev-B KiCad project/netlist files and the tracker also contain modifications. Do not discard or overwrite these changes during cleanup.

KiCad lock files were present at stop in the HAT Rev-C folder:

- `~DSP-Regulator-HAT-RevC.kicad_pro.lck`
- `~DSP-Regulator-HAT-RevC.kicad_sch.lck`

Remove them only after confirming KiCad is closed; do not commit them.

## Safe restart state

- Rev-B remains the bench-learning baseline and requires the external PSU current limit.
- HAT Rev-C OCP schematic connectivity is ERC-clean, but the OCP is not yet layout-complete or bench-calibrated.
- INA2180 devices must remain powered from `+3.3V Boot` or another supply no higher than 5.5V.
- TLV1702 comparators may remain on 12V.
- Range-switch bypasses remain installed for bench work.

## Next-session priority

1. Choose whether to resume the HAT range-switch bench audit or begin Rev-C PCB placement.
2. If beginning PCB placement, start with RB-012 shunt polarity/Kelvin mapping and U2/U6 bypass placement before routing load-current paths.
3. Complete the four-channel threshold and saturation calculations before considering the PCB finished.
4. Run fresh ERC/netlist/DRC checks after layout-driven schematic edits.

## Suggested restart prompt

Resume from `docs/DEV_STATION_HANDOFF_2026-08-14.md`. Preserve the Rev-B bench bypasses and the verified Rev-C OCP supply split: INA2180 U2/U6 on `+3.3V Boot`, TLV1702 U3/U7 on `+12V`. If starting PCB layout, execute the RB-012 completion checklist before fabrication release.
