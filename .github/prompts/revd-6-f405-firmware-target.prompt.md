---
mode: agent
description: "Rev-D session 6 — add an STM32F405 PlatformIO firmware target"
---
Work on issue #102, session 6: the F405 firmware target. Use branch `firmware/f405-target`.

Prerequisite: session 2 (frozen pin map) must be merged.

Read first: `docs/FIRMWARE_DEVELOPMENT_PLAN.md`, `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`, `stm32-bluepill-bringup/platformio.ini`, and `stm32-bluepill-bringup/src/main.cpp`.

Goal:
- Add a PlatformIO environment for the STM32F405RG. Keep the existing F103 environment building unchanged.
- Move the pin constants behind a per-board pin header, so the Rev-C F103 and Rev-D F405 maps both come from their contract docs.
- Enable USB CDC on the OTG FS pins (PA11/PA12, VBUS sense PA9). Keep the UDI on USART3, debug on USART1 (PB6/PB7), and the C3 link on UART4 (PC10/PC11).
- Build both environments and report the flash and RAM usage.

Rules:
- Make no hardware claims; this target is build-only until a Rev-D board exists.
- Update the #102 row in `docs/FIRMWARE_DEVELOPMENT_PLAN.md`.
- Commit with the Copilot co-author trailer and open a PR that references #102.
