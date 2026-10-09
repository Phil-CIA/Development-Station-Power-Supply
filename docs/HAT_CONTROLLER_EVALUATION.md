# HAT Controller Decision and Rev-D Migration (#102)

**Status:** Controller architecture and MCU selected for future HAT Rev-D:
STM32F405RG bare MCU plus ESP32-C3-MINI Wi-Fi coprocessor. Schematic/layout
migration and verification remain to be done; Rev-C files are not changed by
this decision. The reviewed F405 support project is preserved; use the
separate editable integration starter in
`hardware/kicad/dsp-regulator-hat-rev-d-full/` for full-HAT work. It currently
contains F405 support and an initial committed-source HAT reuse pass, not
the complete HAT, bare C3 module or PCB.

This decision applies to the future HAT revision, referred to here as
Rev-D. Continue Rev-C bring-up and firmware work separately.

## Confirmed future requirements

| Function | Current direction | Boundary |
|---|---|---|
| PC display link | USB device with CDC serial telemetry; a separate PC application renders a larger dashboard | Data/telemetry path, not USB host functionality. The existing CH340C/USART debug path is distinct from the future native USB interface. |
| Wi-Fi | Local-network telemetry and OTA updates | No remote power control has been requested. OTA security, recovery, and update interruption behavior must be evaluated before selection. |
| Power control | Preserve a deterministic, safe control path | USB, Wi-Fi, and update activity must not compromise safe output states, hardware protection, or fault reporting. |

These are future-revision requirements, not authorization to enable Wi-Fi,
OTA, or a new USB path on Rev-C. The exact PC telemetry schema, update
security/recovery policy, and network provisioning behavior remain open.

## Selected Rev-D architecture

The selected architecture is:

- **STM32F405RG** as the on-board, bare LQFP64 control MCU. It replaces the
  STM32F103C8T6 Blue Pill development board used by the current firmware
  bring-up setup and remains the deterministic real-time/control and safety
  authority.
- **ESP32-C3-MINI** as a separate, low-cost Wi-Fi coprocessor for local
  network telemetry and OTA.
- **USB CDC device telemetry** from the HAT to a PC, where a separate
  application can render the larger dashboard.
- Keep Wi-Fi/network activity from directly owning time-critical rail control
  or bypassing STM32 safety/fault handling.

The STM32F405RG selection is final for this architecture decision. The cited
F405 pin allocation and minimum-support targets are frozen in the
[Rev-D pin and support contract](HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md);
schematic capture, electrical review, USB routing, and C3 module integration
remain open before a Rev-D schematic or PCB is ready. Session 3 confirmed
PC0=`C3_EN_N` and PC1=`C3_BOOT_N` for the bare ESP32-C3-MINI-1U external-antenna
module path; exact ordering code and electrical implementation remain to be
verified.
The exact C3 module variant and its use of the current Rev-C U2 footprint and
routing also require verification; the existing `ESP32_C3_mini` symbol at U2
does not prove that it is fully wired or firmware-ready. The Rev-C netlist
identifies that footprint as Seeed SKU `113991054` (XIAO ESP32C3), not a bare
ESP32-C3-MINI module. Its 5 V input is connected, its 3.3 V output is
explicitly no-connected, two GPIOs link to the STM32, two more have isolated
RTS/CTS-named net stubs, and nine GPIOs are explicit no-connects. EN and USB
programming signals are not exposed by the schematic symbol. Rev-D therefore
uses a bare ESP32-C3-MINI-1U module with external antenna, with explicit
power, EN/boot control, UART, and recovery access. Confirm the exact orderable
module variant and antenna/connector before layout; see the C3 audit in the
[Rev-D pin and minimum-support contract](HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md).

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

## Migration and implementation checklist

The [Rev-D pin and minimum-support contract](HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md)
provides the frozen, cited F405 allocation and schematic review gates.
Confirmed interface decisions: supply-powered USB device (data/VBUS sensing
only) and a dedicated UART between the STM32 and C3. Session 3 confirmed the
bare ESP32-C3-MINI-1U external-antenna path and PC0/PC1 as the C3 EN/BOOT
controls. The support schematic has passed initial ERC/netlist review, and a
separate editable full-HAT project now adds an initial HAT reuse pass.
The current partial integration has 0 ERC errors and 80 unresolved warnings;
the earlier support-only 0/0 result is not a current full-HAT claim.
Full-HAT integration and electrical review remain open. Under the user's 2026-10-09 direction,
prior HAT circuits are working reuse candidates, replaceable as needed;
full Rev-C bench requalification and the untested fan's later bench check
do not block integration. Production-source/physical-board correlation and
Rev-D validation remain open; see the contract for the manufacturing-reference
boundary and remaining gates.

The F405RG provides up to 1 MiB of flash, 192 KiB SRAM, and USB OTG FS
according to ST documentation. Confirm the exact datasheet limits and memory
regions during firmware migration. The ESP32-C3 remains a separate radio
coprocessor; do not merge its networking responsibilities into the STM32
control loop.

- [ST STM32F405RG product page](https://www.st.com/en/microcontrollers-microprocessors/stm32f405rg.html)
- [ST STM32F405RG datasheet](https://www.st.com/resource/en/datasheet/stm32f405rg.pdf)
- [Espressif ESP32-C3-MINI-1 datasheet](https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.html)

1. **Board-level STM32 integration:** replace the Blue Pill module representation
   with the exact STM32F405RG LQFP64 symbol and footprint; allocate VDD/VSS,
   VDDA/VSSA, VCAP, decoupling, reset, boot-strapping, clock source, and SWD
   programming/debug access from the current ST documentation and design
   requirements.
2. **Pin/peripheral migration:** map every current F103 signal contract to
   legal F405RG LQFP64 pins and alternate functions. Preserve USART for CH340
   debug and UDI, I2C, SPI W25Q128, shift-register controls, AW9523 interrupt,
   and needed timers/PWM/ADC. Check all simultaneous peripheral use and avoid
   assuming pin-name equivalence between MCUs.
3. **USB CDC hardware:** reserve and route the F405 USB OTG FS device D+/D-
   signals with the required connector, protection, VBUS sensing/power
   policy, and routing constraints. Confirm USB CDC does not conflict with
   existing debug/programming paths.
4. **C3 coprocessor interface:** verify the ESP32-C3-MINI variant, U2
   footprint/power/RF layout, and available GPIOs. Define a robust STM32-C3
   transport, framing, reset/boot control, and behavior when Wi-Fi is absent
   or the coprocessor resets.
5. **Firmware migration:** add a dedicated STM32F405RG build target and move
   the controller firmware off the F103C8 memory map. Revalidate startup,
   safe output defaults, fault latency, watchdog behavior, calibration and
   persistence, UDI, USB CDC telemetry, and build-size/RAM headroom.
6. **Network/update boundary:** define provisioning and local telemetry,
   OTA authentication/signing, rollback/recovery, and power-loss behavior.
   OTA may update the C3 firmware; any STM32 update path must be designed and
   independently validated. Remote power control remains out of scope.
7. **Hardware verification:** run schematic ERC, pin/net contract checks,
   footprint review, PCB DRC, and programming/boot/USB/Wi-Fi coexistence
   tests. Keep fabrication release gated on the applicable Rev-D electrical
   and safety review.

The W25Q128 remains external SPI data flash. It may support data storage or
update staging only if a future boot/update design explicitly implements and
validates that role; it does not replace the F405's executable flash or RAM.
