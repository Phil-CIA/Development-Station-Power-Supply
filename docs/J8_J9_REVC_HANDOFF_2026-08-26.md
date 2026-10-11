# J8/J9 Rev-C Connector Handoff - 2026-08-26

Purpose: restart the Rev-C coplanar connector work at the exact blocker that must be cleared before PCB purchase review can proceed.

**Follow-up hold:** The fan interface decision in this handoff is superseded by [FAN_TACH_REV_C_HANDOFF_2026-08-28.md](FAN_TACH_REV_C_HANDOFF_2026-08-28.md). The next HAT prototype must review a 3-pin fan/tach connector before the HAT PCB package is ordered.

## Current decision state

Decision: **Hold** for PCB purchase or final connector/layout release. The electrical connector mapping is now implemented and passes the current netlist/DRC/ERC checks, but the selected connector parts and mechanical contract are not yet fully reviewed.

Reason: the connector references changed during the edit, so the old J8/J9 names are no longer the active regulator-side references. Regulator J6/J7 now map to HAT J18/J12, but the connector family, exact manufacturer PNs, datasheet links, mating parts, and mechanical/current validation still need closure.

Use [PCB_PURCHASE_REVIEW_PROCESS.md](PCB_PURCHASE_REVIEW_PROCESS.md) as the release gate for this work.

Use [J7_J12_POWER_HARNESS_CURRENT_TEST_2026-08-27.md](J7_J12_POWER_HARNESS_CURRENT_TEST_2026-08-27.md) to calculate and capture the rail-current budget, return-current, voltage-drop, and temperature evidence before bench testing.

**Architecture clarification:** HAT U15 (`AMS1117-3.3`) is the single source of `+3.3V Boot`; the regulator board consumes that rail through J7/J12. The shared `+3.3V Boot` harness is therefore an outbound HAT supply to a regulator-board load, not a connection between two regulator outputs.

The upstream 12 V source is the user's 12 V, 30 A SMPS, entering the regulator board at J4. J4 is identified in the current regulator netlist as Phoenix Contact `1935161`, Digi-Key `277-1667-ND`. The 30 A value is the source capacity; the J7/J12 harness must still be sized from its actual downstream 12 V loads.

For the current architecture, regulator-side LM2596/comparator 12 V current is local to the regulator board and does not pass through the J7/J12 cable. The J7/J12 `+12V` conductor carries the downstream HAT fan branch through JP1: 60 mA nominal, with 120 mA as the provisional startup/design screen.

Q9 is recorded there as the provisional fan-switch screen: `AO3400A`, LCSC `C20917`, metadata `Id = 5.7 A`. The proposed GDSTIME fan listing is 12 V, 60 mA, 0.5 W, 3-pin, ASIN `B0DHKJRQL8`. The former 2-pin power/ground-only decision is superseded; the next HAT prototype must add and review a dedicated tachometer input. The schematic and PCB currently implement optional 3-pad open solder jumper `JP1`: pad 1 = `+5V_Boot`, pad 2 = fan-positive output to J7 pin 2, and pad 3 = `+12V`; bridge pad 3 to pad 2 for the selected 12 V fan and leave pad 1 open. Never bridge both sides together. Q9's 5.7 A value is a MOSFET capability ceiling, not the fan load or an approval to carry 5.7 A through the Dupont power harness. See `FAN_TACH_REV_C_HANDOFF_2026-08-28.md` before ordering.

Fan connector decision is superseded by the 2026-08-28 tach implementation: retain HAT fan J7 as the horizontal 3-pin 2.54 mm header, BOOMELE `2510-3A`, LCSC `C29275`. Use a verified 3-position 2.54 mm mating termination for all fan conductors. J7.1 is switched return, J7.2 is JP1-selected fan positive, and J7.3 is `Fan_Tach` to STM32 U11 PA0 through the 3.3 V pull-up. Do not leave the tach lead unused and do not connect it to 12 V or the Q9 drain.

## Current connector export

Fresh netlist state currently in the worktree:

| Connector | Intended current nets |
|---|---|
| Regulator J6 Signal | pin 1 = `I2C SDA_0`, pin 2 = `I2C SCL_0`, pin 3 = `GND`, pin 4 = `AW9523_INT` |
| Regulator J7 Power | pin 1 = `GND`, pin 2 = `+5V_Boot`, pin 3 = `+3.3V Boot`, pin 4 = `+12V`, pins 5-6 = `GND` |
| HAT J18 Signal | pin 1 = `I2C SDA_0`, pin 2 = `I2C SCL_0`, pin 3 = `GND`, pin 4 = `AW9523_INT` |
| HAT J12 Power | pin 1 = `GND`, pin 2 = `+5V_Boot`, pin 3 = `+3.3V Boot`, pin 4 = `+12V`, pins 5-6 = `GND` |

The HAT and regulator mappings above are electrically one-for-one in the current netlists. J12 is retained as the HAT power connector reference; its legacy name must not be confused with the old provisional 12-pin harness map.

## Proposed cable contract

The selected prototype cable source is the [pzsmocn Dupont 2.54 mm cable listing](https://www.amazon.com/pzsmocn-Dupont-2-54mm-4-Pin-Connector/dp/B0DWX5T76M), Amazon ASIN `B0DWX5T76M`, sold by Pzsmocn-US. The linked item describes 20 cm, silicone, female-to-female Dupont cables with 2.54 mm pitch and 24 AWG conductors in the 4-pin variant. Use the corresponding conductor-count variant for each harness: 4 conductors for the signal harness and 6 conductors for the power harness. Record the separate 6-pin variant or ASIN when selected; do not assume the linked 4-pin ASIN includes a 6-pin cable.

The listing claims 2 A AC/DC cable current, but this is seller-product information rather than a manufacturer qualification datasheet. Use it as a procurement reference and verify the actual cable/contact construction when received.

| Harness | 24 AWG Dupont status | Conditions |
|---|---|---|
| J6 to J18 signal | Acceptable for prototype signal use | Use the 4-conductor female-to-female variant. Keep `I2C SDA_0`, `I2C SCL_0`, `AW9523_INT`, and signal ground together; confirm ribbon orientation and pin 1 before power-up. |
| J7 to J12 power | Conditional, not yet released | Use the 6-conductor female-to-female variant. Calculate maximum current per rail, cable length, bundle derating, voltage drop, connector contact rating, and temperature rise. Current netlists assign pins 4, 5, and 6 to GND; verify whether all three are intentionally used as parallel returns. |

Do not approve the 24 AWG power cable solely from the wire gauge or the listing's claimed 2 A rating. The design must show that the worst-case `+5V_Boot`, `+3.3V Boot`, and `+12V` currents, plus the return current, remain within the wire, contact, header, and temperature limits. Use the external bench current limit for any first cable test.

## Connector part review table

The electrical mapping is verified, but the connector purchase review is not complete until every row has an exact orderable PN, exact datasheet, and checked mating part.

| Ref | Role | Current value/footprint state | Exact manufacturer PN | Exact datasheet | Mating part | Status |
|---|---|---|---|---|---|---|
| Regulator J6 | Signal source | Vertical SMD 4-pin header footprint | hanxia `HX PZ2.54-1x4P TP-YQ`, LCSC `C41417361` | [LCSC datasheet](https://www.lcsc.com/datasheet/C41417361.pdf) | 4-conductor 2.54 mm signal cable; exact mate TBD | CONDITIONAL |
| HAT J18 | Signal mate | Horizontal 4-pin header footprint | XFCN `PZ254R-11-04P`, LCSC `C492412` | [LCSC datasheet](https://www.lcsc.com/datasheet/C492412.pdf) | 4-conductor 2.54 mm signal cable; exact mate TBD | CONDITIONAL |
| Regulator J7 | Power source | Vertical SMD 6-pin header footprint | hanxia `HX PZ2.54-1x6P TP-YQ`, LCSC `C41417363` | [LCSC datasheet](https://www.lcsc.com/datasheet/C41417363.pdf) | 6-conductor 2.54 mm power cable; exact mate TBD | CONDITIONAL |
| HAT J12 | Power mate | Horizontal 6-pin header footprint | XFCN `PZ254R-11-06P`, LCSC `C492414` | [LCSC datasheet](https://www.lcsc.com/datasheet/C492414.pdf) | 6-conductor 2.54 mm power cable; exact mate TBD | CONDITIONAL |

Do not treat a generic `PinHeader` or `PinSocket` library name as a manufacturer PN. For this prototype, the Amazon ASIN is the cable procurement reference, not a connector manufacturer PN. The review must still verify the board-side 2.54 mm pin geometry, exposed pin length, shroud/interference, polarity/orientation, retention, and that the female-to-female cable mates with both board-side parts without relying on an undocumented mechanical assumption.

### Cable decision inputs still required

Record these values before approving the power cable:

- Maximum simultaneous current on `+5V_Boot`, `+3.3V Boot`, and `+12V`.
- Maximum cable length and expected ambient temperature.
- Whether the two ground conductors are physically paralleled end-to-end.
- Dupont contact and header current rating for the selected parts.
- Calculated voltage drop and dissipation for each rail and return conductor.
- Cable bundle derating and acceptable temperature rise.

Until those values are recorded, the 24 AWG power harness remains **conditional**.

### Preliminary 20 cm cable calculation

For screening only, use approximately 0.084 ohm/m for 24 AWG copper. A 20 cm outbound conductor is therefore approximately 16.8 mOhm. If J7/J12 power pins 5 and 6 are paralleled as the return, the two return conductors are approximately 8.4 mOhm in parallel, before contact resistance.

Approximate cable-only loop resistance for any one rail is therefore:

`R_loop ~= 16.8 mOhm + 8.4 mOhm = 25.2 mOhm`

Approximate cable-only drop is:

`V_drop ~= I_rail * 25.2 mOhm`

| Current in one rail | Approximate cable-only drop | Approximate cable-only loss |
|---:|---:|---:|
| 0.12 A | 3.0 mV | 0.36 mW |
| 0.50 A | 12.6 mV | 6.3 mW |
| 1.00 A | 25.2 mV | 25.2 mW |
| 2.00 A | 50.4 mV | 100.8 mW |

These figures exclude both Dupont contact resistances, PCB/header resistance, temperature effects, cable-bundle derating, and supply-transient margin. They are not a current rating. The existing 118 mA bench input observation is useful as a low-load reference, but it does not establish the maximum current of each J7/J12 rail.

## Working contract to freeze before schematic release

Use the following as the electrical contract to carry into the connector part-selection and mechanical review pass.

| Connector | Recommended pinout |
|---|---|
| Regulator J6 / HAT J18 Signal | pin 1 = `I2C SDA_0`, pin 2 = `I2C SCL_0`, pin 3 = `GND`, pin 4 = `AW9523B ~INT` |
| Regulator J7 / HAT J12 Power | pin 1 = `GND`, pin 2 = `+5V_Boot`, pin 3 = `+3.3V Boot`, pin 4 = `+12V`, pins 5-6 = `GND` |

Rationale:

- `AW9523B ~INT` is the cleanest way to expose asynchronous fault/state notification without forcing the HAT to poll I2C; it keeps the four-conductor signal contract deterministic.
- J7/J12 should not be frozen with a single return contact. The current netlists assign three ground pins in the six-conductor power harness; verify the intended parallel-return allocation and do not assume the contacts share current evenly.
- This preserves the existing regulator-local ownership boundary: all raw Kelvin sense, shunt, comparator, and range-node nets remain local to the regulator board, while the harness carries only the required rail and control signals.

Current implementation status:

- Regulator J6 is a 4-pin signal connector and regulator J7 is a 6-pin power connector.
- HAT J18 is a 4-pin signal connector and HAT J12 is a 6-pin power connector.
- Current fresh netlists show matching pin counts and pin order across the two boards.
- Current fresh ERC reports are 0 errors / 0 warnings for both boards.
- Current fresh DRC reports are 0 violations, 0 unconnected pads, and 0 footprint errors for both boards.
- Connector exact PNs, datasheet links, mating parts, current ratings, keying, and mechanical fit are still open.

## Goal for next work session

Freeze the Rev-C coplanar harness contract enough that schematic cleanup, ERC/netlist regeneration, PCB placement, and the purchase review worksheet can proceed.

Minimum success criteria:

1. Select final connector families or explicit provisional review candidates for signal and power.
2. Verify the exact manufacturer PNs and datasheet links for J6/J7/J18/J12.
3. Verify each symbol and footprint against the selected connector package and mating part.
4. Verify every regulator connector pin maps one-for-one to the HAT connector pin.
5. Remove or explicitly disposition legacy HAT crossings that no longer belong in the harness.
6. Recalculate current, return capacity, wire gauge, cable length, bundle derating, and connector derating for the planned 24 AWG power cable. Confirm whether J7/J12 pins 4-6 are all parallel ground returns.
7. Regenerate fresh netlists after any connector metadata or wiring edits.
8. Record the resulting pinout as the current harness contract.

## Do not carry forward

Do not use the older 12-pin J8 / 12-pin J9 provisional map as a cable contract. That map is retained only as migration history in [DEV_STATION_HANDOFF_2026-08-17.md](DEV_STATION_HANDOFF_2026-08-17.md).

Do not route raw Kelvin sense, shunt terminals, comparator threshold nodes, regulator feedback nodes, or range-switch current paths through the coplanar harness unless a new review explicitly reverses the regulator-local ownership decision.

## Decisions to make before editing too far

1. Signal harness pin count:
   - Minimum: `I2C SDA_0`, `I2C SCL_0`, `GND`.
   - Open decision: add AW9523B `~INT` as a fourth signal pin for asynchronous fault/state notification, or rely on I2C polling.
2. Power harness return allocation:
   - Current logical map has only one explicit `GND` pin.
   - Recalculate current and return capacity before freezing J9.
3. Connector family and physical orientation:
   - Confirm current rating, voltage rating, wire gauge, keying, latch access, cable bend direction, board-edge clearance, probe access, and serviceability.
4. HAT legacy cleanup:
   - Remove orphaned `VSENSE_*` crossings.
   - Remove direct `ISET_MPU_*` / range-control crossings if those functions remain regulator-local through AW9523B.

## Suggested execution order

1. Open both Rev-C schematics in KiCad and visually confirm J6/J7/J18/J12.
2. Confirm the 4-pin signal and 6-pin power contract is intentional.
3. Verify exact connector PNs, datasheets, mating parts, footprints, pin 1, keying, and orientation.
4. Wire matching pins one-for-one if any metadata or connector choice changes.
5. Remove orphaned HAT analog/range crossings that contradict regulator-local ownership.
6. Recalculate power return capacity and harness derating.
7. Regenerate both netlists.
8. Run targeted netlist checks for connector pins, legacy orphan nets, and regulator-local analog ownership.
9. Confirm fresh ERC/DRC reports remain clean.
10. Update the harness contract and purchase-review evidence.

## Targeted validation checks

After edits, prove these statements from netlists or schematic inspection:

- Regulator J6 signal pin count matches its footprint pin count.
- Regulator J7 power pin count matches its footprint pin count.
- HAT J18 signal connector has the same pin count and pin order as regulator J6.
- HAT J12 power connector has the same pin count and pin order as regulator J7.
- Each selected connector PN has an exact manufacturer datasheet link.
- Each connector symbol and footprint matches its selected PN and mating part.
- 24 AWG signal cable is documented as the prototype signal-harness choice.
- 24 AWG power cable passes a documented current, voltage-drop, contact-rating, three-conductor return-path, and temperature check.
- `I2C SDA_0` and `I2C SCL_0` cross the signal harness exactly as intended.
- Power rails and grounds cross the power harness exactly as intended.
- No raw `VSENSE_*` Kelvin nets cross between HAT and regulator.
- INA2180, TLV1702, TLV9352, INA3221, AW9523B, shunt Kelvin, OCP, feedback selector, and range-switch paths remain regulator-local.
- ERC reports are current or clearly marked unavailable.

## Stop conditions

Stop and do not continue into PCB placement if any of these happen:

- Exact connector PN, datasheet, mating part, or footprint is still unverified.
- Symbol and footprint do not match the selected connector PN.
- HAT and regulator pin order do not match exactly.
- Power return capacity is still unknown.
- AW9523B `~INT` decision is unresolved but would change the connector pin count.
- Legacy analog/range-control crossings remain without an explicit disposition.

## Next restart prompt

Resume from [J8_J9_REVC_HANDOFF_2026-08-26.md](J8_J9_REVC_HANDOFF_2026-08-26.md). The electrical connector mapping is implemented as regulator J6/J7 to HAT J18/J12. Finish the remaining connector release work: verify exact PNs and datasheets, confirm symbols/footprints and mating parts, validate current/return capacity and mechanical fit, remove orphaned legacy HAT crossings, rerun netlists/ERC/DRC, and record the frozen harness contract before PCB purchase release.
