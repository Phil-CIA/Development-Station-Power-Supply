# STM32 Blue Pill Bring-up (First Flash)

This project is a minimal first-flash target for STM32F103C8 (Blue Pill).

## Behavior

- Forces control outputs low at startup:
  - PA0 = ISET_MPU_5V
  - PA1 = ISET_MPU_3V3
  - PA2 = ISET_MPU_Channel_3
- Blinks PC13 status LED every 1 second.
- Emits heartbeat on USB/USART monitor (`Serial`) at 115200.
- Emits extended binary telemetry on USART3 via `HardwareSerial SerialU3(PB11, PB10)` at 115200.

## Telemetry

- Current bring-up firmware publishes an extended UART frame once per second.
- Frame payload carries:
  - CH1 (+5V) voltage/current
  - CH2 (+3.3V) voltage/current
  - temperature
  - status byte
  - protection flags byte
- Values are still placeholders for UI and transport bring-up, not real ADC-backed measurements yet.

## Build

`platformio run -d stm32-bluepill-bringup -e bluepill_f103c8`

## Upload (ST-Link)

`platformio run -d stm32-bluepill-bringup -e bluepill_f103c8 -t upload`
