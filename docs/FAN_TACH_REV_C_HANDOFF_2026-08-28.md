# Rev-C Fan Tachometer Feedback Handoff - 2026-08-28

## Purpose

Add fan motor speed feedback to the Rev-C HAT/control board before the revised prototype PCB order is placed. The original design omitted tachometer feedback because the fan interface was simplified to 2-pin power and ground; J7 has now been replaced with a 3-pin header and the tach circuit is wired.

This is a hardware feature change, not a documentation-only change. Do not order the current Rev-C HAT PCB until the tach implementation is reviewed and the fabrication package is regenerated.

## Current baseline

- Fan: GDSTIME 40 x 40 x 10 mm, 12 V, 60 mA nominal, 3-pin, Amazon ASIN `B0DHKJRQL8`.
- Amazon listing evidence: red = 12 V power, black = ground, yellow = tachometer/RPM feedback; rated 12 V, 60 mA, 4500 RPM, and 7 to 13.5 V operating range.
- Fan leads: positive, ground, tachometer.
- HAT fan connector: J7, updated 2026-08-28 to `Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical`, LCSC `C29275` (exact mating method and received header verification still required).
- Fan tach signal: net `Fan_Tach` now wired J7 pin 3 -> R3 pin 2 -> U11 (STM32 Blue Pill) pin 25 (PA0). R3 pin 1 -> `+3.3V Boot` (pull-up). Confirmed via fresh HAT netlist export 2026-08-28.
- Fan power selector: JP1, 3-pad open solder jumper.
  - JP1.1 = `+5V_Boot`
  - JP1.2 = fan positive to J7 pin 2
  - JP1.3 = `+12V`
  - Selected 12 V fan configuration bridges JP1.3 to JP1.2.
- Fan low-side switch: Q9 `AO3400A`, LCSC `C20917`.
  - Fan negative returns through Q9 drain.
  - Q9 source returns to GND.
- Current J7 fan wiring is power, switched return, and tach (J7 pin 3, see above). No longer insulated/unused.
- Active HAT files:
  - `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch`
  - `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_pcb`
  - `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`

## Required design decision

Use the STM32 Blue Pill U11 as the active controller for tach capture unless the schematic/netlist proves that another controller owns the required input. The tach signal should be treated as an open-collector/open-drain pulse output until the exact fan documentation or a received-fan measurement proves otherwise.

Recommended interface concept:

- Fan connector is a 3-position 2.54 mm generic strip header.
- J7 pin 1: fan negative to Q9 drain/switched return.
- J7 pin 2: fan positive from JP1-selected `+12V`.
- J7 pin 3: fan tachometer to a dedicated STM32 GPIO input.
- Add a pull-up to `+3.3V Boot`, not to 12 V.
- Add input protection and edge conditioning only as justified by the selected STM32 pin and tach waveform. A small series resistor and optional RC filter may be appropriate; do not add a capacitor large enough to distort the pulse frequency.
- Do not connect the tach lead to the Q9 drain or to any 12 V node.
- Keep tach return/reference at the fan ground, while preserving the low-side switching topology.

The GPIO, pull-up, connector footprint, and pin order must be verified against the active schematic/netlist and the received connector. The current implementation uses J7.1 switched return, J7.2 fan positive, and J7.3 tach.

## Review tasks

1. DONE (2026-08-28): U11 pin 25 (PA0) selected — free, not used by any other net, has TIM2_CH1 input-capture + EXTI0 capability.
2. DONE (2026-08-28): confirmed via fresh netlist scan — PA0 is not used by SPI flash, display UART, I2C, AW9523 interrupt, SWD, or boot/reset. All other in-use pins were enumerated and PA0 was the only free timer-capable candidate chosen.
3. OPEN: confirm STM32F103 PA0 absolute maximum and VIH/VIL against the 3.3 V pull-up (should be a formality since PA0 is a standard 3.3V-domain GPIO, not 5V-tolerant, and the pull-up is 3.3 V — but not yet checked against the datasheet electrical table).
4. PARTIALLY DONE (2026-08-28): the GDSTIME listing identifies red as 12 V power, black as ground, and yellow as tachometer/RPM feedback; this agrees electrically with J7.1 switched return, J7.2 positive, J7.3 tach. Tach output type, pulses per revolution, max frequency, and open-collector behavior remain bench-measurement items.
5. PARTIALLY DONE (2026-08-28): J7 is now a 3-pin, 2.54 mm generic strip header, LCSC `C29275`, with its datasheet link recorded in the schematic/netlist. OPEN: confirm the received header, mating connector or wire termination method, pin 1 marking, and board-edge clearance.
6. DONE (2026-08-28): schematic updated. Net name is `Fan_Tach` (not `FAN_TACH` — close enough, no functional difference, cosmetic only if renaming is wanted later). Wiring: J7 pin 3 -> R3 (pull-up to `+3.3V Boot`) -> U11 pin 25 (PA0). Confirmed in fresh HAT netlist export.
7. PARTIALLY DONE (2026-08-28): `Fan_Tach` is routed on the HAT PCB from J7.3 through R3 and onward to the U11 PA0 net; the route includes one via near R3. Final layout review must still confirm clearance from Q9 drain, JP1 power selection, switching nodes, inductors, and high-current paths.
8. OPEN: test point for `Fan_Tach` not yet added; add if board space permits.
9. OPEN: firmware not yet started (correctly deferred until this schematic step closed). GPIO is now fixed (PA0); pulse polarity still needs bench confirmation (task 4) before writing capture code.
10. PARTIALLY DONE (2026-08-28): regenerated the HAT netlist, BOM, CPL, ERC, DRC, Gerbers, drill file, and JLCPCB ZIP from the updated schematic and PCB. ERC reports 0 errors; DRC reports 0 unconnected pads and 0 footprint errors, with custom-library resolution warnings.
11. OPEN: upload the regenerated HAT ZIP to JLCPCB and rerun the online DFM review; verify the new 3-pin connector footprint and drill/mask/silkscreen results.
12. DONE (2026-08-28): purchase-review worksheet records the confirmed J7 three-pin mapping. Remaining documentation work is the final connector mating method and received-header verification.

## Acceptance criteria before ordering

- Exact 3-pin board header PN, datasheet, mating housing, and contacts recorded.
- Received fan pin order verified with a meter or manufacturer documentation.
- Tach pin is not connected to 12 V or Q9 drain.
- Tach pull-up is in the 3.3 V domain and has a documented current limit.
- Selected STM32 GPIO is unused by all other active functions and has a documented input role.
- Symbol pinout, footprint pinout, PCB routing, and connector pin table agree.
- ERC has no new electrical errors.
- DRC has no true layout errors or unconnected pads.
- Fresh netlist, BOM, CPL, Gerbers, and drill files are generated from the updated sources.
- JLCPCB online DFM is rerun and has no unreviewed manufacturing changes.
- Prototype firmware has a documented tach test plan, including fan stopped, fan running, missing tach, and unexpected pulse-rate cases.

## Prototype test plan

1. Power the HAT with the bench supply current-limited.
2. Verify the fan positive pin is the selected 12 V node before connecting the fan.
3. Verify the tach pull-up voltage with the fan disconnected.
4. Connect the fan and measure tach waveform amplitude, frequency, and duty cycle at minimum and maximum practical fan operating conditions.
5. Confirm Q9 switching does not corrupt the tach waveform.
6. Confirm firmware reports running, stopped/no-pulse, and plausible speed states.
7. Check tach behavior during startup, fan disable, JP1 open, and controller reset.

## Purchase status

- Current Rev-C HAT fabrication package: **DO NOT ORDER** until the regenerated package passes the JLCPCB online DFM review and the mating termination is selected.
- Existing regulator-board package is unaffected unless the tach signal is intentionally routed across the inter-board signal harness.
- If tach telemetry must reach the regulator board or display path, add it to the connector contract and verify that the signal harness has an available conductor and matching pin on both boards.

## Source references

- [J7/J12 power harness review](J7_J12_POWER_HARNESS_CURRENT_TEST_2026-08-27.md)
- [J8/J9 Rev-C connector handoff](J8_J9_REVC_HANDOFF_2026-08-26.md)
- [STM32 Blue Pill pin table](STM32_BLUEPILL_PIN_TABLE.md)
- [PCB purchase review process](PCB_PURCHASE_REVIEW_PROCESS.md)
