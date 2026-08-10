# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-07-20.md](docs/DEV_STATION_HANDOFF_2026-07-20.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-14.md](docs/DEV_STATION_HANDOFF_2026-07-14.md)

Session status: closed on 2026-07-20 and superseded by the 2026-07-20 active handoff.

## Closeout Note - 2026-07-20

This handoff is now historical. The new active session resumes from [docs/DEV_STATION_HANDOFF_2026-07-20.md](docs/DEV_STATION_HANDOFF_2026-07-20.md) and shifts focus to reestablishing HAT USB/JTAG comms and continuing CrowPanel communication work.

## Session Closeout - 2026-07-14 (STM32-Only Branch)

### Decision lock

1. Controller path for this branch is STM32 Blue Pill only.
2. ESP32-C6 upload flow is retired for this branch.
3. Regulator U5 remains removed for current revision baseline.
4. HAT U3 and U4 remain removed in this stop state.

### Hardware baseline at stop

1. Regulator board:
   - U5 removed.
   - R15 removed.
   - R28 removed.
   - R9 reinstalled.
   - R10 reinstalled.
   - VSENSE_3V3+ to +3.3V_Reg jumper removed.
   - +5V_Reg to VSENSE_5V+ jumper installed.
2. HAT board:
   - Third INA3221 position U3 removed; only two INA3221 devices remain in this revision baseline (0x40 and 0x41).
   - U4 removed.
   - D1 removed.
   - D2 removed.

### Firmware implementation completed in this session

1. STM32 command shell in [stm32-bluepill-bringup/src/main.cpp](stm32-bluepill-bringup/src/main.cpp):
   - `HELP`
   - `FTEST`
   - `AHTNOW`
   - `AHTRESET`
   - `SRTEST`
   - `INAPROBE`
   - `INANOW`
2. W25Q128 memory bring-up implemented and command-rerunnable (`FTEST`).
3. AHT20 I2C path implemented on PB8/PB9 with startup probe and on-demand read/reset.
4. Shift-register interface-only self-test implemented using SPI + SR latch pulse.
5. Startup and periodic health summary reporting added.
6. INA3221 bring-up shell support added for the two remaining HAT devices only:
   - `0x40` = 5V monitor
   - `0x41` = 3.3V monitor + incoming rail monitor
   - `0x43` not expected on this revision baseline

### Live validation evidence captured

1. ST-Link upload and verify passed.
2. COM7 serial monitor stable at 115200 with recurring heartbeat (`hb`).
3. `HELP` response confirmed command set.
4. `AHTRESET` response: reset + probe OK.
5. `AHTNOW` response produced valid numeric readings (example captured):
   - `aht20: T=31.08C RH=31.61% status=0x18`
6. `SRTEST` response produced full deterministic pattern sweep with begin/end markers.
7. `FTEST` response passed full flash path:
   - JEDEC: `mfg=0xEF type=0x40 cap=0x18`
   - status read
   - erase/program/readback PASS
8. Automatic periodic summaries confirmed (examples captured):
   - `status: flash=PASS aht=PASS T=30.12C RH=32.94% runs=1`
   - `status: flash=PASS aht=PASS T=30.00C RH=32.99% runs=1`

### Scope closed in this session

1. Firmware checkout for STM32 baseline: PASS and closed.
2. Remaining deferred item is hardware-population dependent only:
   - U4 device-level channel verification (not interface-level SR test).

### Files changed in this session scope

1. [stm32-bluepill-bringup/src/main.cpp](stm32-bluepill-bringup/src/main.cpp)
2. [src/i2c_scanner.cpp](src/i2c_scanner.cpp)
3. [docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md](docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md)

## Next Session Start Checklist

1. Read this handoff and [docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md](docs/HARDWARE_REWORK_DECISION_RUNSHEET_2026-07-14.md).
2. Flash STM32 via ST-Link from [stm32-bluepill-bringup/platformio.ini](stm32-bluepill-bringup/platformio.ini) if needed.
3. Open COM7 monitor and confirm heartbeat.
4. Run `HELP`, `AHTNOW`, `FTEST` as quick readiness checks.
5. Continue from deferred U4 hardware-dependent verification or next integration objective.

## Suggested Prompt For New Chat

Continue from the active 2026-07-14 handoff. STM32 Blue Pill is now the only active controller path, ST-Link flash and COM7 serial are validated, and firmware checkout is closed PASS for AHT20 + SR interface + W25Q128 bring-up. Start from deferred U4 hardware-dependent device-level verification or the next planned system integration step.
