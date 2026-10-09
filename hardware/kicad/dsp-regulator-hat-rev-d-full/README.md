# DSP Regulator HAT Rev-D Full - Initial Integration

**Status: editable PARTIAL INTEGRATION, not a complete full-HAT schematic.**
Created 2026-10-09 for #102 from the reviewed session-4 support project.
The first source-backed circuit integration pass is now present; remaining
bulk circuit edits belong here. This project is not fabrication-ready or
bench-validated; there is no PCB or PCB DRC result.

## Open and edit this project

- `DSP-Regulator-HAT-RevD-Full.kicad_pro` - project setup
- `DSP-Regulator-HAT-RevD-Full.kicad_sch` - root schematic
- `MCU.kicad_sch` - independent, local hierarchical F405 support sheet
- `HAT.kicad_sch` - independent local copy of committed HAT Rev-C reuse
  candidates, with the old Blue Pill and XIAO C3 removed
- `build_kicad/` - gitignored ERC reports and exported netlists

Open the `.kicad_pro` in KiCad 10.0.5, then open its root schematic and enter
the MCU and HAT sheets. Project/instance names and sheet paths are consistent
with this three-sheet hierarchy. The MCU is a real local copy, not a shared
reference to `../dsp-regulator-hat-rev-d/`; all source projects remain separate.

The reviewed support-only project in `../dsp-regulator-hat-rev-d/` and all
Rev-C source files are unchanged. Do not edit those projects when integrating
here. Preserve the frozen pin/support requirements in
[`HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`](../../../docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md).

## Present and intentionally absent

Present: bare STM32F405RGT6 support (power/VCAP, HSE, reset, boot and SWD),
USB 2.0 Type-C/ESD/CC and isolated PA9 VBUS sensing, J4's logical C3
UART/EN/BOOT interface, and 62 retained HAT Rev-C components. J4 is not a
C3 module. Crystal/load and EN candidate/DNP markings remain unchanged.

Source: `../dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch`, not
the uncorrelated production PCB. Added MCU/HAT global interfaces:

| F405 pins | Reused endpoint |
|---|---|
| PA0 / PA1 | J12 ISET 5V / 3V3; each has a new 10k pull-down |
| PA5 / PA6 / PA7 / PA8 | U12 W25Q128 SPI / CS; new 47k SCK/MOSI pull-downs and 10k CS pull-up |
| PB6 / PB7 | U16 CH340 debug TX / RX; each has a new 10k pull-up; not legacy PB6/PB7 connector nets |
| PB10 / PB11 | UDI via R77/R79 to J9; each has a new 10k pull-up |
| PB8 / PB9 | J10/U13 I2C; retains source R69/R70 4.7k pull-ups |
| PB5 | R63/Q9 fan control; R64 changed from 100k DNP to populated 10k |
| PA13 / PA14 / NRST | Existing SWD test points/header plus MCU support header |

The source `+3.3V Boot` rail is now `+3V3`, joining U15's output to the
F405 support rail; duplicate MCU rail/ground PWR_FLAGs were removed. This
is connectivity capture, not proof of regulator headroom for the bare C3.
The legacy WS2812B D12 is retained with DIN tied LOW, not wired to PC13;
the frozen low-current sink LED still needs a replacement circuit.

Absent: AW9523/interrupt and shift-register device (not in this source),
complete measurement/protection and fault/control integration, bare
ESP32-C3-MINI-1U and antenna implementation, C3 local supply/straps/UART0
recovery, and PCB. PA4, PC4 and PC13 remain explicit no-connects pending
those endpoints/replacements. Legacy expansion, sense and fault-label gaps
remain visible; do not invent mappings or treat ERC as protection coverage.

## Integration direction and open gates

User direction (2026-10-09): prior HAT circuits are working reuse candidates,
replaceable as needed. Full Rev-C bench requalification is not a prerequisite
to integration; the untested fan's later bench check is nonblocking. Replace
the old C3 development-board implementation, do not carry it forward.

The supplied 2026-08-28 production Rev-C PCB/Gerber/drill set is the working
manufacturing reference, but exact physical-board and schematic correlation
is unresolved. This starter does not claim a match and does not copy or
modify external OneDrive production files. PR #111 records the mismatch
but was not merged when this starter was created. Keep source identity and
block-level connectivity uncertainties explicit during integration rather
than treating manufacturing outputs or screenshots as a verified schematic.

Still required: integrated pin/net and safe-state review, ISET polarity and
shift-register all-off verification, complete ERC after integration,
crystal/load and C3 EN timing qualification, module/antenna and connector
footprint selection, 3.3 V rail budget, layout/USB/ESD/RF review, PCB DRC,
and Rev-D hardware/bench validation (including powered/unpowered USB VBUS
insertion, oscillator startup, reset/boot, fan and protection behavior).

## Validation and unresolved ERC warnings

Original support-only starter: KiCad CLI 10.0.5 reported 0 errors / 0 warnings
before importing the HAT candidates (31 components, 68 nets).

**Current first pass: KiCad CLI 10.0.5 reports 0 errors / 80 warnings**:
32 footprint-library, 32 symbol-library, 3 symbol/library mismatch and
13 isolated-label warnings. No warning exclusions were added to hide them.
The three mismatches are inherited `1My_Connectors:5015` test points
TP9, TP2_+5V1 and TP_+3.3V1.
Isolated labels are the four VSENSE endpoints, Incoming (+/-), legacy
PB6/PB7/B13/B14/B15/SPI_CS expansion endpoints and `FAULT_WARNING_SUM`.
These are unresolved integration gaps, not routed MCU functions.

Exports contain **102 components, 128 nets**, with all 64 F405 pins accounted
for. Root/MCU/HAT netlist and SVG exports resolve as pages 1/2/3. Pairwise
connectivity checks cover all 215 retained HAT pins except the deliberate
D12 DIN grounding, which is checked separately; support pin topology,
13 frozen interfaces, nine new bias resistors, R64 population, SWD,
rail joins and native USB VBUS isolation were checked. SHA-256 confirms
the original support project and HAT Rev-C files remain byte-identical.
This is not a clean full-HAT release, PCB DRC, fabrication or bench validation.

Re-run from the repository root in PowerShell:

```powershell
$project = "hardware\kicad\dsp-regulator-hat-rev-d-full"
$kicad = "$env:LOCALAPPDATA\Programs\KiCad\10.0\bin\kicad-cli.exe"
& $kicad sch erc "$project\DSP-Regulator-HAT-RevD-Full.kicad_sch" `
  --output "$project\build_kicad\ERC.rpt" --format report `
  --severity-all --exit-code-violations
& $kicad sch export netlist "$project\DSP-Regulator-HAT-RevD-Full.kicad_sch" `
  --output "$project\build_kicad\DSP-Regulator-HAT-RevD-Full.net" `
  --format kicadsexpr
```

The strict ERC command intentionally returns a nonzero status while these
warnings remain; inspect the report rather than interpreting export success
as design acceptance.
