# U10 Pin Wiring Workthrough — STM32 Blue Pill (1My_MPUs:stm32_BLUE)

**Date started:** 2026-05-25  
**Last updated:** 2026-05-27  
**Schematic:** `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch`  
**Symbol:** U10 (`1My_MPUs:stm32_BLUE`) — renamed from U9A ✓  
**Old symbol to remove:** U9 (`Local:BluePill_STM32F103C8_Migrated`) — delete after SR_CLK/SR_Data migration

---

## 🚨 STOP — Fix This First

**Pin 37 (R = NRST) is connected to GND.** The MCU will be held in permanent reset and will never run.

- [ ] **Remove the GND connection from pin 37 (R)**
- [ ] **Wire pin 37 → NRST net** (its own dedicated net)

---

## Architecture Notes (Updated 2026-05-26)

**SWD pins preserved — debugging is available:**
- Pin 42 (SW0 / PA13) → `SWDIO` net → test point TP14
- Pin 43 (SWCLK / PA14) → `SWDCLK` net → test point TP13
- Programming via SWD or UART bootloader (BOOT0 jumper high) both available.

**SPI bus (single, shared on-board + off-board via J13):**
- PA4=SR_Latch, PA5=SPI SCK, PA6=SPI_MISO, PA7=SPI_MOSI, PA8=Memory_CS
- PB12=SPI_CS (J13 pin 9 via R83)

**Two I2C buses:**
- Bus 0 (on-board sensors): PB8=I²C SCL_0, PB9=I²C SDA_0 — wired ✓

**Display UART (USART3):**
- PB10=DISP_UART_TX, PB11=DISP_UART_RX — wired to J21 via R78/R79 ✓ (2026-05-27)

---

## Step 0 — Rename U9A → U10

- [x] ~~Double-click U9A in schematic → Edit Properties → set Reference to **U10**~~ ✓ Done

---

## Group A — Power & Ground (7 pins) ✓ (NRST fix pending)

| Pin | Name | Net | Status |
|---|---|---|---|
| 18 | 5V | +5V_Boot | ✓ Done |
| 19 | G | GND | ✓ Done |
| 20 | 3.3 | No connect | ✓ Done |
| 38 | 3.3 | No connect | ✓ Done |
| 39 | GND | GND | ✓ Done |
| 40 | GND | GND | ✓ Done |
| 44 | GND (bottom) | GND | ✓ Done |

---

## Group B — UART1 Debug Bus ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 6 | A9 | UART1_TX | ✓ Done |
| 7 | A10 | UART1_RX | ✓ Done |
| 8 | A11 | CTS | ✓ Done (→ R77 → J13 pin 6) |
| 9 | A12 | RTS | ✓ Done (→ R73 → J13 pin 4) |

---

## Group C — I2C Bus 0 — PB8/PB9 ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 16 | B8 | I²C SCL_0 | ✓ Done |
| 17 | B9 | I²C SDA_0 | ✓ Done |

---

## Group D — Display UART (USART3) — PB10/PB11 ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 35 | B10 | DISP_UART_TX | ✓ Done (2026-05-27) |
| 36 | B11 | DISP_UART_RX | ✓ Done (2026-05-27) |

> I2C Bus 1 removed 2026-05-27. J14/J15 deleted; R78/R79 repurposed as 33Ω series resistors to J21 (display connector). PB10/PB11 reallocated to USART3 display interface.

---

## Group E — SWD / Debug / Reset ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 37 | R | NRST | ✓ Done |
| 42 | SW0 | SWDIO | ✓ Done (test point TP14) |
| 43 | SWCLK | SWDCLK | ✓ Done (test point TP13) |
| 41 | 3V3 | No connect | ✓ Done |

---

## Group F — ISET Analog Outputs ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 25 | A0 | ISET_MPU_5V | ✓ Done |
| 26 | A1 | ISET_MPU_3V3 | ✓ Done |
| 27 | A2 | ISET_MPU_Channel_3 | ✓ Done |

---

## Group G — Fault Inputs ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 28 | A3 | FAULT_CRITICAL_SUM | ✓ Done |
| 33 | B0 | FAULT_WARNING_SUM | ✓ Done |

---

## Group H — SPI Bus ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 5 | A8 | Memory_CS | ✓ Done |
| 29 | A4 | SR_Latch | ✓ Done |
| 30 | A5 | SPI SCK | ✓ Done (also U7/U8 SHCP) |
| 31 | A6 | SPI_MISO | ✓ Done (also U11 DO) |
| 32 | A7 | SPI_MOSI | ✓ Done (also U7 DS, U11 DI) |
| 1 | B12 | SPI_CS | ✓ Done (→ R83 → J13 pin 9) |

> **U11 (W25Q128, 16MB SPI Flash) role decision (2026-05-26):** Originally intended for ESP32 OTA staging.
> **Repurposed as STM32 telemetry ring buffer** — stores raw V/I sample records for bulk transfer to PC via UART/USB (CH340C bridge, J20).
> STM32 OTA over SPI flash requires a custom IAP bootloader; deferred unless needed.

---

## Group I — Status LED ✓

| Pin | Name | Net | Status |
|---|---|---|---|
| 22 | C13 | Status_LED | ✓ Done (→ D12 DIN) |

---

## Group J — No Connect ✓

All NC pins confirmed in netlist (pintype includes `+no_connect`):
- Pin 10 (A15), 20 (3.3), 21 (VB), 34 (B1), 38 (3.3), 41 (3V3)
- Pins 2–4 (B13–B15), 11–15 (B3–B7), 23–24 (C14–C15)

> Pins 13/14/15 (B5/B6/B7) will be reassigned to Group K GPIO — remove NC markers.

## Group K — Development GPIO Connector Pins

Three development I/O pins brought out to expansion connectors. Assign to adjacent free GPIOs.
Net names follow STM32 port/pin convention (same approach as ESP32 GPIO naming).

| U10 Pin | STM32 Pin | Net Name | Connector | Status |
|---|---|---|---|---|
| 13 (B5) | PB5 | `PB5` | J12 pin 6 (was GPIO10) | ✓ Direct connection |
| 14 (B6) | PB6 | `PB6` | J12 pin 8 (was GPIO11) | ✓ Direct connection |
| 15 (B7) | PB7 | `PB7` | J13 pin 10 (was GPIO12) via R86 | ✓ Series resistor |

**KiCad steps:**
1. Remove NC markers from U10 pins 13, 14, 15
2. Add net label `PB5` on pin 13, `PB6` on pin 14, `PB7` on pin 15
3. Change label at J12 pin 6: `GPIO10` → `PB5`
4. Change label at J12 pin 8: `GPIO11` → `PB6`
5. Change label at J13 pin 10: `GPIO12` → `PB7`

---

## Step Z — Clean Up Complete ✓

- Old U9 (`Local:BluePill_STM32F103C8_Migrated`) deleted ✓
- U9 is now AMS1117-3.3 LDO (VI→+5V_Boot, VO→+3.3V Boot, GND) ✓
- SR_CLK/SR_Data migrated to SPI SCK/SPI_MOSI ✓
- 3V3_ESP renamed to `+3.3V Boot` ✓

---

## Verification Checklist

- [x] ~~Fix NRST (pin 37 off GND)~~ ✓
- [x] ~~SR_CLK/SR_Data labels updated at shift registers~~ ✓
- [x] ~~All NC markers placed~~ ✓
- [x] ~~U9 deleted / replaced with AMS1117-3.3 LDO~~ ✓
- [x] ~~CTS/RTS wired (PA11→J13 pin 6, PA12→J13 pin 4)~~ ✓
- [x] ~~3V3_ESP renamed to +3.3V Boot~~ ✓
- [x] ~~Group K GPIO assignments (J12 pin 6/8, J13 pin 10 → PB5/PB6/PB7)~~ ✓
- [x] ~~KiCad ERC~~ ✓ **0 errors / 9 warnings (all environment/stub label — no electrical issues)**
- [x] ~~J19 SWD Cortex Debug 10-pin header added to schematic~~ ✓ (2026-05-26)
- [x] ~~BOOT0 — closed loop: Blue Pill module JP3 jumper + onboard pull-down handles this; no HAT-level circuit needed~~ ✓
- [x] ~~U11 role decision documented~~ ✓ (telemetry ring buffer, see Group H note)
- [x] ~~Run `scripts/verify-connector-contract.ps1`~~ ✓ passes in `reduced` mode (18/18)
- [x] ~~Add CH340C USB-UART bridge (U12) + USB-C connector (J20) via KiCad GUI~~ ✓ (2026-05-27)
- [x] ~~Re-export netlist after CH340C addition~~ ✓
- [x] ~~Re-run ERC after CH340C addition~~ ✓ 0 errors / 0 warnings
- [x] ~~Display UART: J21 (JST XH 4-pin) added, I2C_1 removed, DISP_UART_TX/RX on PB10/PB11~~ ✓ (2026-05-27)
- [ ] Update `docs/STM32_BLUEPILL_PIN_TABLE.md` validation log

---

## Pin Count Summary

- **Connections wired:** 25 nets
- **No connects:** 19 pins
- **Total:** 44 pins ✓
