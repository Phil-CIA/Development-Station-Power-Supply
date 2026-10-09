# STM32 Blue Pill Bring-up (First Flash)

This project is the active bench-controller firmware baseline for STM32F103C8 (Blue Pill) on the current Rev-B HAT bring-up branch.

## Behavior

- Forces control outputs low at startup:
  - PA0 = ISET_MPU_5V
  - PA1 = ISET_MPU_3V3
  - PA2 = ISET_MPU_Channel_3
- Blinks PC13 status LED every 1 second.
- Emits heartbeat/logging on USART1 (CH340 path, PA10 RX / PA9 TX) at 115200 8N1.
- Emits extended binary telemetry on USART3 via `HardwareSerial SerialU3(PB11, PB10)` at 115200.
- Native USB CDC console is not provided by this maintained configuration.

## Current role in this repo

- This is the controller-of-record for current bench bring-up and worksheet command checks.
- The active command shell lives in `src/main.cpp`.
- The root-repo ESP32-C6 HAT environments are not the active control path for the current Rev-B bypass worksheet.

## Command shell

- Compact/default profile (`bluepill_f103c8`) keeps only:
  - `HELP`, `DIAG`, `D9ON`, `D9OFF`
  - `CALSHOW`, `CFGSHOW`, `CALSET`, `CFGSAVE`, `CFGLOAD`, `CFGRESET`, `CFGERASE`
  - Display-over-UDI command channel on USART3:
    - `CMD:OUTPUT ON|OFF`
    - `CMD:ILIM CH1|CH2 <mA>`
    - `CMD:GET OUTPUT|STATE|CFGREC|ILIM CH1|CH2`
    - Responses: `ACK:...` / `ERR:...` / `EVT:...`

- Full-bench profile (`bluepill_f103c8_bench`) includes all compact commands plus bench-only commands:
  - `FTEST`, `AHTNOW`, `AHTRESET`, `SRTEST`, `D9FLASH`, `HBON`, `HBOFF`
  - `Q1ON/Q1OFF`, `Q2ON/Q2OFF`, `Q3ON/Q3OFF`, `Q4ON/Q4OFF`, `Q5ON/Q5OFF`
  - `Q9ON/Q9OFF`, `Q39ON/Q39OFF`, `Q612ON/Q612OFF`, `QSTATE`, `QSEQ`, `Q9DIAG`
  - `INAPROBE`, `INANOW`, `INARAILS`, `INADIAG`
  - `AWPROBE`, `AWMODE`, `AWHB`, `AWP10ON`, `AWP10OFF`

- Compact profile behavior for excluded bench commands:
  - Bench command names are not listed by `HELP`.
  - Entering one returns an explicit unsupported response (`cmd: unsupported in compact profile '...'`).

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

- Compact/default profile:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`
- Full-bench profile:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8_bench`

Native USB CDC is absent in both maintained profiles.

## STM32 flash budget checks (Issue #101 Step 6)

Approved flash limits for CI size gates:

- Compact/default `bluepill_f103c8`: 52,428 bytes maximum complete image size.
- Full-bench `bluepill_f103c8_bench`: 65,536 bytes maximum complete image size.

What is measured:

- Acceptance is based on `firmware.bin` byte length (the programmed image).
- The checker cross-validates with ELF FLASH load-span accounting and fails if
  `firmware.bin`, `firmware.elf`, `firmware.map`, or the build log is missing,
  empty, or malformed.
- The checker reports PlatformIO Flash and static RAM metrics separately; RAM is
  never counted as flash usage.

Local commands (no upload):

- Build compact profile and capture log:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 | Tee-Object -FilePath stm32-bluepill-bringup/ci-artifacts/build-bluepill_f103c8.log`
- Build full-bench profile and capture log:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8_bench | Tee-Object -FilePath stm32-bluepill-bringup/ci-artifacts/build-bluepill_f103c8_bench.log`
- Run checker for compact profile:
  - `py -3.13 stm32-bluepill-bringup/tools/check_stm32_size_budget.py --env bluepill_f103c8 --build-dir stm32-bluepill-bringup/.pio/build/bluepill_f103c8 --build-log stm32-bluepill-bringup/ci-artifacts/build-bluepill_f103c8.log --flash-capacity-bytes 65536 --flash-limit-bytes 52428 --report-text stm32-bluepill-bringup/ci-artifacts/size-report-bluepill_f103c8.txt --report-json stm32-bluepill-bringup/ci-artifacts/size-report-bluepill_f103c8.json`
- Run checker for full-bench profile:
  - `py -3.13 stm32-bluepill-bringup/tools/check_stm32_size_budget.py --env bluepill_f103c8_bench --build-dir stm32-bluepill-bringup/.pio/build/bluepill_f103c8_bench --build-log stm32-bluepill-bringup/ci-artifacts/build-bluepill_f103c8_bench.log --flash-capacity-bytes 65536 --flash-limit-bytes 65536 --report-text stm32-bluepill-bringup/ci-artifacts/size-report-bluepill_f103c8_bench.txt --report-json stm32-bluepill-bringup/ci-artifacts/size-report-bluepill_f103c8_bench.json`

CI evidence artifacts:

- `stm32-size-evidence-bluepill_f103c8`
- `stm32-size-evidence-bluepill_f103c8_bench`

Each artifact contains the build log, `firmware.bin`, `firmware.elf`,
`firmware.map`, and concise generated size reports (`.txt` + `.json`).

These CI build/size checks do not replace bench testing; runtime and hardware
evidence remains required.

## Issue #101 Step 7 (2026-10-09) - bench validation evidence capture

Scope executed: Step 7 bench validation only for issue #101 / draft PR #103.
No firmware source changes were made in this step.

Authorized deviation recorded before execution:

- CrowPanel remained connected on COM12, and the user explicitly authorized
  proceeding with outputs isolated.
- During Setup interaction on this path, CrowPanel automatically generated host
  traffic that included:
  - `udi ack: OUTPUT ON`
  - `udi ack: ILIM CH1 2500`
  - `udi ack: ILIM CH2 1500`
- Those write-path commands were not manually issued by this agent.
- This is an observed deviation from the no-write-command test plan and is
  recorded as such; the run is not described as no-write-traffic.

Profile uploads and observed artifacts:

- Compact/default upload command:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload`
  - Result: SUCCESS (exit 0)
- Full-bench upload command:
  - `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8_bench -t upload`
  - Result: SUCCESS (exit 0)

Boot logs (USART1 CH340 @115200 8N1, COM7) captured after each upload confirm:

- startup banner
- single console path behavior (no duplicate boot line pairs observed)
- startup external-flash bring-up test sequence and PASS:
  - `flash: begin bring-up test`
  - `flash: JEDEC ID mfg=0xEF type=0x40 cap=0x18`
  - `flash: SR1 before=0x00`
  - `flash: erase/program/readback PASS`
- config recovery/load summary (`[CFG] Loaded`, `[CFG] REC r=O d=0 s=0`)

Compact profile command evidence (USART1/COM7):

- `HELP`: core commands + UDI contract line printed; no bench command list.
- `DIAG`: health snapshot returned.
- `CFGSHOW`: persisted config values printed.
- `INAPROBE`: `cmd: unsupported in compact profile 'INAPROBE'`.
- `INANOW`: `cmd: unsupported in compact profile 'INANOW'`.

Full-bench profile command evidence (USART1/COM7):

- `HELP`: bench command list appears (includes `INAPROBE`, `INANOW`, `AHTNOW`).
- `INAPROBE`: `ina: 0x40=ACK 0x41=ACK`.
- `INANOW`: INA readings returned for 0x40/0x41 channels.
- `AHTNOW`: valid AHT sample returned.

USART3 telemetry and UDI evidence path used:

- Observed via CrowPanel USB console (COM12), where CrowPanel bridges to STM32
  host-link UART and echoes `udi ack/err/evt` lines.
- Telemetry continuity observed (`rx frames` counters increase with stable
  `errs=0`, refreshed `seq` and `age` fields).
- Read-only GET traffic that is exposed by this path was captured by issuing
  `SCREEN SETUP`, which triggers:
  - `GET OUTPUT`
  - `GET ILIM CH1`
  - `GET ILIM CH2`
  and corresponding `udi ack:` lines.

UDI limitations encountered in this setup:

- CrowPanel USB CLI does not expose a raw pass-through command for arbitrary
  host payloads, so direct `GET STATE`, `GET CFGREC`, and direct malformed
  host-command injection to STM32 were BLOCKED in this run.
- Sending `CMD:...` text on COM12 is consumed by CrowPanel CLI itself and
  returns CrowPanel-local `ERR unknown: ...` (not an STM32 host-link ERR).
- No further hardware tests were conducted after recognizing and documenting
  this automatic write traffic.

Safe-output pin verification:

- PA0/PA1/PA2 direct voltage measurement: NOT MEASURED in this step.
- No invasive probing was performed from this session.

Step 7 criterion status from this run:

| Criterion | Status | Notes |
|---|---|---|
| Compact upload/build and boot capture | PASS | Upload succeeded; boot log captured including authorized startup flash-sector test PASS. |
| Compact read-only USART1 commands (`HELP`, `DIAG`, `CFGSHOW`, `INAPROBE`, `INANOW`) | PASS | `INAPROBE`/`INANOW` correctly reported unsupported in compact profile. |
| Full-bench upload/build and boot capture | PASS | Upload succeeded; boot log captured including startup flash-sector test PASS. |
| Full-bench read-only diagnostics (`HELP`, `INAPROBE`, `INANOW`, `AHTNOW`) | PASS | Bench command presence and read-only diagnostics verified. |
| USART3 telemetry observed | PASS | CrowPanel `RX` output shows ongoing frame updates, `errs=0`. |
| UDI `GET OUTPUT` / `GET ILIM CH1` / `GET ILIM CH2` | PASS | Observed via CrowPanel `SCREEN SETUP`-triggered host queries and `udi ack:` lines. |
| UDI `GET STATE` / `GET CFGREC` | BLOCKED | No raw host-command pass-through exposed on CrowPanel USB CLI in this setup. |
| Malformed/unknown read-only UDI command producing STM32 `ERR:` | BLOCKED | CrowPanel USB CLI intercepts unknown `CMD:` lines locally; cannot inject malformed host payload to STM32 through this interface. |
| PA0/PA1/PA2 safe inactive-state voltage measurement | NOT MEASURED | Output safety was authorized by isolation, but non-invasive meter measurement was not captured in this session. |

Step 7 command and flash-operation statement:

- The agent did not manually issue `OUTPUT`, `ILIM`, `D9`, range-gate/AW9523,
  `FTEST`, CFG/CAL write, or deliberate fault-injection commands.
- CrowPanel automatically sent the host traffic listed above
  (`udi ack: OUTPUT ON`, `udi ack: ILIM CH1 2500`, `udi ack: ILIM CH2 1500`).
- The automatic startup flash bring-up test erased/programmed only the
  user-authorized disposable `FLASH_TEST_ADDR` sector and passed.
- No other external-flash erase/program operation was performed in this step.

## Issue #101 Step 3 (2026-10-08) - compact + full-bench profiles

Scope executed: Step 3 only for issue #101 / draft PR #103.

- Maintained environments:
  - `bluepill_f103c8`: compact/default profile (`STM32_ENABLE_BRINGUP_BENCH_COMMANDS=0`)
  - `bluepill_f103c8_bench`: full-bench profile (`STM32_ENABLE_BRINGUP_BENCH_COMMANDS=1`)
- Shared configuration is defined in non-environment section `stm32_common` and referenced by both maintained environments in `platformio.ini`.
- Bench evidence status: NOT RUN (build/static analysis only).

Measured build results (clean builds, Python 3.13, no upload):

| Profile | PlatformIO flash used/free | PlatformIO static RAM | Complete loadable image used/free* | firmware.bin length |
|---|---:|---:|---:|---:|
| Compact `bluepill_f103c8` | 44,956 / 20,580 B | 2,000 B | 45,276 / 20,260 B | 45,276 B |
| Full-bench `bluepill_f103c8_bench` | 52,256 / 13,280 B | 2,000 B | 52,576 / 12,960 B | 52,576 B |

*Complete loadable image counts every flash `LOAD` section (`.isr_vector`, `.text`, `.rodata`, `.ARM`, `.init_array`, `.fini_array`, and `.data` initializers).

Delta versus Step 2 baseline (`bluepill_f103c8`, CDC removed, 51,980 B PlatformIO flash and 52,300 B complete image):

- Compact/default profile:
  - PlatformIO flash delta: -7,024 B
  - Complete image delta: -7,024 B
- Full-bench profile:
  - PlatformIO flash delta: +276 B
  - Complete image delta: +276 B

Evidence artifacts (ELF/map/objdump/symbol/log extracts):

- `C:\Users\user\.copilot\session-state\c4c48da5-f21f-413b-b805-fa311f4486a7\files\issue-101-step-3`

## Issue #101 Step 1 Baseline (2026-10-08)

Measured on branch `phil-cia-stm32-flash-headroom` at commit `cbeee06`.

- Build (unchanged target): `python -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`
- Flash usage: 64,892 / 65,536 bytes (free: 644 bytes)
- Static RAM usage: 5,536 / 20,480 bytes

Flash accounting note for Step 1 baseline:

- PlatformIO reported metric: 64,892 bytes used / 644 bytes free.
- Complete loadable image footprint (all flash `LOAD` sections, including vectors/init/fini/.ARM and `.data` initializers): 65,216 bytes used / 320 bytes free.

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

## Issue #101 Step 2 (2026-10-08) - CDC removal + console consolidation

Scope executed: Step 2 only for issue #101 / draft PR #103.

- Removed `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` from `bluepill_f103c8`.
- Console ownership is consolidated to one USART1 owner: core `Serial1` via `HardwareSerial& SerialConsole = Serial1`.
- Explicit pin setup is applied before `begin()`:
  - `SerialConsole.setRx(PA10)`
  - `SerialConsole.setTx(PA9)`
  - `SerialConsole.begin(115200)`
- App-owned `SerialDbg(PA10, PA9)` is removed.
- USART3 UDI path remains independent and unchanged: `HardwareSerial SerialU3(PB11, PB10)` at 115200 8N1.

Measured build (clean, build-only; no upload/bench):

- Command: `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t clean`
- Command: `py -3.13 -m platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`
- Python: 3.13.15
- PlatformIO Core: 6.2.0
- Platform/platform packages: unchanged from baseline (`ststm32` 19.7.0, core 2.12.0, GCC 12.3.1)
- Direct/transitive libs preserved: `Adafruit AW9523@1.0.5`, `Adafruit BusIO@1.17.4`
- Flash usage (PlatformIO reported metric): 51,980 / 65,536 bytes (free: 13,556 bytes)
- Static RAM usage: 2,000 / 20,480 bytes

Complete image accounting (all loadable flash sections):

- `.isr_vector` 268 B
- `.text` 44,256 B
- `.rodata` 7,596 B
- `.ARM` 8 B
- `.init_array` 28 B
- `.fini_array` 16 B
- `.data` initializers (flash load) 128 B
- Total complete image footprint = 52,300 B
- Free by complete image accounting = 13,236 B

Method comparison and deltas versus Step 1 baseline:

| Method | Step 1 used/free | Step 2 used/free | Delta used | Delta free |
|---|---:|---:|---:|---:|
| PlatformIO reported | 64,892 / 644 | 51,980 / 13,556 | -12,912 | +12,912 |
| Complete loadable image | 65,216 / 320 | 52,300 / 13,236 | -12,916 | +12,916 |

Compact-ceiling status note:

- Proposed compact ceiling remains pending user review: 52,428 bytes.
- Current complete image footprint is 52,300 bytes, leaving only 128 bytes to that proposed ceiling; this does not by itself demonstrate adequate remaining feature headroom.

Evidence artifacts for Step 2:

- `C:\Users\user\.copilot\session-state\c4c48da5-f21f-413b-b805-fa311f4486a7\files\issue-101-step-2`

Bench evidence status: NOT RUN in Step 2.

### User-run bench checklist (not executed in this step)

- Confirm boot logs appear on CH340 USART1 at 115200 8N1.
- Run `HELP`, `DIAG`, and `CFGSHOW`; verify readable responses and no duplicate lines.
- Confirm periodic binary telemetry continues on USART3 (PB11/PB10 path).
- Run read-only UDI round trips and confirm ACK/ERR behavior:
  - `CMD:GET OUTPUT`
  - `CMD:GET STATE`
  - `CMD:GET CFGREC`
  - `CMD:GET ILIM CH1`
  - `CMD:GET ILIM CH2`
  - malformed command (expect `ERR:`)

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
