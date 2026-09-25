# System Pinout Reference

Status: active. This document is the top-level index for signal/pin contracts
used by this repo. It links each subsystem to its authoritative mapping source.

## Authoritative pin-contract split

| Subsystem | Authoritative source |
|---|---|
| STM32 controller path (Rev-C HAT + `stm32-bluepill-bringup`) | `docs/STM32_BLUEPILL_PIN_TABLE.md` |
| Custom front-panel board (ESP32-C6 + ST7796S path) | This file, sections below |
| Display host interface standard (both display paths) | `docs/DISPLAY_INTERFACE_STANDARD.md` |

---

## A) Custom front-panel board (ESP32-C6) pin mapping

Framework: Arduino  
Display architecture: display-side LVGL with local SPI peripherals  
Host link: UART over UDI connector

### A1) UDI host connector (J3, JST XH 4-pin)

| J3 Pin | Net Name | Direction (Host ↔ Display) | ESP32-C6 role | Notes |
|---:|---|---|---|---|
| 1 | +5V | Host -> Display | Power in | Board regulates to 3.3V via AMS1117-3.3 |
| 2 | GND | — | Ground | Common return |
| 3 | DISP_UART_TX | Host -> Display | UART RX | Host TX into display RX |
| 4 | DISP_UART_RX | Display -> Host | UART TX | Display TX back to host RX |

### A2) TFT module header (J1) -> ESP32-C6 (local SPI master)

| J1 Pin | Net Name | Function | ESP32-C6 GPIO | Notes |
|---:|---|---|---:|---|
| 3 | CS | TFT chip-select | GPIO10 | ST7796S CS |
| 4 | RST | TFT reset | GPIO19 | Active-low reset |
| 5 | D/C | TFT data/command | GPIO18 | |
| 6 | MISO | SPI MISO | GPIO11 | Shared with touch/SD |
| 7 | SCLK | SPI SCLK | GPIO12 | Shared with touch/SD |
| 8 | PWM | Backlight PWM | GPIO20 | LED/backlight control |
| 9 | MOSI | SPI MOSI | GPIO13 | Shared with touch/SD |
| 11 | Touch CS | Touch chip-select | GPIO22 | XPT2046 CS |
| 14 | IRQ | Touch interrupt | GPIO21 | Optional (can be polled) |

### A3) SD header (J2) -> ESP32-C6

| J2 Pin | Net Name | Function | ESP32-C6 GPIO |
|---:|---|---|---:|
| 1 | SCLK | SPI SCLK | GPIO12 |
| 2 | MOSI | SPI MOSI | GPIO13 |
| 3 | MISO | SPI MISO | GPIO11 |
| 4 | SD card CS | SD chip-select | GPIO15 |

Firmware notes:
- Host protocol: UART 115200 8N1 with `CMD:`/`ACK:`/`ERR:`/`EVT:` framing.
- The host sends semantic commands; display firmware renders locally with LVGL.

---

## B) STM32 controller path summary (Rev-C)

For STM32 signal ownership and status, use:
- `docs/STM32_BLUEPILL_PIN_TABLE.md` (authoritative matrix)

Current high-impact contract notes:
1. Rail-control outputs are active on `PA0/PA1/PA2` as `ISET_MPU_5V`,
   `ISET_MPU_3V3`, and `ISET_MPU_Channel_3`.
2. Current Rev-C fan connector (`J7`) is a 2-pin control connector and does
   not expose a dedicated tach net in this revision.
3. `FAULT_CRITICAL_SUM` is not currently routed to an STM32 GPIO in Rev-C.
   Firmware now disables GPIO fault monitoring for this net (`PIN_FAULT_CRITICAL_SUM = -1`)
   until hardware exposes a dedicated MCU fault input pin.
