---
mode: agent
description: "Rev-D session 3 — audit the ESP32-C3-MINI Wi-Fi coprocessor integration"
---
Work on issue #102, session 3: the ESP32-C3 coprocessor. Use branch `hw/revd-c3-coprocessor`.

Read first: `docs/HAT_CONTROLLER_EVALUATION.md` and `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`. Then inspect U2 (`ESP32_C3_mini`, footprint `113991054:MODULE_113991054`) in `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch` and its `.net` file.

Goal: define the C3's role on Rev-D. The C3 is a low-cost Wi-Fi/OTA coprocessor; the STM32 stays deterministic.
- Record the current U2 connections and how many GPIOs are unconnected.
- From Espressif's ESP32-C3-MINI-1 datasheet and hardware design guidelines, record:
  - 3V3 supply and decoupling needs, plus peak Wi-Fi current;
  - the EN RC network and the strapping pins (GPIO2, GPIO8, GPIO9);
  - the antenna keep-out.
- Specify the UART link to STM32 UART4 (PC10/PC11): which C3 pins, voltage levels, and whether flow control is needed.
- Specify how the STM32 controls C3 EN and boot. Specify the C3 programming and recovery access (USB or UART header).
- Define the boundary: the C3 never drives the power stage directly, and OTA cannot change STM32 safety limits.

Rules:
- Make doc changes only.
- Record the open signal names so session 2 can assign STM32 pins.
- Do not edit the Rev-C files.
- Commit with the Copilot co-author trailer and open a PR that references #102.
