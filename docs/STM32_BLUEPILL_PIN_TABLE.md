# STM32 Blue Pill Pin Table and Project Signal Reference

MCU: STM32F103C8T6 (LQFP-48)
Board: Blue Pill (generic STM32 dev board)
Project role: HAT MCU for rail-control outputs, fault input, I2C telemetry, and debug/program access

## Purpose

This document is a learning and tracking table for the MCU migration work.
Use it to map each project net to a physical STM32 pin, keep draft/fallback options visible, and record validation status.

## Tracking Table

| Project signal | Current Rev-B MCU mapping (ESP32) | Target Blue Pill pin (Draft A) | Target Blue Pill pin (Draft B fallback) | Direction at MCU | Schematic net label | Contract-sensitive | Validation status | Notes |
|---|---|---|---|---|---|---|---|---|
| ISET_MPU_5V | U9 GPIO path | PA0 (LQFP pin 10) | PB0 (LQFP pin 18) | Output | ISET_MPU_5V | Yes | Planned | Rail current-set control for 5V channel |
| ISET_MPU_3V3 | U9 GPIO path | PA1 (LQFP pin 11) | PB1 (LQFP pin 19) | Output | ISET_MPU_3V3 | Yes | Planned | Rail current-set control for 3.3V channel |
| ISET_MPU_Channel_3 | U9 GPIO path | PA2 (LQFP pin 12) | PB10 (LQFP pin 21) | Output | ISET_MPU_Channel_3 | Yes | Planned | Rail current-set control for adjustable channel |
| FAULT_CRITICAL_SUM | U9 GPIO2 | PA3 (LQFP pin 13) | PB11 (LQFP pin 22) | Input | FAULT_CRITICAL_SUM | Yes | Planned | Aggregated fault input |
| I2C0_SCL | U9 I2C path | PB8 (LQFP pin 45) | PB6 (LQFP pin 42) | Bidirectional | I2C0_SCL | No | Planned | Telemetry bus clock |
| I2C0_SDA | U9 I2C path | PB9 (LQFP pin 46) | PB7 (LQFP pin 43) | Bidirectional | I2C0_SDA | No | Planned | Telemetry bus data |
| UART1_TX | U9 TXD path | PA9 (LQFP pin 30) | N/A | Output | UART1_TX | No | Planned | Serial debug/program TX |
| UART1_RX | U9 RXD path | PA10 (LQFP pin 31) | N/A | Input | UART1_RX | No | Planned | Serial debug/program RX |
| SWDIO | Not explicit in current Rev-B net labels | PA13 (LQFP pin 34) | N/A | Bidirectional | SWDIO | No | Planned | Reserved for ST-Link debug |
| SWDCLK | Not explicit in current Rev-B net labels | PA14 (LQFP pin 37) | N/A | Input | SWDCLK | No | Planned | Reserved for ST-Link debug |
| NRST | Reset path (ESP32 EN/RESET today) | NRST (LQFP pin 7) | N/A | Input | NRST | No | Planned | Add deterministic reset network |
| BOOT0 | ESP32 boot pin naming today | BOOT0 (LQFP pin 44) | N/A | Input | BOOT0 | No | Planned | Strap low for normal flash boot |
| Status LED (optional) | Existing status path | PC13 (LQFP pin 2) | N/A | Output | STATUS_LED | No | Planned | Optional heartbeat indicator |

## Debug and Boot Access Checklist

| Signal | Target pin | Access requirement | Status | Notes |
|---|---|---|---|---|
| SWDIO | PA13 | Test point or header access | Complete | J19 pin 2; TP14; 2×5 Cortex Debug header added 2026-05-26 |
| SWDCLK | PA14 | Test point or header access | Complete | J19 pin 4; TP13; 2×5 Cortex Debug header added 2026-05-26 |
| GND (debug reference) | Any GND | Adjacent to SWD points | Complete | J19 pins 3, 5, 9 |
| NRST | NRST pin | Test point access | Complete | J19 pin 10; SWD header provides reset control |
| BOOT0 | BOOT0 pin | Strap and test access | Closed | Blue Pill JP3 jumper + onboard pull-down; no HAT circuit needed |
| UART1_TX | PA9 | Test point/header | Planned | Optional serial bring-up path |
| UART1_RX | PA10 | Test point/header | Planned | Optional serial bring-up path |

## Pin Assignment Rules

1. Reserve PA13 and PA14 for SWD only.
2. Do not use PB2 for general project I/O; treat BOOT1 as strap behavior.
3. Keep FAULT_CRITICAL_SUM on a low-noise route and away from power-switching fanout.
4. Keep I2C SCL and SDA as a matched pair on the same peripheral mapping.
5. Promote Draft A to schematic only after pin-conflict and reset-safety checks pass.
6. If Draft A fails, try Draft B once before escalating MCU class.

## Validation Log

| Date | Change | Evidence | Result |
|---|---|---|---|
| 2026-05-24 | Pin table initialized | Migration planning baseline | In progress |
| 2026-05-24 | Rev B interim label remap | `TXD`->`UART1_TX`, `RXD`->`UART1_RX`, `GPIO8/BOOT`->`BOOT0`, `GPIO9/BOOT`->`BOOT1` in Rev B schematic | In progress |
| 2026-05-24 | Rev B phase-3 access points added | Added `TP_UART1_TX`, `TP_UART1_RX`, `TP_SWDIO`, `TP_SWDCLK`, `TP_NRST`, `TP_BOOT0`; added `SWDIO`, `SWDCLK`, `NRST` net labels on U9-side debug pins | In progress |
| 2026-05-26 | U10 44-pin wiring complete | All pins wired, 0 ERC errors, `verify-connector-contract.ps1` passes reduced mode | Complete |
| 2026-05-26 | J19 SWD Cortex Debug header added | 2×5 10-pin ARM Cortex Debug header; SWDIO/SWDCLK/NRST/VCC/GND wired; NRST ERC warning should clear on next KiCad open | Complete |
| 2026-05-26 | BOOT0 loop closed | Blue Pill module JP3 jumper + onboard pull-down resistor handles BOOT0; no HAT-level circuit needed | Closed |
| 2026-05-26 | U11 W25Q128 role repurposed | Was: ESP32 OTA staging flash. Now: STM32 telemetry ring buffer for raw V/I data; bulk transfer to PC via UART/USB | Closed |
| 2026-05-26 | CH340C USB-UART bridge spec ready | U12=CH340C, J20=USB-C; pending KiCad GUI placement; UART1_TX→CH340C RXD, UART1_RX→CH340C TXD | Pending |

## Related Files

- docs/GPIO_PINOUT.md
- docs/KERC-04_RESET_STARTUP_VALIDATION.md
- docs/PROJECT_SPLIT_ISSUES_AND_PRIORITIES.md
- hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch
- hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net
