# Stacked Board First Power Baseline — May 10, 2026

## Event Summary
First successful stacked power-up of regulator board + HAT board (ESP32-C6) with feedback divider jumper workaround.

**Status**: ✅ Baseline established; healthy no-load operation confirmed. Ready for load testing phase.

---

## Test Setup

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Bench PSU** | 12.07V input | Input verified 12.05V on multimeter |
| **Current Limit** | 0.5A | Not reached during baseline |
| **USB Power** | Isolated | No USB Vbus backfeed (workaround active) |
| **Feedback Path** | Jumpered before MOSFET | RB-001 temporary fix; pre-MOSFET sense point |
| **Firmware** | hat_c6_i2c_probe | Running on ESP32-C6, GPIO5/GPIO6 I2C bus |
| **INA3221 Addresses** | 0x40, 0x41, 0x43 | All three devices detected and confirmed |

---

## Baseline Measurements (No External Load)

### Input Side
| Measurement | Value | Source | Status |
|-------------|-------|--------|--------|
| Input Voltage | 12.05V | Multimeter (DC) | ✅ Clean, stable |
| Input Current | 50mA | Bench PSU display | ✅ Excellent baseline |
| Input Power | 0.603W | Bench PSU calc (50mA × 12.07V) | ✅ Nominal |

### Output Rails (INA3221 Telemetry)
| Rail | Ch | Voltage | Spec Range | Status | Notes |
|------|----|---------|-----------|---------|----|
| 5V | Ch1 | 5.031V | 4.9–5.1V (±2%) | ✅ Nominal | Excellent regulation |
| 3.3V | Ch2 | 3.322V | 3.234–3.366V (±2%) | ✅ Nominal | Stable, clean |
| Adjustable | Ch3 | 3.322V | TBD (design varies) | ⚠️ TBD | Suspect: same as 3.3V rail; check feedback |

---

## Analysis

### ✅ What Worked
- **No USB backfeed**: Feedback jumper + USB isolation eliminated the 520mA current-limiting issue from previous test
- **Clean idle current**: 50mA from bench PSU indicates healthy quiescent draw (ESP32-C6 + control circuits)
- **Regulated rails stable**: 5V and 3.3V both nominal and holding steady
- **All three INA devices responsive**: Serial monitor confirmed 0x40, 0x41, 0x43 detected on I2C bus

### ⚠️ Items to Investigate
- **Adjustable rail**: Reading 3.322V (same as 3.3V reference) suggests feedback divider not connected or in default state
  - Action: Verify feedback network and potentiometer continuity
  - Expected: Adjustable rail should respond to ISET current or trim input

### 📊 Power Efficiency (No Load)
- Input: 0.603W @ 12.07V
- Estimated losses: 0.603W (idle overhead + regulator quiescent)
- Efficiency: N/A (no output load to measure)

---

## Load Test Results

### Load Test #1 — 100 ohm resistor on 5V rail
| Measurement | Value | Notes |
|-------------|-------|-------|
| 5V rail voltage under load | 5.02V | Stable, no oscillation observed |
| Input current under load | 76mA | PSU reading |
| Baseline input current | 50mA | No-load baseline |
| Delta input current | +26mA | Expected from converter efficiency |
| Estimated output current | 50.2mA | $I = V/R = 5.02V / 100ohm$ |

### Interpretation
- **Pass**: 5V rail remained in spec and stable under resistive load.
- **Power consistency check**: Output load power is ~0.252W (5.02V × 50.2mA). Input delta power from PSU is ~0.314W (12.07V × 26mA), which is consistent with converter losses and control overhead during light-load operation.
- **System behavior**: No reset or current-limit event reported during this test point.

---

### Load Test #2 — 100 ohm resistor on 3.3V rail
| Measurement | Value | Notes |
|-------------|-------|-------|
| 3.3V rail open-circuit voltage | 3.321V | Baseline before applying load |
| 3.3V rail voltage under load | 3.316V | Stable under 100 ohm load |
| Input current under load | 62mA | PSU reading |
| Baseline input current | 50mA | No-load baseline |
| Delta input current | +12mA | Expected at light-load and converter overhead |
| Estimated output current | 33.2mA | I = V/R = 3.316V / 100ohm |

### Interpretation
- **Pass**: 3.3V rail remained in spec and stable under resistive load.
- **Voltage sag**: 5mV from open-circuit to loaded (3.321V to 3.316V), which is a very small drop.
- **System behavior**: No reset or current-limit event reported during this test point.

---

## INA Telemetry Confidence Check

### Snapshot — 3.3V rail with 10 ohm load
Observed bench condition:
- Measured 3.3V rail voltage: ~3.273V
- External load: 10 ohm resistor across 3.3V output
- Expected load current: ~327mA

Serial snapshot from COM11 probe:
- INA 0x41 CH1: 3.304V, 18.0mA
- INA 0x41 CH2: 3.304V, 0.2mA
- INA 0x41 CH3: 11.544V, 252mA

Interpretation:
- **Voltage telemetry looks directionally correct**: 3.304V reported vs. ~3.273V measured is close enough for bring-up.
- **Current telemetry is not yet trustworthy**: expected ~327mA on the loaded 3.3V rail, but neither CH1 nor CH2 is reporting anywhere near that value.
- **Input rail telemetry also looks suspect**: measured input was about 12.02V while INA CH3 reported 11.544V and 252mA, which is directionally plausible under load but not accurate enough to trust yet.

Working conclusion:
- INA address discovery and channel voltage reporting are good enough for development.
- Current-path mapping, shunt values, or channel-to-rail assumptions still need verification before current telemetry can be used for calibration or protection decisions.

Likely causes to check in next board-debug session:
1. CH1/CH2 rail-to-shunt mapping may not match schematic assumptions.
2. Actual shunt resistor values may differ from assumed 20mohm / 200mohm values.
3. Sense routing may not place the expected load current through the measured channel.
4. Library configuration / conversion assumptions may still not match the hardware topology.

---

## Next Test Steps

### Phase 1: Load Testing (100mA increments)
1. Capture INA telemetry snapshots for both completed load points (5V and 3.3V with 100 ohm)
2. Repeat 5V test while watching for stability over a longer dwell time (2-5 minutes)
3. If another 100 ohm resistor is available later, test 5V at ~50 ohm effective load (~100mA)
4. Continue stepping load while checking for sag, resets, and current-limit behavior

### Phase 2: Adjustable Rail Diagnosis
1. Check feedback divider continuity (from output to feedback pin)
2. Verify potentiometer (ISET) range and orientation
3. Measure feedback network voltage vs. potentiometer output
4. If needed, check LM2596 feedback pin (pin 4) for proper divider scaling

### Phase 3: Calibration Offset Tuning
1. Compare INA readings vs. DMM for each rail under load
2. Record voltage offset per rail (INA output voltage - DMM voltage)
3. Calculate calibration trim factors for NVS storage
4. Deploy per-rail calibration firmware

### Phase 4: Current-Limit Mode Validation
1. Gradually increase load on one rail until OCP triggers
2. Verify regulator enters current-limiting (constant current, voltage sag)
3. Confirm firmware detects overcurrent event and logs it
4. Test HICCUP mode (retry after delay) if implemented

---

## Key Reference Documents
- [REGULATOR_BOARD_CHANGE_TRACKER.md](docs/REGULATOR_BOARD_CHANGE_TRACKER.md) — Design issues and workarounds
- [GPIO_PINOUT.md](docs/GPIO_PINOUT.md) — I2C bus and control signal mapping
- [power_telemetry_map.h](src/power_telemetry_map.h) — INA address definitions and rail mapping

---

## Revision History

| Date | Event | Notes |
|------|-------|-------|
| 2026-05-10 | First stacked power baseline | All three rails nominal; feedback jumper active; 50mA idle current; ready for load test |
| 2026-05-10 | Load Test #1 completed | 100 ohm on 5V rail: 5.02V stable, input current 76mA, no reset/current-limit |
| 2026-05-10 | Load Test #2 completed | 100 ohm on 3.3V rail: 3.316V loaded (3.321V open), input current 62mA, no reset/current-limit |
| 2026-05-11 | INA confidence check | Voltage telemetry close enough for bring-up; current telemetry does not match known 10 ohm load and needs mapping/shunt verification |

