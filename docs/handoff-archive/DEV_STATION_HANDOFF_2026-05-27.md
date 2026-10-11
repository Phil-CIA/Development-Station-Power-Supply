# DEV STATION SESSION PLAN — 2026-05-27

Date: 2026-05-27
Project: Development Station Power Supply
Phase: Rev-B HAT — USB circuit completion + STM32 conversion final cleanup

---

## Session Goal

Complete the USB-to-STM32 schematic circuit and close all remaining STM32 conversion items.
Today's primary deliverables are:
1. Fix the CH340G/CH340C part mismatch (CH340G with NC'd XI/XO will not enumerate)
2. Add required decoupling to U12 and J20 VBUS
3. Add USB ESD/TVS protection on DP/DM/VBUS
4. Review and clean up CTS/RTS routing from U12 to STM32
5. Review +3.3V Boot rail decoupling near the STM32 module area
6. Run ERC and re-export netlist

---

## Current Schematic Audit (from netlist 2026-05-26)

### USB Circuit — What Is Done

| Item | Status | Net / Notes |
|------|--------|-------------|
| U12 placed (CH340G symbol) | Done | LCSC C14267 — **WRONG PART, see below** |
| U12 VCC pin 16 → +3.3V Boot | Done | Verified in netlist |
| U12 V3 pin 4 → +3.3V Boot | Done | Verified in netlist |
| U12 TXD (pin 2) → UART1_RX | Done | Correct direction ✓ |
| U12 RXD (pin 3) → UART1_TX | Done | Correct direction ✓ |
| U12 UD+ (pin 5) → J20 D+ | Done | Verified in netlist |
| U12 UD- (pin 6) → J20 D- | Done | Verified in netlist |
| J20 placed (USB-C 16P) | Done | LCSC C2927039 |
| J20 CC1 → R88 → GND (5.1 kΩ) | Done | Verified in netlist |
| J20 CC2 → R87 → GND (5.1 kΩ) | Done | Verified in netlist |
| J20 GND + SHIELD → GND | Done | Verified in netlist |
| J20 VBUS isolated (no backfeed) | Done | Net-(J20-VBUS-PadA9B4) is floating |

### USB Circuit — What Is MISSING (must fix today)

| Item | Priority | Action Required |
|------|----------|-----------------|
| **CH340G → CH340C part swap** | **CRITICAL** | Change U12 LCSC from C14267 to C2609442; CH340G needs 12 MHz crystal; CH340C has internal oscillator — XI/XO can be NC'd |
| **C_CH340_VCC** (100 nF) | High | Add bypass cap: U12 VCC (pin 16) → GND, 0603 100nF, placed next to U12 |
| **C_CH340_V3** (100 nF) | High | Add bypass cap: U12 V3 (pin 4) → GND, 0603 100nF, placed next to U12 |
| **VBUS decoupling — C_VBUS1** (100 nF) | Medium | Add bypass cap: J20 VBUS → GND, 0603 100nF |
| **VBUS decoupling — C_VBUS2** (10 µF) | Medium | Add bulk cap: J20 VBUS → GND, 0805 10µF 10V, for surge absorption |
| **USB ESD protection — D_USB** | High | Add USBLC6-2SC6 or PRTR5V0U2X between J20 and U12 (see spec below) |
| **No-connect flags on unused U12 pins** | Medium | Pins 7 (XI), 8 (XO), 11 (RI#), 12 (DCD#), 13 (DTR#), 14 (RTS#), 15 (~RST) — add X markers in KiCad to prevent ERC pin-not-connected warnings |
| **CTS/RTS routing review** | Low | Pins 9/10 currently routed to U10 PA11/PA12 — see discussion below |

---

## Critical Fix: CH340G → CH340C

### Problem
- Current U12 symbol: `PCM_JLCPCB-Extended:USB-Serial, CH340G`, LCSC **C14267** (CH340G)
- CH340G requires a 12 MHz crystal on XI (pin 7) and XO (pin 8) to generate the USB clock
- XI and XO are currently NC'd in the schematic — **the device will NOT enumerate over USB**

### Solution
Swap the LCSC part number to **CH340C** (LCSC **C2609442**).
- Same SOIC-16 package and pin-compatible with CH340G
- CH340C has an internal 12 MHz oscillator; XI/XO are unused → apply no-connect flags
- JLCPCB Extended component, confirmed available

### KiCad GUI Steps for Part Swap
1. Open schematic, double-click U12 to edit properties
2. Change **LCSC** property value: `C14267` → `C2609442`
3. Change **Value** property: `CH340G` → `CH340C`
4. Change **Datasheet** property to: `https://www.lcsc.com/datasheet/lcsc_datasheet_CH340C_C2609442.pdf`
5. Change **Description** property to: `USB serial converter, UART, SOIC-16, internal oscillator`
6. Confirm **Footprint** remains: `PCM_JLCPCB:SOIC-16_3.9x9.9mm_P1.27mm`
7. Click OK
8. Add no-connect X flags on pins 7 (XI) and 8 (XO) if not already present

---

## Decoupling Capacitors — U12 and J20

### U12 CH340C Local Bypass (two caps)

Add two 100 nF bypass caps close to U12 (within 1–2 mm on PCB):

| Ref | Value | Package | Net (pin 1) | Net (pin 2) | LCSC | Notes |
|-----|-------|---------|-------------|-------------|------|-------|
| C_U12_VCC | 100 nF 10V X7R | 0603 | +3.3V Boot | GND | C14663 | Bypass for U12 pin 16 (VCC) |
| C_U12_V3 | 100 nF 10V X7R | 0603 | +3.3V Boot | GND | C14663 | Bypass for U12 pin 4 (V3) |

> Use next available C reference numbers (check schematic — last used appears to be around C37; use C38 and C39, or whichever is next available).

### J20 VBUS Decoupling (two caps)

VBUS receives 5 V from the USB host. Do NOT route VBUS to any supply rail.
These caps protect the connector and limit inrush.

| Ref | Value | Package | Net (pin 1) | Net (pin 2) | LCSC | Notes |
|-----|-------|---------|-------------|-------------|------|-------|
| C_VBUS1 | 100 nF 16V X7R | 0603 | Net-(J20-VBUS) | GND | C14663 | HF bypass |
| C_VBUS2 | 10 µF 10V X5R | 0805 | Net-(J20-VBUS) | GND | C96446 | Bulk / inrush |

> Net name for J20 VBUS: `Net-(J20-VBUS-PadA9B4)` (both VBUS pads are tied together internally in the connector symbol).

---

## USB ESD / TVS Protection — D_USB

### Requirement
USB 2.0 data lines (D+, D-) and VBUS are exposed at the panel connector and subject to ESD from
cable plug/unplug events. USB D+/D- run at 3.3 V logic level (CH340C UD+/UD-); a standard 600 W
SMB TVS (SMBJ6.0CA already in schematic for power rails) is far too large and slow for USB
signal protection.

### Recommended Part: USBLC6-2SC6

| Property | Value |
|----------|-------|
| Part | USBLC6-2SC6 |
| Manufacturer | STMicroelectronics |
| Package | SOT-23-6 |
| LCSC | C7519 |
| JLCPCB | Basic Component |
| Protection | D+ (3.3 V clamp), D- (3.3 V clamp), VBUS (5 V clamp) |
| Standby voltage | 5 V (VBUS), 3.3 V (data lines) |
| Clamping voltage (8/20 µs) | 7.5 V @ VBUS, 4.2 V @ data lines |
| Capacitance (D+/D-) | <1 pF — will not degrade USB FS eye |

### Alternative if USBLC6-2SC6 unavailable: PRTR5V0U2X
- LCSC C12333, SOT-143, same function

### Schematic Placement

Place D_USB between J20 and U12 in the signal path:

```
J20 D+  ──┬──  D_USB I/O1  ──┬──  U12 UD+
           |                  |
J20 D-  ──┴──  D_USB I/O2  ──┴──  U12 UD-
                D_USB VBUS ───── J20 VBUS net
                D_USB GND  ───── GND
```

### USBLC6-2SC6 Pin Assignments (SOT-23-6)

| Pin | Name | Net |
|-----|------|-----|
| 1 | I/O1 | Net-(U12-UD+) — the D+ signal net |
| 2 | GND | GND |
| 3 | I/O2 | Net-(U12-UD-) — the D- signal net |
| 4 | VBUS | Net-(J20-VBUS-PadA9B4) |
| 5 | I/O2 | (same as pin 3 — tied in the SOT-23-6 package) |
| 6 | I/O1 | (same as pin 1 — tied in the SOT-23-6 package) |

> In practice: in KiCad, place the USBLC6-2SC6 symbol, connect I/O1 to the D+ wire between J20 and U12, I/O2 to the D- wire, VBUS to the J20 VBUS net, GND to GND.

### KiCad Symbol Library
- Library: `Device:USBLC6-2SC6` (available in KiCad standard library)
- Footprint: `Package_TO_SOT_SMD:SOT-23-6`
- If not in local library: use `Add Symbol`, search `USBLC6`

### Reference Designator
Use **D13** (next available D ref after D12 which is the WS2812 LED).

---

## CTS / RTS Routing Review

### Current State (from netlist)
- Net "CTS": U12 pin 9 (~CTS) → R77 → U10 PA11 (A11 pin 8)
- Net "RTS": U12 pin 10 (~DSR) → R73 → U10 PA12 (A12 pin 9)
- Both nets also route through J13 (general expansion header)

### Analysis
PA11 and PA12 on STM32F103C8 are the hardware USB D- and D+ pins (USB_DM/USB_DP for the
STM32's built-in USB peripheral). These are NOT being used for hardware USB on this board
(USB is handled by CH340C external bridge). As general GPIO, PA11/PA12 work fine for
software-controlled CTS/RTS or any other purpose.

For a telemetry application (raw V/I logging to PC), software UART without flow control
is entirely sufficient. CTS/RTS add complexity and are not needed at bring-up.

### Decision Options

| Option | Complexity | Recommendation |
|--------|-----------|----------------|
| **A. Keep CTS/RTS as optional flow control** | Low | Preferred — no schematic change needed; STM32 firmware can ignore these pins initially and enable later if needed |
| B. Remove CTS/RTS routing, add NC flags to U12 pins 9/10 | Medium | Clean but requires routing changes to J13 area |
| C. Add explicit no_connect on U12 pins 9/10, disconnect from U10 | Medium | Similar to B |

**Recommendation: Keep Option A for now.** The wiring is already done, it costs nothing in
BOM, and the option to use hardware flow control at bring-up is preserved. Document that these
pins are currently NC at firmware level.

---

## STM32 Conversion — Remaining Checklist

### Items to Verify in KiCad

| Item | Status | Action |
|------|--------|--------|
| U10 44-pin wiring complete | ✅ Done | Confirmed by netlist — no action |
| J19 SWD header wired correctly | ✅ Done | Confirmed by netlist |
| +3.3V Boot rail has LDO (U9 AMS1117-3.3) | ✅ Done | Confirmed in netlist |
| BOOT0 circuit | ✅ Closed | Blue Pill JP3 handles it |
| NRST circuit | ✅ Closed | Blue Pill onboard RC handles it |
| CH340C VCC decoupling | ❌ **Missing** | Add C_U12_VCC per spec above |
| CH340C V3 decoupling | ❌ **Missing** | Add C_U12_V3 per spec above |
| VBUS decoupling | ❌ **Missing** | Add C_VBUS1 + C_VBUS2 per spec above |
| USB ESD protection | ❌ **Missing** | Add D13 USBLC6-2SC6 per spec above |
| U12 part number correct | ❌ **Wrong** | Swap C14267 → C2609442 (CH340C) |
| Unused U12 pins NC'd | ❓ Unverified | Confirm/add NC flags: pins 7, 8, 11, 12, 13, 14, 15 |
| ERC recheck after all additions | ❌ Pending | Run after all above are done |
| Netlist re-export | ❌ Pending | Export after ERC confirms clean |
| verify-connector-contract.ps1 | ❌ Pending | Re-run after netlist export |

### +3.3V Boot Rail Decoupling Assessment

Current capacitors on +3.3V Boot net (from netlist):
`C13, C27, C29, C30, C33, C34, C35, C36` (8 caps)

These caps are shared across all ICs on the +3.3V Boot rail (INA3221s, shift registers, STM32
module, CH340C, W25Q128). The STM32 Blue Pill module has onboard decoupling. The shared rail
caps provide bulk charge reservoir. Adding two dedicated 100 nF caps close to U12 pins 4 and 16
is the minimum required — the existing caps are sufficient for the rest of the rail.

**No additional +3.3V Boot decoupling is needed beyond the C_U12_VCC and C_U12_V3 caps.**

---

## KiCad Work Order (Recommended Sequence)

Execute in this order to minimize ERC noise:

### Step 1 — Fix CH340G → CH340C part number
1. Double-click U12 → edit properties → change LCSC to C2609442, Value to CH340C
2. Confirm footprint unchanged (SOIC-16_3.9x9.9mm_P1.27mm)

### Step 2 — Add no-connect flags to unused U12 pins
In KiCad schematic editor, press `Q` or use Place → No Connect:
- Add X on U12 pin 7 (XI)
- Add X on U12 pin 8 (XO)
- Add X on U12 pin 11 (RI#)
- Add X on U12 pin 12 (DCD#)
- Add X on U12 pin 13 (DTR#)
- Add X on U12 pin 14 (RTS#)
- Add X on U12 pin 15 (~RST) — if not driven

### Step 3 — Add C_U12_VCC and C_U12_V3
1. Place two 100 nF 0603 capacitors near U12 in the schematic
2. Connect each cap: pin 1 → +3.3V Boot power symbol, pin 2 → GND power symbol
3. Set reference C_U12_VCC and C_U12_V3 (or use next sequential C refs)
4. Library: `PCM_JLCPCB-Capacitors:0603,100nF` or equivalent; LCSC C14663

### Step 4 — Add D13 USB ESD protection
1. Place `Device:USBLC6-2SC6` symbol (or `PCM_JLCPCB-Diodes:USBLC6-2SC6` if available)
   - Footprint: `Package_TO_SOT_SMD:SOT-23-6`
   - LCSC: C7519
2. Place between J20 and U12 in the D+/D- signal path
3. Wire: I/O1 → D+ signal wire, I/O2 → D- signal wire, VBUS → J20 VBUS net, GND → GND
4. Reference: D13

### Step 5 — Add VBUS decoupling caps
1. Place 100 nF 0603 cap: pin 1 → J20 VBUS net, pin 2 → GND (reference C_VBUS1)
2. Place 10 µF 0805 cap: pin 1 → J20 VBUS net, pin 2 → GND (reference C_VBUS2)
   - LCSC for 10µF/0805: C96446 (Samsung 10µF 10V X5R 0805)

### Step 6 — Run ERC
- Inspect → Electrical Rules Checker → Run ERC
- Expected: 0 errors; warnings reduced vs prior run
- If pin-not-connected warnings appear on U12 pins 7/8/11-15 → confirm NC flags placed

### Step 7 — Re-export netlist
- Tools → Generate Netlist → Export
- Save as: `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net`

### Step 8 — Re-run connector contract script
```powershell
cd C:\Users\forch\GitHub\Development-Station-Power-Supply
.\scripts\verify-connector-contract.ps1 -Mode reduced
```
Expected: 18/18 PASS, exit 0

---

## ERC Expected Outcomes

After all steps above:

| Check | Expected |
|-------|----------|
| Hard errors | 0 |
| Pin not connected (U12 unused pins) | 0 — cleared by NC flags in Step 2 |
| Power pin not driven | 0 — +3.3V Boot already driven by U9 |
| USBLC6-2SC6 passive pins | May generate "pin not connected" if VBUS is not driven — add NC if needed |
| Warnings (library mismatch, etc.) | ≤ 9 (same as prior baseline) |

---

## Component Reference Summary (New Parts This Session)

| Ref | Value | LCSC | Package | Net A | Net B |
|-----|-------|------|---------|-------|-------|
| C_U12_VCC | 100 nF | C14663 | 0603 | +3.3V Boot | GND |
| C_U12_V3 | 100 nF | C14663 | 0603 | +3.3V Boot | GND |
| C_VBUS1 | 100 nF | C14663 | 0603 | VBUS | GND |
| C_VBUS2 | 10 µF | C96446 | 0805 | VBUS | GND |
| D13 | USBLC6-2SC6 | C7519 | SOT-23-6 | D+/D-/VBUS | GND |

---

## Post-Session Closeout Requirements

1. Update docs/DEV_STATION_HANDOFF_2026-05-27.md (this file) with "What Was Completed Today" section
2. Update /memories/repo/active-baseline.md
3. Update /memories/repo/open-loops.md — close USB circuit items as each is done
4. Update docs/STM32_BLUEPILL_PIN_TABLE.md Validation Log with today's results
5. Re-export netlist after ERC confirms clean

---

## What Was Completed This Session (2026-05-27)

### Netlist Verification — All Connections Confirmed Correct

The user implemented all KiCad edits and re-exported the netlist. Full netlist audit confirmed:

#### CH340C Placement
| Item | Result |
|------|--------|
| U12 symbol = `Interface_USB:CH340C` | ✅ Confirmed |
| U12 VCC (pin 16) → +3.3V Boot | ✅ Confirmed |
| U12 V3 (pin 4) → +3.3V Boot (3.3V mode = tied to VCC) | ✅ Confirmed |
| U12 TXD (pin 2) → UART1_RX → U10 PA10 | ✅ Confirmed |
| U12 RXD (pin 3) → UART1_TX → U10 PA9 | ✅ Confirmed |
| U12 ~CTS (pin 9) ← R77 ← U10 PA11 | ✅ Confirmed — Option A |
| U12 ~RTS (pin 14) → R73 → U10 PA12 | ✅ Confirmed — Option A |

#### Unused CH340C Pins — All NC'd
| Pin | Function | Status |
|-----|----------|--------|
| 7 | XI | ✅ NC (pintype "no_connect") |
| 8 | XO | ✅ NC (pintype "no_connect") |
| 10 | ~DSR | ✅ NC |
| 11 | ~RI | ✅ NC |
| 12 | ~DCD | ✅ NC |
| 13 | ~DTR | ✅ NC |
| 15 | R232/~RST | ✅ NC |

#### D13 USBLC6-2SC6 — Wiring Verified Correct
Using `dk_TVS-Diodes:USBLC6-2SC6` with footprint `digikey-footprints:SOT23-6L`.

USBLC6-2SC6 SOT-23-6 physical pinout:
- Pins 1/6 = I/O1 (same signal)
- Pins 3/4 = I/O2 (same signal)  
- Pin 5 = VBUS
- Pin 2 = GND

| D13 Pin | Physical Function | Connected To | Status |
|---------|------------------|--------------|--------|
| Pin 1 | I/O1 (D+) | U12 UD+ | ✅ Correct |
| Pin 2 | GND | GND | ✅ Correct |
| Pin 3 | I/O2 (D-) | U12 UD- | ✅ Correct |
| Pin 4 | I/O2 (D-) | J20 A7/B7 (D-) | ✅ Correct |
| Pin 5 | VBUS | VBUS net (J20 + C40) | ✅ Correct |
| Pin 6 | I/O1 (D+) | J20 A6/B6 (D+) | ✅ Correct |

D+ passes through D13 pins 6→1 (J20 to U12). D- passes through D13 pins 4→3.

#### Decoupling Caps
| Ref | Value | Package | LCSC | Net+ | Net- | Status |
|-----|-------|---------|------|------|------|--------|
| C38 | 100nF | 0805 | C49678 | +3.3V Boot | GND | ✅ Verified |
| C39 | 100nF | 0805 | C49678 | +3.3V Boot | GND | ✅ Verified |
| C40 | 100nF | 0805 | C49678 | VBUS | GND | ✅ Verified |

> Note: Plan called for 0603, user placed 0805. Electrically equivalent. Acceptable for bring-up.

#### Contract Script
```
.\scripts\verify-connector-contract.ps1 -Mode reduced
→ 18/18 PASS
```

---

### Open Items Carried Forward

| Item | Priority | Action Required |
|------|----------|-----------------|
| Add LCSC C7519 to D13 | Medium | KiCad: double-click D13 → Add Field → LCSC = C7519 |
| Add 10µF VBUS bulk cap (C41) | Low | Optional for bring-up; add if USB enumeration issues arise |
| Re-run ERC | High | Open schematic in KiCad → Inspect → Electrical Rules Checker |
| ERC expected result | — | 0 errors; ≤6 lib warnings; no UART/NRST pin warnings (now connected) |

### Notes for Next Session
- USB circuit is electrically complete — can proceed to PCB layout when ready
- D13 footprint (`digikey-footprints:SOT23-6L`) vs standard (`Package_TO_SOT_SMD:SOT-23-6`) — verify pad count/pitch matches before Gerber generation
- CTS/RTS are connected but firmware can ignore them at bring-up (Option A)
- LCSC C7519 = USBLC6-2SC6 (STMicro) — add to D13 before generating JLC BOM
6. Re-run verify-connector-contract.ps1

---

## Source of Truth Files

- docs/DEV_STATION_HANDOFF_2026-05-27.md (this file)
- docs/CH340C_KICAD_PLACEMENT_SPEC.md
- docs/STM32_BLUEPILL_PIN_TABLE.md
- hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch

---

## END-OF-SESSION CLOSEOUT — 2026-05-27

DEV STATION CLOSEOUT START

Date: 2026-05-27
Project: Development Station Power Supply
Phase: Rev-B HAT — USB circuit completion (schematic phase complete)

### What Was Completed Today

1. Identified and fixed CH340G → CH340C part mismatch (CH340G without crystal would never enumerate USB)
2. Swapped U12 symbol to `Interface_USB:CH340C` (internal oscillator, LCSC C2609442)
3. Added C38/C39 (100 nF 0805) bypass caps on +3.3V Boot at U12 VCC and V3 pins
4. Added C40 (100 nF 0805) VBUS decoupling cap at J20
5. Added D13 USBLC6-2SC6 USB TVS protection between J20 and U12, wiring verified
6. NC'd all unused U12 pins (7, 8, 10, 11, 12, 13, 15)
7. Confirmed CTS/RTS wired Option A (U12→R77→PA11, U12→R73→PA12)
8. Fixed ERC conflict: U12 V3 pin type changed from power_out to power_in
9. ERC final result: 0 errors, 0 warnings (2026-05-27T03:42)
10. Added LCSC = C7519 to D13 properties
11. Re-exported netlist (final: 2026-05-27T03:50:39)
12. verify-connector-contract.ps1 -Mode reduced: 18/18 PASS

### What Changed

- Files updated: DSP-Regulator-HAT-RevB.kicad_sch, DSP-Regulator-HAT-RevB.net, ERC.rpt, docs/DEV_STATION_HANDOFF_2026-05-27.md
- Behavior changed: USB circuit is now electrically complete; CH340C will enumerate over USB; ESD protection present on D+/D-/VBUS
- Hardware changes: None (schematic only)
- Bench evidence captured: No — schematic phase only

### Current State At Stop

- Current active target: Rev-B HAT schematic → transition to PCB layout
- Current highest-risk unresolved item: D13 footprint validation — `digikey-footprints:SOT23-6L` vs `Package_TO_SOT_SMD:SOT-23-6` pad pitch must be confirmed to match before Gerbers
- Safe resume point: Schematic is ERC-clean with 0 errors. Next action is PCB layout in KiCad.
- Hardware left connected: No

### Memory Pool Update

- active-baseline.md: 2026-05-27 entry updated — USB circuit fully complete, LCSC C7519 on D13, netlist final 03:50:39
- decision-log.md: Add — "CH340C selected over CH340G: internal oscillator eliminates crystal dependency; V3 pin in 3.3V mode is an input reference not a power output"
- open-loops.md: All 2026-05-27 USB circuit items CLOSED. Remaining: deferred 10µF VBUS bulk cap only.
- bench-log.md: No bench work today — schematic only
- development-station-power-supply.md: No new durable lesson (previous lesson about CH340G oscillator issue already covers this)

### Known-Good Checks

1. ERC: 0 errors, 0 warnings (2026-05-27T03:42:01)
2. verify-connector-contract.ps1 -Mode reduced: 18/18 PASS (exit 0)
3. Netlist net audit: all 17 USB/STM32 nets verified correct (see "What Was Completed" section above)

### Still Blocked By

1. D13 footprint pad-pitch validation before Gerber export (SOT23-6L vs SOT-23-6 — confirm pin count and pitch match)
2. Pre-order blockers from earlier (RB-005 ESP32 footprint, RB-007 stack height) still open from prior sessions

### What's Next For Rev-B — Priority Order

**Phase: PCB Layout**

1. **Open DSP-Regulator-HAT-RevB.kicad_sch in KiCad PCB editor** — update from schematic to import new components (U12, D13, C38/C39/C40)
2. **Validate D13 footprint** — confirm `digikey-footprints:SOT23-6L` pad pitch/count matches USBLC6-2SC6 SOT-23-6 physical. If mismatch, change to `Package_TO_SOT_SMD:SOT-23-6` in schematic before layout proceeds.
3. **Place USB cluster on PCB** — J20 (USB-C) near panel edge; D13 within 5 mm of J20 (short D+/D- stubs); C40 (VBUS bypass) at J20 pins; C38/C39 within 2 mm of U12 pins 16/4
4. **Route USB differential pair** — D+/D- from J20 → D13 → U12 as matched-length pair; avoid layer changes; keep away from switching nodes
5. **Close pre-order blockers** — RB-005 (ESP32 footprint confirm), RB-007 (stack height measurement), Issue 11 (adjustable rail feedback continuity)
6. **Generate Gerbers and BOM** — after layout DRC clean; BOM will now include D13 with LCSC C7519

### First Action Next Session

Open KiCad PCB editor for DSP-Regulator-HAT-RevB. Run "Update PCB from Schematic" to import new components. Confirm D13 footprint placement options and validate pad geometry against USBLC6-2SC6 datasheet before routing begins.

### Instructions For Next Chat

1. Read docs/DEV_STATION_HANDOFF_2026-05-27.md — specifically the "What Was Completed" section and the "What's Next" priority list above.
2. Read /memories/repo/active-baseline.md and /memories/repo/open-loops.md before making changes.
3. Schematic is complete and ERC-clean. The work is now in PCB layout — do not re-open schematic unless a layout issue requires a schematic correction.

---

## SESSION CLOSEOUT — Afternoon: Display Interface (2026-05-27)

DEV STATION CLOSEOUT START

Date: 2026-05-27 (afternoon session)
Project: Development Station Power Supply
Phase: Rev-B HAT — Display interface schematic + Universal Display Interface (UDI) standard definition

### What Was Completed

1. Added J21 display UART connector (JST XH B4B-XH-A, 4-pin, LCSC C144394, value "Display Uart") to HAT Rev-B schematic
2. Removed I2C_1 from schematic: deleted J14 and J15 headers; repurposed R78/R79 from 4.7kΩ pull-ups to 33Ω UART series resistors
3. Reassigned U10 PB10/PB11 to USART3: PB10 (pin 35) = DISP_UART_TX, PB11 (pin 36) = DISP_UART_RX — TX/RX swap caught and corrected during netlist review
4. Wired J21: pin 1 → +5V_Boot, pin 2 → GND, pin 3 → R78 → DISP_UART_TX, pin 4 → R79 → DISP_UART_RX
5. ERC: 0 errors, 0 warnings after all display changes
6. Netlist exported (2026-05-27T07:48:36); verify-connector-contract.ps1 -Mode reduced: 18/18 PASS
7. Updated docs/U10_WIRING_WORKTHROUGH.md — I2C Bus 1 removed, Display UART (USART3) group added, checklist updated
8. Updated docs/GPIO_PINOUT.md — SPI IDC-10 interface removed, UDI 4-pin UART table documented
9. Created docs/DISPLAY_INTERFACE_STANDARD.md — canonical UDI spec: JST XH pin assignments, 115200 8N1, CMD:/ACK:/ERR:/EVT: framing, design rationale
10. Updated docs/display-project/README.md — UDI standard section added, custom board rev-1 requirements documented
11. Logged J21 display power open loop: +5V_Boot LDO insufficient for CrowPanel 2A max draw; bench workaround = power via USB-C; production solution deferred to post-bench measurement

### What Changed

- Files updated:
  - hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch (J21 added; J14/J15 deleted; R78/R79 repurposed; PB10/PB11 nets renamed)
  - hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net (re-exported 07:48:36)
  - docs/U10_WIRING_WORKTHROUGH.md
  - docs/GPIO_PINOUT.md
  - docs/display-project/README.md
- Files created:
  - docs/DISPLAY_INTERFACE_STANDARD.md (new — UDI canonical spec)
  - docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md (new — Phase 4 bench procedure)
- Behavior changed: HAT Rev-B now has a defined 4-pin UART connector for display communication. I2C_1 bus is removed from schematic.
- Hardware changes: Schematic only

### Current State At Stop

- Schematic: ERC-clean, 0 errors / 0 warnings. Netlist final as of 07:48:36.
- J13 (2×5 IDC SPI expansion): still present on schematic. Fate TBD — remove or repurpose as GPIO expansion.
- J21 pin 1 power: netted to +5V_Boot — power design open loop (see open-loops.md 2026-05-27 J21 Display Power section)
- CrowPanel XH2.54-4P pin 3/4 order (IO44 vs IO43): unconfirmed — must verify from Elecrow schematic before building a cable
- Next phase: CrowPanel bench bring-up (Phase 4) — see docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md

### Memory Pool Update

- active-baseline.md: add 2026-05-27 afternoon entry — display interface schematic complete, UDI standard defined
- open-loops.md: J21 display power open loop added (see file)

### Known-Good Checks

1. ERC: 0 errors, 0 warnings (2026-05-27T07:48, after all display changes)
2. verify-connector-contract.ps1 -Mode reduced: 18/18 PASS (exit 0)
3. Net audit confirmed: DISP_UART_TX (PB10) → R78 → J21 pin 3 ✓; DISP_UART_RX (PB11) → R79 → J21 pin 4 ✓; J21 pin 1 = +5V_Boot ✓; J14/J15 absent from netlist ✓

### Still Blocked By

1. CrowPanel XH2.54-4P pin 3/4 order (IO44 vs IO43): unconfirmed — check https://github.com/Elecrow-RD/CrowPanel-Advance-4.3--HMI-ESP32-800x480 before wiring
2. J21 pin 1 power: +5V_Boot LDO may be insufficient for CrowPanel 2A — defer until bench current measurement
3. J13 fate: still on schematic; no decision made
4. Phase 3 KiCad (custom front-panel board redesign): starts after Phase 4 UART protocol validated

### What's Next — Priority Order

1. **CrowPanel bring-up** — follow docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md step-by-step procedure
2. **Measure CrowPanel current draw** — update open-loops.md with result; decide J21 pin 1 power supply solution
3. **J13 fate** — decide: remove or repurpose as GPIO expansion before PCB layout
4. **Phase 3 KiCad** — custom front-panel board redesign; starts after UART protocol validated on CrowPanel
5. **PCB layout** (from morning session): D13 footprint validation, then place USB cluster and route differential pair

### First Action Next Session

Check docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md. Start at Step 1: power CrowPanel via USB-C and confirm boot screen.

### Instructions For Next Chat

1. Read docs/DEV_STATION_HANDOFF_2026-05-27.md — both the morning and afternoon closeout sections.
2. For bench work, read docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md.
3. Read /memories/repo/active-baseline.md and /memories/repo/open-loops.md.
4. Display interface schematic is ERC-clean. Do not re-open schematic unless a bench issue requires a correction.
5. CrowPanel UART is NOT I2C — 0x30 and 0x5D are CrowPanel-internal (backlight + touch). The host interface is UART on UART0-IN.
6. Do not wire J21 pin 1 to CrowPanel until the power design decision is made (open loop).

DEV STATION CLOSEOUT END
