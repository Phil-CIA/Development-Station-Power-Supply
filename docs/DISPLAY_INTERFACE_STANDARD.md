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
CMD:<command>\n       Host → Display  (command request)
ACK:<response>\n      Display → Host  (success response)
ERR:<message>\n       Display → Host  (error response)
EVT:<event>\n         Display → Host  (unsolicited event, e.g. button press)
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

## Compatible Display Devices

| Device | Connector | Display MCU | Notes |
|---|---|---|---|
| Elecrow CrowPanel Advance 4.3" | UART0-IN (XH2.54-4P) | ESP32-S3-WROOM-1-N16R8 | Runs LVGL 9.2 natively; GPIO44=RX, GPIO43=TX |
| Custom front-panel board (future rev) | J3 (XH2.54-4P) | ESP32-C6 | To be redesigned to LVGL+UART; currently legacy SPI design |

> **CrowPanel cable note:** Verify pin 3/4 polarity from Elecrow Eagle schematic before building cable. Power (5V/2A) is on pin 1 — incorrect polarity will damage the display.
