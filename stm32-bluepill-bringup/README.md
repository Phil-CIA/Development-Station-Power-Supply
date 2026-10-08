# STM32 Blue Pill Bring-up (First Flash)

This project is the active bench-controller firmware baseline for STM32F103C8 (Blue Pill) on the current Rev-B HAT bring-up branch.

## Behavior

- Forces control outputs low at startup:
  - PA0 = ISET_MPU_5V
  - PA1 = ISET_MPU_3V3
  - PA2 = ISET_MPU_Channel_3
- Blinks PC13 status LED every 1 second.
- Emits heartbeat on USB/USART monitor (`Serial`) at 115200.
- Emits extended binary telemetry on USART3 via `HardwareSerial SerialU3(PB11, PB10)` at 115200.

## Current role in this repo

- This is the controller-of-record for current bench bring-up and worksheet command checks.
- The active command shell lives in `src/main.cpp`.
- The root-repo ESP32-C6 HAT environments are not the active control path for the current Rev-B bypass worksheet.

## Command shell

- `HELP`
- `FTEST`
- `AHTNOW`
- `AHTRESET`
- `SRTEST`
- `D9FLASH`
- `D9ON`
- `D9OFF`
- `Q1ON`
- `Q1OFF`
- `Q2ON`
- `Q2OFF`
- `Q3ON`
- `Q3OFF`
- `Q4ON`
- `Q4OFF`
- `Q5ON`
- `Q5OFF`
- `Q39ON`
- `Q39OFF`
- `Q612ON`
- `Q612OFF`
- `QSTATE`
- `QSEQ`
- `INAPROBE`
- `INANOW`
- `INARAILS`
- Display-over-UDI command channel on USART3:
  - `CMD:OUTPUT ON|OFF`
  - `CMD:ILIM CH1|CH2 <mA>`
  - Responses: `ACK:...` / `ERR:...` / `EVT:...`
- `AWPROBE`
- `AWMODE`

Range pair notes:
- `Q1ON` / `Q1OFF` drive AW9523 `P0.2` (`ESP- GPIO 5V Low`) for the Q1/Q7 gate path.
- `Q2ON` / `Q2OFF` drive AW9523 `P0.1` (`ESP- GPIO 5V Hi`) for the Q2/Q8 gate path.
- `Q3ON` / `Q3OFF` drive only AW9523 `P0.0` (`ISET_MPU_5V`) for isolated Q3-path testing.
- `Q39ON` / `Q39OFF` drive AW9523 `P0.0` (`ISET_MPU_5V`) and `P0.5` (`ISET_MPU_3V3`) together for the Q3/Q9 test path.
- `Q4ON` / `Q4OFF` drive AW9523 `P0.4` (`ESP- GPIO 3V3 Low`) for the Q4/Q10 gate path.
- `Q5ON` / `Q5OFF` drive AW9523 `P0.3` (`Channel 3 Hi-Range`) for the Q5/Q11 gate path.
- `Q612ON` / `Q612OFF` drive AW9523 `P0.1` (`ESP- GPIO 5V Hi`) for the Q6/Q12 test path.
- `QSTATE` prints AW9523 `P0` output/config register state and decoded ON/OFF state, including separate Q3 (P0.0) and Q9 (P0.5) lines.
- `QSEQ` runs the full test sequence with tagged snapshots: baseline, Q3/Q9 OFF/ON/OFF, then Q6/Q12 OFF/ON/OFF, including INA rail summaries after each step.
- If AW9523 is unavailable, range commands automatically fall back to the legacy SR emulation path.

AW9523 mode notes:
- Control commands force AW9523 into GPIO + push-pull mode before writing outputs.
- `AWMODE` explicitly reapplies and prints mode registers (`GCR`, `LEDMODE_P0`, `LEDMODE_P1`) for bench verification.

## Telemetry

- Current bring-up firmware publishes an extended UART frame once per second.
- Frame payload carries:
  - CH1 (+5V) voltage/current
  - CH2 (+3.3V) voltage/current
  - temperature
  - status byte
  - protection flags byte

## Bench connections

- Flash path: ST-Link using `upload_protocol = stlink`
- Debug shell: USART1/CH340 path at 115200 8N1
- Historical monitor port in prior captures: COM7

## Build

`platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`

## Issue #101 Step 1 Baseline (2026-10-08)

Measured on branch `phil-cia-stm32-flash-headroom` at commit `cbeee06`.

- Build (unchanged target): `python -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`
- Flash usage: 64,892 / 65,536 bytes (free: 644 bytes)
- Static RAM usage: 5,536 / 20,480 bytes

Resolved tooling and package versions used for this baseline:

- PlatformIO Core: 6.1.19
- Platform: `ststm32` 19.7.0
- Arduino core package: `framework-arduinoststm32` 4.21200.0 (core 2.12.0)
- Toolchain package: `toolchain-gccarmnoneeabi` 1.120301.0 (GCC 12.3.1)
- Direct library: `Adafruit AW9523` 1.0.5
- Transitive library (pinned for reproducibility): `Adafruit BusIO` 1.17.4

Reproducibility pinning for STM32 target (`stm32-bluepill-bringup/platformio.ini`):

- `adafruit/Adafruit AW9523@1.0.5`
- `adafruit/Adafruit BusIO@1.17.4`

Python runtime note for reproducibility on this workstation:

- `python -m platformio ...` under Python 3.14 fails package constraints for this target.
- `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8` succeeds on the same source/dependencies.
- Verified with PlatformIO Core 6.2.0 on Python 3.13: flash and RAM remain 64,892 and 5,536 bytes.

Evidence artifacts (ELF, map, section/symbol reports, and build logs) were captured to:

- `C:\Users\user\.copilot\session-state\c4c48da5-f21f-413b-b805-fa311f4486a7\files\issue-101-step-1`

Bench evidence status: NOT bench-tested in this step (build and static artifact analysis only).

## Issue #101 Gate A Proposals - Pending Coordinator Review

These are Step 1 proposals only. Approval and Step 2 execution authority belong to the coordinator.

### Console ownership proposal for Step 2

- Proposed single USART1 console owner: `SerialDbg` on PA10/PA9 (CH340 path), 115200 8N1.
- Keep UDI on `SerialU3` (USART3 PB11/PB10) unchanged.
- This ownership model is not implemented in Step 1.

Core/linkage evidence from the Step 1 ELF confirms why consolidation is required before CDC removal:

- CDC-enabled image links USB CDC and USB serial symbols (`SerialUSB`, `USBSerial`, `USBD_CDC*`, `CDC_*`).
- The same image also links core `Serial1` and app-defined `SerialDbg`/`SerialU3`.
- Symbol evidence file: `C:\Users\user\.copilot\session-state\c4c48da5-f21f-413b-b805-fa311f4486a7\files\issue-101-step-1\serial-symbols-after-pin.txt`.

Step 2 should avoid duplicate USART1 ownership after CDC removal by routing all console init/print/poll paths through one USART1 object and removing the parallel path, while leaving USART3 UDI independent.

### Command-scope proposal for compact profile planning

No gating/deletion is done in Step 1.

- Proposed must-remain commands in compact builds: `HELP`, `DIAG`, `D9ON`, `D9OFF`, `CALSHOW`, `CFGSHOW`, `CALSET`, `CFGSAVE`, `CFGLOAD`, `CFGRESET`, `CFGERASE`, plus UDI contract handlers (`CMD:OUTPUT`, `CMD:GET OUTPUT`, `CMD:GET STATE`, `CMD:GET CFGREC`, `CMD:GET ILIM`, `CMD:ILIM`).
- Proposed bench-focused candidates behind a bring-up flag: `FTEST`, `AHTNOW`, `AHTRESET`, `SRTEST`, `D9FLASH`, `HBON`, `HBOFF`, `Q1ON/OFF`, `Q2ON/OFF`, `Q3ON/OFF`, `Q4ON/OFF`, `Q5ON/OFF`, `Q9ON/OFF`, `Q39ON/OFF`, `Q612ON/OFF`, `QSTATE`, `QSEQ`, `INAPROBE`, `INANOW`, `INARAILS`, `INADIAG`, `AWPROBE`, `AWMODE`, `AWHB`, `AWP10ON`, `AWP10OFF`, `Q9DIAG`.

### Shared-helper analysis (what must remain ungated)

Some bench-facing commands call helpers that are also used by startup, periodic runtime, output control, fault handling, telemetry, or persistence. Gate command entry points only; do not gate these shared runtime helpers:

- Boot and periodic runtime helpers that must remain ungated: `logHealthSummary()`, `refreshIncomingRailSample()`, `readIna3221()`, `readAht20Now()`, `publishTelemetry()`, `serviceAw9523FaultPath()`, `isFaultCriticalActive()`, `is3v3PathEnabled()`, and boot sequencing in `setup()`.
- Output-control helpers that must remain ungated: `setD9PathEnabled()` and `g_output_enabled` state flow, because they are used by startup restore and UDI `CMD:OUTPUT` handling.
- Persistence helpers that must remain ungated: `initPersistentConfigAtBoot()`, `loadPersistentConfig()`, `savePersistentConfig()`, `erasePersistentConfig()`, `printPersistentConfig()`, `configRecoveryReasonCode()`, and CAL/CFG data structures used by startup recovery and UDI-visible state.
- Fault/UDI contract helpers that must remain ungated: `sendUdiAck()`, `sendUdiErr()`, `sendUdiEvt()`, `handleUdiCommandLine()`, `pollUdiCommands()`, and AW9523 fault-event signaling (`EVT:FAULT TRIP/CLEAR`).

Bench-only command handlers should be the gating boundary, not the low-level helper functions above.

## Upload (ST-Link)

`platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload`
