---
mode: agent
description: "Rev-D session 1 — reconcile the Rev-C HAT schematic and netlist (#62)"
---
Work on issue #102, session 1: Rev-C source reconciliation. Use branch `hw/revc-source-reconcile`.

Read first: `README.md`, `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md`, `docs/HAT_CONTROLLER_EVALUATION.md`, `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`, and `docs/STM32_BLUEPILL_PIN_TABLE.md` (including its #62 conflict notes).

Goal: decide which Rev-C source Rev-D copies its circuits from.
- Export a fresh netlist from `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch`. Use the VS Code task `Rev-C: Export netlist`, or run `kicad-cli sch export netlist`.
- Compare the fresh export with the committed `.net` and with the documented #62 findings. Cover the Q9/Q3/Q12/U4 range-switch and fault paths.
- For every block Rev-D will reuse, record whether it is confirmed, conflicting or unknown. The blocks are the power stage, ISET DAC/control, the SR latch, I2C/AW9523, the fan, W25Q flash, UDI, CH340 debug, and the ESP32-C3 (U2).
- Run the `Rev-C: KiCad ERC` task and summarize the relevant violations.

Rules:
- Do not edit the Rev-C KiCad files.
- Record the results in `docs/STM32_BLUEPILL_PIN_TABLE.md`, then update gate 1 in `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`.
- Do not create root-level handoff or summary files.
- Commit with the Copilot co-author trailer and open a PR that references #62 and #102.

Stop when every reused block has a recorded source of truth. List any items that need a bench measurement to settle.
