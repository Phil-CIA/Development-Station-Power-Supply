# J7/J12 Power Harness Current Test - 2026-08-27

Purpose: measure the actual current carried by the 24 AWG, 6-conductor Dupont power harness before approving it for Rev-C use.

Related handoff: [J8_J9_REVC_HANDOFF_2026-08-26.md](J8_J9_REVC_HANDOFF_2026-08-26.md)

**Tachometer change hold:** The previously accepted 2-pin fan interface is superseded for the next Rev-C prototype spin. See [FAN_TACH_REV_C_HANDOFF_2026-08-28.md](FAN_TACH_REV_C_HANDOFF_2026-08-28.md). Do not order the current HAT PCB until the 3-pin fan connector and `FAN_TACH` input are reviewed and the fabrication package is regenerated.

## JLCPCB Prototype DFM Screen

The Rev-C prototype PCB files were compared with the published JLCPCB rigid-PCB capabilities on 2026-08-27. Both boards use 1.6 mm FR-4. File inspection found a minimum drilled feature of 0.30 mm, 0.60/0.30 mm vias, 0.15 mm radial via annular copper, and 0.20 mm minimum routed track width. No exposed thermal-pad or via-in-pad construction was found, so the earlier tiny thermal-pad-via risk is not present in these files.

Gerber, drill, BOM, and CPL packages were generated for both boards under their respective `prototype-gerbers/` folders and `prototype-*.csv` files. This is a pre-upload screen only. Upload both ZIP packages to JLCPCB, inspect the online DFM result, and stop for any automatic drill upgrade, outline/slot issue, solder-mask sliver, copper-clearance violation, missing layer, or unexpected footprint change.

## Harness Under Test

| Pin | Net | Conductor role |
|---:|---|---|
| 1 | `GND` | Ground return |
| 2 | `+5V_Boot` | Outbound rail |
| 3 | `+3.3V Boot` | Outbound rail |
| 4 | `+12V` | Outbound rail |
| 5 | `GND` | Ground return, planned parallel return |
| 6 | `GND` | Ground return, planned parallel return |

Pins 1, 5, and 6 are assigned to GND in the current netlist. Pin 2 is `+5V_Boot`, pin 3 is `+3.3V Boot`, and pin 4 is assigned to `+12V`. The present design therefore provides three possible return conductors, but confirm that all three are physically present and intentionally used before testing. Do not assume duplicate net labels automatically mean the cable assembly shares current evenly.

## Design-Current Calculation Before Bench Measurement

The J7/J12 harness is not sized for the external regulated output loads. The current design intent is:

- `+3.3V Boot`: HAT U15 (`AMS1117-3.3`) is the source; the regulator board consumes this rail through the harness for its local analog/control devices.
- `+5V_Boot`: regulator board supplies the HAT board input; HAT U15 converts it to `+3.3V Boot` for both HAT loads and the regulator-board load crossing the harness.
- `+12V`: regulator board supplies its local 12 V devices through the harness; the HAT also uses the rail through JP1 for the optional fan branch.

The upstream source is the user's 12 V, 30 A switching power supply (SMPS), entering the regulator board at J4. The current J4 schematic/netlist entry is Phoenix Contact `1935161`, Digi-Key `277-1667-ND`, with the 12 V source connected to the regulator `+12V` distribution. The 30 A SMPS rating is source capacity only; it is not the required current rating of the J7/J12 Dupont harness branch.

### Confirmed 3.3 V source architecture

HAT U15 (`AMS1117-3.3`) is the single source of `+3.3V Boot`. The regulator board is a consumer of that rail through J7/J12. The current netlists therefore do not represent two paralleled 3.3 V sources. The `+3.3V Boot` harness requirement is the regulator-board load, while HAT U15's input budget must include both the HAT 3.3 V loads and the regulator-board 3.3 V load reflected through the LDO input.

The current HAT netlist shows `+12V` at J12, while the HAT fan connector is a separate `Fan Control` reference. The current fan-positive path is selected through JP1 before reaching J7 pin 2.

The proposed GDSTIME fan listing ([Amazon ASIN `B0DHKJRQL8`](https://www.amazon.com/dp/B0DHKJRQL8)) specifies 12 V, 60 mA, 0.5 W, 3-pin, 40 x 40 x 10 mm operation. Its three leads are power, ground, and tachometer. The HAT netlist now identifies J7 as a 3-pin connector: J7.1 is the Q9-switched fan-negative return, J7.2 is the JP1-selected fan-positive node, and J7.3 is `Fan_Tach`.

Before cable testing, verify the updated fan interface. The schematic now implements an optional 3-pad open solder jumper `JP1`: pad 1 = `+5V_Boot`, pad 2 = fan-positive output to J7 pin 2, and pad 3 = `+12V`. Leave the jumper open by default; bridge pad 2 to pad 3 only when the 12 V fan is installed. Do not bridge pad 1 to pad 2 at the same time, and never bridge both sides together. The intended low-side-switched arrangement is: HAT `+12V` -> JP1 pad 3 -> JP1 pad 2 -> fan positive; fan negative -> Q9 drain; Q9 source -> GND. Connect the fan tachometer lead to J7 pin 3.

For the selected connector path, use HAT J7 as the 3-pin `PinHeader_1x03_P2.54mm_Vertical`, LCSC `C29275`. Select and verify the mating termination for fan positive, fan negative, and tach before powering the fan.

### Q9-based fan screen

The current HAT netlist identifies Q9 as `AO3400A`, LCSC `C20917`, with a metadata field of `Continuous Drain Current (Id) = 5.7 A`. Use this value as the requested provisional Q9-based switch ceiling only. It is not the fan's expected operating current and does not prove that the fan connector, PCB trace, Dupont contacts, or 24 AWG harness can carry 5.7 A.

At 5.7 A, the preliminary 20 cm cable-only drop would be approximately 144 mV before connector resistance, and a Dupont harness should not be approved for that current without a specific contact and thermal qualification. The actual fan current must come from the fan nameplate/datasheet or a controlled measurement. Until then, treat the `+12V` fan requirement as **5.7 A maximum switch-screen / fan current TBD**, not as an approved cable rating.

### SPICE evidence update

The current selector decks were rerun with ngspice on 2026-08-27. The sweep confirms a `5.043 V` local setpoint, local-rail selection when remote sense is low, and remote-sense selection at `5.20 V`. The fault deck confirms that a low or disconnected remote-sense line leaves the output at `5.043 V`; the disconnected no-leakage sense node is approximately `52 uV`.

The modeled disconnected-line leakage sweep injects up to `1 uA`, producing approximately `10 V` at the isolated sense node, while the selector output remains at the local `5.043 V` in this simplified model.

Status: **PASS for the current simplified selector/fault model**. Bench validation remains required for diode leakage, PCB contamination, line capacitance, startup sequencing, and the actual LM2596 and remote-sense network.

### Fan connector part-alignment finding

The current HAT fan connector J7 is a 3-pin `Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical` footprint, LCSC `C29275`. The proposed GDSTIME fan and Dupont cable use the common 2.54 mm pitch family. Confirm the exact received header, mating method, pin 1 orientation, and retention before wiring the fan.

The project decision is to use the installed 3-pin 2.54 mm generic strip header with a verified mating method for all three fan conductors.

Required mating path:

- J7 board header: `PinHeader_1x03_P2.54mm_Vertical`, LCSC `C29275`.
- Fan-side mating path: exact 3-position 2.54 mm housing or jumper termination to be selected and recorded.
- Fan wiring: J7.1 fan negative/switched return, J7.2 fan positive, J7.3 tachometer.

The supplied 2.54 mm fan/Dupont connector is not the selected board mating interface.

Previous alternatives, not selected for this revision:

1. Replace J7 with a verified 2.54 mm connector that mates with the selected fan harness.
2. Use a documented adapter or short pigtail with a different board-side connector.

Current decision: **ACCEPTED FOR PROTOTYPE WITH PIN-ORDER HOLD**. The JP1/Q9 electrical path and 3-pin tach routing are implemented and netlist-confirmed. Assembly still requires the exact mating method, polarity, retention, and physical fit check.

The schematic/netlist identifies the loads, but it does not by itself provide their operating current. Complete the following table from the exact selected-part datasheets. Use maximum or worst-case values for release sizing, and use typical values only for expectation-setting.

### Regulator-side `+3.3V Boot` budget

Current netlist load families on the regulator `+3.3V Boot` side include two INA2180A2 packages, two INA3221 devices, one AW9523B, pull-ups, and local indicator/logic loads. The TLV1702 comparators and LM74502 controllers are not part of this 3.3 V load budget; their supply nets must be reviewed separately. This is the `+3.3V Boot` current that must cross J7/J12 from HAT U15.

### 3.3 V rail part-alignment finding

Resolved in the current regulator netlist: U14-U16 use value `LM74502DDFR` and the datasheet field now points to the TI LM74502 datasheet, `https://www.ti.com/lit/ds/symlink/lm74502.pdf`. The prior `LM74502HDDFR` datasheet-field mismatch is closed for the schematic/netlist review.

Status: **PASS for schematic/netlist alignment**. Before fabrication release, confirm that the BOM/orderable PN also uses `LM74502DDFR` and that its package and pinout match the selected footprint.

| Load group | Quantity | Datasheet maximum or design value per device | Subtotal |
|---|---:|---:|---:|
| INA2180A2 supply current | 2 | ______ mA | ______ mA |
| INA3221 supply current | 2 | ______ mA | ______ mA |
| AW9523B supply current | 1 | ______ mA | ______ mA |
| Pull-ups, LEDs, and other DC loads | - | calculated from values | ______ mA |
| Regulator 3.3 V operating subtotal | | | ______ mA |
| Design margin | | recommended 30% until characterized | ______ mA |
| **Required `+3.3V Boot` harness capacity** | | | **______ mA** |

Do not add the external `+3.3V_Reg` output-load current to this harness budget. That load is on the regulator board's separate output power path, not the `+3.3V Boot` control rail.

### HAT-side `+12V` harness budget

J4 and the regulator board distribute the upstream 12 V source locally. The regulator netlist shows J7 pin 4 on the `+12V` net, and local regulator-side devices such as the LM2596 stage and TLV1702 comparators are connected to that same net on the regulator side of J7. Those local currents do not flow through the J7/J12 harness.

The HAT netlist shows J12 pin 4 feeding JP1 pad 3, which supplies the optional fan branch. Therefore, the `+12V` current carried from regulator J7 to HAT J12 is the downstream HAT fan current, not the full current delivered by the 30 A SMPS or the regulator board's local 12 V loads.

| Load group | Quantity | Datasheet maximum or design value per device | Subtotal |
|---|---:|---:|---:|
| GDSTIME fan branch | 1 | 60 mA nominal; 120 mA provisional startup/design screen | 120 mA |
| HAT-side direct 12 V loads other than fan | - | confirm from netlist | ______ mA |
| `+12V` harness operating subtotal | | | 120 mA provisional |
| Design margin | | recommended 30% until characterized | ______ mA |
| **Required HAT-side `+12V` harness capacity** | | | **156 mA provisional** |

The current fan branch is selected by JP1 pad 3-to-pad 2 and switched on the low side by Q9. The provisional 156 mA capacity is `120 mA * 1.30`; replace it with the fan's verified startup/locked current and any additional HAT-side 12 V load before final release. Do not size the J7/J12 harness from the 30 A SMPS rating or from local regulator-side 12 V current that does not cross the connector.

### HAT-side `+5V_Boot` budget

The HAT `+5V_Boot` rail supplies the HAT input and the input of its AMS1117-3.3. Because U15 is a linear regulator, its input current is approximately its 3.3 V output current, not `P_3V3 / V_5V`. The 5 V source must supply the HAT 3.3 V load plus the regulator-side 3.3 V load crossing J7/J12, plus any direct 5 V loads. U15 dissipation is approximately `(V_5V - V_3V3) * I_3V3` and must be checked separately for thermal margin.

Do not count a device twice. Loads powered from HAT `+3.3V Boot` are represented once in the combined U15 output-current line; only devices directly powered from `+5V_Boot` are listed separately below.

### U15 part-alignment finding

U15 is verified in the fresh HAT netlist as `AMS1117-3.3`, LCSC `C6186`, with the `Package_TO_SOT_SMD:SOT-223-3_TabPin2` footprint and the stored Advanced Monolithic AMS1117 datasheet link. Its connectivity is correct: pin 3 (`VI`) is on `+5V_Boot`, pin 2 (`VO`) is on `+3.3V Boot`, and pin 1 (`GND`) is on ground.

Status: **PASS for schematic/netlist PN, datasheet, footprint, and connectivity alignment**. The remaining check is PCB thermal validation at the combined HAT plus regulator 3.3 V load.

### HAT power-chain part-alignment finding

Resolved in the current source files and fresh generated netlist: placed HAT U16 is `CH340C`, LCSC `C84681`, with datasheet `https://www.lcsc.com/datasheet/C84681.pdf` and footprint `Package_SO:SOIC-16_3.9x9.9mm_P1.27mm`. The prior invalid `C2609442` assignment was corrected in the HAT schematic and regenerated BOM/netlist.

Status: **PASS for schematic/netlist metadata alignment**. Confirm the final JLC assembly preview uses WCH `CH340C`, LCSC `C84681`, and the matching SOIC-16 footprint before fabrication release.

The supplied procurement references partially identify the two modules:

| Reference | Supplied procurement evidence | Alignment result | Remaining blocker |
|---|---|---|---|
| U2 | [Teyleten Robot ESP32-C3 Supermini, Amazon ASIN B0D47G24W3](https://www.amazon.com/dp/B0D47G24W3) | Product family is accepted; active Rev-C symbol/value and custom footprint are now `ESP32_C3__Super_Mini` / `ESP32 C3 Super Mini` | Confirm exact board revision/dimensions, header/pin map, 5 V input path, and current maximum against the received board |
| U11 | [ALMOCN STM32F103C8T6 Blue Pill bundle, Amazon ASIN B0C9PKJF2T](https://www.amazon.com/dp/B0C9PKJF2T) | Board family and the existing Blue Pill datasheet link agree with `STM32_BLUE -`; the listing is a two-board bundle and does not expose a unique board manufacturer PN | Record the exact received board revision, regulator/input-current behavior, and mechanical drawing; verify the placed footprint against the received board |

Electrical identity is provisionally resolved from the listings. The C3 listing states that external power may be applied to its `5V` position and supports `3.3-6 V`; USB and external power must not be used simultaneously. The Blue Pill listing identifies the STM32F103C8T6 device, 5 V USB input, and an onboard RT9193 3.3 V regulator rated by the listing at 300 mA maximum. These facts support the direct `+5V_Boot` topology, but neither listing supplies a controlled maximum input-current specification or a dimensioned mechanical drawing.

U2 and U11 direct `+5V_Boot` load assumptions therefore remain screening values. U2's MCU family and supply range are accepted; verify its received-board pinout, dimensions, header placement, and high-activity current. U11's MCU family and 5 V input path are accepted; verify its received-board revision, dimensions, header placement, regulator current, and high-activity current. The U11 reference remains prototype procurement evidence, not a unique manufacturer PN.

Status: **HOLD for final HAT power-budget and BOM signoff** until U2's received-board pinout/footprint/current behavior and U11's received-board revision/current behavior are recorded.

| Load group | Quantity | Datasheet maximum or design value per device | Equivalent `+5V_Boot` current |
|---|---:|---:|---:|
| ESP32-C3 module/board, direct 5 V input | 1 | ______ mA at selected Wi-Fi/CPU mode | ______ mA |
| STM32 module, direct 5 V input | 1 | ______ mA at selected clock/peripheral mode | ______ mA |
| HAT indicator/other direct 5 V loads | - | calculated from schematic | ______ mA |
| AMS1117 conversion overhead/quiescent current | 1 | ______ mA | ______ mA |
| HAT and regulator 3.3 V load through U15 | - | Includes W25Q128, CH340C, AHT20, HAT logic, and regulator-side `+3.3V Boot`; linear U15 input current is approximately `I_3V3_total` | ______ mA |
| HAT `+5V_Boot` operating subtotal | | | ______ mA |
| Design margin | | recommended 30% until characterized | ______ mA |
| **Required `+5V_Boot` harness capacity** | | | **______ mA** |

Do not use the ESP32 peak radio current as a continuous average without documenting the operating mode. For cable and connector stress, use the maximum credible simultaneous operating condition, including USB activity, display communication, radio transmit, and flash writes if those can overlap.

### Provisional engineering screen

Until the blanks above are populated from exact datasheets, use the existing measured `118 mA` total input only as a low-load sanity reference. It is not a release value for either harness rail. The pre-purchase gate remains open until both rail budgets have numeric maximums and at least 30% documented margin.

For initial harness and connector screening only, use these conservative provisional design limits:

| Harness rail | Provisional design limit | Basis | Status |
|---|---:|---|---|
| Regulator `+3.3V Boot` | 50 mA | Conservative first-pass limit for the listed regulator-side IC/control load families plus margin; replace with verified datasheet sum | Screening only |
| HAT `+5V_Boot` | 800 mA | Conservative first-pass limit from the corrected numeric HAT plus regulator 3.3 V budget below, with margin; not the external 5 V output rating | Screening only |
| HAT-side `+12V` harness | 120 mA provisional fan branch; 156 mA with 30% margin | GDSTIME fan is 60 mA nominal; regulator-side local 12 V loads remain on the regulator side of J7 | Screening only; verify fan startup and any other HAT 12 V load |
| Q9 fan-switch screen | 5.7 A maximum MOSFET rating | AO3400A metadata, LCSC `C20917`; switch rating only, not fan load | Hold; not a cable rating |

These are not claims that the circuits draw those currents. They are temporary upper-bound requirements for checking cable/contact suitability. At these screening limits, the preliminary 20 cm cable drop is approximately 1.3 mV at 50 mA, 17.9 mV at 800 mA, and 3.0 mV at the 60 mA fan nominal current, excluding contacts.

### Combined worst-case harness screen

Using the current provisional limits, the known minimum outbound current is:

`I_known = 50 mA + 800 mA + 120 mA = 0.97 A`

With all three 24 AWG ground conductors used as equal parallel returns, the approximate return resistance for 20 cm is `16.8 mOhm / 3 = 5.6 mOhm`. The approximate loop resistance for each outbound rail is then `16.8 mOhm + 5.6 mOhm = 22.4 mOhm`, before contacts and PCB/header resistance.

| Rail | Provisional current | Approximate cable-only drop |
|---|---:|---:|
| `+3.3V Boot` | 50 mA | 1.1 mV |
| `+5V_Boot` | 800 mA | 17.9 mV |
| `+12V` HAT fan branch | 120 mA provisional | 2.7 mV |
| Combined return | 0.97 A total, before unmeasured sharing effects | 5.4 mV common return drop |

This is an electrical cable-only screen, not approval of the Dupont contacts. The 24 AWG power harness remains conditional until contact rating, crimp quality, connector temperature, and the actual six-conductor cable variant are verified.

### First-pass numeric design budget

This is a screening calculation, not a datasheet signoff. The values below intentionally err high where the exact operating mode or selected-device maximum is not yet captured.

#### Regulator-side `+3.3V Boot`

| Load group | Quantity | Screening current per device | Subtotal |
|---|---:|---:|---:|
| INA2180A2 | 2 | 0.60 mA | 1.20 mA |
| INA3221 | 2 | 1.00 mA | 2.00 mA |
| AW9523B | 1 | 1.00 mA | 1.00 mA |
| Pull-ups, LEDs, and other DC loads | - | calculated/allowance | 3.00 mA |
| Operating subtotal | | | 7.20 mA |
| 30% design margin | | | 2.16 mA |
| **Screening requirement** | | | **9.36 mA** |

Use **50 mA** as the temporary harness design limit until the selected-part maximums and measured startup behavior are entered. The external `+3.3V_Reg` load is excluded.

#### HAT-side `+5V_Boot`

| Load group | Quantity | Screening current | Equivalent `+5V_Boot` current |
|---|---:|---:|---:|
| ESP32-C3 board, high activity case, direct 5 V input | 1 | 350 mA | 350 mA |
| STM32 module, direct 5 V input | 1 | 50 mA | 50 mA |
| Indicators and direct 5 V loads | - | allowance | 10 mA |
| AMS1117 quiescent/overhead allowance | 1 | 15 mA | 15 mA |
| HAT and regulator 3.3 V load through U15 | - | Includes W25Q128, CH340C, AHT20, HAT logic, and regulator-side `+3.3V Boot`; provisional combined load | 160 mA |
| Operating subtotal | | | 585 mA |
| 30% design margin | | | 176 mA |
| **Screening requirement** | | | **761 mA** |

Use **800 mA** as the temporary `+5V_Boot` harness design limit. This rounds the corrected first-pass 761 mA requirement upward and remains below the listing's claimed 2 A cable current, but the Dupont contact rating, actual ESP32 operating profile, and AMS1117 thermal dissipation still require verification.

At the provisional combined 3.3 V load of `160 mA`, U15 dissipation is approximately `(5.0 V - 3.3 V) * 0.160 A = 0.272 W`. Confirm the actual HAT 3.3 V load and U15 PCB thermal performance before treating the 800 mA `+5V_Boot` value as a final release requirement.

### U15 AMS1117 thermal screen

The `+5V_Boot` harness limit is not the same as the U15 output-current limit. U15 only converts the portion of the 5 V input current that becomes `+3.3V Boot`; direct 5 V loads and the 12 V fan branch do not contribute to U15 dissipation.

Use:

`P_U15 = (V_IN - V_OUT) * I_3V3_total`

For the nominal 5 V to 3.3 V conversion, the first-pass dissipation screen is:

| Combined HAT + regulator `+3.3V Boot` current | Approximate U15 dissipation |
|---:|---:|
| 100 mA | 0.17 W |
| 200 mA | 0.34 W |
| 300 mA | 0.51 W |
| 500 mA | 0.85 W |

Compare the resulting junction temperature against the exact U15 datasheet thermal resistance, PCB copper area, ambient temperature, and package limit. Do not assume the AMS1117 can deliver its nominal 1 A rating continuously on a small SOT-223 PCB without a thermal calculation. If the combined 3.3 V load is high, consider a switching regulator or reduce the 5 V to 3.3 V conversion burden before release.

## Safety Rules

1. Power OFF before inserting, removing, or repinning any Dupont cable.
2. Never place a current-mode DMM directly across a voltage rail.
3. Start with the bench supply current limit at 0.10 A to 0.20 A, then increase only as required.
4. Stop for unexpected heating, rail collapse, oscillation, smoke, odor, or a rapidly rising input current.
5. Use a current probe or an inline ammeter connection that cannot short the rail if possible.
6. Keep the Rev-B range-switch bypass and other documented bring-up limits in force.
7. Do not test the full output-load current through J7/J12. This harness feeds the HAT/control board bootstrap rails, not the external regulated load rails.

## Equipment

- Bench supply with adjustable current limit.
- Two DMMs or one DMM plus oscilloscope/current probe.
- Calibrated low-resistance test leads or current shunt if available.
- Thermocouple or IR thermometer.
- Selected 4-pin and 6-pin Dupont cables.
- Camera for connector, pin-order, and temperature evidence.

## Test Configuration

Record before power:

| Item | Required value | Actual |
|---|---|---|
| Cable source | pzsmocn ASIN `B0DWX5T76M` or selected 6-pin variant | |
| Cable conductor count | 6 | |
| Cable gauge | 24 AWG as listed | |
| Cable length | 20 cm as listed | |
| Power source voltage | 12 V nominal | |
| Ambient temperature | Record actual | |
| HAT board revision | Rev-C | |
| Regulator board revision | Rev-C | |
| Bench current limit | Start 0.10-0.20 A | |

Before connecting, photograph the cable orientation and mark pin 1 on both boards and both cable ends.

## Measurement Method

Measure one outbound rail at a time while keeping the other two rails connected normally.

For each rail:

1. Power OFF.
2. Open the selected outbound conductor near one connector.
3. Insert the ammeter or calibrated current shunt in series with that conductor.
4. Verify continuity and polarity with power OFF.
5. Power ON at the conservative current limit.
6. Record startup current, steady-state current, and peak current during the expected worst operating action.
7. Measure voltage at the regulator connector and HAT connector at the same time if possible.
8. Measure temperature at both cable contacts and the cable near each connector after a 5-minute dwell.
9. Power OFF and restore the original conductor connection before testing the next rail.

Do not open the ground return and insert only one ammeter unless the return-current split is understood. For the first run, measure total input current and the outbound rail currents independently, then measure the voltage and temperature of each ground contact if possible.

## Rail Current Results

| Rail | Startup current | Steady current | Peak current | Source-side V | HAT-side V | Drop | Contact/cable temp | Pass/Fail |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| `+3.3V Boot` | | | | | | | | |
| `+5V_Boot` | | | | | | | | |
| `+12V` | | | | | | | | |
| Total input | | | | | | | | |

## Return-Path Results

| Return conductor | Measured current | Contact temperature | Continuity/retention result | Pass/Fail |
|---|---:|---:|---|---|
| J7/J12 pin 3 `GND` | | | | |
| J7/J12 pin 5 `GND` | | | | |
| J7/J12 pin 6 `GND` | | | | |
| Combined return | | | | |

If the three ground conductors are paralleled, verify that their resistance and termination are similar enough that one contact is not carrying most of the return current.

## Voltage-Drop Calculation

For each rail:

`V_drop_measured = V_source_connector - V_HAT_connector`

`R_path_measured = V_drop_measured / I_rail`

Compare the measured result with the preliminary 24 AWG cable-only estimate in the connector handoff. Include both connector contacts and PCB/header resistance in the final result.

| Rail | Peak current | Measured drop | Calculated path resistance | Maximum allowed drop | Margin | Pass/Fail |
|---|---:|---:|---:|---:|---:|---|
| `+3.3V Boot` | | | | | | |
| `+5V_Boot` | | | | | | |
| `+12V` | | | | | | |

Define the maximum allowed drop from the HAT rail operating requirement before declaring pass. Do not use an arbitrary percentage after the measurement.

## Acceptance Criteria

The power harness may move from conditional to accepted only when all are true:

1. The actual 6-pin cable variant and procurement reference are recorded.
2. Pin order and polarity are verified at both boards.
3. Each rail's startup, steady-state, and expected worst-case peak current is recorded.
4. The total return path is verified, including the actual number of parallel ground conductors.
5. Measured rail drop stays within the HAT input operating requirement with documented margin.
6. No connector, crimp, wire, or PCB/header exceeds the selected temperature limit during the dwell.
7. No intermittent contact occurs during a controlled cable movement/retention check with power limited.
8. First-power testing is repeated with the bench current limit and no abnormal behavior.

## Decision

- [ ] Accepted for prototype power harness
- [ ] Accepted with current or temperature derating documented
- [ ] Signal-only; reject for power
- [ ] Hold pending additional measurement

Decision notes:

- Maximum allowed `+3.3V Boot` current: ______ A
- Maximum allowed `+5V_Boot` current: ______ A
- Maximum allowed `+12V` current: ______ A
- Maximum combined return current: ______ A
- Maximum cable length: ______ cm
- Maximum observed temperature rise: ______ deg C
- Evidence photos/logs: ______
