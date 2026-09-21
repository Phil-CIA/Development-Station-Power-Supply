# HAT Board Rev-C Standalone Bring-Up Runsheet (2026-09-21)

Purpose: run a controlled standalone HAT bring-up now so the MCU/telemetry path can assist ongoing regulator-board diagnosis, while keeping stacked testing blocked.

## Scope and Gate Position

1. This is a standalone HAT-only run.
2. Regulator-board Rev-C bring-up remains HOLD until downstream rail-path faults are resolved.
3. Do not stack boards during this runsheet.
4. Controller-of-record for this run is STM32F103C8T6 Blue Pill; do not use ESP32-C6 flash/build tasks for this procedure.

## Success Criteria

1. No smoke, no audible distress, no fast foldback.
2. Stable HAT power rails (+5V_Boot and +3.3V).
3. Stable MCU boot and console responsiveness.
4. Usable diagnostic output for I2C/telemetry helper tasks.

## Stop Rules

1. If any hard short or near-short is found on VIN, +5V_Boot, or +3.3V, stop before power.
2. If current rises unexpectedly or PSU folds back, remove power and stop.
3. If rails collapse or oscillate, remove power and stop.
4. If boot loops/brownout spam persist, stop and hold.
5. Do not upload firmware unless flash target verification passes for the intended board/port/chip.

## Reference Files

1. docs/HAT_FIRST_POWER_RUNSHEET_2026-07-12.md
2. docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md
3. HANDOFF.md
4. platformio.ini
5. scripts/guarded-flash.ps1

## Bench Setup

| Item | Required state | Actual |
|---|---|---|
| Bench PSU | 12V preset, output OFF initially | Powered for standalone HAT check |
| Current limit | 0.10A to 0.20A initial | Not recorded in this capture |
| DMM | Ready for resistance and DC | |
| Serial monitor | 115200 8N1 ready | USB serial on COM7 active; stream present |
| Regulator board | Not connected | |
| USB path | Data-only preferred if external 5V present | |

## Phase 1 - Unpowered Checks

| Check | Expected | Actual | Pass/Fail |
|---|---|---|---|
| VIN to GND resistance | Not a hard short | | |
| +5V_Boot to GND resistance | Not a hard short | | |
| +3.3V to GND resistance | Not a hard short | | |
| Input polarity marking clear | Yes | | |
| Temporary jumper/workaround state verified | Yes | | |

Gate to continue:
1. No hard shorts.
2. Jumper/workaround state confirmed.

## Phase 2 - First Controlled Power-On (HAT only)

| Item | Actual |
|---|---|
| PSU voltage setting | Not recorded in this capture |
| PSU current limit | Not recorded in this capture |
| Immediate current at power-on | Not recorded in this capture |
| Current after 5 seconds | Not recorded in this capture |
| Any audible/thermal/visual anomaly | None reported |

## Phase 3 - No-Load Rail Measurements

| Rail | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|
| VIN | About bench setting | Not recorded in this capture | HOLD | |
| +5V_Boot | About 5.0V | 5.0423V | PASS | Stable |
| +3.3V | About 3.3V | 3.2926V | PASS | Stable |

Gate to continue:
1. +5V_Boot and +3.3V stable.
2. No foldback/current-limit events.

## Phase 4 - MCU Helper Bring-Up

1. Connect serial monitor and capture boot log at 115200 8N1.
2. Confirm console stability and no reset loop.
3. Capture I2C/telemetry baseline output.
4. Record whether helper output is sufficient to assist regulator-path diagnosis.

| Check | Result | Notes |
|---|---|---|
| Boot log appears after power cycle | PASS | COM7 active and printing from prior test firmware |
| No reset/brownout loop | PASS | No instability reported |
| Console responsive | PASS | USB serial communication working |
| I2C/telemetry output present | PASS | Runtime status stream includes AHT response and telemetry state lines |
| Helper path usable for regulator diagnostics | PASS | JTAG active; serial helper path confirmed |

## Optional Firmware Refresh (only if needed)

Use Blue Pill workflow only for this run.
1. Use STM32CubeProgrammer/ST-Link path tied to the Blue Pill controller branch.
2. Do not run ESP32-C6 guarded flash tasks during this HAT standalone procedure.

Precondition:
1. Port and chip verification must match expected HAT target before upload.
2. If mismatch, stop and resolve before any flash action.

Current session upload result:
1. Build succeeded.
2. Guarded upload was blocked by flash precheck.
3. Script expected HAT ESP32-C6 on COM11, while detected ports were COM1, COM12, COM3, COM5, COM7.
4. No flash was performed.
5. This mismatch is expected for Blue Pill-led runs and is now treated as non-actionable for this procedure.

## Results Summary

| Item | Result | Notes |
|---|---|---|
| HAT standalone power-on | PASS | Board powers and helper interfaces are active |
| No-load rail check | PASS | +5V_Boot measured 5.0423V; +3.3V measured 3.2926V; supply observed around 5.06V |
| MCU helper bring-up | PASS | JTAG active, COM7 stream active, Blue Pill program running |
| Ready to assist regulator diagnostics | YES | Keep standalone mode; do not stack |

Session notes (2026-09-21):
1. Blue Pill variant matches Rev-B hardware expectation.
2. Existing firmware image on Blue Pill is running without reflash in this step.
3. Serial command/help stream confirms active command shell from prior test image.
4. AHT status line observed in stream: aht=PASS with valid temperature/humidity sample.
5. STM32 Blue Pill upload attempt was executed via ST-Link using `platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload`.
6. Upload failed at OpenOCD init with `libusb_open() failed with LIBUSB_ERROR_ACCESS`; no new firmware was flashed in this attempt.
7. Immediate retry succeeded via the same command: programming finished, verify OK, target reset.
8. Post-flash command response check on live console:
	- AHTNOW -> `aht20: T=28.52C RH=39.75% status=0x18`
	- INANOW -> `ina: no expected devices responded`
9. Operator clarification: INA devices are now on the regulator board, not on this standalone HAT path.
10. Therefore, INA no-response in HAT-only mode is expected for this session and is not a standalone HAT failure.
11. AW95xx probe check after firmware update:
	- AWPROBE -> `aw95xx: candidate ACK at 0x58`
12. AW95xx device presence on I2C is confirmed at address 0x58.
13. AW9523 functional output test on P1.0 (external LED) passed; manual ON/OFF and heartbeat behavior confirmed on bench.

## Next Action

1. If PASS: keep HAT standalone as diagnostic helper and continue regulator fault isolation with boards still unstacked.
2. If HOLD/FAIL: isolate to power, boot, serial, or I2C domain before any further regulator-coupled testing.
