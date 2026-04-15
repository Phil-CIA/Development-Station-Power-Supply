# Display Project Reference

This folder captures the current starting point for the display direction being evaluated for the Development Station Power Supply front panel.

## Current candidate
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
- This is the practical off-the-shelf path for UI bring-up while the existing custom hardware is still used as the system baseline and learning platform.
- When the physical board arrives, confirm the exact board revision against the Elecrow wiki version notes before selecting example code.
- The wiki indicates 5V input, LVGL support, and external interfaces for UART, I2C, audio, battery, and storage.
