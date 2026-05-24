# STM32 Blue Pill — Pin Table & Project Signal Reference

**MCU**: STM32F103C8T6 (LQFP-48)  
**Board**: "Blue Pill" (generic STM32 dev board)  
**Project role**: HAT MCU — rail-control outputs, fault input, I2C telemetry bus, UART debug  

---

## How to read this table

| Column | Meaning |
|---|---|
| **LQFP Pin #** | Physical pin number on the 48-pin IC package |
| **GPIO** | Port/pin name used in firmware (e.g. `PA0`) |
| **Default function** | What the pin does at reset before any config |
| **Key alt. functions** | Most useful peripheral options (not exhaustive) |
| **Project assignment** | Net name wired in the HAT schematic (Draft A) |
| **Direction** | From MCU perspective: O = output, I = input, B = bidirectional |
| **Notes** | Learning context |

---

## Full Pinout — STM32F103C8T6 (LQFP-48)

### Power & Special Pins

| LQFP Pin # | Name | Function | Project assignment | Notes |
|---:|---|---|---|---|
| 1 | VBAT | Battery / RTC supply (3.3V) | 3.3V (tied to VDD) | Backup domain power; tie to VDD if not using RTC battery |
| 7 | NRST | External reset (active-low) | `NRST` | Test point on HAT; pull-high, active-low input; used for bench reset |
| 8 | VSSA | Analog ground reference | GND | Must be connected even if ADC unused |
| 9 | VDDA | Analog supply (3.3V) | 3.3V | Separate decoupling recommended |
| 23 | VSS | Digital ground | GND | |
| 24 | VDD | Digital supply (3.3V) | 3.3V | |
| 35 | VSS | Digital ground | GND | |
| 36 | VDD | Digital supply (3.3V) | 3.3V | |
| 44 | BOOT0 | Boot mode select | `BOOT0` | LOW = boot from flash (normal); HIGH = boot from system memory (serial flasher); test point on HAT |
| 47 | VSS | Digital ground | GND | |
| 48 | VDD | Digital supply (3.3V) | 3.3V | |

### Port A — PA0 through PA7

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 10 | PA0 | GPIO / WKUP | ADC12_IN0, TIM2_CH1_ETR, USART2_CTS | `ISET_MPU_5V` | O | Rail-control DAC/PWM output for +5V current set |
| 11 | PA1 | GPIO | ADC12_IN1, TIM2_CH2, USART2_RTS | `ISET_MPU_3V3` | O | Rail-control DAC/PWM output for +3.3V current set |
| 12 | PA2 | GPIO | ADC12_IN2, TIM2_CH3, USART2_TX | `ISET_MPU_Channel_3` | O | Rail-control DAC/PWM output for Adj channel current set |
| 13 | PA3 | GPIO | ADC12_IN3, TIM2_CH4, USART2_RX | `FAULT_CRITICAL_SUM` | I | Fault aggregation input; route away from noisy connectors |
| 14 | PA4 | GPIO | ADC12_IN4, SPI1_NSS, USART2_CK | *(unassigned)* | — | Available; DAC output on F103 variants with DAC |
| 15 | PA5 | GPIO | ADC12_IN5, SPI1_SCK | *(unassigned)* | — | Available |
| 16 | PA6 | GPIO | ADC12_IN6, SPI1_MISO, TIM3_CH1 | *(unassigned)* | — | Available |
| 17 | PA7 | GPIO | ADC12_IN7, SPI1_MOSI, TIM3_CH2 | *(unassigned)* | — | Available |

### Port B — PB0 through PB2

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 18 | PB0 | GPIO | ADC12_IN8, TIM3_CH3 | *(unassigned — Draft B fallback for ISET_MPU_5V)* | O | Draft B alternate for ISET_MPU_5V if PA0 fails |
| 19 | PB1 | GPIO | ADC12_IN9, TIM3_CH4 | *(unassigned — Draft B fallback for ISET_MPU_3V3)* | O | Draft B alternate for ISET_MPU_3V3 if PA1 fails |
| 20 | PB2 | BOOT1 | — | *(reserved)* | — | **Boot strap only** — do not use as GPIO; controls boot mode with BOOT0 |

### Port B — PB10 through PB15

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 21 | PB10 | GPIO | I2C2_SCL, USART3_TX | *(unassigned — Draft B fallback for ISET_MPU_Channel_3)* | O | Draft B alternate for channel 3 if PA2 fails |
| 22 | PB11 | GPIO | I2C2_SDA, USART3_RX | *(unassigned — Draft B fallback for FAULT_CRITICAL_SUM)* | I | Draft B alternate for fault input |
| 25 | PB12 | GPIO | SPI2_NSS, I2C2_SMBA, USART3_CK, TIM1_BKIN | *(unassigned)* | — | Available |
| 26 | PB13 | GPIO | SPI2_SCK, USART3_CTS, TIM1_CH1N | *(unassigned)* | — | Available |
| 27 | PB14 | GPIO | SPI2_MISO, USART3_RTS, TIM1_CH2N | *(unassigned)* | — | Available |
| 28 | PB15 | GPIO | SPI2_MOSI, TIM1_CH3N | *(unassigned)* | — | Available |

### Port A — PA8 through PA15

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 29 | PA8 | GPIO | USART1_CK, TIM1_CH1, MCO | *(unassigned)* | — | MCO = microcontroller clock output; useful for debug |
| 30 | PA9 | GPIO | USART1_TX, TIM1_CH2 | `UART1_TX` | O | UART programming/debug TX; connects to USB-serial adapter |
| 31 | PA10 | GPIO | USART1_RX, TIM1_CH3 | `UART1_RX` | I | UART programming/debug RX; connects to USB-serial adapter |
| 32 | PA11 | GPIO | USART1_CTS, CANRX, USBDM, TIM1_CH4 | *(unassigned)* | — | USB D- on Blue Pill board; often used for USB device |
| 33 | PA12 | GPIO | USART1_RTS, CANTX, USBDP, TIM1_ETR | *(unassigned)* | — | USB D+ on Blue Pill board |
| 34 | **PA13** | **JTMS / SWDIO** | SWD data | `SWDIO` | B | **SWD debug** — always reserve; do not reassign for GPIO |
| 37 | **PA14** | **JTCK / SWCLK** | SWD clock | `SWDCLK` | I | **SWD debug** — always reserve; do not reassign for GPIO |
| 38 | PA15 | JTDI | SPI1_NSS (remap) | *(unassigned)* | — | JTAG only by default; free after JTAG disable with `AFIO_MAPR` |

### Port B — PB3 through PB9

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 39 | PB3 | JTDO | SPI1_SCK (remap) | *(unassigned)* | — | JTAG only by default; free after JTAG disable |
| 40 | PB4 | NJTRST | SPI1_MISO (remap) | *(unassigned)* | — | JTAG only by default |
| 41 | PB5 | GPIO | I2C1_SMBA, SPI1_MOSI (remap) | *(unassigned)* | — | Available |
| 42 | PB6 | GPIO | I2C1_SCL, USART1_TX (remap), TIM4_CH1 | *(unassigned — Draft B fallback for I2C SCL)* | B | Draft B I2C SCL fallback |
| 43 | PB7 | GPIO | I2C1_SDA, USART1_RX (remap), TIM4_CH2 | *(unassigned — Draft B fallback for I2C SDA)* | B | Draft B I2C SDA fallback |
| 45 | **PB8** | GPIO | TIM4_CH3, I2C1_SCL (remap), CAN_RX (remap) | `I2C0_SCL` | B | **Draft A I2C SCL** — telemetry bus to INA3221 |
| 46 | **PB9** | GPIO | TIM4_CH4, I2C1_SDA (remap), CAN_TX (remap) | `I2C0_SDA` | B | **Draft A I2C SDA** — telemetry bus to INA3221 |

### Port C

| LQFP Pin # | GPIO | Default function | Key alt. functions | Project assignment | Dir | Notes |
|---:|---|---|---|---|---|---|
| 2 | **PC13** | GPIO / TAMPER-RTC | RTC tamper, output | *(optional Status LED)* | O | On-board LED on Blue Pill; active-LOW; useful for boot-alive blink |
| 3 | PC14 | OSC32_IN | RTC 32kHz crystal input | *(unassigned)* | — | Do not use if RTC crystal planned |
| 4 | PC15 | OSC32_OUT | RTC 32kHz crystal output | *(unassigned)* | — | Do not use if RTC crystal planned |

### Port D

| LQFP Pin # | GPIO | Default function | Project assignment | Notes |
|---:|---|---|---|---|
| 5 | PD0 | OSC_IN (HSE) | *(unassigned)* | External crystal input; used by Blue Pill 8MHz crystal |
| 6 | PD1 | OSC_OUT (HSE) | *(unassigned)* | External crystal output; used by Blue Pill 8MHz crystal |

---

## Project Signal Summary (Draft A — Active Baseline)

| Net name (HAT schematic) | Blue Pill GPIO | LQFP Pin | Direction | Purpose |
|---|---|---:|---|---|
| `ISET_MPU_5V` | PA0 | 10 | Output | +5V rail current-set control |
| `ISET_MPU_3V3` | PA1 | 11 | Output | +3.3V rail current-set control |
| `ISET_MPU_Channel_3` | PA2 | 12 | Output | Adj channel current-set control |
| `FAULT_CRITICAL_SUM` | PA3 | 13 | Input | Aggregated fault signal from regulator board |
| `I2C0_SCL` | PB8 | 45 | Bidir | I2C clock to INA3221 telemetry IC |
| `I2C0_SDA` | PB9 | 46 | Bidir | I2C data to INA3221 telemetry IC |
| `UART1_TX` | PA9 | 30 | Output | UART debug / programming TX |
| `UART1_RX` | PA10 | 31 | Input | UART debug / programming RX |
| `SWDIO` | PA13 | 34 | Bidir | SWD debug data (ST-Link) |
| `SWDCLK` | PA14 | 37 | Input | SWD debug clock (ST-Link) |
| `NRST` | NRST | 7 | Input | Reset (test point) |
| `BOOT0` | BOOT0 | 44 | Input | Boot mode strap (test point) |
| *(Status LED)* | PC13 | 2 | Output | On-board LED, optional blink indicator |

---

## Draft B Fallback Map (Use only if Draft A pin conflict found)

| Net name | Draft A GPIO | Draft B GPIO | Reason to switch |
|---|---|---|---|
| `ISET_MPU_5V` | PA0 | PB0 | PA0 conflict or noise |
| `ISET_MPU_3V3` | PA1 | PB1 | PA1 conflict or noise |
| `ISET_MPU_Channel_3` | PA2 | PB10 | PA2 conflict or noise |
| `FAULT_CRITICAL_SUM` | PA3 | PB11 | PA3 conflict or noise |
| `I2C0_SCL` | PB8 | PB6 | PB8 conflict (remap not needed on PB6) |
| `I2C0_SDA` | PB9 | PB7 | PB9 conflict (remap not needed on PB7) |

> **Note**: UART (PA9/PA10) and SWD (PA13/PA14) have no Draft B fallback — these are fixed by the UART1 and SWD peripheral mapping.

---

## Pin Assignment Rules (copy from GPIO_PINOUT.md)

1. **SWD pins (PA13, PA14) are dedicated** — never dual-assign to project signals.
2. **BOOT1 (PB2) is boot-strap only** — do not route to any project signal.
3. **FAULT input (PA3 or PB11) should be kept away from noisy connector fanout.**
4. **I2C pair must stay together** on the same I2C peripheral instance.
5. **Promote Draft A to schematic only after pin-conflict and KERC-04 reset-safety checks pass.**
6. **If Draft A fails, try Draft B once before escalating MCU class.**

---

## Learning Notes

### Why PA0–PA3 for ISET/FAULT?
- These pins have **ADC capability** (ADC12_IN0–IN3), so they can optionally read back analog signals for calibration — no extra pin cost.
- They also map to TIM2_CH1–CH4, enabling hardware PWM generation for DAC-less current setting.

### Why PB8/PB9 for I2C?
- PB8/PB9 use **I2C1 in remap mode**. The default I2C1 pins (PB6/PB7, Draft B) are also valid.
- PB8/PB9 keep the I2C pair away from the PA0–PA3 cluster used for rail control, reducing crosstalk risk.

### Why PA9/PA10 for UART?
- PA9/PA10 are **USART1 TX/RX** — the primary UART on STM32F103.
- USART1 has the highest baud-rate capability and is the default target for STM32 bootloader serial flashing.
- This is the same UART used to flash the chip via USB-serial if SWD is unavailable.

### SWD vs JTAG
- Blue Pill uses **SWD** (2-wire: SWDIO + SWCLK) — simpler than 5-wire JTAG.
- PA13 and PA14 default to JTMS/JTCK at reset; the ST-Link probe switches them to SWD mode automatically.
- Leaving `PA15`, `PB3`, `PB4` as JTAG pins at reset costs you those GPIOs until you call `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` (or equivalent HAL call) in firmware.

### NRST and BOOT0 as test points
- Both are exposed as connector pins on the HAT (`NRST` and `BOOT0` net names).
- Holding BOOT0 HIGH while releasing NRST puts the chip in **system memory boot** mode — this lets you reflash via UART even if a bad firmware image has locked out SWD.
- This is a safety escape hatch: always leave both accessible on the PCB.
