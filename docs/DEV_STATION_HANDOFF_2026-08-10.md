# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-20-session2.md](docs/DEV_STATION_HANDOFF_2026-07-20-session2.md)

Session status: STM32 command-path debug completed; post-session bench rework moved the board into temporary jumper-bypass mode after range MOSFET configuration was found incorrect.

## Post-Session Bench Update (2026-08-10, later)

1. Range MOSFET stage was identified as incorrectly configured for current bring-up intent.
2. Range MOSFET devices on the affected path were removed for temporary bypass-mode validation.
3. Hard jumpers were installed to bypass the removed range-switch stage:
   - `+5V_Reg` -> `R19`
   - `+3.3V_Reg` -> `R17`
4. Current Rev-B bring-up state should be treated as **jumper-bypass baseline**, not switched-path baseline.
5. Until the range-switch network is reintroduced, command-path checks (`D9ON`, `D9OFF`, `D9FLASH`) remain useful for firmware/control visibility but are no longer proof of end-to-end switched-path behavior.

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

1. Validate temporary jumper-bypass baseline stability first:
   - verify VIN, `+5V_Boot`, `+5V_Reg`, and `+3.3V_Reg` under no-load and light-load dwell.
2. Capture a bypass-mode measurement table at the installed jumpers:
   - voltage at `R19` feed point from `+5V_Reg`
   - voltage at `R17` feed point from `+3.3V_Reg`
   - any unexpected droop/heating at jumper wires or destination nodes.
3. Reclassify firmware command tests in this mode:
   - use `D9ON`, `D9OFF`, `D9FLASH` only as command/telemetry checks while switched hardware is bypassed.
4. Decide forward path once bypass baseline is characterized:
   - keep bypass for continued subsystem bring-up, or
   - reintroduce corrected range-switch topology via controlled rework/revision.

## Suggested Restart Prompt

Resume from [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md). Assume STM32 command control is working and focus on PCB-level correction strategy for the 3.3V switch path (Q1/Q2/Q7/Q8 network). Provide a concrete rework-or-revision execution plan and measurement checklist for validation.

## Rev-C Split Kickoff (2026-08-10)

1. Created isolated Rev-C KiCad project folders for both boards:
   - [hardware/kicad/dsp-regulator-rev-c](hardware/kicad/dsp-regulator-rev-c)
   - [hardware/kicad/dsp-regulator-hat-rev-c](hardware/kicad/dsp-regulator-hat-rev-c)
2. Renamed project roots to `RevC` across primary KiCad files (`.kicad_sch`, `.kicad_pcb`, `.kicad_pro`, `.kicad_prl`, `.net`) in both Rev-C folders.
3. Updated schematic title blocks to Rev C with date `2026-08-10`:
   - [hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch](hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.kicad_sch)
   - [hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch](hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch)
4. Isolated manufacturing output paths to OneDrive Rev-C locations:
   - Regulator Gerber path now targets `.../Regulator board/Rev C/Gerbers/`
   - HAT plot/BOM paths now target `.../Regulator Hat/REV C/...`
5. Created external Rev-C manufacturing directories:
   - `C:\Users\forch\OneDrive\JLCPCB files\Development station supply\Regulator board\Rev C\Gerbers`
   - `C:\Users\forch\OneDrive\JLCPCB files\Development station supply\Regulator Hat\REV C\KiCad Files`

### Next Immediate Rev-C Tasks

1. Open both Rev-C projects in KiCad and run fresh ERC/DRC (copied Rev-B reports were intentionally removed in Rev-C folders).
2. Re-export both Rev-C netlists and run connector-contract verification against Rev-C netlist paths.
3. Apply/confirm RB-010 mitigation edits only in Rev-C projects before layout start.

## Session Close Transition (for next chat)

1. This session is closed with Rev-C file separation completed and validated at the net-contract level.
2. The next chat should focus on continued Rev-B bring-up issue discovery and bench evidence capture.
3. Keep change hygiene strict:
   - Rev-B folders for bench debug and measurement correlation.
   - Rev-C folders for redesign implementation only.

### Next Chat Prompt (Rev-B bring-up)

Resume from [docs/DEV_STATION_HANDOFF_2026-08-10.md](docs/DEV_STATION_HANDOFF_2026-08-10.md). Assume Rev-B is now in temporary jumper-bypass mode with range MOSFETs removed on the affected path and hard jumpers installed from `+5V_Reg` to `R19` and `+3.3V_Reg` to `R17`. Focus first on proving bypass-mode electrical stability and documenting exact node voltages/thermal behavior, then define the decision gate for when to keep bypass for continued bring-up versus when to reintroduce corrected switching hardware.
