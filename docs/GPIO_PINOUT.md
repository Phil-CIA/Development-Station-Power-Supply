# Display Board – Authoritative Pinout (ESP32-C6)

This document is the single source of truth for wiring and firmware pin definitions.

## Summary
- Framework: **Arduino**
- Host ↔ Display-board MCU link: **SPI over IDC-10 ribbon**
  - Display-board ESP32-C6 acts as **SPI SLAVE** on this link.
- Local peripherals: TFT (ST7796S) + Touch (XPT2046) + SD share a local SPI bus
  - Display-board ESP32-C6 acts as **SPI MASTER** on the local bus.
- Power: IDC-10 provides **5V_IN only**
  - Display board generates **3.3V** locally using **AMS1117-3.3**.

---

## A) IDC-10 Host Connector (J3, 2×5 IDC) → ESP32-C6 (SPI SLAVE)

(Per your schematic: Pin-odd = GND, pin-even = signals)

| J3 Pin | Net Name   | Direction (Host ↔ Display) | ESP32-C6 GPIO | Notes |
|---:|---|---|---:|---|
| 1  | GND       | — | — | Ground |
| 2  | 5V_IN     | Host → Display | — | Feeds AMS1117 input |
| 3  | GND       | — | — | Ground |
| 4  | SPI_SCLK  | Host → Display | GPIO4 | Host clock into display MCU |
| 5  | GND       | — | — | Ground |
| 6  | SPI_MISO  | Display → Host | GPIO6 | Display MCU to host |
| 7  | GND       | — | — | Ground |
| 8  | SPI_MOSI  | Host → Display | GPIO5 | Host to display MCU |
| 9  | GND       | — | — | Ground |
| 10 | SPI_CS    | Host → Display | GPIO7 | Host chip-select |

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
- Host link (SPI slave): start host clock at **1 MHz** for bring-up. Increase later if stable.
- Local display SPI master: start moderate (e.g. **8–10 MHz**) and tune upward.
- This design uses **two separate SPI roles**:
  - ESP32-C6 as SPI **slave** on GPIO4–7 (host link)
  - ESP32-C6 as SPI **master** on GPIO10–13/etc (local peripherals)

---

## STM32 Migration Target (Draft, Hardware-Only)

This section is the current hardware migration target for the HAT MCU path. It is a planning baseline, not a committed schematic change.

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
