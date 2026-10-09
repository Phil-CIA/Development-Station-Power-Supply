# HAT Rev-D F405 Pin and Minimum-Support Contract

**Status:** F405RG pin allocation and minimum-support values are frozen for
schematic implementation. Session 4 captured a separate MCU support-sheet
project; KiCad ERC reports 0 errors and 0 warnings, and the exported netlist
passed a pin-by-pin contract review. The sheet is not routed or fabrication
ready and does not change the Rev-C pin contract. Session 3 confirmed the
PC0/PC1 C3 control assignments for the bare ESP32-C3-MINI-1U external-antenna
module; exact ordering code, antenna, and electrical implementation still
require schematic/layout verification.

Selected architecture: bare STM32F405RG LQFP64 plus a separate ESP32-C3
Wi-Fi coprocessor. See [controller decision](HAT_CONTROLLER_EVALUATION.md).

## Confirmed interface decisions

- The HAT is supply-powered. Native USB provides PC data and VBUS detection
  only; USB VBUS must not power or backfeed the HAT.
- The STM32-C3 link uses a dedicated UART, separate from UDI and CH340 debug.
- Keep SWD programming/recovery independent of USB and Wi-Fi firmware.
- Continue compatible Rev-C work separately; do not replace its design files.

## Frozen STM32 pin allocation

LQFP64 pin numbers were cross-checked against ST DS8626 Rev. 9, Table 7.
Alternate functions were checked against Table 9; citations below apply to
each allocation row. `DS-PIN` means Table 7, pp. 47–58; `DS-AF` means Table 9,
pp. 62–71. The allocation is frozen as a design contract, not evidence that
any Rev-D net is routed.

| Function | F405 pin | LQFP64 pin | Peripheral / mode | Reset/boot safe state and external bias | Evidence |
|---|---|---:|---|---|---|
| 5V control | PA0 | 14 | GPIO | High-Z during reset; 10 kΩ pull-down holds LOW/inactive. Firmware must preload LOW before output mode. | DS-PIN |
| 3V3 control | PA1 | 15 | GPIO | High-Z during reset; 10 kΩ pull-down holds LOW/inactive. Firmware must preload LOW before output mode. | DS-PIN |
| Legacy CH3 control reservation | PA2 | 16 | GPIO, reserved | Keep unconnected unless its retained purpose is confirmed; if connected as a control, 10 kΩ pull-down and LOW/inactive startup. Not a new adjustable rail. | DS-PIN |
| Shift-register latch | PA4 | 20 | GPIO; SPI1 NSS unused | Reset LOW; 10 kΩ pull-down. Load the all-off shift-register word before the first latch pulse. | DS-PIN; DS-AF (PA4/NSS) |
| SPI clock | PA5 | 21 | SPI1 SCK, AF5 | Reset Hi-Z; 47 kΩ pull-down gives SPI mode 0 idle LOW. | DS-PIN; DS-AF (AF5) |
| SPI input | PA6 | 22 | SPI1 MISO, AF5 | Input during reset; no external bias required. | DS-PIN; DS-AF (AF5) |
| SPI output | PA7 | 23 | SPI1 MOSI, AF5 | Reset Hi-Z; 47 kΩ pull-down keeps the line LOW until selected. | DS-PIN; DS-AF (AF5) |
| W25Q128 chip select | PA8 | 41 | GPIO | Reset HIGH/inactive; 10 kΩ pull-up to 3.3 V. Firmware preloads HIGH before SPI starts. | DS-PIN |
| CH340 debug TX | PB6 | 58 | USART1 TX, AF7 | UART idle HIGH; 10 kΩ pull-up. | DS-PIN; DS-AF (AF7) |
| CH340 debug RX | PB7 | 59 | USART1 RX, AF7 | Input idle HIGH; 10 kΩ pull-up. This displaces the Rev-C AW9523 interrupt from PB7. | DS-PIN; DS-AF (AF7) |
| AW9523 interrupt | PC4 | 24 | GPIO / EXTI4 | Input; 10 kΩ pull-up to the AW9523 logic rail for its active-low open-drain interrupt. | DS-PIN |
| UDI host TX | PB10 | 29 | USART3 TX, AF7 | UART idle HIGH; 10 kΩ pull-up to the shared 3.3 V logic rail. | DS-PIN; DS-AF (AF7) |
| UDI host RX | PB11 | 30 | USART3 RX, AF7 | Input idle HIGH; 10 kΩ pull-up to the shared 3.3 V logic rail. | DS-PIN; DS-AF (AF7) |
| I2C clock | PB8 | 61 | I2C1 SCL, AF4 | Open-drain bus idle HIGH; 4.7 kΩ pull-up to 3.3 V, sized again for final bus capacitance. | DS-PIN; DS-AF (AF4) |
| I2C data | PB9 | 62 | I2C1 SDA, AF4 | Open-drain bus idle HIGH; 4.7 kΩ pull-up to 3.3 V, sized again for final bus capacitance. | DS-PIN; DS-AF (AF4) |
| Fan control | PB5 | 57 | GPIO / TIM3_CH2, AF2 | High-Z during reset; 10 kΩ pull-down at the MOSFET gate holds fan OFF. No tach input is allocated. | DS-PIN; DS-AF (AF2) |
| C3 link TX | PC10 | 51 | UART4 TX, AF8 | UART idle HIGH; 10 kΩ pull-up to the shared C3/F405 3.3 V domain. C3 GPIO0 is the application UART RX; no RTS/CTS. | DS-PIN; DS-AF (AF8) |
| C3 link RX | PC11 | 52 | UART4 RX, AF8 | Input idle HIGH; 10 kΩ pull-up to the shared C3/F405 3.3 V domain. C3 GPIO1 is the application UART TX; no RTS/CTS. | DS-PIN; DS-AF (AF8) |
| C3 active-low enable/reset | PC0 | 8 | GPIO, open-drain; confirmed by session 3 | `C3_EN_N`: HIGH-Z releases reset; drive LOW to hold C3 in reset. Add a 10 kΩ pull-up; 1 µF from CHIP_EN to GND is the Espressif reference, but fit/value remains gated on exact module and rail-ramp review. | DS-PIN; session 3 C3 audit |
| C3 active-low boot strap | PC1 | 9 | GPIO, open-drain; confirmed by session 3 | `C3_BOOT_N` controls C3 GPIO9: 10 kΩ pull-up to C3 3.3 V selects normal boot; assert LOW only while resetting for ROM download mode, then release HIGH-Z. | DS-PIN; session 3 C3 audit |
| Native USB VBUS sense | PA9 | 42 | OTG_FS_VBUS input | Sense through 4.7 kΩ series from connector VBUS and 10 kΩ from PA9 to GND. No connection to any HAT power rail; the divider keeps PA9 within the unpowered-pin voltage limit. | DS-PIN |
| Native USB D− | PA11 | 44 | OTG_FS_DM, AF10 | USB peripheral pins remain Hi-Z until configured; no external pull-up/down. Route as a 90 Ω differential pair through the ESD device. | DS-PIN; DS-AF (AF10) |
| Native USB D+ | PA12 | 45 | OTG_FS_DP, AF10 | USB peripheral pins remain Hi-Z until configured; no external pull-up/down. Route as a 90 Ω differential pair through the ESD device. | DS-PIN; DS-AF (AF10) |
| USB ID reservation | PA10 | 43 | OTG_FS_ID function unused | Device-only design; leave unconnected and do not enable USB host/ID behavior. | DS-PIN; DS-AF (AF10) |
| SWD data | PA13 | 46 | SWDIO, AF0 | Keep on the programming header; configure SWD-only (disable JTAG). No user circuit or external pull. | DS-PIN; DS-AF (AF0) |
| SWD clock | PA14 | 49 | SWCLK, AF0 | Keep on the programming header; configure SWD-only. No user circuit or external pull. | DS-PIN; DS-AF (AF0) |
| Optional SWO | PB3 | 55 | TRACESWO, AF0 reservation | Debug-only; disabled and high-Z in normal operation. Do not use as SPI1 SCK. | DS-PIN; DS-AF (AF0) |
| Status LED | PC13 | 2 | GPIO, active-low | Reset Hi-Z; 10 kΩ pull-up keeps LED OFF. Limit LED sink current to 2 mA pending package current review. | DS-PIN |
| HSE crystal | PH0 / PH1 | 5 / 6 | OSC_IN / OSC_OUT | Reserve exclusively for the crystal and its two ground-referenced load capacitors. | DS-PIN |
| Boot configuration | BOOT0 / PB2 (BOOT1) | 60 / 28 | Boot straps | Fit 10 kΩ pull-down on each. Normal boot is BOOT0=0; recovery boot raises BOOT0 while PB2 stays LOW. | DS-PIN |

The LOW defaults for the ISET controls follow the existing firmware's
initialization intent. The Rev-C schematic/netlist source conflict remains a
gate: confirm the copied Rev-D control polarity and all-off shift-register
word against the reconciled hardware source before wiring the new sheet.
Passive pulls define reset behavior; they do not replace hardware interlocks
or protection.

### Peripheral resource conflict check

- The selected pins support SPI1 AF5, USART1/USART3 AF7, UART4 AF8, I2C1 AF4,
  TIM3_CH2 AF2, USB OTG FS AF10, and SWD/SWO AF0 with no pin mux overlap
  (DS8626 Table 9, pp. 62–71).
- PA4 is used as a GPIO latch, not SPI1 hardware NSS. PB5 is TIM3_CH2, not
  SPI1 MOSI or I2C1_SMBA. USART3 stays on PB10/PB11 while UART4 uses PC10/PC11.
- If DMA is enabled, the following RM0090 Rev. 19 Tables 42–43 stream/channel
  choices are mutually distinct: SPI1 RX DMA2 Stream 0 Channel 3 / TX Stream
  3 Channel 3; USART1 RX DMA2 Stream 2 Channel 4 / TX Stream 7 Channel 4;
  USART3 RX DMA1 Stream 1 Channel 4 / TX Stream 3 Channel 4; UART4 RX DMA1
  Stream 2 Channel 4 / TX Stream 4 Channel 4 (pp. 307–308). No DMA stream
  collision is present in this candidate allocation.
- TIM3_CH2 is the only allocated TIM3 channel. No timer conflict is present
  in this map; the PWM frequency and any future timer/timebase use remain
  firmware integration decisions.

## Minimum-support values for schematic capture

| Circuit block | Frozen target values / connection | Evidence and remaining qualification |
|---|---|---|
| Exact MCU | STM32F405RGT6, LQFP64; retain the LQFP64 pin assignments above. | DS8626 Rev. 9 Table 7, pp. 47–58. Verify selected ordering code and footprint before capture. |
| Digital power | 100 nF ceramic at each VDD pin, plus 4.7 µF local bulk on the 3.3 V MCU rail; VSS to the common ground plane. | AN4488 §2.2, p. 8. Confirm regulator headroom for C3 radio peaks and capacitor DC-bias derating. |
| Analog power | VDDA from 3.3 V through a 0 Ω link (ferrite option only if analog-noise review requires); 100 nF + 1 µF local bypass; VSSA to common ground. | AN4488 §2.2, p. 8. No separate floating analog ground. |
| Internal regulator | VCAP_1 (pad 31) and VCAP_2 (pad 47): one 2.2 µF low-ESR ceramic (ESR <2 Ω) from each pin to GND; no other load. | AN4488 §2.2, p. 8. Place each capacitor adjacent to its pin. |
| Backup domain | VBAT (pad 1) tied to VDD/3.3 V when no backup battery is fitted; do not leave it floating. | AN4488 §2.1.2, p. 7. Add a local 100 nF bypass if required by the final layout. |
| HSE clock | 8.000 MHz crystal, target ±20 ppm and CL=8 pF; start with two 10 pF C0G/NP0 load capacitors to GND. Firmware PLL target: PLLM=8, PLLN=336, PLLP=2, PLLQ=7 (168 MHz SYSCLK and 48 MHz USB). | AN4488 §4.1.1, p. 27 and crystal-load calculation, p. 26. 10 pF assumes about 3 pF stray capacitance; recalculate from the selected crystal datasheet/layout and validate oscillator startup before fabrication. |
| Reset | NRST (pad 7): 10 kΩ pull-up to 3.3 V, 100 nF to GND, reset switch and SWD header able to pull low. | AN4488 §2.3.5, p. 14, and reference design, p. 58. Confirm release timing with the selected reset supervisor/debugger circuit. |
| Boot/recovery | BOOT0: 10 kΩ pull-down; PB2/BOOT1: 10 kΩ pull-down; provide a jumper/test point to raise BOOT0 for system-memory recovery. | AN4488 §5.1, p. 30, and reference design, p. 58. Normal boot selects main Flash. |
| Programming/debug | SWDIO, SWCLK, NRST, GND, and target 3.3 V reference on a keyed header; debugger must not source target power. | DS8626 Table 7, pp. 47–58; keep the F405 as the sole board-power source. |
| USB connector | GCT USB4105-GF-A USB Type-C receptacle, USB 2.0 only; 5.1 kΩ Rd from each CC pin to GND; connector shield bonded to GND at the connector. | [GCT USB4105-GF-A](https://gct.co/connector/usb4105-gf-a). No SuperSpeed or source/host role. |
| USB protection and VBUS | ST USBLC6-2SC6 for D+/D− ESD protection, placed at the receptacle. VBUS reaches PA9 only through 4.7 kΩ series / 10 kΩ pulldown sensing; never connect VBUS to +5V_Boot, 3.3 V, or any HAT supply rail. | [ST USBLC6-2](https://www.st.com/en/protections-and-emi-filters/usblc6-2.html); [ST VBUS-sensing guidance](https://community.st.com/stm32-mcus-60/management-of-vbus-sensing-for-usb-device-design-93); [AN4879](https://www.st.com/resource/en/application_note/dm00296349-usb-hardware-and-pcb-guidelines-using-stm32-mcus-stmicroelectronics.pdf). The divider is a sense path only and keeps PA9 within its unpowered input limit. |
| C3 support/control | Bare ESP32-C3-MINI-1U with external antenna; PC0=`C3_EN_N`, PC1=`C3_BOOT_N`, confirmed by session 3. Both are F405 open-drain controls with 10 kΩ pull-ups to C3 3.3 V. | See the C3 audit below for 3.3 V power, RF, UART, strap, timing, recovery, and authority requirements. Espressif's 1 µF CHIP_EN capacitor is a reference, not a final BOM commitment; confirm fit/value against the exact module and final rail ramp. |
| UART interfaces | CH340 debug (USART1), UDI (USART3), and C3 application UART (UART4) are separate 3.3 V logic links. UART lines idle HIGH; keep endpoints on a common powered logic domain and avoid driving an unpowered target. | Verify final interface circuits, series protection, and startup behavior in the schematic review. |

## Remaining uncertainties and gates

- Session 3 confirmed the bare ESP32-C3-MINI-1U external-antenna module path
  and PC0=`C3_EN_N` / PC1=`C3_BOOT_N`. Confirm the exact orderable module and
  antenna/connector before layout; verify the EN capacitor and reset timing
  against the selected module and final 3.3 V rail ramp.
- The Rev-C source reconciliation / #62 conflict must confirm the ISET
  control polarity and the shift-register all-off word before those safe
  states are copied into the new schematic.
- The chosen crystal's exact manufacturer part and load must be checked
  against its datasheet and final PCB parasitics. The 8 MHz / CL=8 pF /
  2×10 pF target is not startup- or USB-bench-validated.
- Verify PA9 VBUS sensing with both HAT-powered and HAT-unpowered cable
  insertion, including VBUS maximum and divider tolerance. USB routing,
  connector footprint, ESD placement, and signal-integrity review remain open.
- Confirm the reset-capacitor release time, regulator headroom, VDDA noise,
  PC13 sink-current margin, and fan PWM frequency during schematic/firmware
  integration.
- The session-4 support sheet is schematic-only. The C3 connector represents
  an external module interface; module supply decoupling, GPIO2/GPIO8 boot
  straps, UART0 recovery header, antenna/RF layout, and final connector
  selection remain for full HAT integration. Its SWD header footprint and
  exact USB footprint mapping also require review.
- No Rev-D PCB DRC or bench validation exists; the ERC and netlist review do
  not validate capacitor selection, clock startup, reset timing, USB signal
  integrity, ESD placement, or VBUS behavior.

## C3 coprocessor audit and Rev-D contract

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
the following signal names. Session 2 froze the F405 assignments, and session
3 confirmed PC0 and PC1 for the selected MINI-1U control signals:

| Net name | C3 connection | F405 connection | Direction |
|---|---|---|---|
| `C3_UART_RX` | GPIO0, app UART RX | UART4 TX, PC10 | STM32 -> C3 |
| `C3_UART_TX` | GPIO1, app UART TX | UART4 RX, PC11 | C3 -> STM32 |
| `C3_EN_N` | CHIP_EN | PC0, confirmed | Active-low reset/disable, open-drain |
| `C3_BOOT_N` | GPIO9 strap | PC1, confirmed | Active-low download-mode request, open-drain |

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
2. The F405 pin allocation, reset-safe bias targets, clock target, boot straps,
   and USB connector/protection choices are frozen here with citations.
   Session 3 confirmed the C3 module path and PC0/PC1 controls; schematic
   capture must still qualify the exact module/antenna, EN circuit, crystal
   load, VBUS divider, and unresolved Rev-C source conflicts.
3. **Session 4 completed (2026-10-09):** created
   `hardware/kicad/dsp-regulator-hat-rev-d/` with a root project and separate
   hierarchical `MCU.kicad_sch`. Rev-C source files and PCB were left
   untouched; no Rev-C board content was copied.
4. **Session 4 review completed:** exported the netlist to the project's
   gitignored `build_kicad/` directory, accounted for all 64 F405 pins, and
   checked power/support pins, C3 UART direction and controls, USB connector
   through ESD and PA9 VBUS sensing, and SWD. KiCad 10.0.5 ERC reported
   0 errors and 0 warnings. The detailed results and open gates are below.
5. Full-HAT integration, PCB DRC, and bench validation remain separate
   fabrication-release gates. ERC alone cannot validate capacitor
   requirements, USB clock accuracy, reset safety, signal integrity, or
   pin-multiplexing assumptions.

The controller choice and cited F405 allocation/support targets are settled.
The session-4 support sheet has passed an initial ERC/netlist review, but it is
**not routed or electrically released**. USB timing/ESD review, part
qualification, full-HAT integration, PCB DRC, and bench validation remain
open.

### Session 4 schematic review (2026-10-09)

Project: `hardware/kicad/dsp-regulator-hat-rev-d/`
(`DSP-Regulator-HAT-RevD-Support.kicad_sch` with child `MCU.kicad_sch`).
The machine-readable ERC report and exported netlist are in the gitignored
project `build_kicad/` directory. KiCad CLI version: 10.0.5. ERC result:
**0 errors, 0 warnings**.

Netlist review confirmed:

- All 64 STM32F405RG LQFP64 pins are accounted for. VDD pins 19/32/48/64 and
  VBAT pin 1 map to `+3V3`; VSS pins 18/63 and VSSA pin 12 map to GND;
  VDDA pin 13 is supplied through the 0 Ω link and has local bypass; VCAP_1
  pin 31 and VCAP_2 pin 47 each connect only to their own 2.2 µF capacitor.
- The four digital VDD pins have one 100 nF bypass each and a local 4.7 µF
  rail capacitor. NRST pin 7 has its 10 kΩ pull-up, 100 nF capacitor, reset
  switch, and SWD-header connection. BOOT0 pin 60 and PB2/BOOT1 pin 28 each
  have a 10 kΩ pull-down; the BOOT0 recovery jumper raises BOOT0 from 3.3 V.
- HSE PH0/PH1 (pins 5/6) connect to the crystal and separate ground-referenced
  load-capacitor nets. Their fields retain the candidate/unverified status
  described below.
- PC10/pin 51 is `C3_UART_RX` (F405 UART4 TX to C3 GPIO0/RX), and PC11/pin 52
  is `C3_UART_TX` (C3 GPIO1/TX to F405 UART4 RX). PC0/pin 8 is `C3_EN_N`;
  PC1/pin 9 is `C3_BOOT_N`. Both controls have 10 kΩ pull-ups to 3.3 V and
  terminate at the external C3 interface connector.
- USB PA11/pin 44 maps to D− and PA12/pin 45 to D+ through U2, whose Value is
  `USBLC6-2SC6`. Type-C connector D+/D− pins A/B are paired correctly, both
  CC pins have 5.1 kΩ Rd resistors, and the connector shield/common ground
  is tied to GND. Connector VBUS reaches PA9/pin 42 only through R9 (4.7 kΩ)
  and R10 (10 kΩ to GND); VBUS is not on a HAT power rail. PA10/USB ID and
  SBU1/SBU2 are explicitly unconnected.
- PA13/pin 46 and PA14/pin 49 map to SWDIO and SWCLK. The keyed 10-pin SWD
  header also exposes target 3.3 V reference, GND, and NRST; SWO is
  unconnected, and the debugger reference is not a target-power input.
- Unallocated F405 GPIOs are explicitly marked no-connect in this minimum
  support project; this does not reserve or remove their frozen Rev-D
  assignments from the full-HAT design.

Review findings and qualification status:

- The 8.000 MHz crystal and two 10 pF C0G/NP0 load capacitors are marked
  `candidate; UNVERIFIED` in their schematic Value fields. The two capacitors
  are DNP pending selection of the crystal, load calculation including board
  parasitics, and oscillator-startup review.
- The optional 1 µF C3 CHIP_EN capacitor is marked
  `candidate; UNVERIFIED` and DNP. Confirm its fit/value against the exact
  ESP32-C3-MINI-1U ordering code and 3.3 V rail ramp before populating it.
- J4 is an interface connector, not the C3 module symbol. C3 local supply
  bypassing, GPIO2/GPIO8 strap pulls, UART0 recovery, module/antenna choice,
  RF keep-out, and antenna validation are not implemented by this support
  sheet.
- Exact SWD connector/footprint selection and validation of the USB4105
  footprint against the orderable GCT variant remain open. No PCB, DRC, USB
  powered/unpowered insertion test, crystal startup test, or bench test was
  performed. Review ESD placement and 90 Ω USB routing during layout.

## Session breakdown

Each session uses its own branch and PR. Each session also has a reusable
VS Code Copilot Chat prompt in `.github/prompts/` (run `/<prompt-name>` in
chat). The same text can serve as a Copilot app session kickoff. Every session
updates this document with its evidence and remaining gates, and leaves the
Rev-C KiCad files unchanged.

| # | Branch | Prompt | Scope | Depends on |
|---|---|---|---|---|
| 1 | `hw/revc-source-reconcile` | `revd-1-revc-source-reconcile` | Reconcile the Rev-C schematic and netlist, including the #62 Q9/Q3/Q12/U4 conflicts | — |
| 2 | `phil-cia-f405-pin-freeze` | `revd-2-f405-pin-freeze` | Freeze cited F405 pins, reset-safe states, clock, boot, and USB support choices | C3 control GPIOs confirmed by session 3; electrical fit/value checks remain |
| 3 | `hw/revd-c3-coprocessor` | `revd-3-c3-coprocessor` | Select bare ESP32-C3-MINI-1U external-antenna path and define power, RF, UART, reset/boot, and recovery requirements | — |
| 4 | `hw/revd-f405-support-sheet` | `revd-4-f405-support-sheet` | Separate Rev-D KiCad project, MCU support sheet, ERC and netlist review | 2, 3 |
| 5 | `hw/revd-hat-integration` | `revd-5-hat-integration` | Full Rev-D HAT integration, ERC and DRC | 1, 4 |
| 6 | `firmware/f405-target` | `revd-6-f405-firmware-target` | F405 PlatformIO target using the frozen pin map | 2 |

Sessions 1–3 can run in parallel. Rev-C bring-up continues independently.
VS Code tasks `Rev-C: KiCad ERC` and `Rev-C: Export netlist` write their
reports to the gitignored `build_kicad/` directory.

## References

- [ST DS8626, STM32F405/407 datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf)
- [ST AN4488, STM32F4 hardware development](https://www.st.com/resource/en/application_note/dm00115714.pdf)
- [ST RM0090, STM32F405/407 reference manual](https://www.st.com/resource/en/reference_manual/dm00031020.pdf)
- [ST AN4879, USB hardware and PCB guidelines](https://www.st.com/resource/en/application_note/dm00296349-usb-hardware-and-pcb-guidelines-using-stm32-mcus-stmicroelectronics.pdf)
- [Rev-C signal contract](STM32_BLUEPILL_PIN_TABLE.md)
- [UDI contract](DISPLAY_INTERFACE_STANDARD.md)

The installed KiCad symbol verifies pad numbering only, not the legality of
alternate functions or the adequacy of board support circuits.
