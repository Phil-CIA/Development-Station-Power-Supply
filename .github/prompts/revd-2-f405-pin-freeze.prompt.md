---
mode: agent
description: "Rev-D session 2 — verify and freeze the STM32F405RG pin, clock and boot plan"
---
Work on issue #102, session 2: freeze the F405 pin map. Use branch `hw/revd-f405-pin-freeze`.

Read first: `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`, `docs/HAT_CONTROLLER_EVALUATION.md`, and `docs/STM32_BLUEPILL_PIN_TABLE.md`.

Goal: turn the draft STM32F405RGT6 (LQFP64) allocation into a frozen, cited table.
- Check every assignment against the alternate-function and pinout tables in ST DS8626. This covers SPI1, USART1 on PB6/PB7, USART3, UART4 on PC10/PC11, I2C1, the TIM option on PB5, the USB OTG FS pins PA9/PA11/PA12, and SWD/SWO. Cite table and page numbers. Flag any DMA or timer conflicts.
- Define the reset and boot safe state for every output. Specify the external pull-ups and pull-downs needed (ISET, latch, fan, flash CS, C3 enable/boot).
- Assign the still-open signals: C3 EN/reset and C3 boot GPIO.
- Select the HSE crystal frequency and load capacitors for an accurate USB clock. Specify the VBAT, VCAP, VDD/VDDA bypass, NRST and BOOT0/PB2 strap values, citing ST AN4488.
- Select a USB connector and ESD part. USB carries data and VBUS sense only and must never power or backfeed the HAT.

Rules:
- Update only the docs, mainly `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`. Mark the table frozen only after every row is cited.
- Do not edit the Rev-C files.
- Commit with the Copilot co-author trailer and open a PR that references #102.

Stop when the table is cited and frozen and the support-part values are listed. List anything that is still uncertain.
