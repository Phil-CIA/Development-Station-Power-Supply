# DSP Regulator HAT Rev-D Full - Integration Starter

**Status: editable INTEGRATION STARTER, not a complete full-HAT schematic.**
Created 2026-10-09 for #102 from the reviewed session-4 support project.
The user will perform bulk circuit integration here. This project is not
fabrication-ready or bench-validated; there is no PCB or PCB DRC result.

## Open and edit this project

- `DSP-Regulator-HAT-RevD-Full.kicad_pro` - project setup
- `DSP-Regulator-HAT-RevD-Full.kicad_sch` - root schematic
- `MCU.kicad_sch` - independent, local hierarchical F405 support sheet
- `build_kicad/` - gitignored ERC reports and exported netlists

Open the `.kicad_pro` in KiCad 10.0.5, then open its root schematic and enter
the MCU sheet. Project/instance names were changed consistently; sheet and
symbol UUIDs and annotation were retained from the source within this
separate project. `MCU.kicad_sch` is a real local copy, not a shared reference
to `../dsp-regulator-hat-rev-d/`.

The reviewed support-only project in `../dsp-regulator-hat-rev-d/` and all
Rev-C source files are unchanged. Do not edit those projects when integrating
here. Preserve the frozen pin/support requirements in
[`HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`](../../../docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md).

## Present and intentionally absent

Present: bare STM32F405RGT6 support (power/VCAP, HSE, reset, boot and SWD),
USB 2.0 Type-C/ESD/CC and isolated PA9 VBUS sensing, and J4's logical C3
UART/EN/BOOT interface. J4 is not a C3 module. The copied MCU sheet retains
its session-4 title and candidate/DNP markings for traceability.

Absent: reused full-HAT measurement/protection/control/peripheral circuits,
their connections and reset-safe biases, the bare ESP32-C3-MINI-1U module
and antenna implementation, C3 local supply/straps/UART0 recovery, and any
PCB. Unallocated F405 pins remain no-connects from the support-only input;
remove the appropriate markers as the frozen interfaces are implemented.
Avoid reference-designator collisions when importing blocks.

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

## Starter validation

KiCad CLI **10.0.5: 0 errors, 0 warnings** for the copied support circuitry
only. Netlist export and hierarchy inspection confirm the local MCU sheet
loads and preserves the support project's components and pin/net topology:
31 components, 68 nets and all 64 F405 pins accounted for.
SHA-256 comparison confirms the original support project and HAT Rev-C
files stayed byte-identical. This is not full-HAT ERC, PCB DRC, fabrication
or hardware validation.

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
