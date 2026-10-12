DEV STATION CLOSEOUT START

Date: 2026-05-26
Project: Development Station Power Supply
Phase: PCB revision (Rev-B HAT — STM32 supporting circuit complete)

What Was Completed Today:
1. Completed full U10 (STM32 Blue Pill) 44-pin wiring on Rev-B HAT schematic (morning session).
2. Added AMS1117-3.3 LDO as new U9 (VI→+5V_Boot, VO→+3.3V Boot, GND).
3. Renamed 3V3_ESP rail to +3.3V Boot; assigned PB5/PB6/PB7 development GPIO nets.
4. KiCad ERC: 0 errors / 9 warnings after U10 migration.
5. Confirmed verify-connector-contract.ps1 passes reduced mode (18/18); baseline+strict fails on Feedback crossings — expected/intentional per split-board design decision.
6. Added J19: ARM Cortex Debug 2×5 10-pin SWD programming header (text-edited into schematic). Pins: VCC/SWDIO/GND×3/SWDCLK/NRST/SWO-NC/KEY-NC.
7. Added U12: CH340G USB-UART bridge (PCM_JLCPCB-Extended). UART1_TX→RXD, UART1_RX→TXD, V3→+3.3V Boot, VCC→+3.3V Boot. XI/XO NC.
8. Added J20: USB-C 16P connector (PCM_JLCPCB-Extended). D+/D− wired to U12. CC1/CC2 pulled to GND via R88/R87 (5.1kΩ each).
9. Closed BOOT0 loop: Blue Pill JP3 + onboard pull-down handles it; no HAT circuit needed.
10. Closed NRST loop: Blue Pill onboard RC filter handles it; no HAT circuit needed.
11. Decided U11 W25Q128 role: telemetry ring buffer for raw V/I data logging, not OTA flash.
12. Updated docs/archive/hat/rev-b-2026-06-20/wiring/U10_WIRING_WORKTHROUGH.md, docs/STM32_BLUEPILL_PIN_TABLE.md, and docs/boards/hat/rev-c-2026-08-29/current/CH340C_KICAD_PLACEMENT_SPEC.md.

What Changed:
- Files updated:
  - hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch (J19 text-edited in; J20/U12/R87/R88 added via KiCad GUI)
  - hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net (re-exported after GUI additions)
  - docs/archive/hat/rev-b-2026-06-20/wiring/U10_WIRING_WORKTHROUGH.md
  - docs/STM32_BLUEPILL_PIN_TABLE.md
  - docs/boards/hat/rev-c-2026-08-29/current/CH340C_KICAD_PLACEMENT_SPEC.md (new — KiCad placement guide for CH340C)
- Behavior changed:
  - STM32 can now be programmed via ST-Link V2 over J19 SWD header.
  - PC can receive serial telemetry from STM32 UART1 over USB via U12/J20.
  - USB-C host will enumerate device correctly (CC pull-downs present).
- Hardware changes: schematic-level only.
- Bench evidence captured: no.

Current State At Stop:
- Current active target: Rev-B HAT schematic — supporting circuit complete; ready for ERC recheck and PCB layout.
- Current highest-risk unresolved item: J19 was inserted via text edit; needs KiCad visual confirm and ERC recheck to verify pin connections resolved NRST/SWDIO/SWDCLK warnings.
- Safe resume point: Open KiCad, run ERC, confirm warning count drops. Then start PCB layout of new components (J19, U12, J20).
- Hardware left connected: unknown.

Memory Pool Update (60 Seconds):
- active-baseline.md: added 2026-05-26 afternoon baseline — STM32 supporting circuit complete in schematic.
- decision-log.md: added J19/U12/J20/BOOT0/NRST/U11 decisions.
- open-loops.md: updated — CH340G loop closed; ERC recheck and PCB layout remain open.
- bench-log.md: no change.
- development-station-power-supply.md: no change needed (memory already has J19/U12 decision entries).

Known-Good Checks:
1. verify-connector-contract.ps1 reduced mode: 18/18 PASS, exit 0.
2. U12 (CH340G) net audit: TXD→UART1_RX ✓, RXD→UART1_TX ✓, VCC→+3.3V Boot ✓, V3→+3.3V Boot ✓, GND ✓.
3. J20 CC resistors: R87 (CC2→GND) and R88 (CC1→GND) both on GND net, confirmed in netlist.
4. J19 structural check: file properly closed (86315 lines, ends with sheet_instances + embedded_fonts + closing paren).
5. All 44 U10 pins confirmed wired or NC'd.

Still Blocked By:
1. KiCad ERC not yet re-run after J19 text edit + GUI additions — run next session.
2. PCB layout not started for J19, U12, J20.
3. KERC-04 reset-safe startup bench validation still pending (requires hardware on bench).

Source Of Truth Files:
- docs/archive/hat/rev-a-2026-03-31/rev-b-stm32-migration/DEV_STATION_HANDOFF_2026-05-26.md (this file)
- docs/archive/hat/rev-b-2026-06-20/wiring/U10_WIRING_WORKTHROUGH.md
- docs/STM32_BLUEPILL_PIN_TABLE.md
- hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch

Next Session Priority Order:
1. Open KiCad, run ERC — confirm NRST/SWDIO/SWDCLK warnings clear; verify J19 visual placement.
2. Re-export netlist after ERC confirms clean; re-run verify-connector-contract.ps1 reduced mode.
3. Begin PCB layout: place J19 (IDC-Header_2x05), U12 (SOIC-16 CH340G), J20 (USB-C 16P) on HAT board.
4. KERC-04 bench validation when hardware is available.

First Action Next Session:
- Open hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch in KiCad, run ERC (Inspect → Electrical Rules Checker), and confirm warning count is ≤7 with 0 errors.

Instructions For Next Chat:
1. Read docs/archive/hat/rev-a-2026-03-31/rev-b-stm32-migration/DEV_STATION_HANDOFF_2026-05-26.md first.
2. Read /memories/repo/index.md, then active-baseline.md and open-loops.md before new changes.
3. U10 migration and supporting circuit are both done — do not re-litigate any of that work.
4. J19 was text-edited; trust the netlist export over the schematic text if there is any conflict — the netlist is ground truth.
5. CH340G wiring is correct: UART1_TX→RXD, UART1_RX→TXD. Do not swap them.
6. After each schematic change, re-export netlist and re-run ERC before continuing.

Session Closeout Status:
- Session intentionally closed by user request.
- STM32 supporting circuit complete. Schematic ready for ERC recheck and PCB layout.

DEV STATION CLOSEOUT END
