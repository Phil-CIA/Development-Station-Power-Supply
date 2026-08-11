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
- `INAPROBE`
- `INANOW`
- `INARAILS`

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
