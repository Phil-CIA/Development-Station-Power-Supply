# Development-Station-Power-Supply

Integrated hardware and firmware repository for the development-station bench supply, its control electronics, front-panel display path, and supporting boards.

## What this repo actually contains

This is the active working repo for the full bench-supply platform, not just a single firmware target. It currently includes:

- ESP32 control firmware
- DSP regulator board design
- DSP regulator HAT / measurement-control board design
- original front-panel display board files
- USB hub board files and next-iter reference cleanup
- bring-up notes, pin maps, and display evaluation references

## Current direction (2026-08-17)

The project has shifted to a bring-up-first path.

**Recent progress:**
- Completed design simplification: migrated from adjustable 3-channel architecture to single +5V_Boot rail
- Connector contract verification passing (10 signal mappings validated across regulator↔HAT interface)
- Both RegulatorRevB and HAT-RevB boards electrically verified and ready for order prep

Current plan:
1. Use the hardware already built as the learning and validation baseline
2. Keep Rev-B bench work in proven bypass states while Rev-C electrical issues are closed
3. Continue Rev-C as two coplanar, side-by-side regulator and control/HAT boards with separate power and signal harnesses
4. Keep Kelvin sensing, current monitoring, fast OCP, and feedback processing local to the regulator board
5. Complete Rev-C regulator and HAT design validation before PCB fabrication release
6. Continue front-panel work inside this repo
7. Evaluate two display paths in parallel:
   - the existing custom front-panel hardware already designed
   - the Elecrow CrowPanel Advance 4.3 inch HMI display
8. Avoid any new display redesign until bench results show it is needed

Current power-hardware status:

- The 3.3V regulator selector redesign is bench-proven in normal remote-sense operation; fallback and 5V TLV9352 checks remain open.
- The Rev-B HAT current-range switch remains bypassed pending MOSFET pinout and Vgs validation.
- HAT Rev-C implements four-channel differential OCP with dual INA2180A2 amplifiers and dual TLV1702 comparators.
- The Rev-C coplanar migration now places the INA2180, TLV1702, and INA3221 analog/telemetry stages on the regulator board; the HAT remains the control board.
- Fresh Rev-C netlists now show the coplanar connector mapping implemented as regulator J6/J7 to HAT J18/J12; orphaned legacy `VSENSE_*` crossings still require cleanup or disposition.
- Regulator local control now uses AW9523B alone; the prior parallel 74HC595 output contention is closed. Reset-safe AW9523-to-LM74502 behavior still requires validation.
- Coplanar harness electrical mapping is implemented as a 4-pin signal connector and 6-pin power connector with two ground returns; exact connector PNs, datasheets, mating parts, derating, and mechanical fit remain provisional until reviewed.
- RB-012 remains a PCB release gate for Kelvin routing, bypassing, thresholds, hysteresis, footprints, and final DRC/connectivity validation.
- See `docs/DEV_STATION_HANDOFF_2026-08-17.md` for the active restart state.

## Hardware scope in this repo

- External supply 12V feeds the regulator and HAT control stack
- Regulator board generates the main rails (+5V, +3.3V primary; +12V intermediate)
- HAT board handles measurement, feedback, and control behavior
- Front-panel display path is now treated as a system integration problem, not a redesign-first task
- USB hub files are retained as reference and redesign starting point where needed

## Hardware board status

**DSP-Regulator-RevB:** Ready for order
- ERC: 0 electrical errors (8 library warnings, non-critical)
- Connector contract: Verified against HAT-RevB (all 10 required signal mappings present)
- Latest netlist export: 2026-06-11 01:43:33

**DSP-Regulator-HAT-RevB:** Ready for order
- ERC: Clean (0 errors, 0 warnings)
- Connector contract: Verified with Regulator-RevB (all 10 required signal mappings present)
- Latest netlist export: 2026-06-11 01:10:51
- Design: Single +5V_Boot rail (simplified from adjustable 3-channel architecture)

These Rev-B files remain the built/bench baseline. Active redesign work now lives in the corresponding Rev-C project folders; do not release Rev-C fabrication outputs until its tracker gates are closed.

## Firmware status

Current firmware work in this repo includes:

- logical rail telemetry mapping
- automatic range selection support
- per-rail calibration and persistent configuration storage
- current-limit operating modes
- TFT front-panel support hooks

## Current bench priorities

- Complete DRC checks and generate fabrication outputs for RegulatorRevB and HAT-RevB
- Place orders for both boards
- Safe first power-up and validation of the new boards
- Confirm regulator and HAT behavior on the bench
- Resume front-panel display bring-up
- Map the existing front-panel connector to the smart-display interface as needed

## Useful project references

- docs/display-project/README.md
- docs/PCB_PURCHASE_REVIEW_PROCESS.md
- docs/GPIO_PINOUT.md
- docs/USB_HUB_CHANGE_TRACKER.md
