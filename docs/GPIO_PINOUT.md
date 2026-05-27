# Display Board – Authoritative Pinout (ESP32-C6)

This document is the single source of truth for wiring and firmware pin definitions.

## Summary
- Framework: **Arduino**
- Host ↔ Display-board MCU link: **UART over JST XH 4-pin (UDI standard)**
  - Display-board ESP32-C6 runs **LVGL**; host sends commands, display handles rendering.
- Local peripherals: TFT (ST7796S) + Touch (XPT2046) + SD share a local SPI bus
  - Display-board ESP32-C6 acts as **SPI MASTER** on the local bus.
- Power: J3 pin 1 provides **+5V** (host → display)
  - Display board generates **3.3V** locally using **AMS1117-3.3**.

---

## A) Display UART Connector (J3, JST XH 4-pin) → ESP32-C6

Follows the Universal Display Interface (UDI) standard. See `docs/DISPLAY_INTERFACE_STANDARD.md`.

| J3 Pin | Net Name      | Direction (Host ↔ Display) | ESP32-C6 GPIO | Notes |
|---:|---|---|---:|---|
| 1  | +5V           | Host → Display | — | Powers display board (via AMS1117) |
| 2  | GND           | — | — | Ground |
| 3  | DISP_UART_TX  | Host → Display | UART_RX | Host transmit → display receive |
| 4  | DISP_UART_RX  | Display → Host | UART_TX | Display transmit → host receive |

---

## B) TFT Module Header (J1, “Back of TFT”) → ESP32-C6 (SPI MASTER)

### Power + TFT control
| J1 Pin | Net Name | Function | ESP32-C6 GPIO | Notes |
|---:|---|---|---:|---|
| 1 | 3V3 | 3.3V rail | — | From AMS1117-3.3 output |
| 2 | GND | Ground | — | |
| 3 | CS  | TFT chip-select | GPIO10 | ST7796S CS |
| 4 | RST | TFT reset | GPIO19 | |
| 5 | D/C | TFT data/command | GPIO18 | |
| 8 | PWM | Backlight PWM | GPIO20 | Backlight/LED control |

### Shared local SPI bus (TFT + Touch + SD)
| J1 Pin | Net Name | Function | ESP32-C6 GPIO |
|---:|---|---|---:|
| 6 | MISO | SPI MISO | GPIO11 |
| 7 | SCLK | SPI SCLK | GPIO12 |
| 9 | MOSI | SPI MOSI | GPIO13 |

### Touch controller
| J1 Pin | Net Name | Function | ESP32-C6 GPIO | Notes |
|---:|---|---|---:|---|
| 11 | Touch CS | Touch chip-select | GPIO22 | XPT2046 CS |
| 14 | IRQ | Touch interrupt | GPIO21 | Optional (can be polled) |

---

## C) SD Header (J2) → ESP32-C6 (SPI MASTER)

| J2 Pin | Net Name | Function | ESP32-C6 GPIO |
|---:|---|---|---:|
| 1 | SCLK | SPI SCLK | GPIO12 |
| 2 | MOSI | SPI MOSI | GPIO13 |
| 3 | MISO | SPI MISO | GPIO11 |
| 4 | SD card CS | SD chip-select | GPIO15 |

---

## Firmware notes (Arduino)
- Host link (UART): bring up at **115200 baud**. Protocol: `CMD:`/`ACK:`/`ERR:`/`EVT:` framing.
- Display renders locally via **LVGL** — host does not push pixels.
- Local display SPI master (TFT/touch/SD): start at **8–10 MHz** and tune upward.
- ESP32-C6 UART roles:
  - UART_RX ← DISP_UART_TX from host (receives commands)
  - UART_TX → DISP_UART_RX to host (sends ACK/EVT responses)

---

## STM32 Migration Target (Draft, Hardware-Only)

This section is the current hardware migration target for the HAT MCU path. It is a planning baseline, not a committed schematic change.

Detailed learning/tracking table:
- `docs/STM32_BLUEPILL_PIN_TABLE.md`

### Target posture
- First candidate: STM32F103C8T6 "Blue Pill"
- If the pin budget or debug path fails, escalate to a larger STM32 family
- Keep both UART and SWD available in hardware

### Draft pin map A

| Logical function | STM32 pin | Notes |
|---|---:|---|
| `ISET_MPU_5V` | PA0 | Rail-control output |
| `ISET_MPU_3V3` | PA1 | Rail-control output |
| `ISET_MPU_Channel_3` | PA2 | Rail-control output |
| `FAULT_CRITICAL_SUM` | PA3 | Fault input; keep low-noise route |
| I2C SCL | PB8 | Telemetry bus |
| I2C SDA | PB9 | Telemetry bus |
| UART TX | PA9 | Programming/debug |
| UART RX | PA10 | Programming/debug |
| SWDIO | PA13 | Reserved for debug only |
| SWDCLK | PA14 | Reserved for debug only |
| Status LED | PC13 | Optional indicator |
| NRST | NRST | Reset access / test point |
| BOOT0 | BOOT0 | Boot-mode strap / test access |

### Draft pin map B

| Logical function | STM32 pin | Notes |
|---|---:|---|
| `ISET_MPU_5V` | PB0 | Fallback rail-control output |
| `ISET_MPU_3V3` | PB1 | Fallback rail-control output |
| `ISET_MPU_Channel_3` | PB10 | Fallback rail-control output |
| `FAULT_CRITICAL_SUM` | PB11 | Fallback fault input |
| I2C SCL | PB6 | Fallback telemetry bus |
| I2C SDA | PB7 | Fallback telemetry bus |
| UART TX | PA9 | Programming/debug |
| UART RX | PA10 | Programming/debug |
| SWDIO | PA13 | Reserved for debug only |
| SWDCLK | PA14 | Reserved for debug only |
| Status LED | PC13 | Optional indicator |
| NRST | NRST | Reset access / test point |
| BOOT0 | BOOT0 | Boot-mode strap / test access |

### Use rules
1. Keep SWD pins dedicated and do not dual-assign them.
2. Keep the fault input and I2C pair away from noisy connector fanout.
3. Promote this draft to the schematic only after pin-conflict and reset-safety checks pass.
4. If Draft A fails, try Draft B once before escalating MCU class.
