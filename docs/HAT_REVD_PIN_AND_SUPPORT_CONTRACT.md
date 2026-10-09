# HAT Rev-D F405 Pin and Minimum-Support Contract

**Status:** Draft for schematic implementation and electrical review.
No Rev-D schematic has been created or reviewed yet. This document does not
establish fabrication readiness or change the Rev-C pin contract.

Selected architecture: bare STM32F405RG LQFP64 plus a separate ESP32-C3
Wi-Fi coprocessor. See [controller decision](HAT_CONTROLLER_EVALUATION.md).

## Confirmed interface decisions

- The HAT is supply-powered. Native USB provides PC data and VBUS detection
  only; USB VBUS must not power or backfeed the HAT.
- The STM32-C3 link uses a dedicated UART, separate from UDI and CH340 debug.
- Keep SWD programming/recovery independent of USB and Wi-Fi firmware.
- Continue compatible Rev-C work separately; do not replace its design files.

## Proposed STM32 pin allocation

Physical pad numbers below were checked against the installed KiCad 10
`MCU_ST_STM32F4:STM32F405RGTx` symbol. Alternate functions and electrical
limits still require verification against ST DS8626 before schematic freeze.
Assignments are proposals, not evidence that a net is routed.

| Function | F405 pin | LQFP64 pad | Peripheral / proposal | Migration notes |
|---|---|---|---|---|
| 5V control | PA0 | 14 | GPIO | Preserve logical Rev-C function; confirm safe external bias |
| 3V3 control | PA1 | 15 | GPIO | Preserve logical Rev-C function; confirm safe external bias |
| Legacy CH3 control reservation | PA2 | 16 | GPIO reservation | Confirm actual retained purpose before connecting; not a new adjustable rail |
| Shift-register latch | PA4 | 20 | GPIO | Preserve latch ownership |
| SPI clock | PA5 | 21 | SPI1 SCK, AF5 | Verify against routed SR/flash nets, not just firmware defaults |
| SPI input | PA6 | 22 | SPI1 MISO, AF5 | W25Q128 return; check bus sharing |
| SPI output | PA7 | 23 | SPI1 MOSI, AF5 | W25Q128 / shift-register data; audit transaction/latch isolation |
| W25Q128 CS | PA8 | 41 | GPIO | Pull inactive during reset |
| CH340 debug TX | PB6 | 58 | USART1 TX, AF7 | Moves from PA9 to free native USB VBUS sensing |
| CH340 debug RX | PB7 | 59 | USART1 RX, AF7 | Moves from PA10; displaces Rev-C AW9523 interrupt |
| AW9523 interrupt | PC4 | 24 | GPIO / EXTI4 | Moves from PB7; check polarity, pull-up voltage and interrupt ownership |
| UDI host TX | PB10 | 29 | USART3 TX, AF7 | Preserve display interface |
| UDI host RX | PB11 | 30 | USART3 RX, AF7 | Preserve display interface |
| I2C clock | PB8 | 61 | I2C1 SCL, AF4 | Preserve bus; verify pull-ups and any remap assumptions |
| I2C data | PB9 | 62 | I2C1 SDA, AF4 | Preserve bus |
| Fan control | PB5 | 57 | GPIO / TIM3 CH2, AF2 proposal | Actual PWM requirement and circuit must be verified; no tach assumed |
| C3 link TX | PC10 | 51 | UART4 TX, AF8 | F405 TX -> C3 RX; C3-side pins not selected yet |
| C3 link RX | PC11 | 52 | UART4 RX, AF8 | C3 TX -> F405 RX |
| Native USB VBUS sense | PA9 | 42 | OTG_FS VBUS | Define compliant sensing circuit and unpowered behavior; never connect to HAT power rail |
| Native USB D- | PA11 | 44 | OTG_FS DM, AF10 | Dedicated USB differential pair |
| Native USB D+ | PA12 | 45 | OTG_FS DP, AF10 | Dedicated USB differential pair |
| USB ID reservation | PA10 | 43 | Unused in device-only design | No USB host/OTG role requested |
| SWD data | PA13 | 46 | SWDIO, AF0 | Keep accessible and unshared |
| SWD clock | PA14 | 49 | SWCLK, AF0 | Keep accessible and unshared |
| Optional SWO | PB3 | 55 | Trace, AF0 reservation | Do not allocate before debug decision |
| Status LED | PC13 | 2 | GPIO proposal | Review drive limits; Blue Pill's LED circuit is not present automatically |
| HSE clock | PH0 / PH1 | 5 / 6 | OSC_IN / OSC_OUT | Reserve both until crystal or external-clock circuit is selected |
| Boot configuration | BOOT0 / PB2 | 60 / 28 | Boot straps | Define deterministic normal boot and recovery access |

The C3 control signals are defined below; their F405 GPIO assignments remain
open for Session 2. Do not wire module boot pins by assumption.

## Minimum-support schematic scope

| Circuit block | Required content | Open review item |
|---|---|---|
| Exact MCU | STM32F405RG LQFP64 symbol and matching footprint | Confirm full ordering code/temperature grade and symbol-to-pad mapping |
| Digital power | VDD pads 19, 32, 48, 64; VSS pads 18, 63; local bypass for every supply pin and bulk bypass | Calculate regulator capacity including radio peaks; choose capacitor parts/placement per ST |
| Analog power | VDDA pad 13, VSSA pad 12; bypass/filter network | Verify sequencing and supply requirements; no separate floating analog ground |
| Internal regulator | VCAP_1 pad 31 and VCAP_2 pad 47, each with its required dedicated capacitor to ground | Check ST capacitance/ESR requirements; never tie VCAP to 3.3V or use it to power loads |
| Backup domain | VBAT pad 1 | Define no-battery connection per ST guidance; do not leave floating |
| Clock | PH0/PH1 allocation, HSE source, and firmware PLL configuration | Choose crystal/oscillator and validate USB 48 MHz clock accuracy; crystal load capacitors depend on chosen part/layout |
| Reset/boot | NRST pad 7, BOOT0 pad 60, PB2 pad 28 straps; reset/recovery access | Verify reset defaults keep outputs inactive before firmware starts |
| Programming/debug | SWDIO, SWCLK, NRST, GND, target-voltage reference | Preserve recovery access and prevent external debugger back-powering |
| USB device | D+/D-, VBUS sensing, connector, ESD and shield/ground policy | Connector type and protection parts not selected; supply-powered device must survive cable insertion/removal and HAT power-off |
| UART interfaces | CH340 debug, UDI, dedicated C3 UART | Check voltages, disconnected/unpowered endpoints and reset behavior |
| Control/measurement | Existing GPIO/I2C/SPI/fault interfaces with safe external biases | Reconcile Rev-C source conflicts before copying gate/protection circuitry |

## C3 coprocessor audit and proposed Rev-D contract

The Rev-C schematic and exported netlist were inspected without modifying
either file. U2 is named `ESP32_C3_mini`, but its footprint and part metadata
identify Seeed SKU `113991054` (XIAO ESP32C3), a development board rather than
a bare ESP32-C3-MINI module. The symbol does not expose the board's EN or USB
signals, so this footprint does not support the required STM32-controlled
reset or a schematic-verifiable USB programming path. Select a bare module
with exposed EN and strap pins for Rev-D. The selected module path is a bare
ESP32-C3-MINI-1U with an external antenna, allowing the antenna to be
positioned clear of the HAT's switching power hardware. Confirm the exact
orderable module variant and antenna/connector before layout.

### Rev-C U2 netlist findings

| U2 pad | Symbol signal | Current net | Finding |
|---:|---|---|---|
| 16 | 5V | `+5V_reg` | U2 is powered from 5 V through the XIAO board |
| 15 | GND | GND | Common ground |
| 14 | 3.3V | Explicit no-connect | The XIAO's regulated 3.3 V output is not used |
| 9 | GPIO0 | `RXD_ESP` | Connected to STM32 U11 pad 28 (A3) |
| 10 | GPIO1 | `TXD_ESP` | Connected to STM32 U11 pad 27 (A2) |
| 7 | GPIO20 | `CTS_ESP` | Net has no other endpoint; no working CTS connection |
| 8 | GPIO21 | `RTS_ESP` | Net has no other endpoint; no working RTS connection |
| 11, 12, 13, 1-6 | GPIO2-10 | Explicit no-connects | Nine GPIOs are explicitly unused |

Only GPIO0 and GPIO1 have peer connections to the STM32; GPIO20 and GPIO21
have isolated net stubs, and GPIO2-10 are no-connects. In total, 11 of the
13 symbol GPIOs have no functional external peer. The current netlist does
not provide explicit C3 EN or boot control, supply-decoupling evidence for a
bare module, or an implemented C3 programming/recovery path.

### Power and RF requirements

- Supply the bare module from a regulated 3.3 V rail within its 3.0-3.6 V
  operating range. Size the source for at least 500 mA; the module datasheet
  lists 350 mA peak for 802.11b transmit at 20.5 dBm. Include local 10 uF
  bulk and 0.1 uF high-frequency decoupling at the module supply entry, with
  short return paths. Recalculate the shared 3.3 V rail budget; do not assume
  the Rev-C `+5V_reg`/XIAO regulator arrangement transfers to a bare module.
- For ESP32-C3-MINI-1, preserve the datasheet's marked antenna keep-out:
  no copper, traces, or components beneath the antenna region on any layer;
  observe the keep-out in the recommended land-pattern drawing (datasheet
  Figure 11-1), place the antenna at a board edge, and keep nearby metal
  clear. MINI-1U uses an external antenna connector instead of the integrated
  antenna; the antenna is not included and must be placed clear of metal.
  Validate RF performance in the final HAT/enclosure.
- References: [ESP32-C3-MINI-1 datasheet](https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.html),
  [ESP32-C3 schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/schematic-checklist.html),
  and [Seeed XIAO ESP32C3 documentation](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/).

### UART4 link and STM32-owned controls

Use the dedicated 3.3 V UART4 interface already reserved on the F405. Route
the following signal names. Session 2 has confirmed provisional F405
assignments PC0 and PC1 for the control signals; retain these as provisional
until its complete pin-freeze review is finalized:

| Net name | C3 connection | F405 connection | Direction |
|---|---|---|---|
| `C3_UART_RX` | GPIO0, app UART RX | UART4 TX, PC10 | STM32 -> C3 |
| `C3_UART_TX` | GPIO1, app UART TX | UART4 RX, PC11 | C3 -> STM32 |
| `C3_EN_N` | CHIP_EN | PC0, provisional | Active-low reset/disable, open-drain |
| `C3_BOOT_N` | GPIO9 strap | PC1, provisional | Active-low download-mode request, open-drain |

Use 3.3 V CMOS levels and common ground; no level shifter is needed when both
devices use the same 3.3 V logic rail. A 115200-baud 8-N-1 link is adequate
for command/telemetry framing. Hardware RTS/CTS is not required for this
traffic; leave C3 UART0 GPIO20/21 available for programming and do not carry
the Rev-C `CTS_ESP`/`RTS_ESP` stubs forward as flow-control signals.

Provide 10 kOhm pull-ups from both `C3_EN_N` and `C3_BOOT_N` to C3 3.3 V.
The module datasheet's peripheral reference circuit recommends a 10 kOhm
CHIP_EN pull-up and 1 uF capacitor to ground; the capacitor value and
population remain gated on verification against the exact orderable MINI-1U
variant and the board's power-up behavior. Keep the CHIP_EN trace short.
Allow the 3.3 V rail to stabilize for at least 50 us before enabling the chip;
hold CHIP_EN low for at least 50 us to reset. These controls are open-drain /
low-side so the F405 cannot back-power an unpowered C3. Default both controls
released so the C3 boots normally.

GPIO2, GPIO8, and GPIO9 are boot strapping pins. Pull GPIO2 and GPIO8 high
for deterministic SPI-flash boot and download-mode entry; keep GPIO9 high by
default. To request ROM download mode, hold GPIO9 low while resetting via
CHIP_EN, then keep the strap stable for at least 3 ms after CHIP_EN rises.
GPIO9 is the `C3_BOOT_N` control; GPIO2 and GPIO8 must not be repurposed
without rechecking boot behavior.

### Programming, recovery, and authority boundary

Use a dedicated 3.3 V UART0 recovery header rather than adding another USB
device path. Expose C3 GPIO21/U0TXD (`C3_PROG_TX`, C3-to-adapter RX),
GPIO20/U0RXD (`C3_PROG_RX`, adapter TX-to-C3), GND, a 3.3 V
target-reference pin, `C3_EN_N`, and `C3_BOOT_N`. Connect an external 3.3 V
USB-UART adapter to the UART signals; the reference pin is not a power input.
The boot and reset controls must remain manually accessible at the header so
recovery does not depend on the STM32 firmware running.

The C3 owns local-network telemetry and, subject to a separately reviewed
security/recovery policy, OTA of its own firmware only. It must never drive
a power-stage, rail-enable, current-limit, or protection signal directly.
All power-control requests pass through the STM32's validated command
handling; safety limits, fault handling, and power-stage state remain
STM32-owned. C3 OTA must not write or alter STM32 firmware/configuration or
safety limits. Loss of C3, Wi-Fi, or an update must not interrupt
deterministic STM32 control.

## Work sequence and review gates

1. **Gate 1 source reconciliation completed (2026-10-09):** the current
   Rev-C HAT schematic and its fresh KiCad netlist export are the source for
   HAT connector/control routing. The export matches the committed `.net`
   electrically; the HAT PCB is not reconciled and is not an approved copy
   source for its PCB-only circuitry. `docs/STM32_BLUEPILL_PIN_TABLE.md`
   records the confirmed, conflicting, and unknown Rev-D reuse blocks,
   including the separate Regulator Rev-C source applicable to the power
   stage. Do not copy the old AW9523/Q3/Q9/Q12 screenshot mapping into Rev-D.
   Identify the physical HAT/Regulator revisions and measure or trace the
   ISET, fault, shift-register, fan, and U2 paths before treating the
   unresolved circuits as reusable.
2. Check this complete allocation in ST's LQFP64 pin/alternate-function
   tables, including DMA/timer conflicts if used. Freeze GPIO safety states
   and resolve the clock, C3 module/boot access and USB connector details.
3. Create a separate Rev-D KiCad project and MCU minimum-support sheet.
   Keep Rev-C untouched; do not present a copied PCB as migrated or validated.
4. Export the schematic netlist and inspect every MCU power/support pin,
   UART direction, USB mapping and debug connection. Run ERC and review its
   findings alongside the datasheet; ERC alone cannot validate capacitor
   requirements, USB clock accuracy, reset safety or pin multiplexing.
5. Record review results and remaining gates here before integrating the
   MCU sheet into the full HAT. Full-board ERC/DRC and bench validation remain
   separate fabrication-release gates.

The controller choice and interface decisions are settled. The proposed
allocation and support circuit are **not yet electrically reviewed**.

## Session breakdown

Each session uses its own branch and PR. Each session also has a reusable
VS Code Copilot Chat prompt in `.github/prompts/` (run `/<prompt-name>` in
chat). The same text can serve as a Copilot app session kickoff. Every session
updates this document with its evidence and remaining gates, and leaves the
Rev-C KiCad files unchanged.

| # | Branch | Prompt | Scope | Depends on |
|---|---|---|---|---|
| 1 | `hw/revc-source-reconcile` | `revd-1-revc-source-reconcile` | Reconcile the Rev-C schematic and netlist, including the #62 Q9/Q3/Q12/U4 conflicts | — |
| 2 | `hw/revd-f405-pin-freeze` | `revd-2-f405-pin-freeze` | Verify the pins and alternate functions against ST; freeze clock, boot, USB connector and ESD | — |
| 3 | `hw/revd-c3-coprocessor` | `revd-3-c3-coprocessor` | Audit the C3 module: footprint, power, RF keep-out, UART, reset/boot access | — |
| 4 | `hw/revd-f405-support-sheet` | `revd-4-f405-support-sheet` | Separate Rev-D KiCad project, MCU support sheet, ERC and netlist review | 2, 3 |
| 5 | `hw/revd-hat-integration` | `revd-5-hat-integration` | Full Rev-D HAT integration, ERC and DRC | 1, 4 |
| 6 | `firmware/f405-target` | `revd-6-f405-firmware-target` | F405 PlatformIO target using the frozen pin map | 2 |

Sessions 1–3 can run in parallel. Rev-C bring-up continues independently.
VS Code tasks `Rev-C: KiCad ERC` and `Rev-C: Export netlist` write their
reports to the gitignored `build_kicad/` directory.

## References

- [ST DS8626, STM32F405/407 datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf)
- [ST AN4488, STM32F4 hardware development](https://www.st.com/resource/en/application_note/dm00084117.pdf)
- [Rev-C signal contract](STM32_BLUEPILL_PIN_TABLE.md)
- [UDI contract](DISPLAY_INTERFACE_STANDARD.md)

The installed KiCad symbol verifies pad numbering only, not the legality of
alternate functions or the adequacy of board support circuits.
