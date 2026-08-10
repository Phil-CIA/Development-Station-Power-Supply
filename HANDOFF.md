# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-07-29.md](docs/DEV_STATION_HANDOFF_2026-07-29.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-03.md](docs/DEV_STATION_HANDOFF_2026-07-03.md)

Session status: LM74502H high-side NMOS range-switch driver integration validated at netlist/ERC level; next step is layout + single-channel bench validation.

## Session Closeout - 2026-08-10

This stop captured focused SPICE validation for the 5V feedback selector behavior, including normal crossover and fault cases for VSENSE_5V+.

## What Was Completed In This Stop Window

1. Added headroom check deck for LMV358-class stage:
	- hardware/sim/voltage_reference_headroom.cir
2. Added selector crossover sweep deck and outputs:
	- hardware/sim/5v_reg_selector_sweep.cir
	- hardware/sim/5v_reg_selector_sweep.csv
	- hardware/sim/5v_reg_selector_sweep.log
	- hardware/sim/5v_reg_selector_sweep.svg
3. Added low/disconnected sense fault-case deck and outputs:
	- hardware/sim/5v_reg_selector_fault_cases.cir
	- hardware/sim/5v_reg_selector_fault_cases.log
	- hardware/sim/5v_reg_selector_fault_sweep.csv
4. Added lightweight plotting helper:
	- hardware/sim/plot_5v_reg_selector.py
5. Added July 29 project handoff note for range-switch driver checkpoint:
	- docs/DEV_STATION_HANDOFF_2026-07-29.md

## Key Simulation Findings

1. Selector crossover behavior:
	- When VSENSE_5V+ is below approximately 5.043 V, 5V_reg holds at approximately 5.043 V (local fallback).
	- When VSENSE_5V+ is above approximately 5.043 V, 5V_reg follows VSENSE_5V+.
2. Fault behavior:
	- VSENSE_5V+ low or disconnected with no leakage: 5V_reg remains near local fallback.
	- VSENSE_5V+ disconnected and floating with injected leakage: once floating node rises above local setpoint, selector can be pulled high.
3. Floating-node threshold with current model values:
	- With 10 Mohm bias to ground, crossover occurs near 0.504 uA injected current (5.043 V / 10 Mohm).

## Current Objective At Stop

Harden the VSENSE_5V+ path against open-line and leakage-driven false dominance, then apply the chosen fix in schematic and re-run targeted simulation/bench checks.

Rev-C routing gate:
- Do not start Rev-C PCB routing until RB-010 (VSENSE_5V+ open/floating dominance risk) is mitigated and re-validated in SPICE.

## Next Session Priority Order

1. Decide final hardening method for VSENSE_5V+ open/floating cases:
	- stronger pull-down or bias network,
	- selector clamping or buffering changes,
	- op-amp supply and output-limiting strategy.
2. Update regulator schematic accordingly and re-export netlist/ERC.
3. Re-run the two SPICE decks:
	- 5v_reg_selector_sweep.cir
	- 5v_reg_selector_fault_cases.cir
4. Bench-check one 5V channel with forced low/open sense conditions before cloning across rails.

## Repo Checkpoint Snapshot At Stop

Tracked modified files include HANDOFF.md plus KiCad project/schematic/netlist/erc updates under:
- hardware/kicad/dsp-regulator-hat-rev-b/
- hardware/kicad/dsp-regulator-rev-b/

New/untracked session artifacts include:
- HANDOFF_2026-07-28_RANGE_MOSFET_RESTART.md
- HANDOFF_2026-07-28_SPICE_FOLDER_TRANSITION.md
- docs/DEV_STATION_HANDOFF_2026-07-29.md
- hardware/sim/
- hardware/kicad/dsp-regulator-hat-rev-b/backups/

Note: a KiCad lock file is present and should be ignored/removed before commit if not needed:
- hardware/kicad/dsp-regulator-hat-rev-b/~DSP-Regulator-HAT-RevB.kicad_pro.lck

## Current Stop Summary (2026-07-20)

1. STM32 Blue Pill firmware is built and flashed successfully over ST-Link.
2. HAT to CrowPanel transport is working; the CrowPanel monitor shows `rx frames` advancing with no errors.
3. `SCREEN MAIN` was accepted, so the remaining work is to confirm the visible dashboard counts and finish the UI-side cleanup.

## Next Chat Start

1. Open [docs/DEV_STATION_HANDOFF_2026-07-20-session2.md](docs/DEV_STATION_HANDOFF_2026-07-20-session2.md).
2. Verify the CrowPanel is on the main dashboard and the counts are visible on-screen.
3. If needed, inspect the LVGL update path in `crowpanel-43-bringup/src/main.cpp`.
