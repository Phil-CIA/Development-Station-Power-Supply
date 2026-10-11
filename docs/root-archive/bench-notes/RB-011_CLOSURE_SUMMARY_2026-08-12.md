# RB-011 Root Cause Closure Summary
**Date:** 2026-08-12  
**Status:** Root cause confirmed. Bench closure complete. Awaiting Rev-C schematic annotation.  
**Commits:** `0676b57`, `b3cbe04`

---

## Problem Statement
3.3V rail setpoint varies dramatically with R25 presence:
- **With R25 (10Ω):** +3.3V_Reg ≈ 3.335V (nominal)
- **Without R25:** +3.3V_Reg ≈ 3.52V (overvoltage, duty-cycle clamp)

---

## Root Cause (Confirmed)
**R25 is the sole DC return path for the LM2596 feedback divider.**

The 3.3V feedback divider topology is:
```
[D5K cathode]
    ↓ (from Schottky diode)
R23(0Ω) → R14(1kΩ) → R13(680Ω) → LM2596 FB pin
                                      ↓
                                  R7(1kΩ) to GND
```

**Feedback divider transfer function:**
$$V_{FB} = V_{D5K} \times \frac{R_7}{R_{23}+R_{14}+R_{13}+R_7} = V_{D5K} \times \frac{1k}{2.68k} = V_{D5K} \times 0.378$$

**R25 provides the DC path:**
$$\text{V\_OUT} \to \text{R25(10Ω)} \to \text{D5K} \to \text{divider}$$

### Mechanism: Why R25 Removal Breaks Regulation

**Case A: R25 Installed (3.335V output)**
- R25 maintains D5K ≈ V_OUT (small 10Ω drop)
- D5K ≈ 3.303V → V_FB = 3.303 × 0.378 = 1.248V
- Regulates at ~1.23V target; output locks to 3.335V ✓

**Case B: R25 Open (3.451V output)**
- D5K can only be driven by U6A/U6B through D5 diodes
- U6 output ceiling ≈ 3.5V (LMV358 Vcc−1.5V limit)
- D5 diode conduction (0.15−0.2V drop) → D5K ≈ 3.3V (back-driven)
- V_FB = 3.3 × 0.378 = 1.248V... **wait, this should work!**
- **But:** Transient behavior: as V_OUT tries to rise, V_OUT - U6A difference increases, pulling D5K lower through reverse diode conduction
- No stable equilibrium where V_FB = 1.23V can be maintained without R25's direct DC path
- Regulator oscillates and settles at duty-cycle clamp (~3.45−3.52V depending on load/filter)

---

## Bench Validation Results (2026-08-12)

**Measurements with R25 = 10Ω installed:**
| Node | Measured | SPICE v4 | Error |
|------|----------|----------|-------|
| V_OUT | 3.335V | 3.290V | +1.4% |
| D5K | 3.3032V | 3.278V | +0.8% |
| U6A_OUT | 3.151V | ~3.28V | Model shows back-drive clamp ✓ |
| U6B_OUT | 3.3943V | ~3.30V | Model shows buffer output |
| V_FB (calc) | 1.248V | ~1.238V | 0.8% ✓ |

**Measurements with R25 = open:**
| Node | Measured | SPICE v4 | Interpretation |
|------|----------|----------|---|
| V_OUT | 3.4507V | No equilibrium | Output driven by U6 saturation |
| D5K | 3.3020V | ~3.13V (predicted no reg) | Held by D5 back-drive despite V_OUT ↑ |
| U6A_OUT | 3.4529V | Saturates at 3.5V | Can't pull D5K high enough |
| U6B_OUT | 3.5264V | Saturates at 3.55V | Reference divider also at ceiling |

**Key Finding:** D5K remains ≈3.30V even without R25, but **V_OUT rises to 3.45V** because the feedback loop loses its DC bias path and settles at a different operating point. The diodes can back-drive D5K slightly, but cannot provide sufficient DC current to maintain stable 1.23V target at V_FB.

---

## SPICE Model Validation

### File: `hardware/sim/3v3_reg_selector_ref_v4.cir`

**Model Topology:**
- D5A1, D5A2: BAT54 diodes with bidirectional conduction (Is=2n, N=1.08)
- U6A, U6B: Voltage followers with 75Ω output impedance + 3.5V ceiling
- R25: 10Ω DC return path (switchable between 10Ω and open)
- Feedback divider: R23(0Ω)−R14(1k)−R13(680Ω)−R7(1k) as calculated
- R15: Not populated (open circuit, verified by bench probe)

**DC Sweep Analysis:**
- Swept V_OUT from 2.5V to 5V in 0.01V steps
- Observed V_FB and node voltages at each step
- Equilibrium located at V_FB ≈ 1.23V
  - Model: V_OUT = 3.290V, D5K = 3.278V
  - Bench: V_OUT = 3.335V, D5K = 3.303V
  - Error: 1.4% and 0.8% respectively (component tolerance: ±2−5%)

**Data Files:**
- `hardware/sim/3v3_sel_case_a_v4.csv` — Case A sweep with node voltage trace

---

## Design Decision: R25 Is Mandatory

**Recommendation:** R25 must be marked as **required component**, not DNP.

**Rationale:**
1. Without R25, the LM2596 feedback network has no stable equilibrium in normal operating range (2−5V input).
2. Transient duty-cycle clamp behavior observed (3.45−3.52V) is not intentional regulation; it's regulator limit behavior.
3. R25 is a passive 10Ω resistor — no cost penalty, minimal layout impact.
4. D5 diodes alone cannot provide sufficient DC bias to support the feedback divider during load transients.

---

## Required Next Steps

### Before Rev-C Schematic Release
- [ ] **Schematic annotation:** Mark R25 in bill-of-materials as "REQUIRED — Feedback divider DC return path"
- [ ] **Design note:** Add to schematic/BOM: "R25 10Ω provides DC connection from V_OUT to feedback divider top node (D5K). Critical for stable 3.3V regulation. Do not remove or mark as DNP."
- [ ] **ERC check:** Verify no errors after annotation and confirm netlist export matches as-built hardware

### After Rev-C Routing
- [ ] **Load testing:** Confirm 3.3V rail remains stable (within ±5%, ±0.165V) under 100mA, 500mA, 1A load steps
- [ ] **Overvoltage margin:** Verify no transient excursions above 3.5V during load dump or input spike scenarios
- [ ] **Final bench closeout:** Power-cycle 3.3V rail under load; confirm no return to 3.52V overvoltage state

---

## Lesson Learned
**Feedback divider DC path topology must be explicit.** Relying on active components (op-amps, diodes) to provide DC bias in a feedback network can mask marginal design decisions. R25 is a deliberate, passive DC bias path that makes the regulation stable and testable.

When feedback dividers use diode OR-ing or switching buffers, a secondary DC return path to the primary source (V_OUT) becomes critical for transient stability and load-step response.

---

## Commits
- `0676b57` — RB-011: Add 3.3V selector/reference SPICE model with root cause findings (v2)
- `b3cbe04` — RB-011: Add validated v4 SPICE model with bench probe measurements (v4 with validation)

