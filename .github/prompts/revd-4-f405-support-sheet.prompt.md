---
mode: agent
description: "Rev-D session 4 — create the Rev-D KiCad project and F405 minimum-support sheet"
---
Work on issue #102, session 4: the F405 support sheet. Use branch `hw/revd-f405-support-sheet`.

Prerequisites: sessions 2 (frozen pin map) and 3 (C3 interface) must be merged. If `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md` is not marked frozen, stop and report.

Goal:
- Create a separate project, `hardware/kicad/dsp-regulator-hat-rev-d/`. Do not copy or modify the Rev-C PCB.
- Add a hierarchical MCU sheet with the `MCU_ST_STM32F4:STM32F405RGTx` symbol and the frozen support parts: bypass and VCAP capacitors, VDDA filtering, VBAT, the HSE crystal, NRST, BOOT0/PB2 straps, the SWD header, USB with ESD and VBUS sense, and the C3 UART/EN/boot nets.
- Use net labels that match the contract signal names.
- Run `kicad-cli sch erc` and export a netlist into `build_kicad/`. Check every MCU power and support pin, the UART directions, the USB mapping and SWD against the contract.

Rules:
- Record ERC results and review findings in the contract doc. ERC alone does not validate the design.
- Commit with the Copilot co-author trailer and open a PR that references #102.
