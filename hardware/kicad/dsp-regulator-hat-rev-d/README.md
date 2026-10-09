# DSP Regulator HAT Rev-D F405 Support Sheet

Status: session-4 schematic capture and initial contract review complete.
This is a separate, hierarchical KiCad project containing the F405
minimum-support circuitry and external C3 interface. It has no PCB and is not
a fabrication release. Rev-C schematic and PCB files were not copied or
modified.

## Project files

- `DSP-Regulator-HAT-RevD-Support.kicad_pro` — project
- `DSP-Regulator-HAT-RevD-Support.kicad_sch` — root sheet
- `MCU.kicad_sch` — hierarchical F405 and support sheet
- `build_kicad/` — gitignored ERC report and netlist exports

## Captured circuits

- `MCU_ST_STM32F4:STM32F405RGTx` symbol with the frozen LQFP64 allocation;
  exact schematic Value is `STM32F405RGT6`.
- Four VDD-pin 100 nF bypass capacitors, local 4.7 µF bulk, VDDA 0 Ω link
  and 100 nF/1 µF bypass, VBAT-to-3.3 V tie, common VSS/VSSA ground, and one
  2.2 µF low-ESR capacitor at each VCAP pin.
- HSE crystal, NRST pull-up/capacitor/reset switch, BOOT0 and PB2 pull-downs,
  and BOOT0 recovery jumper.
- SWD-only ARM 10-pin header interface with SWDIO, SWCLK, NRST, target
  3.3 V reference, and GND. The exact keyed connector footprint is not chosen.
- USB 2.0 Type-C receptacle interface, 5.1 kΩ CC resistors, USBLC6-2SC6 ESD
  protection, and isolated PA9 VBUS sense divider. VBUS does not connect to
  a HAT supply rail. Verify the selected GCT footprint against the orderable
  USB4105-GF-A variant before layout.
- External C3 interface connector J4:

  | J4 pin | Net | F405 connection | C3-side meaning |
  |---:|---|---|---|
  | 1 | `+3V3` | Shared logic rail | C3 supply reference |
  | 2 | `GND` | Common ground | C3 ground |
  | 3 | `C3_UART_RX` | PC10 / UART4 TX | GPIO0 / application UART RX |
  | 4 | `C3_UART_TX` | PC11 / UART4 RX | GPIO1 / application UART TX |
  | 5 | `C3_EN_N` | PC0, open-drain control | CHIP_EN / active-low reset |
  | 6 | `C3_BOOT_N` | PC1, open-drain control | GPIO9 / active-low boot request |

Unallocated F405 pins are marked no-connect in this minimum-support project;
the frozen assignments remain requirements for full-HAT integration.

## Unverified values and integration boundary

The 8.000 MHz crystal and both 10 pF C0G/NP0 load-capacitor Value fields say
`candidate; UNVERIFIED`. The load capacitors are DNP until the selected
crystal's datasheet, total board parasitics, load calculation, and oscillator
startup are reviewed. The optional 1 µF C3 CHIP_EN capacitor is likewise
marked `candidate; UNVERIFIED` and DNP pending exact module and rail-ramp
review. These markings are intentional; do not treat them as released BOM
values.

J4 is a logical module interface, not an ESP32-C3-MINI-1U module footprint.
The C3's local bypassing, GPIO2/GPIO8 boot-strapping, UART0 recovery header,
exact module/antenna selection, RF keep-out, and antenna validation remain
outside this F405 support sheet. The SWD header's exact keyed footprint is
also pending.

## Validation

KiCad CLI 10.0.5 ERC result: **0 errors, 0 warnings**. The exported netlist
was checked against all 64 F405 pins and the contract's power, clock, reset,
boot, C3 UART/control, USB D+/D−/CC/VBUS, and SWD mappings. ERC and netlist
results do not establish PCB routing, USB signal integrity, ESD placement,
clock startup, VBUS behavior under powered/unpowered insertion, or bench
readiness.

Re-run from the repository root in PowerShell:

```powershell
$project = "hardware\kicad\dsp-regulator-hat-rev-d"
$kicad = "$env:LOCALAPPDATA\Programs\KiCad\10.0\bin\kicad-cli.exe"
& $kicad sch erc "$project\DSP-Regulator-HAT-RevD-Support.kicad_sch" `
  --output "$project\build_kicad\ERC.rpt" --format report --severity-all
& $kicad sch export netlist "$project\DSP-Regulator-HAT-RevD-Support.kicad_sch" `
  --output "$project\build_kicad\DSP-Regulator-HAT-RevD-Support.net" `
  --format kicadsexpr
```
