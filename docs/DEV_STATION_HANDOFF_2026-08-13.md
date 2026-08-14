# Development Station Power Supply Handoff — 2026-08-13

## Session outcome

The regulator feedback selector investigation moved from diagnosis to a bench-proven Rev-C direction.

### 3.3V channel

Rev-B was reworked and tested with:
- R25 removed from the active feedback path
- 100kΩ pulldown on the remote-sense op-amp input
- upper feedback resistor changed from 1kΩ to 820Ω
- local fallback attenuated enough for the remote path to win

Bench result at approximately 300mA:

| Node | Measurement |
|---|---:|
| Pre-switch rail | 3.3198V |
| Load-side output | 3.3203V |
| Selector remote anode | 3.3201V |
| Selector fallback anode | 3.2204V |
| Selector cathode | 3.0954V |
| LM2596 FB | 1.2391V |

The remote branch won by 99.7mV. This validates the intended op-amp/diode selector with R25 removed. The remaining fallback test only needs to confirm a stable in-range output when remote sense is open; exact fallback voltage is not important.

### 5V channel

The original U5 LMV358-class device was powered from 12V despite being a low-voltage part. The input supply current reached the 400mA bench limit. Removing U5 reduced total powered-system current to approximately 71mA with five LEDs lit and STM32-to-CrowPanel communication active.

Temporary Rev-B state:
- U5 removed
- `+5V_Reg` jumpered to `VSENSE_5V+`
- feedback resistor changed to 1.8kΩ
- measured output = 4.848V

The lower temporary voltage is expected because the jumper bypasses the selector diode drop.

Rev-C direction:
- use TLV9352 for both fixed-rail selector op-amps
- power the selectors from 12V
- add 100kΩ remote-input pulldowns
- use 820Ω for the 3.3V corrected divider leg
- use 1.8kΩ for the 5V corrected divider leg
- retain direct-bypass footprints as DNP service recovery only

## Rev-C netlist status

Latest regulator netlist export reviewed:
- U5 functional value changed to TLV9352 and powered from 12V
- D7 now combines U5A and U5B correctly
- 100kΩ remote-sense pulldowns are present
- 820Ω and 1.8kΩ divider corrections are present

Still required before routing:
1. Replace inherited LMV358 metadata on U5 with TLV9352IDR metadata and datasheet.
2. Clean the corresponding stale metadata on the other TLV9352 symbol(s).
3. Verify selector reference designators after final annotation.
4. Mark service bypasses DNP explicitly.
5. Run fresh ERC, export a new netlist, and repeat the connectivity audit.
6. Bench-test the 5V selector after TLV9352 parts arrive.

## Next HAT-board bench target

Investigate the current-range MOSFET network that is presently bypassed by jumpers (`+5V_Reg` to R19 and `+3.3V_Reg` to R17). Firmware command and shift-register communication already pass; the unresolved problem is the switching hardware.

Start with the 3.3V range path around Q1/Q2/Q7/Q8:

1. With power removed, reconcile symbol pins, footprint pads, body-diode orientation, and the actual fitted part numbers.
2. Record resistance/diode-mode measurements across each MOSFET before reinstalling any removed device.
3. Identify the exact source node for each device; do not infer source from schematic drawing orientation.
4. Power through the bench current limit with the existing bypass retained.
5. Run `D9OFF`, `D9ON`, and `D9FLASH` and measure gate, source, drain, and Vgs for every commanded state.
6. Pass requires a clearly enhanced ON state and a clearly non-conducting OFF state with no intermediate-node hang near the previously observed 1.96–2.1V.
7. Use those measurements to choose the Rev-C topology: back-to-back PMOS high-side switching, NMOS plus a proper high-side gate driver, or permanent range bypass.

Do not remove the working bypass or reconnect the load through the range MOSFETs until the pinout audit and Vgs table are complete.

## Newly confirmed Rev-C OCP issue

The U3 TLV1704 stage does not directly measure the millivolt differential across R16 or R17. Each active channel receives only the upstream shunt terminal on its positive input and a ground-referenced potentiometer threshold on its negative input. It therefore compares approximately `VOUT + ILOAD * RSHUNT` against the pot setting.

At the measured approximately 0.339A load, the useful signal is only about 6.78mV across R16 (20mΩ) or 67.8mV across R17 (200mΩ), even though the U3 positive input is at the approximately 3.3V rail common-mode voltage. A 100mV output change would move the inferred trip point by 5A on R16 or 0.5A on R17.

Rev-C must use Kelvin connections from both shunt terminals and a current-sense/differential front end before the comparator. Treat the current U3 path as unvalidated protection and keep using the bench-supply current limit during Rev-B tests. Full analysis and redesign requirements are recorded as RB-012 in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`.

## Safe restart state

- Regulator/HAT stack can continue operating in range-switch bypass mode.
- 3.3V regulator selector rework is bench-proven in normal remote-sense operation.
- 5V remains in temporary direct-feedback bypass until TLV9352 parts arrive.
- U3 absolute-voltage OCP sensing is not accepted as calibrated protection; RB-012 requires differential shunt sensing in Rev-C.
- Do not populate an LMV358/LMV358-class low-voltage part on a 12V selector supply.

## Suggested restart prompt

Resume from `docs/DEV_STATION_HANDOFF_2026-08-13.md`. Keep the regulator in its current proven bypass states and investigate the HAT 3.3V current-range MOSFET path first. Reconcile Q1/Q2/Q7/Q8 symbol-to-footprint pinout and capture gate/source/drain/Vgs for D9OFF, D9ON, and D9FLASH before proposing the Rev-C switch topology.
