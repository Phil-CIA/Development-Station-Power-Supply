# STM32 Rev-C Pin Contract (Authoritative)

Status: active. This is the controller-of-record pin contract for the STM32
path (`stm32-bluepill-bringup`) against current Rev-C hardware files.

## Scope and sources

- **Repository-design source for this reconciliation:** the Rev-C HAT
  schematic and its committed netlist,
  `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.kicad_sch`
  and `.net`. A fresh KiCad 10.0.5 export on 2026-10-09 confirmed the
  committed netlist's electrical component/net topology.
- **Manufacturing reference selected for production-source reconciliation:**
  the user-supplied 2026-08-28 Rev-C PCB and matching Gerber/drill release
  files. These have not yet been proven to match the physical HAT or the
  later-dated schematic in that folder.
- **Firmware source of truth:** `stm32-bluepill-bringup/src/main.cpp`
- **System/docs entry point:** `docs/GPIO_PINOUT.md`

This table intentionally tracks both aligned and mismatched signals so drift
is visible and actionable.

## Rev-C source reconciliation for Rev-D (2026-10-09)

The repository schematic's fresh export contains 64 components and 105 nets,
matching the committed `.net` electrically. The only text differences are
the export source path and timestamp plus five missing third-party
symbol-library URI entries; no component, pin, or net connectivity changed.
The committed export is dated 2026-08-18. Therefore the Q9/Q3/Q12/U4
discrepancy is not explained by a stale committed HAT netlist. This validates
only the repository schematic against its own netlist, not the production
release.

The user reports no known issues from prior HAT use and says most circuitry
was plug-and-play, but the ESP32 and fan were not tested. This is historical
operator-reported use, not a pin-by-pin bench validation. The Blue Pill and
current ESP32-C3 are to be replaced in Rev-D; this does not establish which
Rev-C controller/module circuits are present in production.

The user selected the 2026-08-28 production PCB plus its same-day Gerber and
drill files as the manufacturing reference. KiCad 10.0.5 reports the supplied
PCB as 50.00 x 100.00 mm with 57 components; the repository PCB is 100.00 x
102.57 mm with 163 components. The supplied folder's BOM/Gerbers/drills are
dated 2026-08-28, while its editable schematic is dated 2026-09-21. Its
exported schematic netlist has 57 components and 83 nets, compared with 64
components and 105 nets in the repository schematic export. These differences
show the production package and repository project are not interchangeable;
the later schematic is explicitly unverified against the selected
manufacturing release. No production files were copied into or modified in
the repository, and no visual comparison to the physical HAT has been made.
**User disposition:** this source-file difference is non-blocking for
continuing Rev-C work. Use the supplied production PCB/Gerber/drill set as the
manufacturing reference; retain the physical-board and schematic-to-release
correlation as unresolved evidence, not a reason to stop the next check.

### Rev-C carry-forward and bench plan (user direction, 2026-10-09)

The user reports no known issues in prior HAT use and says most circuits were
plug-and-play. Treat the existing circuits as reuse candidates; do not require
a full Rev-C requalification before beginning Rev-D integration. Reuse or
replace circuits as integration/source review requires. This operating
assumption is not a new bench-validation claim.

| Item | Plan | Status / gate |
|---|---|---|
| Existing Rev-C circuits | Carry forward as candidates based on prior operation; recheck only where Rev-D changes, source conflicts, or integration findings make it necessary. | Working assumption, not fresh measured evidence. |
| Existing ESP32-C3 | No Rev-C test planned; the current Blue Pill and C3 are being replaced. | The old C3 was not tested. Validate the new Rev-D module and interface on Rev-D. |
| Fan path | At the next bench window, with power removed and the fan disconnected, identify the installed header and trace its ground, supply, and switched pins against the selected production PCB pack. With no fan attached, use a current-limited supply and controlled PB5 gate-drive test to check off/on levels. Then connect a correctly rated fan, verify start/stop, and record supply voltage and steady/startup current. | The fan circuit has not been tested. This is the only specific Rev-C circuit test the user identified; it does not block starting integration. The repository fan net names are not proof of the production-board mapping. |
| Unresolved ISET/fault/shift-register source conflicts | Do not claim polarity, fault routing, or an all-off word as verified. Decide reuse/replace during integration and resolve any required safe-state design before release. | No extra Rev-C bench campaign requested at this stage. |

The current firmware pin contract does not define a fan-control test command;
prepare a controlled PB5 test method before powered switching, and do not
drive the gate with an improvised live jumper. No tach feedback is present in
the repository Rev-C fan contract.

Until a physical-board/order identity or source-to-manufacturing mapping is
established, the table below describes the repository schematic/netlist only.
Do not use its disputed PCB-only circuitry or the supplied later schematic as
a Rev-D copy source. Issue #62's documented physical board and authoritative
design source are for the separate **Regulator Rev-C**, not this HAT. Do not
merge designators or circuitry between those two boards.

| Rev-D reuse block | Status | Source of truth / evidence and disposition |
|---|---|---|
| Power stage | Unknown / not on HAT | The HAT Rev-C schematic/netlist does not contain the regulator power stage. Issue #62 identifies the separate Regulator Rev-C source as `hardware/kicad/dsp-regulator-rev-c/DSP-Regulator-RevC.net`; use that board's schematic/netlist, not HAT designators, for regulator circuitry. Physical assembly and probe-to-net mapping still need bench confirmation. |
| ISET DAC/control | Conflicting | The HAT schematic/netlist routes `ISET_MPU_5V` and `ISET_MPU_3V3` directly between STM32 U11 PA0/PA1 and J12. It contains no DAC or AW9523 path for these nets. Rev-C has no MCU control for CH3; PA2 as CH3 ISET is a Rev-B leftover, while PA2/PA3 are the ESP32-C3 link pair. This is the source for the current connector signal contract only; the intended Rev-D analog/control circuitry is not established. |
| Shift-register latch | Conflicting | The HAT schematic/netlist has `SR_Latch` on U11 PA4 only; ERC calls the label isolated, and there is no shift-register device or data/clock path. The PCB has U7 74HC595D, which is not in the schematic/netlist. Do not copy the PCB-only circuit until board/source identity is resolved. |
| I2C / AW9523 | Conflicting | The HAT schematic/netlist routes PB8/PB9 to J10 and U13 (AHT20); it contains no AW9523 or AW9523 interrupt net. The documented U5/P0.x screenshot mapping is not supported by the current HAT source and is not a Rev-D source. |
| Fan | Confirmed (schematic only) | HAT Q9 is AO3400A: PB5 -> R63 -> Q9 gate, with R64 pull-down; Q9 switches the D11/J7 pin-1 path and J7 pin 2 is `+5V_Boot`. No tach net is present. This confirms the designed HAT fan path, not the installed assembly. |
| W25Q flash | Confirmed (schematic only) | U12 is W25Q128JVSIQ; `Memory_CS` is U11 PA8 to U12 CS, with SPI SCK/MISO/MOSI on PA5/PA6/PA7. It is external data flash, not MCU executable memory. |
| UDI | Confirmed (schematic only) | U11 PB10/PB11 route through 33-ohm series resistors to J9 as `DISP_UART_TX/RX`. |
| CH340 debug | Confirmed (schematic only) | U16 CH340C routes UART1 (`UART1_TX/RX`, U11 PA9/PA10) and USB D+/D- through D14 to J8. |
| ESP32-C3 (U2) | Unknown / incomplete | The schematic identifies U2 as `ESP32_C3_mini`; PA2/PA3 connect to U2 GPIO1/GPIO0, and U2 has `+5V_Boot` and GND. Its 3.3V pin is unconnected, CTS/RTS labels are isolated, and ERC reports missing footprint-library and symbol-mismatch warnings. The exact module, power design, footprint, and intended functions require verification before reuse. |

Issue #62's HAT-relevant Q9/Q3/Q12/U4 findings resolve as follows in the
current HAT source: Q9 is the AO3400A fan switch; Q3, Q12, and U4 are absent
from the schematic and fresh/committed HAT netlists. The HAT PCB still has
PCB-only Q3/U4 circuitry, confirming a schematic-to-PCB source conflict.
The issue #62 Regulator Rev-C findings (Q3/Q9/Q12/U4 in that board's
namespace) do not identify HAT parts and cannot resolve this HAT PCB conflict.

The `Rev-C: KiCad ERC` task (KiCad 10.0.5, all severities) produced 82
warnings and 0 errors. They comprise 34 footprint-library issues, 33
symbol-library issues, 5 symbol/library mismatches, and 10 isolated-label
warnings. Relevant findings include isolated `SR_Latch`, `FAULT_WARNING_SUM`,
`CTS_ESP`, and `RTS_ESP` labels; U2 has a missing footprint-library entry and
symbol mismatch. The committed `ERC.rpt` (2026-08-14, 0 warnings) is stale
relative to this run. ERC does not prove board population, connectivity,
electrical adequacy, or safe behavior.

Source correlation remains useful but is non-blocking by user direction; use
the selected production PCB pack as the manufacturing reference and do not
assume the physical HAT or later schematic has been matched to it. The current
ESP32 is replaced for Rev-D. The fan check above is deferred and does not block
integration. Resolve any ISET/fault/shift-register uncertainties that affect
Rev-D implementation during integration, and confirm the separate regulator
board's power-stage net/probe mapping against its own identified assembly.
No Rev-C KiCad source was edited for this reconciliation.

## Canonical pin matrix

| Logical function | MCU pin | Direction | Electrical role | Owning hardware net label | Firmware symbol/constant | Status |
|---|---|---|---|---|---|---|
| Rail 5V control | PA0 | Output | GPIO | `ISET_MPU_5V` | `PIN_ISET_5V` | Implemented (aligned) |
| Rail 3V3 control | PA1 | Output | GPIO | `ISET_MPU_3V3` | `PIN_ISET_3V3` | Implemented (aligned) |
| Channel 3 control | None | — | No MCU control | — | `PIN_ISET_CH3 = -1` | Not present on Rev-C; PA2 CH3 ISET is a Rev-B leftover |
| ESP32-C3 serial link | PA2 / PA3 | Bidirectional pair; direction TBD | UART signals | `TXD_ESP` / `RXD_ESP` | (not in this firmware contract) | Routed to U2 GPIO1/GPIO0; module/interface not verified |
| Fault summary input | (none verified) | — | Fault path | `FAULT_CRITICAL_SUM` -> R65/R84/TP10; no MCU or expander node | `PIN_AW9523_INT = PB7`, `PIN_FAULT_CRITICAL_SUM = -1` | Mismatch: current HAT source has no AW9523/INT or routed fault input |
| Shift-register latch | PA4 | Output | GPIO label only | `SR_Latch` | `PIN_SR_LATCH` | Mismatch: label ends at U11; isolated in ERC, no shift-register device/path |
| External flash CS | PA8 | Output | SPI CS (GPIO) | `Memory_CS` | `PIN_FLASH_CS` | Routed to U12 CS; firmware alias differs from net label |
| CH340 debug TX | PA9 | Output | UART1 TX | `UART1_TX` | `SerialDbg` TX (`Uart SerialDbg(PA10, PA9)`) | Implemented (aligned) |
| CH340 debug RX | PA10 | Input | UART1 RX | `UART1_RX` | `SerialDbg` RX (`Uart SerialDbg(PA10, PA9)`) | Implemented (aligned) |
| Display-link TX | PB10 | Output | USART3 TX | `DISP_UART_TX` | `SerialU3` TX (`Uart SerialU3(PB11, PB10)`) | Implemented (aligned) |
| Display-link RX | PB11 | Input | USART3 RX | `DISP_UART_RX` | `SerialU3` RX (`Uart SerialU3(PB11, PB10)`) | Implemented (aligned) |
| Telemetry I2C clock | PB8 | Bidirectional | I2C SCL | `I²C SCL_0` | `I2C_SCL_PIN` | Routed to J10 and U13 AHT20; no AW9523 |
| Telemetry I2C data | PB9 | Bidirectional | I2C SDA | `I²C SDA_0` | `I2C_SDA_PIN` | Routed to J10 and U13 AHT20; no AW9523 |
| Fan gate control path | PB5 | Output (intended) | GPIO | `PB5` -> `R63` -> `Net-(Q9-G)` | (not yet defined in firmware) | Schematic/netlist path confirmed; PCB assembly not verified |
| Fan tach feedback | (none) | Input (N/A) | Tach input | (none on J7 in current Rev-C netlist) | (none) | Deprecated/not present on current Rev-C |

## Fan contract (Issue #29)

Current Rev-C netlist contract:
- `J7` is a 2-pin **Fan Control** connector.
- `J7` pin 2 is on `+5V_Boot`.
- `J7` pin 1 is on `Net-(D11-A)` (switched path).
- No dedicated fan tach net is present on J7 in this revision.

Policy for this revision:
- Fan control is currently documented as **open-loop** (no tach feedback).
- Any tach/RPM feature work requires a hardware-net addition and a follow-up
  pin-contract update in this file before firmware work starts.

## Superseded AW9523/Q3/Q9/Q12 mapping

The former U5 AW9523 P0.x table was screenshot-sourced and does not match the
current Rev-C HAT schematic or fresh/committed netlists. Those sources contain
no U5/AW9523, Q3, or Q12. Their Q9 is the fan MOSFET described above, not the
range-switch MOSFET shown in the screenshot. The screenshot mapping and
firmware range/fault behavior must not be treated as this revision's hardware
contract. Issue #62 concerns the separate Regulator Rev-C board and does not
resolve the HAT mapping.

The current export's `FAULT_CRITICAL_SUM` reaches R65, R84, and TP10, but not
U11 or an expander; `FAULT_WARNING_SUM` is an isolated global label. Keep
unavailable direct fault inputs explicitly disabled until a verified board
source and route are supplied.

## Drift-check workflow (for PRs touching STM32 pins)

1. Update this table first when a pin/net assignment changes.
2. Verify firmware constants:
   - `rg -n "Uart Serial|PIN_|I2C_" stm32-bluepill-bringup/src/main.cpp`
3. Verify netlist labels:
   - `rg -n "\\(name \\"ISET_MPU_5V\\"\\)|\\(name \\"ISET_MPU_3V3\\"\\)|\\(name \\"ISET_MPU_Channel_3\\"\\)|\\(name \\"FAULT_CRITICAL_SUM\\"\\)|\\(name \\"UART1_TX\\"\\)|\\(name \\"UART1_RX\\"\\)|\\(name \\"DISP_UART_TX\\"\\)|\\(name \\"DISP_UART_RX\\"\\)|\\(name \\"I²C SCL_0\\"\\)|\\(name \\"I²C SDA_0\\"\\)|\\(name \\"PB5\\"\\)" hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`
4. Ensure any mismatch is explicitly marked in **Status** as either:
   - `Implemented (aligned)`,
   - `Mismatch` (with reason), or
   - `Planned/board-wired` / `Deprecated`.
5. If this table changes, update related references in:
   - `docs/GPIO_PINOUT.md`
   - `docs/FIRMWARE_DEVELOPMENT_PLAN.md` (if behavior/scope changed)
