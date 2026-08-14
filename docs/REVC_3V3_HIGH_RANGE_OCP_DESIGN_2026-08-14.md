# Rev-C 3.3V High-Range Differential OCP Design

Date: 2026-08-14

Status: First-channel design baseline; schematic implementation and bench validation pending.

## Design target

This circuit replaces the Rev-B U3A absolute-voltage comparison for the 3.3V high-current range.

| Item | Design value |
|---|---:|
| Rail | 3.3V |
| Shunt | R16, 20mΩ |
| Nominal rising trip | 2.00A |
| Nominal reset | approximately 1.97A |
| Fault polarity | Active high into `FAULT_CRITICAL_SUM` |
| Response target | Less than 10µs before fault-bus loading effects |

The 2.00A target follows the current provisional CH2 maximum in `docs/PHASE_5_PARAMETER_INTEGRATION_DESIGN.md`. It must be reviewed before the Rev-C schematic is released.

## Selected signal chain

Use an INA180A2 current-sense amplifier followed by one TLV1704 comparator channel.

### TLV9352 standardization assessment

TLV9352 remains the standard Rev-C regulator feedback-selector op amp, but it should not replace the INA180A2 in this OCP channel.

| Consideration | TLV9352 discrete differential stage | INA180A2 current-sense stage |
|---|---|---|
| Minimum supply | 4.5V; cannot operate from `+3.3V Boot` | Operates from `+3.3V Boot` |
| High-side 3.3V/5V common mode | Valid when TLV9352 is powered from 12V | Rated from -0.2V to 26V |
| Maximum input offset used for design | Up to 1.8mV over specified conditions | 150µV family maximum |
| Gain of 50 | Requires four precision, ratio-matched resistors | Factory trimmed |
| Common-mode rejection | Dominated by external resistor-ratio matching | 84dB minimum family specification |
| Layout sensitivity | Four-resistor differential network | Direct Kelvin input pair |

At R16 = 20mΩ, 1.8mV of TLV9352 input offset is equivalent to 90mA before resistor-ratio, reference, shunt, and comparator errors are included. The INA180's 150µV maximum corresponds to 7.5mA.

The second TLV9352 channel should not replace TLV1704 merely to reduce IC types. TLV9352 has a push-pull output referenced to its supply, while TLV1704 provides the open-drain output required for the 3.3V-pulled fault wired-OR. A TLV9352 powered from 12V would require an additional level-shift or open-drain transistor stage.

**Standardization decision:** Standardize by function rather than forcing one device into every analog role:

- TLV9352: regulator remote/local feedback selectors.
- INA180 family: high-side shunt differential amplification.
- TLV1704: multi-channel threshold comparison and `FAULT_CRITICAL_SUM` wired-OR.

```text
3V3 regulator side                         3V3 load side
      |                                          |
      +----[ R16 20mΩ, Kelvin pads ]-------------+
             | K+                    K- |
             |                          |
             +---- INA180A2 IN+  IN- ---+
                          VS = +3.3V Boot
                          GND = GND
                          OUT = 50 x (VK+ - VK-)
                                    |
                                  10kΩ
                                    +------ TLV1704 +IN
                                    |              OUT ----+---- D4 ---> FAULT_CRITICAL_SUM
                                  470pF                     |
                                    |                      10kΩ
                                   GND                      |
                                                             +3.3V Boot
                                    +------ 1MΩ -------------+

REF3120 2.048V --- 1.50kΩ ----+---- TLV1704 -IN
                                |
                              43.2kΩ
                                |
                               GND
```

The 1MΩ connection from comparator output to `+IN` provides positive feedback. The existing D4 isolation position may be retained, but its polarity and the complete fault-bus truth table must be checked after annotation.

## Component values

| Function | Part/value | Requirement |
|---|---|---|
| Current-sense amplifier | INA180A2IDBVR | Gain 50V/V, SOT-23-5 |
| Amplifier bypass | 100nF X7R | At INA180 VS pin |
| Shunt | 20mΩ, 1%, four-terminal preferred | Use Kelvin routing even if a two-terminal footprint is retained |
| Comparator | One TLV1704 channel | Existing 12V-powered comparator is suitable |
| Comparator input resistor | 10.0kΩ, 0.1% | INA output to comparator `+IN` |
| Comparator filter capacitor | 470pF C0G/NP0 | Comparator `+IN` to GND |
| Hysteresis resistor | 1.00MΩ, 1% | Comparator output to comparator `+IN` |
| Comparator pull-up | 10.0kΩ | Comparator output to `+3.3V Boot`; replace the weak 100kΩ value |
| Threshold reference | REF3120AIDBZR, 2.048V | SOT-23-3; power from `+3.3V Boot` |
| Reference input bypass | 100nF X7R | REF3120 input to GND at the device |
| Threshold upper resistor | 1.50kΩ, 0.1% | REF3120 output to comparator `-IN` |
| Threshold lower resistor | 43.2kΩ, 0.1% | Comparator `-IN` to GND |
| Threshold bypass | 100nF C0G/X7R | Comparator `-IN` to GND |

Do not place ordinary RC series resistors in the INA180 Kelvin inputs without calculating their gain error. Provide optional symmetric 0Ω footprints and a DNP differential capacitor only if bench noise requires input filtering.

## Nominal calculations

At 2.00A:

`VSHUNT = 2.00A * 0.020Ω = 40.0mV`

`VINA = 40.0mV * 50 = 2.000V`

The threshold divider produces:

`VTH = 2.048V * 43.2kΩ / (43.2kΩ + 1.50kΩ) = 1.9793V`

With the comparator output low before a trip, the 10kΩ/1MΩ positive-feedback network gives:

`ITRIP = VTH * (1 + 10kΩ / 1MΩ) / (50 * 0.020Ω) = 1.999A`

After the output releases high to 3.3V:

`IRESET = [VTH * (1 + 10kΩ / 1MΩ) - 3.3V * 10kΩ / 1MΩ] / (50 * 0.020Ω) = 1.966A`

Nominal hysteresis is approximately 33mA.

At the 2A trip point, R16 dissipates:

`PSHUNT = I²R = 2.00² * 0.020Ω = 80mW`

## Accuracy budget to verify

The INA180 family data lists up to 150µV input offset and 0.8% gain error. At R16 = 20mΩ, 150µV corresponds to 7.5mA input-referred current error. Gain error contributes approximately 16mA at 2A. Shunt tolerance, threshold-reference accuracy, resistor ratio, PCB thermal gradients, and comparator offset must be added to establish the final guaranteed trip band.

REF3120 is specified for a 2.55V to 5.5V input, 0.2% maximum initial accuracy, and 20ppm/°C maximum drift from -40°C to +125°C. Its initial accuracy alone contributes up to approximately 4mA of trip uncertainty in this 1V/A signal chain.

The TLV1704 maximum 2.5mV input offset is approximately 2.5mA referred to load current because the signal-chain transimpedance is 1V/A. This remains subject to verification across temperature.

## Layout requirements

1. Route K+ and K- independently from the inner edges of the R16 pads; neither trace may carry load current.
2. Route the Kelvin pair together and away from MOSFET gates, switch nodes, inductors, and fault-bus edges.
3. Place INA180 next to R16. Place its 100nF bypass directly between VS and GND.
4. Keep the INA180 output-to-comparator trace short. Place the 10kΩ, 470pF, and 1MΩ parts at the comparator pins.
5. Return the amplifier, filter capacitor, threshold divider, and reference to a quiet analog ground point, then join that point to the board ground plane away from shunt load-current spreading.
6. Add test points for K+, K-, `OCP_3V3_HIGH_AMP`, `OCP_3V3_HIGH_REF`, and comparator output.

## Fault behavior and limits

- Normal current: `VINA < VTH`; TLV1704 output sinks low; D4 does not assert the fault bus.
- Overcurrent: `VINA > VTH`; TLV1704 output becomes high impedance; the 10kΩ pull-up drives the isolated fault output high.
- Comparator unpowered: its open-drain output is high impedance, so the pull-up tends to assert a fault. Confirm this at the complete fault bus.
- INA180 unpowered or disconnected is not inherently fail-safe and may appear as zero current. Firmware plausibility checks and power-good supervision remain necessary.
- This is a fixed emergency ceiling, not the normal programmable current regulator. Firmware may enforce a lower user setting.

## Bench validation gate

1. Validate with the range MOSFET bypass retained and the bench PSU current limit set below the expected destructive level.
2. Confirm zero-load amplifier output and record offset.
3. Apply 0.5A, 1.0A, and 1.5A; compare measured `VINA` against `1.000V/A` nominal scaling.
4. Ramp through trip at least five times and record rising trip, reset current, response time, and temperature.
5. Repeat at minimum and maximum intended rail voltage and after thermal soak.
6. Apply representative load steps and switching events below 1.8A; require no false trips.
7. Open each Kelvin connection individually and record behavior before deciding whether additional sense-open diagnostics are required.

## Deferred decisions

- Lock the guaranteed 3.3V channel current rating and allowed trip tolerance.
- Confirm the REF3120AIDBZR footprint and local availability during BOM review.
- Decide whether the fixed 2A ceiling is sufficient or a DAC-controlled hardware threshold is required.
- Design the 200mΩ low-current channel separately; reusing gain 50 would saturate near 330mA on a 3.3V amplifier supply.
