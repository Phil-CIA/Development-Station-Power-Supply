# HAT Controller Decision and Rev-D Migration (#102)

## Current reconciliation and retirement handoff (2026-10-16)

The October 15 decisions recorded in
[#102](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/102#issuecomment-6191752950)
and implementation tracker
[#126](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/126)
supersede this session's October 13-14 proposals:

| Topic | Current user decision / evidence boundary |
|---|---|
| Controller and radio | Bare STM32F405RG LQFP64 plus the actual Teyleten Robot ESP32-C3 SuperMini, not a bare MINI-1U or XIAO. |
| USB / console | Native F405 USB CDC replaces CH340. ST-Link remains for initial programming, debug, and independent recovery. USB is no longer deferred. |
| UART4 | Custom F405 bootloader transport: PC10 TX to SuperMini GPIO0 RX; PC11 RX from GPIO1 TX. This is not an application-only UART or an assumed factory-ROM loader. |
| Other radio signals | PB6 to C3 GPIO5, PB7 to GPIO6, and PD2 to GPIO7 are spare signals. PC0/PC1 reset/boot controls are not required now. |
| LEDs | No additional physical LEDs. Existing HAT/regulator/SuperMini indication policy is tracked by [#125](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/125); do not add PC13 LED circuitry from the earlier ledger by assumption. |
| Editing workflow | The user manually maintains the readable single-sheet `My_Edits` schematic. Agents review and provide manual instructions; no automatic schematic/PCB reconstruction or edits are authorized here. |
| Latest reported design evidence | #126 records the root netlist exported 2026-10-15 at 06:54:17 and ERC at 06:54:30, 0 errors / 24 warnings, with corrected UART endpoints. These are user-provided VS Code reports, not files independently inspected in this worktree. |
| Remaining gates | Preserve the actual schematic in a reviewed PR; finish SuperMini footprint/pad/mechanical checks, electrical qualification, firmware pin/loader alignment, PCB/DRC, and bench evidence under #126. Do not close #102 or #126 based on this preservation work. |

**Preservation scope:** the complete earlier session state is retained in
commit `dda4f1b`. The reconciled branch tip restores all three firmware files
to the session's original committed baseline: the earlier USB-disable,
candidate-UART gating, and output-preload edits are historical only, not
changes proposed for merging. No firmware implementation, build, flash,
KiCad edit, or hardware operation is part of this retirement pass. Restoring
the baseline does not mean that baseline matches the new schematic.

The unique review aid is [`F405_PAD_LEDGER.md`](F405_PAD_LEDGER.md): a complete
package-pad checklist and historical Blue Pill migration comparison. Its
old assignments (especially CH340 PB6/PB7, PC0/PC1, PC13, and USB/radio scope)
must not override #126. The local bare-MINI datasheet is retained as a
reference, not evidence for the actual SuperMini board.

**Resume in the migration audit / SuperMini verification workstreams**, not
by restarting the obsolete recommendations below. Preserve the user's live
source and its identity there, then reconcile the actual netlist and pin
ownership. This session cannot archive itself; archive it from the app only
after confirming the preservation PR and branch are available remotely.

## Historical October 13-14 evaluation (superseded)

Everything below records the earlier session state, including its then-open
questions and recommendations. It is retained for review/history, not as the
current design contract or authorization to edit.

**Historical status (2026-10-13):** STM32F405RG remains the selected and locked
Rev-D controller. Native USB is deferred to a later HAT revision. Wi-Fi is
likely, but no coprocessor/module or interface has been selected. The current
F405 firmware target keeps native USB CDC and the candidate C3 UART disabled
by default; CH340 USART1 remains the debug console. These user-directed
decisions supersede earlier C3/USB architecture assumptions below.

Schematic/layout migration and verification remain open; Rev-C files are not
changed by this decision. The reviewed F405 support project is preserved; use
the separate editable integration starter in
`hardware/kicad/dsp-regulator-hat-rev-d-full/` for full-HAT work. Its HAT
sheet was rebuilt from a previous schematic selection and is still partial;
it does not incorporate the user's separately provided schematic
interpretation. Make the user-provided source available for review before
KiCad integration. Preserve it and ask the user before any KiCad edits.

This decision applies to the future HAT revision, referred to here as
Rev-D. Continue Rev-C bring-up and firmware work separately.

## Confirmed future requirements

| Function | Current direction | Boundary |
|---|---|---|
| PC display link | Native USB deferred | The existing CH340C/USART debug path remains; do not treat it as a selected PC telemetry interface. |
| Wi-Fi | Likely; preliminary hardware recommendation below, not yet a locked BOM/interface | Do not assume ESP32-C3, OTA, provisioning, or remote-control behavior is approved. |
| Power control | Preserve a deterministic, safe control path | USB, Wi-Fi, and update activity must not compromise safe output states, hardware protection, or fault reporting. |

Native USB, the Wi-Fi coprocessor/interface, and any network/update behavior
remain outside the current F405 firmware scope until selected. The exact PC
telemetry schema, update security/recovery policy, and provisioning behavior
remain open for a later revision.

### Preliminary Wi-Fi hardware recommendation

To make the next schematic pass actionable without silently treating an
earlier candidate as selected, use this as the proposed baseline for review:

- Use a soldered **ESP32-C3-MINI-1U-H4X** module (4 MB flash, extended
  temperature grade) with an external antenna. The external antenna gives
  placement flexibility around the HAT's switching power hardware; keep the
  antenna feed/connector at the board edge and follow Espressif's RF layout
  guidance. Verify exact orderable part and antenna before BOM freeze.
- Provide a dedicated local 3.3 V radio supply path with at least 500 mA
  available for the module, subject to final regulator, thermal, and
  transient-response calculations. The module datasheet reports a 350 mA
  peak Wi-Fi transmit condition. Do not assume the existing logic rail has
  this headroom without checking its source and loads.
- Reserve F405 UART4 PC10/PC11 for an application UART only if the final
  schematic confirms those pins are free. Keep the C3's UART0 GPIO20/GPIO21
  available on a separate recovery/programming header. If the user's J4
  does not already have another confirmed role, use it for a C3 recovery
  header with UART0 TX/RX, GND, 3.3 V target reference (not a power input),
  EN, GPIO8, and GPIO9.
- Provide accessible module reset and complete boot recovery. GPIO8 and GPIO9
  are strap pins: ROM joint-download mode requires GPIO8 high and GPIO9 low
  when reset is released. A single GPIO9 control is insufficient. Include
  GPIO2/GPIO8/GPIO9 default bias and expose GPIO8/GPIO9 plus EN at the
  recovery interface; do not rely on the F405 running to recover the radio.
- Keep the C3 limited to communications. It may request operations through a
  validated STM32 interface but cannot directly control rails, limits,
  protection, or the safety state.

This recommendation is grounded in the local
[ESP32-C3-MINI datasheet notes](datasheets/parts/ESP32-C3-MINI-1/NOTES.md).
It is not approval to edit the KiCad schematic. The actual user schematic,
board outline, antenna location, source-rail capacity, and J4 role are still
needed to confirm or reject the recommendation.

### Clock, programming header, and reset-switch recommendations

- **Y1:** retain an external 8 MHz HSE crystal as the starting board
  recommendation because the documented F405 clock plan is based on that
  reference. Select an orderable crystal and calculate the two load capacitors
  from its specified load capacitance and the final PCB parasitics; the
  support sheet's 10 pF values are unverified and DNP. The F405 can use its
  internal oscillator, but that is a different clock design and requires
  confirming timing/baud-rate margins before removing Y1.
- **J3:** honor the user's JTAG-header identification. Reserve the full
  JTAG-capable pads (PA13/TMS, PA14/TCK, PA15/TDI, PB3/TDO, PB4/NJTRST) and
  shared NRST, GND, and target-voltage reference. SWD can use PA13/PA14 on
  that interface. Confirm J3's connector pin numbering and footprint against
  the user's schematic; the separate support sheet's J1/SWD and J3/USB
  designators are not authoritative.
- **S1:** recommend a normally-open momentary reset switch from NRST to GND,
  independent of the programming header. The support sheet depicts this
  behavior under SW1, but do not assume the user's S1 has that function
  until its nets are traced.

## Selected Rev-D architecture

The current controller decision is:

- **STM32F405RG** as the on-board, bare LQFP64 control MCU. It replaces the
  STM32F103C8T6 Blue Pill development board used by the current firmware
  bring-up setup and remains the deterministic real-time/control and safety
  authority.
- Wi-Fi is expected to remain a separate communications function, but its
  module, UART/pin assignment, protocol, and responsibilities are unselected.
- Native USB CDC is deferred; the existing CH340 USART1 debug path is not
  redefined as a telemetry link.
- Any future communications coprocessor must not own time-critical rail
  control or bypass STM32 safety/fault handling.

The STM32F405RG selection is final. The F405 control-pin allocation and
minimum-support targets are recorded in the
[Rev-D pin and support contract](HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md).
Its C3 and native-USB allocations are retained as unselected candidates, not
active interfaces. The user's separately supplied schematic interpretation
must be reviewed before KiCad integration; no schematic edits are authorized
without asking the user.

The STM32F405RG is an LQFP64 device, not a Blue Pill module. Rev-D must
provide the MCU's required board-level power, clock/reset/boot, programming,
and debug support. The existing Blue Pill is the current Rev-C bring-up
controller and is not the Rev-D implementation.

## Rev-C work continues in parallel

Continue Rev-C firmware development, integration, and bench work wherever
possible while Rev-D migration proceeds. Issue #102 is not a
hold on the current revision and does not replace its remaining firmware and
validation work. Keep changes compatible with Rev-C hardware; when a feature
cannot reasonably fit or operate on the current controller, record that
constraint and defer only that feature or implementation to the future
revision rather than stopping unrelated Rev-C progress.

## Existing controller baseline

| Item | Current evidence |
|---|---|
| Controller | STM32F103C8T6 Blue Pill, 72 MHz; guaranteed 64 KiB program flash and 20 KiB SRAM |
| Firmware links | USART1 to CH340 debug, USART3 to UDI display, I2C on PB8/PB9, SPI external flash, AW9523 interrupt on PB7; see `docs/STM32_BLUEPILL_PIN_TABLE.md` |
| External memory | W25Q128 is SPI data flash for tests/configuration; it is not executable MCU flash or working SRAM |
| Flash constraint | The Rev-C firmware build exceeds the F103C8's 65,536-byte program-flash capacity, and more features remain to be developed. This is sufficient to rule out the F103C8 for the complete planned firmware; the small variation between reported build sizes does not change that decision. |

The remaining Rev-C firmware and validation work is tracked in
`docs/FIRMWARE_DEVELOPMENT_PLAN.md`. Size the F405 firmware for that remaining
work and additional planned features, with an explicit growth reserve; do not
infer the full requirement from the present build alone.

## Rev-D hardware-first execution path

**Primary deliverable:** complete a fabrication-ready Rev-D HAT schematic and
PCB with a bare STM32F405RG on the board, then order/build that PCB for
bring-up. The F405 was selected because the F103C8 is constrained by firmware
capacity; compiling the existing F405 target is supporting evidence, not the
main outcome. Do not spend the critical path on further firmware optimization.

The existing
`hardware/kicad/dsp-regulator-hat-rev-d-full/` project is a partial integration
starter only. Its earlier agent-created schematic is not the design authority
for the next integration pass. The user created
`DSP-Regulator-HAT-Rev_D_My_Edits.kicad_sch` as their own interpretation of
the signal flow, but that file is not available in this worktree. Do not
access the main checkout or silently substitute the starter. Make the user's
schematic available inside the active worktree before using it. Preserve its
original contents and ask the user before making any KiCad edits.

### Review handoff and stopping point (2026-10-14)

**Paused before schematic editing.** The next work package is source-specific
review, not more firmware optimization or immediate PCB layout.

| Item | Handoff state |
|---|---|
| Confirmed direction | Bare STM32F405RG LQFP64; hardware-first HAT PCB; native USB deferred; F405 retains control/safety authority. |
| Review artifacts | `docs/F405_PAD_LEDGER.md` contains a preliminary 64-pad ledger and Rev-C signal migration matrix. The pin/support contract and local C3 datasheet notes are the supporting references. They do not prove connectivity in the user's schematic. |
| Recommendations, not approved design changes | External 8 MHz HSE baseline, NRST reset-switch behavior, full JTAG-capable pad reservations following the J3 callout, and ESP32-C3-MINI-1U-H4X with a separately budgeted supply and independent recovery interface. Neither the radio BOM nor the exact J3/J4/S1/Y1 circuitry is locked by this review. |
| Source blocker | `DSP-Regulator-HAT-Rev_D_My_Edits.kicad_sch` is not available in this active worktree. Do not read the main checkout or substitute the partial integration starter. |
| Repository state at handoff | Changes remain uncommitted on `phil-cia-f405-hat-upgrade-evaluation`: README, firmware plan, controller tracker, pin/support contract, pad ledger, datasheet index/C3 reference folder, and the earlier supporting firmware changes in `platformio.ini`, `board_pins_f405rg.h`, and `main.cpp`. No KiCad source was changed. |
| Evidence boundary | Preliminary pad coverage and documentation checks only for the hardware review. Earlier firmware build evidence is recorded separately; no F405 PCB, flashing, fabrication, or bench validation exists. |

Resume in this order:

1. Have the user provide an unchanged copy of their schematic and any
   referenced child sheets/project-local libraries inside the active
   worktree. Record the source path and SHA-256 before review; retain the
   untouched copy.
2. Export its netlist read-only and trace each former Blue Pill signal to its
   F405 pad and endpoint. Review J3's actual pinout/debug mode, J4 and related
   connectors, Y1 and its load network, S1, bare-MCU supply/VCAP/boot support,
   and the regulator-board AW9523/D9 versus HAT fan paths.
3. Present one bounded change list: confirmed connectivity, corrections
   needed, and decisions to settle for the radio supply/module/antenna,
   application UART, straps, and independent programming/recovery.
4. Obtain explicit approval of that change list before editing KiCad.
   Apply approved edits to a working copy, then reconcile the full netlist
   and ERC against the ledger before beginning PCB layout.

### Hardware release sequence

1. **Use the user's design intent:** review the supplied schematic and explain
   its net/signal flow against the frozen F405 contract. Resolve source
   mismatches before editing: F405 board support, CH340 USART1 on PB6/PB7,
   UDI USART3 on PB10/PB11, SPI W25Q128, I2C wiring, and the AW9523's actual
   location on the regulator board. Use the preliminary
   [`F405_PAD_LEDGER.md`](F405_PAD_LEDGER.md) as a checklist, not as proof of
   the user's schematic connectivity. Specifically reconcile J3 (user-
   identified JTAG), J4, S1, and Y1
   from the staged source. Do not conflate D9 (the yellow LED on AW9523 P1.0)
   with fan control (the separately disputed PB5/Q9/J7 path).
2. **Complete the HAT schematic:** retain the selected F405 LQFP64 and its
   required power, VCAP, clock, reset, boot, JTAG/SWD, decoupling, and
   programming circuits. Include only confirmed HAT/regulator interconnects
   and functions.
   Native USB is deferred; Wi-Fi is likely but its module and interface are
   unselected. Do not add USB/C3 circuitry as assumed requirements.
3. **Schematic gate:** produce a complete netlist, run ERC, reconcile all
   warnings affecting electrical connectivity, check symbol/footprint
   assignments and pin/net contracts, and record unresolved non-blocking
   items. The prior partial project's 95-component/107-net, 0-error/64-warning
   export is not evidence that the user's design is complete or release-ready.
4. **Create the PCB:** assign and verify footprints, establish board outline,
   connector/mechanical locations and assembly constraints, place the F405
   support components and interface circuits, then route the HAT. Run DRC,
   inspect critical nets and copper/clearance/return paths, and generate
   fabrication outputs only after review.
5. **Order and assemble a prototype:** after the user approves fabrication
   release, order and build the board populated with the F405. Preserve
   JTAG/SWD, serial recovery and safe-state access.
6. **First-board bring-up:** separately validate power/clock/reset/boot,
   programming, safe GPIO defaults, CH340 console, UDI route, I2C devices,
   SPI flash, fault/control paths, and recovery with recorded bench evidence.
   Do not treat host builds or prior operator overrides as bench passes.

Firmware work stays supporting and minimal until the F405 PCB exists: keep the
buildable target and use builds only to check compatibility with the selected
pin map. Runtime stack/heap headroom and physical behavior remain unmeasured.
No fabrication, hardware, or bench validation has been performed in this
session.
