# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-08-11.md](docs/DEV_STATION_HANDOFF_2026-08-11.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md)

Session status: Rev-B bring-up is now in temporary jumper-bypass mode after range-switch MOSFET misconfiguration findings. Next bench work is bypass-mode validation and decision gating.

Controller lock for this branch:
1. Active bench controller is STM32F103C8T6 Blue Pill from `stm32-bluepill-bringup/`.
2. Command-path checks in this handoff (`HELP`, `SRTEST`, `D9OFF`, `D9ON`, `D9FLASH`, `INARAILS`) refer to the STM32 command shell in `stm32-bluepill-bringup/src/main.cpp`.
3. Root-repo ESP32-C6 HAT environments are retained for older display/HAT experiments and are not the active bench-control baseline for this stop.

Transport lock for the current code baseline:
1. Host-to-display telemetry path is STM32 USART3 (`PB10`/`PB11`) to CrowPanel UART1 (`IO19`/`IO20`).
2. CrowPanel shared-console UART0 intake was a temporary replacement-panel path and is historical only unless explicitly reselected for bench recovery.
3. Keep USB Serial on the CrowPanel for console/diagnostics and the dedicated UART1 pins for incoming telemetry.

## 2026-08-11 Update Summary

1. Captured hardware-state delta from last bench session:
   - range MOSFETs on affected path removed
   - hard jumper installed from +5V_Reg to R19
   - hard jumper installed from +3.3V_Reg to R17
2. Reframed active bring-up objective from switched-path fault-isolation to bypass-mode stability verification.
3. Created dedicated bench worksheet for immediate workstation use:
   - [docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md](docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md)

## Current Technical Interpretation

1. Firmware command path remains useful for observability and stability checks.
2. In current bypass mode, D9 command behavior no longer proves switched-path electrical transfer.
3. Priority is to prove rail and node stability at bypass injection points before deciding whether to keep bypass for continued subsystem bring-up.
4. New tracked issue from bench evidence: `RB-011` (3.3V selector/reference dependency on `R25`) is now open in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`.
5. Temporary bench disposition for this branch:
   - proceed only with `R25` populated/effective and `+3.3V_Reg` near expected class (`~3.3V`, observed `~3.335V`),
   - treat the `~3.52V` state as HOLD for investigation and redesign follow-up.

## 2026-08-11 Bench Progress Update (current)

1. STM32 firmware identity verified on hardware via ST-Link readback/hash match to current repo build.
2. Phase 2 first-power behavior captured: stable around `12.02V` and `~118mA`, no anomaly reported.
3. Phase 3/4 measurements captured with load:
   - 5V loaded channel observed around `4.960V` / `~341.8mA`
   - 3.3V loaded channel observed around `3.320V` / `~340.0mA`
   - resistor warming observed, no instability/runaway reported
4. Phase 5 command-path sanity completed PASS (`HELP`, `SRTEST`, `D9OFF`, `D9ON`, `D9FLASH`, `INARAILS`) with no reset/brownout.
5. Decision state updated to keep bypass for current bring-up block (conditional on R25-effective 3.3V behavior).

## Files Updated In This Session

1. [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md)
   - Added post-session bench delta noting jumper-bypass state.
2. [docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md](docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md)
   - New bench worksheet with pass/fail criteria, stop rules, measurement tables, and keep-bypass decision gate.
3. [docs/DEV_STATION_HANDOFF_2026-08-11.md](docs/DEV_STATION_HANDOFF_2026-08-11.md)
   - New active handoff for today.

## Open Work For Next Session

1. Close remaining evidence gaps in worksheet artifacts:
   - Phase 1 resistance/continuity values if re-captured
2. Open schematic corrective action for `RB-011` (3.3V selector/reference dependency on `R25`).
3. Continue subsystem bring-up only in the R25-effective state; treat recurrence of ~3.52V state as HOLD.
4. Keep Rev-B debug and Rev-C redesign edits separated per branch discipline.

## Suggested Restart Prompt

Resume from [docs/DEV_STATION_HANDOFF_2026-08-11.md](docs/DEV_STATION_HANDOFF_2026-08-11.md). Assume Rev-B HAT is in temporary jumper-bypass mode with +5V_Reg -> R19 and +3.3V_Reg -> R17 hard jumpers installed. Execute [docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md](docs/REVB_HAT_BYPASS_BENCH_WORKSHEET_2026-08-11.md) and report pass/fail per phase, measured node voltages, thermal observations, and keep-bypass vs reintroduce-switching decision.
