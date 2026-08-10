# Hardware Rework Decision Runsheet (2026-07-14)

Purpose: lock a safe hardware baseline before firmware checkout by deciding, validating, and executing restore-or-keep actions for known removed parts and added jumpers.

Scope: regulator board and HAT board hardware state only.

Policy: safety-first temporary state is valid. Do not restore parts just to match nominal BOM unless pass gates are met.

## Confirmed As-Built Hardware State

Use this as the source of truth before touching hardware.

| Board | Item | Current state | Action class |
|---|---|---|---|
| Regulator | U5 | Removed after failed reintroduction (~7V channel drive) | Keep removed for this revision |
| Regulator | R15 | Removed | Do not reinstall (wrong connection point) |
| Regulator | R28 | Removed | Do not reinstall (wrong connection point) |
| Regulator | R9 | Reinstalled | Hold and validate |
| Regulator | R10 | Reinstalled | Hold and validate |
| Regulator | VSENSE_3V3+ -> +3.3V_Reg | Jumper removed | Hold and validate |
| Regulator | +5V_Reg -> VSENSE_5V+ | Jumper added | Temporary workaround |
| HAT | U3 | Removed | Restore candidate |
| HAT | U4 | Removed | Restore candidate |
| HAT | D1 | Removed after heating observation | Temporary removal |
| HAT | D2 | Removed after heating observation | Temporary removal |

## Global Stop Rules

1. Stop immediately on PSU current foldback, smoke, odor, or fast thermal rise.
2. Stop immediately if +5V or +3.3V exceeds safe expected range during startup.
3. Do not perform stacked tests while this runsheet is in progress.
4. Revert only the most recent hardware change when recovering from regression.

## Bench Setup

| Item | Required state | Actual |
|---|---|---|
| Bench PSU | 12V preset, output off at setup | |
| Current limit | Conservative start 0.10A to 0.20A | |
| DMM | Continuity and DC rails ready | |
| Thermal check | IR thermometer or thermal camera ready | |
| Serial monitor | Ready for active firmware heartbeat logs | |
| Log capture | One worksheet/photo log per step | |

## Phase 1 - Freeze Baseline (No Solder Changes)

### 1.1 Photo and continuity record

1. Capture clear top/bottom photos for both boards.
2. Mark each removed part location as verified empty.
3. Confirm both temporary jumpers are physically present and solid.
4. Run continuity checks around changed pads for obvious shorts.

| Check | Result | Notes |
|---|---|---|
| Regulator U5 pad area clean | | |
| Regulator R15/R28/R9/R10 pad areas clean | | |
| HAT U3/U4 pad areas clean | | |
| HAT D1/D2 pad areas clean | | |
| Jumper VSENSE_3V3+ -> +3.3V_Reg absent (as expected) | | |
| Jumper +5V_Reg -> VSENSE_5V+ present | | |

### 1.2 Powered baseline capture

Status: WAIVED by user decision for this session. No additional Phase 1.2 data required before proceeding.

Previously captured reference values are retained for trace only.

| Measurement | Expected trend | Actual |
|---|---|---|
| VIN current at power-on | No runaway or foldback | Not captured in this log |
| VIN current at 10s | Stable | Not captured in this log |
| VIN current at 2 to 5 min | Stable drift only | 71mA at VIN 11.98V (Blue Pill installed) |
| U5 behavior signal note | No saturation or unstable toggling | Prior issue still open: saturation/unstable behavior reported |
| +5V_Reg | Present and stable | 4.91V |
| +3.3V_Reg | Present and stable | 3.25V |
| +5V_Boot | Present and stable | 4.91V |
| +3.3V_Boot | Present and stable | 3.28V |
| Hotspot location | No rapid rise trend | |
| Serial heartbeat | Present, no reset loop | |

Gate to continue:
1. Phase 1.2 is waived for this session by explicit user direction.
2. Continue using safety-first stop rules during all remaining steps.

## Phase 2 - Keep or Restore Decision Gates

Use this matrix before each physical change.

| Item | Keep for firmware checkout if... | Restore now only if... |
|---|---|---|
| Regulator U5 | Keep removed; reintroduction already caused about 7V channel drive | Only after topology redesign that guarantees headroom and output clamping |
| Regulator R15/R28 | User decision is to keep removed due to wrong connection point | Future PCB revision updates the schematic/net placement before any reintroduction |
| Regulator R9/R10 | Reinstallation remains stable in current bench state | Regression appears during continued bench validation |
| HAT U3 | Immediate test does not require U3 function | Firmware objective needs its path now and board remains stable after restore |
| HAT U4 | Immediate test does not require U4 function | Firmware objective needs its path now and board remains stable after restore |
| HAT D1/D2 | Thermal behavior remains stable without these parts and no clamp/protection fault appears | Part rating and orientation are re-verified for 12V conditions |
| VSENSE jumpers | Cold-start without jumper has not been proven safe | A controlled challenge test passes without overvoltage/instability |

## Phase 3 - Staged Physical Rework Sequence

Order is mandatory. Do not skip forward.

### Step A - No-change validation

1. Phase 1.2 remains waived for this session.
2. Use current recorded baseline plus live safety stop rules during operation.
3. Enter firmware checkout gate directly or continue to Step B if additional hardware changes are required.

### Step B - HAT restore steps (optional, scope-driven)

1. Restore one IC only (U3 or U4 based on immediate firmware objective).
2. Unpowered continuity/short check.
3. Power at conservative current limit.
4. Capture rail/current/thermal/serial status at 10s and 2 to 5 min.

| Step B item | Performed | Pass/Fail | Notes |
|---|---|---|---|
| Restore U3 | | | |
| Restore U4 | | | |

### Step C - Regulator resistor restore steps (optional, scope-driven)

1. R15 and R28 are excluded from restore work by user decision (wrong connection point).
2. Only validate stability of already-reinstalled R9 and R10 during normal operation.
3. Run unpowered continuity check only if a new change is introduced.
4. Log any regression against current reference values.

| Step C item | Performed | Pass/Fail | Notes |
|---|---|---|---|
| Keep R15 removed (intentional) | | | |
| Keep R28 removed (intentional) | | | |
| Validate R9 reinstallation stability | | | |
| Validate R10 reinstallation stability | | | |

### Step D - Regulator U5 reintroduction (deferred by default)

Removed from current revision workflow by user decision after failed reintroduction.

1. Do not restore U5 during firmware checkout.
2. Preserve U5 as depopulated in all current bench validations.
3. Reintroduction is moved to future-revision hardware redesign testing only.

| Step D item | Performed | Pass/Fail | Notes |
|---|---|---|---|
| Keep U5 removed (intentional) | | | |
| U5 redesign required before reintroduction | | | |

### Step E - Jumper challenge test (optional, high caution)

1. Remove one jumper only.
2. Run current-limited cold start while watching rails.
3. If instability appears, power off and restore jumper immediately.
4. Repeat for the other jumper only after first challenge is clean.

| Step E item | Performed | Pass/Fail | Notes |
|---|---|---|---|
| Challenge VSENSE_3V3+ -> +3.3V_Reg jumper | | | |
| Challenge +5V_Reg -> VSENSE_5V+ jumper | | | |

## Firmware Checkout Entry Gate

Proceed only when all selected hardware changes are stable and both conditions below pass:

1. Two consecutive clean power cycles with no foldback, no rail collapse, no reset loop.
2. No unresolved thermal rise trend at any modified area.

| Entry gate check | Pass/Fail | Notes |
|---|---|---|
| Two clean consecutive power cycles | | |
| Thermal trend acceptable | | |
| Firmware checkout unlocked | | |

## Rollback Rule

If any step fails, revert only the latest change and re-run baseline checks. Do not alter multiple components at once during recovery.

## Firmware Checkout Implementation Addendum (2026-07-14)

Scope: firmware-only implementation aligned to current baseline where HAT U3/U4 remain removed and U5 is the installed AHT20 I2C sensor.

Controller decision update: ESP32-C6 path is retired for this bring-up branch. STM32 Blue Pill is the active and only firmware target.

### Execution checklist

1. STM32 Blue Pill build (`stm32-bluepill-bringup`, env `bluepill_f103c8`) completes.
2. STM32 flash/program path uses ST-Link only (no ESP32 guarded flash workflow).
3. STM32 serial command set available: HELP, FTEST, AHTNOW, AHTRESET, SRTEST.
4. U5 AHT20 bring-up path active on I2C address 0x38 with init, measurement, and recovery handling.
5. Shift-register interface-only test emits deterministic patterns over SPI + SR_Latch without requiring U4 population.
6. W25Q128 JEDEC/erase/program/readback bring-up implementation passes at boot and can be rerun on-demand via FTEST.

### Current execution status

| Item | Result | Notes |
|---|---|---|
| STM32 Blue Pill build | PASS | `bluepill_f103c8` build succeeds after command-set and sensor/SR integration updates. |
| STM32 W25Q128 implementation | PASS (build) | JEDEC + erase/program/readback flow present and build-verified in `stm32-bluepill-bringup/src/main.cpp`. |
| STM32 manual flash re-test command | PASS (firmware) | `FTEST` reruns flash bring-up test on demand over Serial and SerialDbg. |
| STM32 AHT20 implementation (U5 @ 0x38) | PASS (firmware) | Added init/read/reset path on PB8/PB9 I2C in `stm32-bluepill-bringup/src/main.cpp`. |
| STM32 shift-register interface self-test | PASS (firmware) | Added `SRTEST` SPI pattern sweep with PA4 latch pulse in `stm32-bluepill-bringup/src/main.cpp`. |

### Hardware-dependent gate still open

1. Live on-hardware validation completed for the current firmware scope (command parser, AHT20, SR interface test, W25Q128 test).
2. Device-level shift-register channel verification remains deferred while U4 is intentionally depopulated.

### STM32 Live Evidence (Captured)

1. ST-Link upload completed with verify OK (`bluepill_f103c8`, OpenOCD reset + verify pass).
2. Serial monitor on COM7 stable at 115200 with continuous heartbeat (`hb`).
3. Command parser confirmed:
	- `HELP` returned `cmd: HELP | FTEST | AHTNOW | AHTRESET | SRTEST`
4. AHT20 path confirmed:
	- `AHTRESET` returned `aht20: reset + probe OK`
	- `AHTNOW` returned valid numeric sample: `aht20: T=31.08C RH=31.61% status=0x18`
5. Shift-register interface test confirmed:
	- `SRTEST` logged full deterministic pattern sweep from `0x0000` through `0x0F00` with begin/end markers.
6. W25Q128 memory path confirmed:
	- `FTEST` output: `JEDEC ID mfg=0xEF type=0x40 cap=0x18`, `SR1 before=0x00`, `erase/program/readback PASS`.
7. Automatic periodic health summary confirmed:
	- `status: flash=PASS aht=PASS T=30.12C RH=32.94% runs=1`
	- `status: flash=PASS aht=PASS T=30.00C RH=32.99% runs=1`

### Closeout Decision (This Session)

1. Firmware checkout scope for STM32 baseline is CLOSED as PASS.
2. Remaining open item is hardware-population dependent only: U4-deferred device-level channel verification.
3. Next session should start from STM32-only path; do not resume ESP32-C6 guarded upload flow for this branch.

## 2026-07-14 Delta Notes

1. R9 and R10 were reinstalled.
2. VSENSE_3V3+ -> +3.3V_Reg jumper was removed.
3. +5V_Reg -> VSENSE_5V+ jumper remains in place.
4. U5 behavior is reported as output saturation/unstable when powered from +5V_Boot path.
5. HAT D1 and D2 were removed after heating observations and voltage-rating concern at 12V bench conditions.
6. User decision: Phase 1.2 is waived as not necessary for this session.
7. User decision: R15 and R28 will remain removed and are not to be reinstalled in current hardware.
8. Future revision action: delete R15 and R28 from the design path or relocate them to the correct connection point in schematic/PCB.
9. U5 reintroduced-and-removed event: channel drove to about 7V during test.
10. User direction: keep U5 removed for this revision due to supply-headroom limitation in current topology.
11. Future revision action: redesign U5 supply/reference/output-limiting path before any reintroduction attempt.

## Session Record

| Date/time | Change made | Outcome | Next action |
|---|---|---|---|
| 2026-07-14 | Baseline measurement with Blue Pill installed and current hardware state | VIN 11.98V, 71mA; +5V_Reg 4.91V; +3.3V_Reg 3.25V; +5V_Boot 4.91V; +3.3V_Boot 3.28V | Capture hotspot location and startup (power-on and 10s) current points |
| 2026-07-14 | STM32 firmware integration for AHT20 + SR interface + W25Q128 + command shell | Build and ST-Link upload PASS; serial command set active (`HELP/FTEST/AHTNOW/AHTRESET/SRTEST`) | Capture live command evidence and lock runsheet status |
| 2026-07-14 | Live command validation on COM7 | `AHTNOW` valid data, `AHTRESET` pass, `SRTEST` pattern sweep pass, `FTEST` flash pass | Enable startup/periodic status reporting and capture trend lines |
| 2026-07-14 | Startup self-check + periodic status monitoring enabled | Repeating status lines show `flash=PASS aht=PASS` with stable numeric T/RH samples | Session closeout complete; next work is deferred U4 device-level verification |
