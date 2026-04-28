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
- No standalone custom display redesign is planned right now.
- The old separate TTF display repo is now reference-only and no longer the active implementation path.
- The front-panel work now lives back under the main Development Station Power Supply project.
- The practical plan is to use the existing custom board as the learning baseline while also testing the Elecrow unit as an off-the-shelf UI option.
- When the physical board arrives, confirm the exact board revision against the Elecrow wiki version notes before selecting example code.
- The wiki indicates 5V input, LVGL support, and external interfaces for UART, I2C, audio, battery, and storage.

## Interface direction
The goal is not identical hardware. The goal is a common front-panel interface so either display path can be used from the host perspective.

Recommended minimum host-to-display contract:
- 5V
- GND
- UART TX
- UART RX
- optional reset or status line

## Local bring-up starter
- Local PlatformIO starter path (CrowPanel):
  - `crowpanel-43-bringup/`
- Starter is based on official Elecrow V1.2 4.3-inch example pin mapping for RGB data bus and touch I2C.
- Current scope is bench bring-up with a minimal serial command stub, not full LVGL UI firmware yet.

## Hosyond MSP4021 (secondary path) reference
- Hardware spec, confirmed pin map, DC polarity, init sequence, and official resource links:
  - `docs/display-project/hosyond-msp4021-st7796-reference.md`
