# DEV STATION SESSION PLAN — 2026-05-28

Date: 2026-05-28
Project: Development Station Power Supply
Phase: Rev-B HAT — Display power architecture decision + HAT schematic J21 update

---

## Context From Previous Session (2026-05-27)

The previous afternoon session completed the display interface schematic (J21 added, UDI standard defined). An open loop was logged:

> **J21 Display Power (Open Loop):** J21 pin 1 is wired to `+5V_Boot`. The `+5V_Boot` rail feeds the HAT control electronics (STM32, AMS1117, WS2812, pots, expansion header). Adding the CrowPanel's 2A max draw on top risks overloading the 5V channel LM2596S-ADJ (rated 3A shared).

The user proposed this session: **"use the 3rd adjustable channel for +5V boot rail"** — meaning, dedicate the adjustable Channel 3 LM2596S-ADJ to a fixed 5V output and route J21 pin 1 to that supply instead of `+5V_Boot`.

---

## Power Architecture Decision

### Decision

**Route J21 pin 1 from `+5V_Boot` to `+V Adj Channel`.**
Configure the adjustable Channel 3 LM2596S-ADJ (U2) as a fixed 5V dedicated display supply.

### Rationale

| Factor | Detail |
|--------|--------|
| `+5V_Boot` current load | STM32 (U10), AMS1117 (U9 → 3.3V), WS2812 LED (D12), POT1–POT6, J13 expansion header. Estimated ~300–500 mA. |
| CrowPanel 2A max | Adds up to 2.5A total on `+5V_Boot` — dangerously close to the 3A LM2596 limit, with no headroom for transients. |
| `+V Adj Channel` current capacity | LM2596S-ADJ (U2) is 3A rated. No other loads except J21. 2A CrowPanel draw = comfortable. |
| Issue 11 (Adj feedback broken) | The adjustable channel remote sense is currently non-functional. Fixing it at 5V via local feedback resolves Issue 11 by design. |
| Adjustable output sacrifice | The channel was "adjustable" but Issue 11 means it never worked adjustably on the physical board. Fixing at 5V is a net improvement. |

### Power Rail Map (After This Change)

| Rail | Source | Loads |
|------|--------|-------|
| `+5V_Boot` | 5V LM2596 channel (existing) via J2-02 on HAT | STM32, AMS1117, WS2812, POT1–6, J13 |
| `+V Adj Channel` | Adj LM2596 (U2), fixed 5V | J21 pin 1 (display power only) |
| `+3.3V Boot` | AMS1117-3.3 (U9) from +5V_Boot | STM32 logic, CH340C, INA3221s, shift registers, SPI flash |

---

## Session Deliverables

### 1 — HAT Schematic: Change J21 pin 1 net

**File:** `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch`

**Change:** Disconnect J21 pin 1 from the `+5V_Boot` power net and connect it to the `+V Adj Channel` net.

#### KiCad Steps

1. Open `DSP-Regulator-HAT-RevB.kicad_sch` in KiCad schematic editor
2. Navigate to J21 (the 4-pin JST XH display connector — "Display Uart")
3. On J21 pin 1, the existing wire goes to a `+5V_Boot` global power label
4. **Delete the `+5V_Boot` global label** on J21 pin 1
5. **Place a new net label** on J21 pin 1: use a local net label or global power label for `+V Adj Channel`
   - In the schematic the `+V Adj Channel` net already exists (it comes from J2 via the HAT inter-board connector)
   - Add a net label `+V Adj Channel` at J21 pin 1 (use the existing net name exactly, including the space)
6. Verify the `+5V_Boot` net no longer lists J21 pin 1 as a node (check Properties → Net Inspector or re-export netlist)
7. Run ERC — expect 0 errors, 0 warnings

> **Net name note:** The existing net on the HAT is `+V Adj Channel` (with a space between `+V` and `Adj`). Use this exact name when placing the label in KiCad to join the existing net. Do not rename it in this schematic revision.

#### Expected Netlist Change

**Before (in HAT netlist):**
```
(net (code "5") (name "+5V_Boot")
  ...
  (node (ref "J21") (pin "1") ...)   ← remove this
  ...)

(net (code "10") (name "+V Adj Channel")
  (node (ref "C4") ...)
  (node (ref "J2") (pin "06") ...)
  ...no J21...)
```

**After:**
```
(net (code "5") (name "+5V_Boot")
  ...
  ← J21 no longer here
  ...)

(net (code "10") (name "+V Adj Channel")
  (node (ref "C4") ...)
  (node (ref "J2") (pin "06") ...)
  (node (ref "J21") (pin "1") ...)   ← J21 added here
  ...)
```

#### Post-Change Steps

1. Run ERC → expect 0 errors, 0 warnings
2. Re-export netlist: Tools → Generate Netlist → save as `DSP-Regulator-HAT-RevB.net`
3. Run connector contract script:
   ```powershell
   cd C:\Users\forch\GitHub\Development-Station-Power-Supply
   .\scripts\verify-connector-contract.ps1 -Mode reduced
   ```
   Expected: **18/18 PASS** (J21 is not in the contract script; this change does not affect any checked nets)

---

### 2 — DSP Regulator: Fix Adjustable Channel for Fixed 5V Output

**File:** `hardware/kicad/dsp-regulator/DSP Regulator.kicad_sch`

#### Current State of Adjustable Channel Feedback

| Component | Value | Role |
|-----------|-------|------|
| U2 | LM2596S-ADJ | Adjustable channel step-down regulator |
| U8 | Relay/selector | Switches between local and remote sense feedback |
| R26 | **0Ω** | Bottom of feedback path (jumper — shorts feedback to GND node of divider chain) |
| R43 | **2 kΩ** | Part of remote sense divider from HAT |
| `Feedback Adj Channel` | Net on J2 pin 4 | Remote sense input from HAT |

**Issue 11 cause:** R26 = 0Ω creates a short in the feedback divider chain. The local feedback does not correctly set 5V output. The remote sense path (via HAT) is also non-functional (Issue 11 diagnosis).

#### Target: Local Feedback at 5V

For LM2596S-ADJ: **Vout = 1.23 V × (1 + R_upper / R_lower)**

To produce 5.0 V:
- R_upper / R_lower = (5.0 / 1.23) − 1 = **3.065**
- Standard value pair: **R_lower = 240 Ω** (0402/0603), **R_upper = 750 Ω** (0402/0603)
  → Vout = 1.23 × (1 + 750/240) = **5.07 V** ✓ (within 2%)
- Alternate: R_lower = 1 kΩ, R_upper = 3.09 kΩ → 5.02 V

The lower resistor connects from FB pin to GND. The upper resistor connects from `+V Adj Channel` output to FB pin.

#### Schematic Fix Required

The current topology routes feedback through the relay U8 and remote sense path. For the fixed 5V approach:

1. **Remove or DNP (Do Not Populate) U8** — the relay/selector is not needed for fixed-voltage operation
2. **Set R_lower** (directly FB to GND) to 240 Ω
3. **Set R_upper** (directly +V Adj Channel to FB) to 750 Ω
4. **Remove R26 (0Ω) and R43 (2kΩ)** from the remote sense path, or DNP them; add NC flags on `Feedback Adj Channel` (J2 pin 4) if no longer used

> **Note:** This is a schematic redesign for PCB Rev-C. The existing physical board (Rev-B) has the relay topology populated. The bench bring-up uses USB-C for display power; this change is for the production PCB.

#### Hardware Bench Workaround (Until PCB Rev-C)

For bench validation with the existing physical board:
- The CrowPanel continues to receive power from its USB-C port
- Do NOT connect J21 pin 1 to anything on the bench hardware
- This session's schematic change is for the future PCB revision

---

### 3 — Connector Contract Script

No changes to the connector contract script are required. The check for `+V Adj Channel` already passes (the net exists on J2 in both netlists). J21 is not a monitored connector in the script.

---

## Open Loops Updated By This Decision

The J21 display power open loop (logged 2026-05-27) is now **closed with a design decision:**

> **J21 Display Power — Closed:** Use `+V Adj Channel` (LM2596S-ADJ U2, fixed 5V) as the J21 pin 1 source. HAT schematic change: J21 pin 1 net → `+V Adj Channel`. DSP Regulator schematic change: remove relay selector (U8), set local feedback R_lower=240Ω / R_upper=750Ω for 5V output. PCB Rev-C implementation.

### Remaining Open Items (Pre-PCB Rev-C)

| Item | Status | Action |
|------|--------|--------|
| Issue 11 (Adj feedback) | CLOSED by design: channel becomes fixed 5V | Implement R-divider change in DSP Regulator schematic |
| J21 pin 1 net change (HAT) | ❌ Pending | Open KiCad, follow Step 1 above |
| DSP Regulator feedback fix | ❌ Pending | Open KiCad, follow Step 2 above |
| CrowPanel XH2.54-4P pin 3/4 order | ❌ Pending | Verify from Elecrow schematic before wiring |
| CrowPanel bench bring-up | ❌ Pending | Follow docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md |
| D13 footprint validation (SOT23-6L vs SOT-23-6) | ❌ Pending | Verify pad pitch before Gerbers |

---

## KiCad Work Order — Today

Execute in order:

1. **HAT schematic J21 change** (Step 1 above) — 10 minutes
2. **Run ERC** — expect 0 errors
3. **Re-export HAT netlist**
4. **Run verify-connector-contract.ps1 -Mode reduced** → expect 18/18 PASS
5. **DSP Regulator schematic feedback fix** (Step 2 above) — 20 minutes
6. **Run ERC on regulator schematic**
7. **Re-export DSP Regulator netlist**

---

## Source of Truth Files

- docs/DEV_STATION_HANDOFF_2026-05-28.md (this file)
- hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch
- hardware/kicad/dsp-regulator/DSP Regulator.kicad_sch
- docs/DISPLAY_INTERFACE_STANDARD.md
- docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md
