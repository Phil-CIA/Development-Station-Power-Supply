# Development Station Power Supply Handoff - 2026-08-17

## Session outcome

Rev-C changed from a stacked regulator/HAT arrangement to a coplanar two-board direction. The boards will mount side by side with separate power and signal harnesses so both component sides remain accessible for probing.

The current schematic migration localizes the power-channel analog functions on the regulator board. This is the intended ownership boundary: Kelvin sensing, telemetry front ends, fast overcurrent protection, feedback selectors, and regulator power stages stay together rather than crossing the board harness.

The coplanar connector strategy is now also narrowed: move the 5 V and 3.3 V range-switch current paths fully onto the regulator board, then use one keyed signal connector family and one separate power connector family rather than reusing the provisional J8/J9 pair.

## 2026-08-18 netlist audit

Fresh netlists now exist for both boards:

- HAT: `DSP-Regulator-HAT-RevC.net`, exported 2026-08-18T06:34:17.
- Regulator: `DSP-Regulator-RevC.net`, exported 2026-08-18T07:51:27.

The exports confirm that the INA2180, TLV1702, TLV9352, and INA3221 devices are all on the regulator board and absent from the HAT. The range-switch MOSFET paths are also local to the regulator board.

The local digital-control contention found in the 06:35 export is closed in the 07:51 export. U15 (74HC595) has been removed, and U18 (AW9523B) is the sole driver for the four range-control nets and both `ISET_MPU_*` nets. U18 is powered from `+3.3V Boot`, has local 100 nF bypassing, shares `I²C SDA_0`/`I²C SCL_0` with U13/U14, and has 10 kOhm pull-ups on reset and interrupt. AD0/AD1 are grounded, selecting address `0x58` per the schematic intent.

Reset-safe range behavior remains a release gate. Each U18 range output connects directly to an LM74502 `EN_UVLO` input with no external bias resistor visible in the netlist. Verify the AW9523 power-on GPIO state together with the LM74502 floating-input behavior, then add explicit safe-state biasing if the combination does not guarantee the intended disabled range state through power-up and reset.

The current regulator connector export is much smaller than the provisional map below:

| Connector | Connected pins in 2026-08-18 netlist |
|---|---|
| J8 Signal | 1=`I²C SDA_0`, 2=`I²C SCL_0`, 3=`GND` |
| J9 Power | 1=`+5V_Boot`, 2=`+3.3V Boot`, 3=`+12V`, 4=`GND` |

The HAT does not yet have matching coplanar harness connectors. Its legacy J12 still carries both `ISET_MPU_*` signals, four orphaned `VSENSE_*` pins, incoming power pins, and multiple grounds. The regulator reads both fault summaries through U18; no dedicated fault or interrupt line currently crosses J8, so the HAT can only observe those states by I2C polling. Decide whether polling is sufficient or whether U18 `~INT` should become a fourth signal pin before freezing J8. The next HAT schematic pass must remove the orphaned analog crossings and implement matching signal and power connectors one-for-one.

## Current circuit ownership

The fresh regulator netlist at `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net` was exported on 2026-08-18T07:51:27 and contains:

- U1/U2 and U17: LM2596S-ADJ regulator stages.
- U4/U5: TLV9352 feedback selectors.
- U9/U10: INA2180A2 dual current-sense amplifiers.
- U11/U12: TLV1702 dual open-collector comparators.
- U13/U14: INA3221 telemetry monitors.
- U18: AW9523B local range/current control and fault-state input.
- J8: provisional control/signal harness connector.
- J9: provisional power harness connector.

The HAT schematic retains the control-side functions, including STM32 U11, W25Q128 storage, CH340C USB-UART, display/control interfaces, and local input-power protection. The previous HAT OCP/current-monitor parts have been removed or replaced in the edited schematic.

## Provisional harness map

The following 2026-08-17 map is retained only as migration history. It is superseded by the 2026-08-18 audit above and must not be used to build a cable.

J8 currently exports the signal contract:

| Pin | Net |
|---:|---|
| 1 | `ISET_MPU_5V` |
| 2 | `ESP- GPIO 5V Low` |
| 3 | `ESP- GPIO 5V Hi` |
| 4 | `VSENSE_5V+` |
| 5 | `VSENSE_3V3+` |
| 6 | `FAULT_CRITICAL_SUM` |
| 7 | `ISET_MPU_3V3` |
| 8 | `ESP- GPIO 3.3V Low` |
| 9 | `ESP- GPIO 3.3V High` |
| 10 | `FAULT_WARNING_SUM` |
| 11 | `I²C SDA_0` |
| 12 | `I²C SCL_0` |

J9 currently exports the power/range-node contract:

| Pin | Net |
|---:|---|
| 1 | `+5V_Boot` |
| 2 | `+3.3V Boot` |
| 3 | `+12V` |
| 4 | `5V_POS` |
| 5 | `5_High` |
| 6 | `5_LOW` |
| 7 | `+5V_reg` |
| 8 | `GND` |
| 9 | `3.3V POS` |
| 10 | `3.3_LOW` |
| 11 | `3.3_HIGH` |
| 12 | Unconnected |

This is a captured work-in-progress map, not a frozen harness contract.

## Validation state and blockers

- The regulator netlist is fresh and confirms the migrated component ownership and provisional J8/J9 connectivity.
- The HAT netlist is fresh and confirms removal of the regulator-side analog ICs, but matching coplanar harness connectors have not been added.
- The checked-in HAT `ERC.rpt` is still dated 2026-08-14 and must not be used to validate the edited HAT schematic.
- A fresh regulator ERC report has not been generated after the migration.
- CLOSED in the 07:51 export: U15/U18 parallel-output contention; U15 is removed and U18 is fully powered and connected to I2C.
- Verify reset-safe AW9523-to-LM74502 behavior; add explicit range-output bias resistors if required.
- KiCad CLI was not available in the current terminal PATH or the standard `C:\Program Files\KiCad` location, so temporary ERC/netlist validation could not be completed from the shell.
- No post-migration PCB DRC result has been captured.
- J8 uses a 3-pin symbol and J9 a 4-pin symbol, but both still specify horizontal `PinHeader_1x20` footprints. Resolve both pin-count/footprint mismatches before connector placement or harness release.
- Connector families, current ratings, wire gauges, keying, retention, and final pin order remain provisional until the new split is implemented.
- J9 currently has only one explicit GND contact. Recalculate outbound and return-current capacity before freezing the power harness; do not assume this return allocation is sufficient.
- The Wi-Fi module and its antenna keepout have not yet been added or validated.

## Electrical rules to preserve

- Keep the INA2180 devices on `+3.3V Boot` or another supply no higher than 5.5V.
- TLV1702 comparators may remain on `+12V`.
- Keep shunt Kelvin pairs, INA2180 inputs, comparator thresholds, and regulator feedback local to the regulator board.
- Do not route raw shunt Kelvin pairs through either harness.
- Keep the Rev-B range-switch bypass installed until the Q1/Q2/Q7/Q8 audit is complete.
- Continue using the external bench-supply current limit for Rev-B work.
- Retain the RB-012 layout and threshold-calculation gate before Rev-C fabrication release.

## Safe restart sequence

1. Open both Rev-C schematics in KiCad and confirm the intended ownership visually.
2. Validate AW9523 power-on/reset output states against LM74502 `EN_UVLO`; add explicit safe-state biasing if needed.
3. Remove orphaned HAT `VSENSE_*` and direct `ISET_MPU_*` connector crossings; retain range switching, current setting, sensing, telemetry, and fast protection locally on the regulator.
4. Replace the provisional J8/J9 scheme with a keyed signal connector and a separate power connector.
5. Size the power connector for rail current plus sufficient return contacts, then decide whether the signal connector remains I2C plus GND or adds AW9523 `~INT` for asynchronous fault notification.
6. Add matching HAT-side connectors and verify every pin one-for-one.
7. Regenerate ERC and netlists for both boards; confirm INA2180, TLV1702, and INA3221 parts exist only on the regulator side.
8. Update the connector-contract verification script for the new coplanar architecture.
9. Finish regulator placement with true Kelvin routing, local INA2180 bypassing, and accessible amplifier/threshold/fault test points.
10. Reserve the HAT board-edge Wi-Fi antenna keepout before detailed control-board placement.
11. Run fresh PCB DRC and a final cross-board connectivity audit before fabrication release.

## Working-tree state

The repository remains intentionally dirty. Modified files at handoff include both Rev-C schematics/projects, the regulator Rev-C PCB, and the fresh regulator netlist. Do not discard or replace these edits during cleanup.

## Suggested restart prompt

Resume from `docs/DEV_STATION_HANDOFF_2026-08-17.md`. Continue the coplanar two-board Rev-C migration. First resolve the J8/J9 symbol-footprint mismatch and freeze a derated signal/power harness contract, then regenerate both ERC reports and netlists before PCB placement continues.