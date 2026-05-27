# Display Project Reference

This folder captures the current starting point for the front-panel display direction inside the Development Station Power Supply project.

## Path priority

| Priority | Path | Hardware | Status |
|----------|------|----------|--------|
| **Primary** | Elecrow CrowPanel 4.3 HMI | ESP32-S3, 800×480 IPS, capacitive touch | Active — main development target |
| Secondary | Custom front-panel board + Hosyond MSP4021 | ESP32-C6, ST7796S SPI, 480×320 | Paused — revisit with replacement module |

## Current direction
The display effort is now on **two paths with a defined priority**:
- **Primary**: Elecrow CrowPanel Advance 4.3 inch HMI — main development target going forward
- **Secondary**: Custom front-panel hardware with Hosyond MSP4021 (ST7796S SPI module) — retained as a documented path but paused pending a replacement module

## Active display candidate
- Elecrow CrowPanel Advance 4.3 inch HMI ESP32 display
- 800x480 IPS panel
- Capacitive touch
- ESP32-S3-WROOM-1-N16R8
- 16MB flash + 8MB PSRAM
- Suitable for Arduino, ESP-IDF, PlatformIO, and LVGL

## Official documentation links
- Product wiki:
  - https://www.elecrow.com/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html
- Product page:
  - https://www.elecrow.com/crowpanel-advance-4-3-hmi-esp32-800x480-ai-display-ips-touch-artificial-intelligent-screen.html
- Elecrow GitHub organization:
  - https://github.com/Elecrow-RD
- Official board repository:
  - https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480
- General CrowPanel course files:
  - https://github.com/Elecrow-RD/CrowPanel-ESP32-Display-Course-File

## Datasheet and hardware resource links
- Board resource page (includes schematics, version notes, and platform support):
  - https://www.elecrow.com/wiki/CrowPanel_Advance_4.3-HMI_ESP32_AI_Display.html#resources
- GitHub repository hardware and docs area:
  - https://github.com/Elecrow-RD/CrowPanel-Advance-4.3-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480
- Espressif ESP32-S3-WROOM module docs:
  - https://www.espressif.com/en/support/documents/technical-documents

## Notes
- The old separate TTF display repo is now reference-only and no longer the active implementation path.
- The front-panel work now lives back under the main Development Station Power Supply project.
- Custom board redesign (Phase 3) is scoped but not started — CrowPanel bring-up (Phase 4) runs first to validate the UART protocol.
- When the CrowPanel arrives, confirm the exact board revision against the Elecrow wiki version notes before selecting example code.
- The wiki indicates 5V input, LVGL support, and external interfaces for UART, I2C, audio, battery, and storage.

## Interface direction
The goal is not identical hardware. The goal is a common front-panel interface so either display path can be used from the host perspective.

**Standard decided (2026-05-27): Universal Display Interface (UDI)**
- Connector: JST XH B4B-XH-A, 4-pin (XH2.54-4P), LCSC C144394
- Pin 1: +5V (host → display), Pin 2: GND, Pin 3: DISP_UART_TX (host TX), Pin 4: DISP_UART_RX (host RX)
- Protocol: LVGL on display MCU; UART 115200 8N1; `CMD:`/`ACK:`/`ERR:`/`EVT:` framing
- Full spec: `docs/DISPLAY_INTERFACE_STANDARD.md`
- HAT Rev-B: implemented at J21 (PB10=TX, PB11=RX via USART3, 33Ω series on both lines)

---

## Custom Board Rev-1 Requirements

The custom front-panel board must be redesigned to comply with the UDI standard. The secondary-path board (ESP32-C6 + ST7796S) remains the hardware baseline; only the host interface and firmware architecture change.

### Host connector (J3) — REPLACE

| Item | Legacy (remove) | Rev-1 target |
|---|---|---|
| Connector | 2×5 IDC, 2.54mm (Conn_02x05) | JST XH B4B-XH-A, 4-pin |
| Signals | +5V, GND, SPI SCK/MISO/MOSI/CS | +5V, GND, UART_RX, UART_TX |
| ESP32-C6 role | SPI slave | UART peripheral |

### Firmware — REPLACE

| Item | Legacy | Rev-1 target |
|---|---|---|
| Graphics | TFT_eSPI / host pixel push | LVGL (display-side rendering) |
| Host link | SPI slave GPIO4–7 | UART RX/TX (GPIO assignment TBD) |
| Protocol | Raw pixel stream | `CMD:`/`ACK:`/`ERR:`/`EVT:` framing |

### What stays unchanged
- MCU: ESP32-C6
- Display panel: Hosyond MSP4021 (ST7796S, 480×320 SPI)
- Local SPI bus: TFT (GPIO10–13), touch XPT2046, SD card
- Power regulator: AMS1117-3.3

### ESP32-C6 UART pin assignment (TBD)
GPIO4/5 (previously SPI slave SCK/MISO) are available candidates for UART RX/TX. Confirm against ESP32-C6 UART peripheral matrix before committing. Document final assignment in `docs/GPIO_PINOUT.md` when redesign begins.

### Scope note
This redesign is Phase 3 — scoped as a requirements spec. KiCad work starts after CrowPanel bring-up (Phase 4) validates the UART protocol end-to-end.

## Local bring-up starter
- Local PlatformIO starter path (CrowPanel):
  - `crowpanel-43-bringup/`
- Starter is based on official Elecrow V1.2 4.3-inch example pin mapping for RGB data bus and touch I2C.
- Current scope is bench bring-up with a minimal serial command stub, not full LVGL UI firmware yet.

## Hosyond MSP4021 (secondary path) reference
- Hardware spec, confirmed pin map, DC polarity, init sequence, and official resource links:
  - `docs/display-project/hosyond-msp4021-st7796-reference.md`
