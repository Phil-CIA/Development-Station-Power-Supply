# Universal Display Interface (UDI) Standard

**Created:** 2026-05-27  
**Status:** Active — HAT Rev-B implements this standard at J21  

---

## Purpose

Defines a reusable, copy-paste connector standard for attaching any display module to any host board in this project family. Both the CrowPanel 4.3" and the custom front-panel board use this interface.

---

## Connector Specification

| Field | Value |
|---|---|
| Connector family | JST XH, 2.54mm pitch |
| Part number | B4B-XH-A (vertical, through-hole) |
| Pin count | 4 |
| LCSC | C144394 |
| KiCad footprint | `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.54mm_Vertical` |
| Max current | 3A per contact |
| Mating cable | JST XH 4-pin pre-crimped (XH2.54-4P) |

---

## Pin Assignment

| Pin | Signal | Direction | Notes |
|---:|---|---|---|
| 1 | +5V | Host → Display | Powers display board; max 2A draw |
| 2 | GND | — | Common ground |
| 3 | DISP_UART_TX | Host → Display | Host transmits; display receives |
| 4 | DISP_UART_RX | Display → Host | Display transmits; host receives |

> **Cable polarity:** Pin 1 (+5V) must be verified before connecting. JST XH connectors are keyed but confirm latch orientation matches host and display footprints before powering.

---

## Communication Protocol

| Parameter | Value |
|---|---|
| Physical | UART, 3.3V logic (both sides) |
| Baud rate | 115200 (bring-up default) |
| Frame format | 8N1 |
| Series resistors | 33Ω on TX and RX lines (host side, ESD/short protection) |

### Message framing

```
CMD:<command>\n       Display → Host  (command request)
ACK:<response>\n      Host → Display  (success response)
ERR:<message>\n       Host → Display  (error response)
EVT:<event>\n         Host → Display  (unsolicited event/state update)
```

---

## Design Rationale

- **LVGL on display MCU** — display handles all rendering; host sends data only. No pixel pushing from host.
- **UART chosen over SPI** — STM32 SPI bus is shared with on-board flash (U11) and shift registers (U7/U8); adding a display as a 4th SPI slave creates bus contention. UART3 (PB10/PB11) is dedicated.
- **3.3V both sides** — STM32F103 and ESP32-S3/C6 both use 3.3V I/O; no level shifting required.
- **4-pin JST XH** — matches CrowPanel Advance UART0-IN connector exactly; straight-through cable works.

---

## HAT Rev-B Implementation (J21)

| Field | Value |
|---|---|
| Reference | J21 |
| Value | XH2.54-4P |
| UART peripheral | USART3 |
| MCU TX pin | PB10 (U10 pin 35) → net `DISP_UART_TX` → R78 → J21 pin 3 |
| MCU RX pin | PB11 (U10 pin 36) → net `DISP_UART_RX` → R79 → J21 pin 4 |
| Series resistors | R78, R79 (33Ω, repurposed from I2C_1 pull-ups) |

---

## CrowPanel Interim Split Connector (UART1 data, UART0-IN power only)

The CrowPanel UART0 is also its CH340K programmer UART, so a Blue Pill on UART0 corrupts uploads (#93). Interim fix (hardware decision, no firmware lease): data goes to the UART1-OUT connector (3.3 V logic), UART0-IN carries only 5 V power. Next HAT/panel revision should use UART1 (3.3 V) alone.

| J21 (HAT) | Signal | CrowPanel connector | Pin |
|---|---|---|---|
| 1 | +5V | UART0-IN | 1 (+5V) |
| 2 | GND | UART0-IN and UART1-OUT | 2 (GND) and 1 (GND) |
| 3 | DISP_UART_TX (PB10) | UART1-OUT (HY2.0-4P) | 3 = RX (IO19) |
| 4 | DISP_UART_RX (PB11) | UART1-OUT (HY2.0-4P) | 4 = TX (IO20) |
| - | - | UART0-IN pins 3, 4 | **leave unconnected** (IO44/IO43 stay on the CH340K only) |
| - | - | UART1-OUT pin 2 (3V3) | leave unconnected |

- CrowPanel K1 must select UART1_OUT (silkscreen: S1 = 0, S0 = 1; verify on the physical panel).
- Firmware: `DISP_LINK_SLAVE_USE_UART0 = 0` is the default; `-DDISP_LINK_SLAVE_USE_UART0=1` restores the UART0 path, which again needs the Blue Pill powered off or isolated for every upload.
- Pin numbers come from `docs/handoff-archive/root-handoffs/HANDOFF_2026-05-29.md` and `HANDOFF_2026-05-30.md` (Elecrow wiki, V1.1 panel); they are not yet re-verified on the current replacement panel.

---

## Compatible Display Devices

| Device | Connector | Display MCU | Notes |
|---|---|---|---|
| Elecrow CrowPanel Advance 4.3" | UART1-OUT (HY2.0-4P) data + UART0-IN (XH2.54-4P) power, interim | ESP32-S3-WROOM-1-N16R8 | Runs LVGL 9.2 natively; data on IO19=RX, IO20=TX (3.3 V). IO44/IO43 (UART0) are reserved for the CH340K programmer. |
| Custom front-panel board (future rev) | J3 (XH2.54-4P) | ESP32-C6 | To be redesigned to LVGL+UART; currently legacy SPI design |

> **CrowPanel cable note:** Verify pin 3/4 polarity from Elecrow Eagle schematic before building cable. Power (5V/2A) is on pin 1 — incorrect polarity will damage the display.
