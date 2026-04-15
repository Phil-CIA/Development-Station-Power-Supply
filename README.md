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

## Current direction (2026-04-15)

The project has shifted to a bring-up-first path.

Current plan:
1. Use the hardware already built as the learning and validation baseline
2. Bring up the power-supply control path on the bench
3. Continue front-panel work inside this repo
4. Evaluate two display paths in parallel:
   - the existing custom front-panel hardware already designed
   - the Elecrow CrowPanel Advance 4.3 inch HMI display
5. Avoid any new display redesign until bench results show it is needed

## Hardware scope in this repo

- External supply 12V feeds the regulator and HAT control stack
- Regulator board generates the main rails
- HAT board handles measurement, feedback, and control behavior
- Front-panel display path is now treated as a system integration problem, not a redesign-first task
- USB hub files are retained as reference and redesign starting point where needed

## Firmware status

Current firmware work in this repo includes:

- logical rail telemetry mapping
- automatic range selection support
- per-rail calibration and persistent configuration storage
- current-limit operating modes
- TFT front-panel support hooks

## Current bench priorities

- safe first power-up and validation of the built boards
- confirm regulator and HAT behavior on the bench
- resume front-panel display bring-up
- map the existing front-panel connector to the smart-display interface as needed

## Useful project references

- docs/display-project/README.md
- docs/GPIO_PINOUT.md
- docs/USB_HUB_CHANGE_TRACKER.md
