# DEV STATION HANDOFF - 2026-09-21

## Session Goal

Stabilize Rev-C bench bring-up flow, confirm active controller path, and recover reliable diagnostics before continuing regulator fault isolation.

## What Was Completed

1. Confirmed active bench controller is STM32F103C8T6 Blue Pill, not root ESP32-C6 HAT path.
2. Successfully flashed Blue Pill firmware via ST-Link (`bluepill_f103c8`) after transient USB access failures.
3. Confirmed HAT standalone rails are stable:
   - +5V_Boot ~5.0423V
   - +3.3V ~3.2926V
4. Confirmed AHT20 communications pass on live shell (`AHTNOW` valid sample).
5. Added AW95xx probe support in Blue Pill command shell and verified device ACK at 0x58.
6. Added AW9523 P1.0 control commands and confirmed bench LED heartbeat test passes.
7. Captured regulator Rev-C evidence in runsheet/tracker:
   - Regulator cores can run when pin5 forced low
   - Board output rails still fail downstream of buck cores
   - Regulator runsheet remains HOLD

## Files Updated This Session

1. docs/REGULATOR_BOARD_CHANGE_TRACKER.md
2. docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md
3. docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md
4. stm32-bluepill-bringup/src/main.cpp

## Current Bench Truth

1. Blue Pill firmware is active and command shell responds.
2. AHT20 path is healthy.
3. AW9523 presence and output control are healthy on 0x58 and P1.0 test LED.
4. INA no-response is expected in standalone HAT context because INA devices are now on regulator board path.
5. Regulator Rev-C remains in HOLD due to downstream rail-path faults (not primary buck loop startup).

## Blue Pill Command Set Additions

New/verified commands:
1. AWPROBE
2. AWHB
3. AWP10ON
4. AWP10OFF

Existing key commands still in use:
1. AHTNOW
2. AHTRESET
3. INAPROBE
4. INANOW
5. INARAILS
6. SRTEST

## Open Blockers

1. Regulator Rev-C output rails remain out of range at board output nodes despite core regulation at IC pins.
2. RB-010 fallback test (`VSENSE_5V+` open-line) still deferred.
3. RB-011 closure pending regulator-side path correction validation.

## Next Session Start Checklist

1. Read docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md and docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md.
2. Keep Blue Pill as controller-of-record.
3. Keep boards unstacked until regulator-only gate conditions pass.
4. Use HAT helper shell to support regulator-coupled diagnostics once regulator board is in test context.
5. Resume regulator downstream path isolation from U2/U4 pin2 nodes toward board output nodes.

## Guardrails

1. Do not use ESP32-C6 upload/build tasks for this bench branch.
2. Do not stack HAT on regulator until regulator-only rails pass.
3. Keep strict flash-target safety checks for any future upload action.

## Continuation Update - 2026-09-22

### Firmware/Test Implementation Status

1. Added targeted range-toggle shell commands on the Blue Pill path for current-range validation:
   - Q39ON / Q39OFF (Q3 and Q9 pair)
   - Q612ON / Q612OFF (Q6 and Q12 pair)
   - QSTATE (reports SR state with decoded pair ON/OFF)
   - QSEQ (optional macro sequence: baseline, then S1..S6 with INA snapshots)
2. Updated HAT standalone Rev-C runsheet with a dedicated Phase 5 sequence for:
   - Q3/Q9 first
   - Q6/Q12 second
   - INARAILS capture at each transition.

### Build/Flash Verification (2026-09-22)

1. Blue Pill firmware build succeeded using:
   - py -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8
2. Blue Pill upload via ST-Link succeeded using:
   - py -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload
3. OpenOCD reported programming complete, verify OK, and target reset.

### Immediate Bench Execution Order

1. AWPROBE
2. SRTEST
3. QSTATE
4. INARAILS
5. QSEQ
6. Fill Phase 5 capture table in docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md from tagged serial output.

### Notes

1. This branch remains STM32 Blue Pill controller-of-record.
2. ESP32-C6 guarded flash tasks remain out-of-scope for this bench branch.

### Continuation Update - AW9523 Control Path Pivot (2026-09-22)

1. Bench evidence from `QSEQ` showed command-state toggling with no physical converter response while SR state changed.
2. Root cause: `Q39/Q612` commands were driving legacy shift-register emulation in firmware, but current hardware control path is AW9523.
3. Firmware was updated so:
   - `Q39ON/OFF` drives AW9523 `P0.3` (`ESP- GPIO 3.3V High`)
   - `Q612ON/OFF` drives AW9523 `P0.1` (`ESP- GPIO 5V Hi`)
   - `QSTATE` now reports AW9523 `P0` output/config decode first (SR only as fallback)
4. Build and ST-Link upload both passed after this pivot.
