# Development Station Power Supply Handoff - 2026-08-26

## Session outcome

This session kept the project on the bring-up-first path and captured the current state of the two active workstreams:

1. Power hardware migration to the Rev-C coplanar two-board architecture.
2. Front-panel display evaluation with the Elecrow CrowPanel Advance 4.3 HMI as the primary target, while the custom display board remains a secondary path.

The repo’s current active direction is still documented in [README.md](../README.md) and [docs/display-project/README.md](display-project/README.md): the display effort is not a redesign-first task, and the power hardware remains the first engineering priority.

## Current working state

### Power hardware

The active hardware direction is a Rev-C migration away from the stacked regulator/HAT arrangement and toward a side-by-side coplanar layout with separate power and signal harnesses.

Key facts captured at this point:

- The regulator board owns the local analog functions: Kelvin sensing, telemetry front ends, fast overcurrent protection, feedback selectors, and regulator power stages.
- The HAT remains the control board for STM32, storage, USB-UART, and display/control interfaces.
- Fresh 2026-08-18 netlists confirm the relevant INA2180, TLV1702, TLV9352, and INA3221 devices are on the regulator side and not on the HAT.
- The earlier 74HC595 output-contention issue is closed; the AW9523B is now the sole local range-control driver.
- Reset-safe behavior remains a hold point: the AW9523 output state at power-up/reset must be verified against the LM74502 EN/UVLO input behavior before release.

### Display path

The active display focus remains:

- Primary: Elecrow CrowPanel Advance 4.3 HMI.
- Secondary: custom front-panel board with the Hosyond MSP4021/ST7796S hardware path.

The CrowPanel path remains the main bring-up target because it validates the required UART/LVGL interface and the common front-panel contract without forcing a redesign before the hardware is proven.

## Current blockers and release gates

The current work is not yet ready for fabrication release or a clean “done” signoff.

### Rev-C blockers

- The connector migration is now implemented as regulator J6/J7 to HAT J18/J12: 4-pin signal and 6-pin power, with matching current netlist mappings.
- Exact connector manufacturer PNs, datasheet links, mating parts, current ratings, keying, and mechanical fit still require review.
- The HAT still needs orphaned VSENSE and legacy direct signal crossings removed or explicitly dispositioned.
- Fresh ERC reports now show 0 errors / 0 warnings for both boards.
- Fresh PCB DRC reports now show 0 violations, 0 unconnected pads, and 0 footprint errors for both boards.
- The power harness current return allocation and connector derating still require documented closure.
- Wi-Fi keepout and final board-edge antenna placement are not yet validated.

### Electrical gates still open

- AW9523 power-on/reset safe-state behavior must be validated against the LM74502 range-control inputs.
- The selected signal and power harness contract must be frozen before cable release.
- The connector PN, datasheet, symbol, footprint, and mating-part review must pass before purchase release.
- The Rev-B bypass remains in place until the range-switch audit is complete.
- The HAT and regulator must be cross-checked one-for-one for every connector signal after the coplanar split.

## Work completed in this session

This session consolidated the current state from the active notes and repo docs:

- Confirmed the active repo direction from [README.md](../README.md) and [docs/display-project/README.md](display-project/README.md).
- Reviewed the current Rev-C migration status in [docs/DEV_STATION_HANDOFF_2026-08-17.md](DEV_STATION_HANDOFF_2026-08-17.md).
- Confirmed the earlier design history in [docs/DEV_STATION_HANDOFF_2026-08-14.md](DEV_STATION_HANDOFF_2026-08-14.md).
- Added [docs/PCB_PURCHASE_REVIEW_PROCESS.md](PCB_PURCHASE_REVIEW_PROCESS.md) as the final high-level-to-part-level design, schematic, PCB, manufacturing-output, and part-alignment gate before buying any PCB.
- Added [docs/J8_J9_REVC_HANDOFF_2026-08-26.md](J8_J9_REVC_HANDOFF_2026-08-26.md) as the focused restart note for closing the Rev-C coplanar connector mismatch and harness contract.
- Captured the session as a clean stopping point rather than continuing into speculative redesign or layout work.

## Recommended next restart

Resume from [docs/DEV_STATION_HANDOFF_2026-08-17.md](DEV_STATION_HANDOFF_2026-08-17.md) and continue the coplanar Rev-C migration in this exact order:

1. Use [docs/PCB_PURCHASE_REVIEW_PROCESS.md](PCB_PURCHASE_REVIEW_PROCESS.md) as the required purchase-release gate.
2. Resume from [docs/J8_J9_REVC_HANDOFF_2026-08-26.md](J8_J9_REVC_HANDOFF_2026-08-26.md) and complete connector PN, datasheet, footprint, mating-part, and mechanical review.
3. Remove the remaining orphaned HAT crossings or record an explicit disposition.
4. Validate AW9523 power-on/reset safe-state behavior against the LM74502 inputs.
5. Rerun netlists/ERC/DRC after any connector or schematic changes.
6. Run the final PCB connectivity and purchase review before any fabrication release.
7. Keep the Rev-B bench bypass in place while the range-switch path is still under audit.
8. Continue the display bring-up work in parallel on the CrowPanel path and keep the custom board as a secondary fallback.

## Suggested restart prompt

Resume from [docs/DEV_STATION_HANDOFF_2026-08-17.md](DEV_STATION_HANDOFF_2026-08-17.md) and use [docs/PCB_PURCHASE_REVIEW_PROCESS.md](PCB_PURCHASE_REVIEW_PROCESS.md) as the required PCB purchase-release gate. Continue the coplanar Rev-C migration and resolve the J8/J9 mismatch first, then finalize the harness contract, validate the AW9523 safe-state outputs, regenerate ERC/netlists, and only then continue PCB placement and DRC. Keep the Rev-B bench bypasses active and continue the CrowPanel front-panel bring-up in parallel.
