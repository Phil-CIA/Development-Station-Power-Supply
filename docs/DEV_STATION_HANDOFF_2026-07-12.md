# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-07-12.md](docs/DEV_STATION_HANDOFF_2026-07-12.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-03.md](docs/DEV_STATION_HANDOFF_2026-07-03.md)

Session status: active for HAT board bring-up and UART validation.

## Session Pause - 2026-07-14 (Closeout)

Session paused by user for continuation in a new chat.

### Locked hardware baseline at pause

1. Regulator board:
	- U5 intentionally removed (reintroduction test drove channel to about 7V).
	- R15 removed and held out for this revision.
	- R28 removed and held out for this revision.
	- R9 and R10 reinstalled.
	- VSENSE_3V3+ to +3.3V_Reg jumper removed.
	- +5V_Reg to VSENSE_5V+ jumper remains installed.
2. HAT board:
	- U3 removed.
	- U4 removed.
	- D1 removed.
	- D2 removed.

### Locked decisions at pause

1. U5 remains depopulated for this revision; no further U5 reintroduction during firmware checkout.
2. R15 and R28 remain removed in this revision due to wrong connection point; carry as future-revision fix.
3. Phase 1.2 powered baseline capture remains waived by user decision for this session.
4. Firmware checkout continues in no-change hardware mode from this baseline.

### Resume checklist for next session

1. Read this handoff and `docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md`.
2. Confirm hardware still matches the locked baseline before power-up.
3. Continue firmware checkout only (no new hardware rework unless a new safety fault appears).
4. If a new hardware change is required, log it immediately in the runsheet and re-lock the baseline.

## Implementation Start - 2026-07-14 (Hardware State Lock)

This section starts execution of the restore-vs-keep plan before firmware checkout.

### Ground-truth hardware state for this implementation pass

1. Regulator board:
	- U5 removed.
	- R15 removed.
	- R28 removed.
	- R9 reinstalled.
	- R10 reinstalled.
	- Jumper removed: VSENSE_3V3+ to +3.3V_Reg.
	- Jumper added: +5V_Reg to VSENSE_5V+.
2. HAT board:
	- U3 removed.
	- U4 removed.
	- D1 removed.
	- D2 removed.

### 2026-07-14 troubleshooting delta

1. R9 and R10 were reinstalled during troubleshooting.
2. The +3.3V sense jumper was removed.
3. The +5V sense jumper was intentionally kept in place due to unresolved U5 behavior.
4. Observed issue: U5 is powered from +5V_Boot and output saturates with unstable behavior.
5. D1 and D2 on the HAT board were found running hot and were removed; reported reason is voltage rating concern at 12V bench conditions.
6. User decision: Phase 1.2 powered baseline capture in the rework runsheet is waived as not necessary for this session.
7. User decision: R15 and R28 will not be reinstalled because they are connected at the wrong point in current design implementation.
8. Future revision note: remove R15 and R28 from the rev path as currently implemented, or relocate them to the correct connection point before reintroduction.
9. U5 was reintroduced for verification and then removed again.
10. Observed behavior during U5 reintroduction: channel drove to about 7V.
11. Working root-cause hypothesis: U5 path does not have enough supply headroom in this topology and saturates/overdrives the sense-control path.
12. Current decision: keep U5 removed in the present hardware baseline.
13. Future revision note: redesign U5 supply/reference topology so output range cannot force channel overvoltage under nominal 12V operation.

### Safety note for next power cycle

1. Treat D1/D2 removal as an active temporary state and verify no reverse-path or clamp dependency is now missing before power-up.
2. U5 disposition is closed for this revision baseline: keep U5 intentionally depopulated during firmware checkout.
3. Do not reinstall or re-enable related U5 path changes in this revision.

### Latest measured baseline snapshot (2026-07-14, Blue Pill installed)

1. VIN: 11.98V at 71mA.
2. +5V_Reg channel: 4.91V.
3. +3.3V_Reg channel: 3.25V.
4. +5V_Boot: 4.91V.
5. +3.3V_Boot: 3.28V.
6. Interpretation for gate decision: rails are present and near expected range; U5 instability concern remains open and still blocks any U5 reintroduction.

### Execution artifact

1. Use runsheet: `docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md`.
2. Use safety-first decision priority:
	- Keep temporary hardware state unless a specific restore is required by immediate firmware scope or safety.
	- Restore one item at a time only, with full baseline/regression checks per step.
3. Firmware checkout stays blocked until the runsheet entry gate passes.

### Immediate next action in lab

1. Proceed with firmware checkout in no-change hardware mode using current baseline (U5 out, R15/R28 out, +5V sense jumper in place).

## Session Closeout - 2026-07-14

Session paused by user for handoff and new-chat continuation.

### What was completed in this stop window

1. Regulator fault isolation and recovery:
	- High current condition traced to hot U5 on 5V path.
	- U5 removed as a temporary hardware state.
2. Power baseline recovered:
	- With HAT connected and Blue Pill absent: 12V input 41mA, +5V channel 5.06V, +5V_Boot 4.92V, +3.3V_Boot 3.28V, +3.3V channel 3.27V.
3. Blue Pill startup clamp root cause found:
	- Wrong Blue Pill variant caused current-limit clamp.
	- Correct variant installed; stable about 70mA at 12V with 6 LEDs on.
4. STM32 programming and local serial path verified:
	- ST-Link detected and upload path passes.
	- COM7 CH340 path receives heartbeat output after USART1 debug mirror update.
5. UART transport alignment work completed:
	- STM32 sender reverted to legacy 10-byte frame format used in prior known-good session.
	- CrowPanel telemetry logging enabled and changed to periodic status print for visibility.
	- CrowPanel UART pin mapping tested in legacy compatibility mode (RX=IO20, TX=IO19) for A/B check.

### Current blocker at stop

- CrowPanel receive counters are not continuously advancing (observed one-shot receive state with no sustained increment), despite verified wiring and successful STM32 heartbeat on COM7.

### Files modified this session

1. `stm32-bluepill-bringup/src/main.cpp`
2. `crowpanel-43-bringup/src/main.cpp`
3. `crowpanel-43-bringup/src/disp_link_slave.cpp`
4. `docs/HAT_FIRST_POWER_RUNSHEET_2026-07-12.md`

### Scope policy for next session

1. Keep U5 removal treated as temporary hardware state; do not close regulator repair disposition yet.
2. Keep strict board identity checks for uploads (CrowPanel COM12 ESP32-S3; STM32 via ST-Link).
3. Focus on UART receive continuity root cause before adding new features.

### First actions next session

1. Read this file and `HANDOFF.md`.
2. Confirm STM32 firmware currently running from `stm32-bluepill-bringup/src/main.cpp` and verify COM7 heartbeat.
3. Confirm CrowPanel firmware currently running from `crowpanel-43-bringup/src/main.cpp` and check periodic `rx frames` output.
4. Capture a 30-second log containing `rx frames`, `errs`, `uart`, `uartB`, `seq`, and `age`.
5. If counters remain flat, perform direct pin activity proof on CrowPanel IO19 and IO20 (PROBE command) and lock final UART pin order before further protocol changes.

### Suggested prompt for new chat

Continue from the active 2026-07-14 closeout. Regulator baseline has been recovered (U5 temporarily removed), correct Blue Pill is installed, STM32 ST-Link flashing works, and COM7 heartbeat is present. Current blocker: CrowPanel UART receive counters are not continuously increasing. Start by validating live counter logs and pin activity on IO19/IO20, then lock final UART pin mapping.

## Field Update - 2026-07-13 (Regulator fault isolation)

- Symptom during bring-up: high no-load current and rail collapse while testing regulator/HAT path.
- Fault isolation found significant heating near both 5V converters; U5 was identified as hot and removed.
- Post-removal state (HAT connected, Blue Pill not installed):
	- Input current: 41mA at 12V
	- +5V channel: 5.06V
	- +5V_Boot: 4.92V
	- +3.3V_Boot: 3.28V
	- +3.3V channel: 3.27V
- Current state indicates regulator board is again in a usable low-load condition for continued HAT boot validation.
- Caution: keep U5 removal documented as a temporary hardware state until root-cause and final repair disposition are completed.

## Field Update - 2026-07-13 (Blue Pill startup clamp resolved)

- Root cause of the new startup current-limit event was incorrect Blue Pill variant insertion attempt.
- After installing the correct Blue Pill variant, startup behavior recovered.
- New measured state: input current about 70mA at 12V with 6 LEDs on.
- This is considered an acceptable bring-up current for the present bench state.
- Next step remains serial boot validation, then I2C/telemetry baseline capture.

## Session Closeout - 2026-07-12

Regulator board first-power check completed successfully and is now a held-good baseline for the HAT bring-up session. The next target is HAT board power-up, boot validation, and link bring-up on the existing firmware path.

## Current Objective At Stop

Bring up the HAT board on the bench, confirm it powers cleanly, and validate the board-side telemetry/control path before any integration with the regulator stack.

## Verified State At Stop

- Regulator board no-load power-up: PASS
- Measured baseline at 12V input: +5V_Boot = 5.02V, 5V channel = 5.04V, +3.3V = 3.314V
- Input current at no load: 27mA
- HAT board not yet powered in this session

## Next Session Priority Order

1. Review HAT power and connector notes before applying bench power.
2. Power the HAT board in isolation first.
3. Confirm boot, serial console, and any required default UART/I2C behavior.
4. Record the HAT baseline state and identify any blockers before stacking or linking to the regulator board.
5. Keep regulator-light-load and stacked tests deferred until HAT standalone power-up is confirmed.

## Scope Policy For Next Session

1. Do not stack the HAT onto the regulator until the HAT standalone power path is proven.
2. Preserve the regulator pass state as a known-good baseline.
3. Avoid reopening regulator feedback debugging unless the HAT session reveals a new coupled fault.
4. Keep USB and board-power routing decisions explicit during first power.

## First Actions Next Session

1. Read this file and `HANDOFF.md`.
2. Open and execute `docs/HAT_FIRST_POWER_RUNSHEET_2026-07-12.md`.
3. Review the HAT bring-up notes and confirm the expected power path.
4. Apply power to the HAT board alone and record the first boot result.
5. If clean, move on to serial/UART validation; if not, stop and log the failure mode.

## Suggested Prompt For New Chat

Continue from the active handoff. The regulator board has already passed no-load power-up, and the session now needs to focus on HAT board standalone power-up and initial validation before any stacked testing.