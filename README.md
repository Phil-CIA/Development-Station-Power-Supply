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

## Current direction (2026-09-23)

The project has shifted to a bring-up-first path. Rev-C regulator and HAT boards (`DSP-Regulator-RevC`, `DSP-Regulator-HAT-RevC`) were ordered 2026-08-28, received, and are now on the bench.

**Recent progress:**
- HAT Rev-C standalone bring-up: **PASS**. Rails stable (+5V_Boot ~5.04V, +3.3V ~3.29V); STM32 Blue Pill controller, AW9523 I/O expander, and AHT20 sensor all confirmed healthy over serial.
- Regulator Rev-C standalone bring-up: **HOLD**. Buck cores regulate correctly at the LM2596 IC pins when force-enabled, but the board output rails collapse downstream of the cores (5V rail ~4.01V, 3.3V rail ~0.06V). Boards remain unstacked until this is root-caused; see RB-011 in the change tracker.
- Range-MOSFET gate-path control (AW9523-driven): **PASS** (2026-09-23). All six range paths (Q1–Q5, Q9) verified with matching commanded/read-back state and correlated INA telemetry.
- Next bench objective: rail/telemetry calibration, per `docs/CALIBRATION_WORKFLOW_2026-09-23.md`.

Current plan:
1. Use the hardware already built as the learning and validation baseline
2. Resolve the Rev-C regulator downstream rail-collapse fault before any stacked (regulator+HAT) testing
3. Continue calibration bring-up on the HAT/Blue Pill path in parallel where it doesn't require stacking
4. Continue front-panel work inside this repo
5. Evaluate two display paths in parallel:
   - the existing custom front-panel hardware already designed
   - the Elecrow CrowPanel Advance 4.3 inch HMI display
6. Avoid any new display redesign until bench results show it is needed

Current power-hardware status:

- Rev-C HAT: standalone power, MCU boot, I2C/telemetry, and AW9523 range-control paths are all bench-verified.
- Rev-C regulator: buck cores are functional at the IC level, but the 5V/3.3V distribution/select path to the board output nodes is faulted (RB-011 HOLD, RB-010 fallback check still pending).
- HAT Rev-C implements four-channel differential OCP with dual INA2180A2 amplifiers and dual TLV1702 comparators; this OCP is not yet bench-calibrated.
- See `docs/DEV_STATION_HANDOFF_2026-09-23.md` and `docs/DEV_STATION_HANDOFF_2026-09-21.md` for the active bench state, and `docs/REVC_BRINGUP_PLAN.md` for the current bring-up plan.

## Hardware scope in this repo

- External supply 12V feeds the regulator and HAT control stack
- Regulator board generates the main rails (+5V, +3.3V primary; +12V intermediate)
- HAT board handles measurement, feedback, and control behavior
- Front-panel display path is now treated as a system integration problem, not a redesign-first task
- USB hub files are retained as reference and redesign starting point where needed

## Hardware board status

**DSP-Regulator-RevB:** Built/bench baseline (superseded by Rev-C bring-up)
- ERC: 0 electrical errors (8 library warnings, non-critical)
- Connector contract: Verified against HAT-RevB (all 10 required signal mappings present)
- Latest netlist export: 2026-06-11 01:43:33

**DSP-Regulator-HAT-RevB:** Built/bench baseline (superseded by Rev-C bring-up)
- ERC: Clean (0 errors, 0 warnings)
- Connector contract: Verified with Regulator-RevB (all 10 required signal mappings present)
- Latest netlist export: 2026-06-11 01:10:51
- Design: Single +5V_Boot rail (simplified from adjustable 3-channel architecture)

**DSP-Regulator-RevC:** Ordered 2026-08-28, received, on the bench
- Standalone bring-up: HOLD — buck cores good at the IC pins, output rails collapse downstream (RB-011)
- Not yet stacked with the HAT

**DSP-Regulator-HAT-RevC:** Ordered 2026-08-28, received, on the bench
- Standalone bring-up: PASS — rails, MCU, AW9523 range control, and AHT20 telemetry all verified
- OCP (INA2180A2/TLV1702) not yet bench-calibrated

These Rev-B files remain the earlier built/bench baseline. Active bring-up now happens on the Rev-C boards; see `docs/REVC_BRINGUP_PLAN.md` for the current state and next steps.

## Firmware status

Current firmware work in this repo includes:

- logical rail telemetry mapping
- automatic range selection support
- per-rail calibration and persistent configuration storage
- current-limit operating modes
- TFT front-panel support hooks

## Current bench priorities

- Root-cause the Rev-C regulator downstream rail-collapse fault (RB-011 HOLD) before any stacked testing
- Run the RB-010 open-remote-sense fallback check once the regulator rails are stable
- Continue HAT/Blue Pill rail and telemetry calibration per `docs/CALIBRATION_WORKFLOW_2026-09-23.md`
- Resume front-panel display bring-up
- Map the existing front-panel connector to the smart-display interface as needed

## Useful project references

- docs/REVC_BRINGUP_PLAN.md — active Rev-C bench bring-up state and next steps
- docs/SYSTEM_DEVELOPMENT_WORKFLOW.md — branch/PR workflow now in effect
- docs/FIRMWARE_DEVELOPMENT_PLAN.md — firmware feature inventory and milestone/branch plan
- docs/display-project/README.md
- docs/GPIO_PINOUT.md
- docs/USB_HUB_CHANGE_TRACKER.md
