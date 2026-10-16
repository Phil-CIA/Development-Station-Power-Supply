# Rev-D STM32F405 LQFP64 Pad Ledger

**Historical ledger, reconciled 2026-10-16:** use the 64 package pads as a
review checklist, not a current wiring contract. Later user decisions in
[#126](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/126)
select the actual SuperMini, native F405 USB CDC, and a custom F405 loader
over UART4 (PC10 to GPIO0; PC11 from GPIO1). PB6/PB7/PD2 are spare links to
C3 GPIO5/6/7, not CH340; PC0/PC1 reset/boot controls are not required. The
PC13 LED and bare-MINI-1U proposals are not approved additions. See
[current reconciliation](HAT_CONTROLLER_EVALUATION.md) before reusing any
allocation or recommendation below.

Status: preliminary cross-check only. This ledger compares the frozen F405
pin contract with the repository's session-4 minimum-support schematic and
netlist. It is **not** a pin-by-pin review of the user's signal-flow
schematic: that source is not available in this worktree. The support sheet
is not a full HAT design and must not be treated as the PCB routing plan.
No KiCad files were edited for this review.

## Sources and status

- Desired F405 allocations: `docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md`.
- Captured support-sheet connectivity: a fresh read-only KiCad CLI netlist
  export from `hardware/kicad/dsp-regulator-hat-rev-d/`.
- Former Rev-C/Blue Pill signal use: `docs/STM32_BLUEPILL_PIN_TABLE.md`.
- Pin names and LQFP64 pad numbers: ST DS8626 Rev. 9, Table 7, as cited in
  the F405 contract.

`Contract allocation` records design intent, not proof of routing. `Support
net` is the actual netlist connection in the support-only sheet. `NC` means
that sheet marks a GPIO no-connect; it does not mean the pad can be left
unreviewed in the complete design.

## All 64 F405 pads

| Pad | F405 pin / fixed function | Contract allocation | Session-4 support net | Rev-C carry-forward / disposition |
|---:|---|---|---|---|
| 1 | VBAT | Tie to 3.3 V when no backup battery is used | `+3V3` | New bare-MCU support |
| 2 | PC13 | Active-low status LED | NC | New allocation; verify LED polarity/current |
| 3 | PC14 / OSC32_IN | Spare; no LSE allocation | NC | No Rev-C signal assigned |
| 4 | PC15 / OSC32_OUT | Spare; no LSE allocation | NC | No Rev-C signal assigned |
| 5 | PH0 / OSC_IN | HSE input; crystal only if external HSE is selected | `HSE_IN` | Blue Pill had an onboard clock source; confirm required system clock |
| 6 | PH1 / OSC_OUT | HSE output; crystal only if external HSE is selected | `HSE_OUT` | Blue Pill had an onboard clock source; confirm required system clock |
| 7 | NRST | Pull-up, reset circuit, SWD/header access | `NRST` | New bare-MCU support |
| 8 | PC0 | Earlier Wi-Fi reset candidate only | `C3_EN_N` | Former C3 reset/link architecture is not selected |
| 9 | PC1 | Earlier Wi-Fi boot candidate only | `C3_BOOT_N` | Former C3 reset/link architecture is not selected |
| 10 | PC2 | Spare | NC | No Rev-C signal assigned |
| 11 | PC3 | Spare | NC | No Rev-C signal assigned |
| 12 | VSSA | Common ground | `GND` | New bare-MCU support |
| 13 | VDDA | 3.3 V through 0-ohm link; local bypass | `VDDA` | New bare-MCU support |
| 14 | PA0 | 5 V rail control | NC | Rev-C PA0 ISET function carried forward by contract |
| 15 | PA1 | 3.3 V rail control | NC | Rev-C PA1 ISET function carried forward by contract |
| 16 | PA2 | Unused spare; not CH3 | NC | Former C3 UART signal not retained; no CH3 control |
| 17 | PA3 | Unallocated spare | NC | Former C3 UART signal not retained |
| 18 | VSS | Common ground | `GND` | New bare-MCU support |
| 19 | VDD | 3.3 V digital supply | `+3V3` | New bare-MCU support |
| 20 | PA4 | Shift-register latch | NC | Rev-C PA4 latch function is carry-forward intent; hardware source must verify shift-register path |
| 21 | PA5 | SPI1 SCK for W25Q128 | NC | Rev-C flash SPI clock function carried forward by contract |
| 22 | PA6 | SPI1 MISO for W25Q128 | NC | Rev-C flash SPI input function carried forward by contract |
| 23 | PA7 | SPI1 MOSI for W25Q128 | NC | Rev-C flash SPI output function carried forward by contract |
| 24 | PC4 | AW9523 active-low interrupt candidate | NC | New F405 allocation; AW9523 is on regulator board per user context |
| 25 | PC5 | Unallocated spare | NC | No Rev-C signal assigned |
| 26 | PB0 | Unallocated spare | NC | No Rev-C signal assigned |
| 27 | PB1 | Unallocated spare | NC | No Rev-C signal assigned |
| 28 | PB2 / BOOT1 | Boot strap, normally held LOW | `PB2_BOOT1` | New bare-MCU boot support |
| 29 | PB10 | USART3 TX, display UDI | NC | Rev-C PB10 UDI TX function carried forward by contract |
| 30 | PB11 | USART3 RX, display UDI | NC | Rev-C PB11 UDI RX function carried forward by contract |
| 31 | VCAP_1 | 2.2 µF low-ESR capacitor to ground only | `VCAP_1` | New bare-MCU support |
| 32 | VDD | 3.3 V digital supply | `+3V3` | New bare-MCU support |
| 33 | PB12 | Unallocated spare | NC | No Rev-C signal assigned |
| 34 | PB13 | Unallocated spare | NC | No Rev-C signal assigned |
| 35 | PB14 | Unallocated spare | NC | No Rev-C signal assigned |
| 36 | PB15 | Unallocated spare | NC | No Rev-C signal assigned |
| 37 | PC6 | Unallocated spare | NC | No Rev-C signal assigned |
| 38 | PC7 | Unallocated spare | NC | No Rev-C signal assigned |
| 39 | PC8 | Unallocated spare | NC | No Rev-C signal assigned |
| 40 | PC9 | Unallocated spare | NC | No Rev-C signal assigned |
| 41 | PA8 | W25Q128 chip select | NC | Rev-C external-flash CS function carried forward by contract |
| 42 | PA9 / OTG_FS_VBUS | USB VBUS-sense reservation only; USB deferred | `USB_VBUS_SENSE` | Rev-C CH340 UART TX moves to F405 PB6 |
| 43 | PA10 / OTG_FS_ID | Unused; USB host/ID disabled | NC | Rev-C CH340 UART RX moves to F405 PB7 |
| 44 | PA11 / OTG_FS_DM | USB D− reservation only; USB deferred | `USB_DM` | No active Rev-D interface assigned |
| 45 | PA12 / OTG_FS_DP | USB D+ reservation only; USB deferred | `USB_DP` | No active Rev-D interface assigned |
| 46 | PA13 / JTMS-SWDIO | JTAG TMS and SWDIO; preserve on requested J3 | `SWDIO` | Preserve programming access; exact J3 pin numbering awaits user's source |
| 47 | VCAP_2 | 2.2 µF low-ESR capacitor to ground only | `VCAP_2` | New bare-MCU support |
| 48 | VDD | 3.3 V digital supply | `+3V3` | New bare-MCU support |
| 49 | PA14 / JTCK-SWCLK | JTAG TCK and SWCLK; preserve on requested J3 | `SWCLK` | Preserve programming access; exact J3 pin numbering awaits user's source |
| 50 | PA15 / JTDI | JTAG TDI; preserve for full JTAG access | NC | Required for full JTAG; unused in SWD debug mode |
| 51 | PC10 | Candidate Wi-Fi UART TX only, not selected | `C3_UART_RX` | Former C3 UART link is not selected; net name reflects C3-side RX |
| 52 | PC11 | Candidate Wi-Fi UART RX only, not selected | `C3_UART_TX` | Former C3 UART link is not selected; net name reflects C3-side TX |
| 53 | PC12 | Unallocated spare | NC | No Rev-C signal assigned |
| 54 | PD2 | Unallocated spare | NC | No Rev-C signal assigned |
| 55 | PB3 / JTDO-TRACESWO | JTAG TDO; alternate optional SWO | NC | Required for full JTAG; reserve from application I/O |
| 56 | PB4 / NJTRST | JTAG reset; preserve for full JTAG access | NC | Required for full JTAG; unused in SWD debug mode |
| 57 | PB5 | Fan-control GPIO; external gate pull-down | NC | Rev-C fan-control path is PB5→Q9/J7 in the pin contract; D9 is a separate AW9523 LED |
| 58 | PB6 | USART1 TX to CH340 debug | NC | F405 remap from Rev-C PA9 |
| 59 | PB7 | USART1 RX from CH340 debug | NC | F405 remap from Rev-C PA10; displaces the older Rev-C AW9523-INT assignment |
| 60 | BOOT0 | Normally LOW; accessible recovery selection | `BOOT0` | New bare-MCU boot support |
| 61 | PB8 | I2C1 SCL | NC | Rev-C I2C SCL carried forward by contract |
| 62 | PB9 | I2C1 SDA | NC | Rev-C I2C SDA carried forward by contract |
| 63 | VSS | Common ground | `GND` | New bare-MCU support |
| 64 | VDD | 3.3 V digital supply | `+3V3` | New bare-MCU support |

## Rev-C Blue Pill signal migration matrix

This maps the controller signals documented for Rev-C to the frozen F405
allocation. It does not claim the user schematic routes these nets, nor does
it resolve the Rev-C source conflicts described in
`docs/STM32_BLUEPILL_PIN_TABLE.md`.

| Rev-C signal/function | Blue Pill/F103 pin | Rev-D F405 pad / pin | Migration disposition |
|---|---|---|---|
| 5 V rail control (`ISET_MPU_5V`) | PA0 | 14 / PA0 | Same MCU signal pin; revalidate HAT net, polarity, and reset-safe pull. |
| 3.3 V rail control (`ISET_MPU_3V3`) | PA1 | 15 / PA1 | Same MCU signal pin; revalidate HAT net, polarity, and reset-safe pull. |
| Legacy CH3 ISET | PA2 in older table | — | Not carried forward; F405 PA2 is explicitly spare, no CH3 MCU control. |
| Legacy ESP32-C3 link | PA2 / PA3 | Candidate PC10/pad 51 and PC11/pad 52 | Old link not carried as a selected interface. PC10/PC11 UART4 assignment is only a candidate pending exact radio selection and source trace. |
| Shift-register latch (`SR_Latch`) | PA4 | 20 / PA4 | Same GPIO/pad allocation; verify the actual shift-register device and all-off word against the source before accepting it. |
| External flash SPI clock | PA5 | 21 / PA5 | Same pin, SPI1 SCK. Full-HAT schematic still must connect it to the verified W25Q device. |
| External flash SPI input | PA6 | 22 / PA6 | Same pin, SPI1 MISO. |
| External flash SPI output | PA7 | 23 / PA7 | Same pin, SPI1 MOSI. |
| External flash chip select | PA8 | 41 / PA8 | Same pin; preserve reset-high/inactive pull-up. |
| CH340 debug TX | PA9 | 58 / PB6 | Remapped to USART1 TX AF7. PA9 is reserved for deferred USB VBUS sense, not debug TX. |
| CH340 debug RX | PA10 | 59 / PB7 | Remapped to USART1 RX AF7. PA10 is reserved/unused for USB ID; Rev-C AW9523 interrupt use on PB7 conflicts and must move. |
| AW9523 interrupt (documented Rev-C firmware assignment) | PB7 | 24 / PC4 | Remap candidate; confirm regulator-board AW9523 interrupt wire/net and active-low bias. Do not retain on PB7, now USART1 RX. |
| UDI/display UART TX | PB10 | 29 / PB10 | Same pin, USART3 TX AF7; verify connector and series path. |
| UDI/display UART RX | PB11 | 30 / PB11 | Same pin, USART3 RX AF7; verify connector and series path. |
| Telemetry I2C SCL | PB8 | 61 / PB8 | Same pin, I2C1 SCL AF4; verify actual I2C participants and pull-ups. |
| Telemetry I2C SDA | PB9 | 62 / PB9 | Same pin, I2C1 SDA AF4; verify actual I2C participants and pull-ups. |
| Fan switch gate (`PB5 → Q9/J7` in current HAT source) | PB5 | 57 / PB5 | Same pad allocation; fan hardware path is source/assembly-conflicted and not bench-validated. Keep separate from AW9523-driven D9. |
| Direct critical-fault GPIO | None confirmed | — | Not carried as a direct MCU pin until a verified route is identified; the frozen direct-fault input remains disabled. |
| D9 yellow status LED, AW9523 P1.0 | No direct F103 GPIO | No F405 GPIO | Expander-driven on the regulator board; retain as an I2C/AW9523 function, not PB5 fan control. |
| Status LED | Not established in Rev-C pin contract | 2 / PC13 | New allocation in F405 contract; confirm LED circuit and safe polarity/current in the user schematic. |
| HSE clock / bare-MCU support | Blue Pill board-level clock source | 5/6 / PH0/PH1 | Bare F405 requires an explicit HSE crystal/external clock or HSI design decision; recommend Y1 HSE baseline pending part/load review. |
| Reset and boot recovery | Blue Pill board circuitry | 7 / NRST; 28 / PB2; 60 / BOOT0 | New bare-MCU support and accessible boot-selection/recovery path required. |
| Programming/debug | Blue Pill programming/debug arrangement | 46/49/50/55/56: PA13/PA14/PA15/PB3/PB4 | Preserve full JTAG access on the user-identified J3 and shared SWD capability; verify actual header pinout. |
| Native USB | Not required for current Rev-D scope | 42/44/45: PA9/PA11/PA12 candidates | Deferred; do not add as an active requirement or use its pins for another conflicting function. |

## Source conflicts and decisions still required

1. **User schematic source:** it must be staged/copied into this worktree
   unchanged before its nets can be traced. Until then, no cell in this
   ledger establishes the user's actual connectivity.
2. **J3 / JTAG:** preserve the user's stated J3 JTAG intent. Full JTAG uses
   PA13/TMS, PA14/TCK, PA15/TDI, PB3/TDO, and PB4/NJTRST; SWD shares PA13 and
   PA14. The local support sheet instead uses J1 for a 10-pin SWD header and
   J3 for USB-C. Confirm the user's connector pin numbering and footprint from
   the staged source; do not reuse those support-sheet designators.
3. **J4 and S1:** the support-only sheet labels J4 as a six-pin external
   ESP32-C3 interface and `SW1` (not S1) as a momentary NRST-to-ground
   switch. These designators/functions do not prove what J4 or S1 mean in
   the user's schematic; reconcile the user's exact switch and connector
   designators rather than carrying over the support-sheet references.
   Recommended reset behavior for S1 is a normally-open momentary connection
   from NRST to GND, independent of the JTAG header.
4. **Y1 / clock source:** the support sheet's Y1 is an 8 MHz crystal, marked
   unverified, with two candidate 10 pF load capacitors marked unverified /
   DNP. Recommended starting point: retain an 8 MHz HSE crystal to match the
   documented 168 MHz clock plan; select an orderable part and recalculate
   load capacitors from its datasheet and final layout. HSI is a possible
   alternative but requires clock/timing and baud-margin review before
   removing Y1. The Blue Pill's preselected clock does not establish the
   bare F405's clock source.
5. **Wi-Fi module and design:** ESP32-C3-MINI-1 / -1U remains only an earlier
   candidate. Before schematic freeze, select the exact module and antenna
   approach (or another option), module power source and peak-current margin,
   UART assignment, reset/boot straps, programming/recovery access, RF
   keep-out/location, and control/safety ownership. The F405 remains the
   power-control and safety authority; Wi-Fi loss or update activity must not
   bypass it. The official module datasheet calls out a 350 mA peak Wi-Fi TX
   condition and requires GPIO8 high with GPIO9 low for joint-download boot;
   the support J4 exposes only GPIO9 as a boot control. The candidate also
   lacks a dedicated module UART0/USB Serial/JTAG programming connection.
   PC0/PC1 and PC10/PC11 are not approved module interfaces until this review
   is complete. See
   [`ESP32-C3-MINI-1` notes](datasheets/parts/ESP32-C3-MINI-1/NOTES.md).
6. **Support-sheet delta:** the captured support netlist routes only the
   power/clock/reset/boot/SWD pins and the earlier USB/C3 candidates. It marks
   most frozen HAT control pins NC, including PA0/PA1, PA4-PA8, PB5-PB11,
   PC4, and PC13. This is expected for a minimum-support sheet and is not
   evidence that the full HAT has been routed.
7. **Legacy signal conflicts:** PB7 is documented as AW9523 interrupt on
   Rev-C but is reassigned to USART1 RX in the F405 contract; PC4 is the new
   AW9523 interrupt candidate. The regulator-board AW9523/D9 function must
   remain distinct from the HAT fan PB5/Q9/J7 path. Verify all carried
   signals against the user's schematic and regulator/HAT connector contract.

## Validation boundary and sequence

**Host-only, once the source is staged:** compare every used Blue Pill signal
to the actual F405 pad and net; check the complete netlist, ERC, pin
numbering, alternate-function conflicts, safe-state pulls, symbol/footprint
mapping, connector pinouts, power and decoupling, crystal load calculation,
and RF/mechanical keep-outs. Create the PCB and run DRC/net/clearance checks
only after schematic reconciliation and the user's approval to edit KiCad.
Firmware builds remain supporting evidence, not PCB validation.

**Bench/prototype-only:** verify supply startup and radio peak-current
headroom; NRST/BOOT0/SWD attach and recovery; HSE/HSI startup and clock
behavior; safe GPIO levels; CH340 console, UDI, I2C, and SPI paths; then
selected Wi-Fi module boot/programming, UART, RF/antenna, reset recovery, and
loss-of-link behavior. No physical board or bench evidence exists for this
F405 design yet.
