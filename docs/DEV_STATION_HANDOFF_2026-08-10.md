# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-20-session2.md](docs/DEV_STATION_HANDOFF_2026-07-20-session2.md)

Session status: STM32 command-path debug completed; hardware behavior points to 3.3V pass-path polarity/orientation issue on current PCB, with board rework vs board revision decision pending.

## 2026-08-10 Session Outcome

1. STM32 Blue Pill firmware was repeatedly built and flashed successfully over ST-Link from [stm32-bluepill-bringup/src/main.cpp](stm32-bluepill-bringup/src/main.cpp).
2. Added bench control commands for the 3.3V path debug:
   - `D9FLASH` (timed pulse test)
   - `D9ON` (hold path enabled)
   - `D9OFF` (hold path disabled)
3. Validated from bench observations that command execution occurs, but electrical transfer remains incorrect on the affected 3.3V path.
4. Observed behavior during testing:
   - Gate movement present around ~3.27V in command transitions
   - Intermediate node around `C20 pad 1` rising only to ~1.96-2.1V in the problematic state
   - Node decaying toward ~0.6V on OFF in at least one captured case
   - Upstream rails near expected levels (for example `+3.3_Reg` around ~3.54V, `Q1-D` around ~3.2V)
5. Net-level inspection from [hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net](hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net) confirms the affected path is through the Q1/Q2/Q7/Q8 switching network and related nodes.

## Current Technical Interpretation

1. Firmware control is functioning (commands execute and shift-register writes occur).
2. The observed node voltages are consistent with weak/partial conduction rather than a clean, low-impedance switched path.
3. Root cause likely resides in PCB implementation/orientation for the 3.3V switch FET network (Q1/Q2/Q7/Q8 path), not in command timing logic.
4. User direction for next phase: prioritize PCB revision/rework decisions over additional schematic-only discussion.

## Files Touched In This Session

1. [stm32-bluepill-bringup/src/main.cpp](stm32-bluepill-bringup/src/main.cpp)
   - Added/updated debug command handling for D9 path toggling and hold behavior.
2. (Context reference) [crowpanel-43-bringup/src/main.cpp](crowpanel-43-bringup/src/main.cpp)
3. (Context reference) [crowpanel-43-bringup/src/disp_link_slave.cpp](crowpanel-43-bringup/src/disp_link_slave.cpp)
4. New handoff: [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md)

## Open Work For Next Session

1. Decide hardware path forward for current PCB:
   - temporary bodge/rework for bring-up continuation, or
   - direct board revision path.
2. If choosing rework: define exact pad-level rework plan for Q1/Q2/Q7/Q8 path and capture before/after measurements.
3. If choosing revision: lock corrective orientation/net mapping in next PCB spin package and continue validation on revised hardware.
4. Keep firmware command hooks (`D9ON`, `D9OFF`, `D9FLASH`) as bench diagnostics for retest on revised hardware.

## Suggested Restart Prompt

Resume from [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md). Assume STM32 command control is working and focus on PCB-level correction strategy for the 3.3V switch path (Q1/Q2/Q7/Q8 network). Provide a concrete rework-or-revision execution plan and measurement checklist for validation.
