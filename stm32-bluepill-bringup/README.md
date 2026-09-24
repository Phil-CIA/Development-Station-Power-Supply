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

## Upload (ST-Link)

`platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload`
