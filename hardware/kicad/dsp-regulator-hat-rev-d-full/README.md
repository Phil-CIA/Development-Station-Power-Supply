# DSP Regulator HAT Rev-D Full - Selected-source integration

**Status: partial integration, not a complete full-HAT schematic.** The
editable HAT sheet was rebuilt on 2026-10-10 from the user's explicitly
selected Rev-C schematic. The design is not a fabrication release or a
bench-validated hardware identity; there is no PCB or PCB DRC result.

## Open and edit this project

- `DSP-Regulator-HAT-RevD-Full.kicad_pro` - project setup
- `DSP-Regulator-HAT-RevD-Full.kicad_sch` - root schematic
- `MCU.kicad_sch` - local hierarchical F405 support sheet
- `HAT.kicad_sch` - HAT circuits rebuilt from the selected Rev-C source
- `build_kicad/` - gitignored KiCad reports and exports

Open the `.kicad_pro` in KiCad 10.0.5. The root, MCU and HAT pages are
hierarchical pages 1, 2 and 3. The existing root schematic and MCU sheet were
left byte-identical. The reviewed support-only project in
`../dsp-regulator-hat-rev-d/` and the Rev-C projects remain separate and
unchanged.

## Selected HAT source

The user selected this exact file as the Rev-D HAT reuse baseline (the
2026-09-21 file):

`C:\Users\forch\OneDrive\JLCPCB files\Development station supply\Regulator Hat\REV C\KiCad Files\DSP-Regulator-HAT-RevC.kicad_sch`

Its SHA-256 is
`34EB41BA12237268CF8E8C640CFA2B76A4710CFF8DE99C9E988C9717D22872B3`.
A fresh KiCad CLI 10.0.5 netlist export contains **57 components and 83
nets**. This is a design-source selection only; it does not establish that
the schematic matches the supplied 2026-08-28 production PCB/Gerbers or any
physical board.

The earlier PR #112 pass used the repository Rev-C schematic (64 components,
105 nets). That source choice and its 0-error / 80-warning ERC result are
superseded and are not current validation evidence.

## HAT rebuild and interface mapping

The rebuilt HAT retains 55 of the selected source's 57 components. The old
Blue Pill U11 and ESP32-C3 development board U2, their no-connects, and
controller-only dangling labels/stubs were removed. No bare C3 module was
added. HAT references R1, R2 and R3 were renamed to R20, R21 and R22 because
the original references collide with the unchanged MCU sheet; their source
values and footprints are unchanged.

Existing F405 global labels were inspected before mapping. The supported
source paths are:

| F405 global interface | F405 pins | Selected-source HAT endpoints |
|---|---|---|
| `SPI_SCK`, `SPI_MISO`, `SPI_MOSI`, `Memory_CS` | PA5, PA6, PA7, PA8 | U12 W25Q128 |
| `UART1_TX`, `UART1_RX` | PB6, PB7 | U16 CH340C |
| `DISP_UART_TX`, `DISP_UART_RX` | PB10, PB11 | R77/R79 and J9 UDI |
| `I²C SCL_0`, `I²C SDA_0` | PB8, PB9 | J10 and U13 AHT20 |
| `FAN_PWM` | PB5 | R63/Q9 fan circuit; all source PB5 labels were normalized to this F405 global name without changing connector pin connectivity |
| `SWDIO`, `SWCLK` | PA13, PA14 | J17 and TP7/TP8 |

Source interface label changes are limited to `SPI SCK` -> `SPI_SCK`,
`SWDCLK` -> `SWCLK`, and `PB5` -> `FAN_PWM`; the source `+3.3V Boot` rail
labels were joined to the MCU's `+3V3` domain. The source J12 connector pin
mapping and all retained source-component pin connectivity remain unchanged.
The legacy WS2812B D12 DIN is grounded because its old controller is removed
and it is not the frozen low-current F405 LED circuit. CH340C CTS/RTS are
explicitly no-connected because the F405 contract uses TX/RX only.

The fan circuit is retained exactly from the selected schematic: J7 is a
3-pin connector; JP1 selects the J7/D11 supply path between `+5V_Boot` and
`+12V`; J7.1 is on the Q9-switched D11 path; J7.3 is a `Fan_Tach` endpoint
pulled up through source R3 (renumbered R22, 10 kΩ), with no F405 tach input.
Source R64 remains 100 kΩ. No fan polarity, supply, connector, resistor value
or population change was made to match the repository copy or frozen pin
target. The fan remains unbench-tested.

## Explicitly absent / unresolved

- The selected source has no ISET signal destinations at J12; its J12 pins
  remain GND, `+5V_Boot`, `+3.3V Boot`, `+12V`, GND, GND. F405 PA0/PA1 have
  their existing local bias circuitry but are not connected to invented HAT
  endpoints.
- The selected source has no shift register or AW9523. PA4/PC4/PC13 and the
  corresponding functions remain unintegrated; no endpoints were imported
  from the repository-only schematic or inferred from PCB content.
- MCU J4 remains a logical C3 interface on the support sheet, not a bare
  ESP32-C3-MINI-1U module implementation. The selected-source HAT UART labels
  are not connected to J4. C3 module power/straps/recovery/RF remain later
  work.
- The fan's source 100 kΩ R64 does not meet the frozen 10 kΩ fan-gate target;
  it was not changed. The pin-contract and safe-state review is still open.
- Remaining isolated legacy connector labels and missing custom library
  definitions are visible ERC warnings, not integrated functions.

## Validation

KiCad CLI 10.0.5 exports the three-sheet project as **95 components and 107
nets**. The comparison against the selected source confirms all 55 retained
source component values/footprints and all **1,056 pairwise source-component
pin connections** are unchanged, except D12 DIN, which is intentionally
grounded. Reference mapping is R1/R2/R3 -> R20/R21/R22. The expected SPI
flash, CH340, UDI, I2C, fan-control and SWD data/clock interfaces were checked
against the actual F405 global labels and netlist. J12 connector pins and the
source fan topology were checked separately.

Full-severity ERC reports **0 errors / 64 warnings**: 28 footprint-library,
28 symbol-library, 3 symbol/library mismatch and 5 isolated-pin-label
warnings. The report is generated with `--severity-all`; no ERC exclusions
were added. The warnings expose missing local library tables and unresolved
single-ended legacy connector labels. ERC/netlist checks are not PCB DRC,
electrical qualification, physical-board correlation, fabrication approval
or bench validation.

The exact source file, the full-project root/MCU files, the support-only
project, both repository Rev-C projects and the external production folder
were hash-checked; only the intended full-project HAT sheet changed among
those KiCad design files. See
[`HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`](../../../docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md)
for current boundaries and remaining release gates.

Re-run from the repository root in PowerShell:

```powershell
$project = "hardware\kicad\dsp-regulator-hat-rev-d-full"
$kicad = "$env:LOCALAPPDATA\Programs\KiCad\10.0\bin\kicad-cli.exe"
& $kicad sch erc "$project\DSP-Regulator-HAT-RevD-Full.kicad_sch" `
  --output "$project\build_kicad\ERC.rpt" --format report --severity-all
& $kicad sch export netlist "$project\DSP-Regulator-HAT-RevD-Full.kicad_sch" `
  --output "$project\build_kicad\DSP-Regulator-HAT-RevD-Full.net" `
  --format kicadsexpr
```
