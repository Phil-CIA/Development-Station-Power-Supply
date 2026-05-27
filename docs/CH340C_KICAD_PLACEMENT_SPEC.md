# CH340C USB-UART Bridge — KiCad Placement Specification

**Purpose:** USB-to-PC telemetry path for raw V/I data logging from STM32 UART1.  
**Date:** 2026-05-26 (updated 2026-05-27)  
**Schematic:** `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch`

---

## ⚠️ CRITICAL: CH340G vs CH340C Part Swap Required

**Current state (as of 2026-05-26):** U12 was placed using symbol
`PCM_JLCPCB-Extended:USB-Serial, CH340G` with LCSC part number **C14267 (CH340G)**.

**Problem:** CH340G requires a 12 MHz crystal on XI (pin 7) and XO (pin 8) to generate the
USB clock. These pins are currently NC'd — the device will NOT enumerate over USB.

**Required fix:** Change U12 LCSC part number to **C2609442 (CH340C)**:
- CH340C is pin-compatible with CH340G (same SOIC-16 package, same pinout)
- CH340C has an internal 12 MHz oscillator — XI/XO are not used (leave as NC)
- CH340C is a JLCPCB Extended component

**KiCad steps:**
1. Double-click U12 → edit Properties
2. LCSC: `C14267` → `C2609442`
3. Value: `CH340G` → `CH340C`
4. Description: `USB serial converter, UART, SOIC-16, internal oscillator`
5. Footprint: unchanged (`PCM_JLCPCB:SOIC-16_3.9x9.9mm_P1.27mm`)
6. Confirm no-connect X flags are on pins 7 (XI) and 8 (XO)

---

## References

| Ref | Part | KiCad library symbol | Footprint |
|-----|------|----------------------|-----------|
| **U12** | CH340C (or CH340G configured for no crystal) | `Interface_USB:CH340G` | `Package_SO:SOIC-16_3.9x9.9mm_P1.27mm` |
| **J20** | USB Type-C receptacle (USB 2.0 only — data lines + VBUS, no SuperSpeed) | `Connector_USB:USB_C_Receptacle_USB2.0_HorizMid_AssyType` or similar | JLCPCB basic part recommended |
| **C_CH340_VCC** | 100 nF bypass cap (VCC supply) | `PCM_JLCPCB-Capacitors:0603,100nF` | 0603 |
| **C_CH340_V3** | 100 nF bypass cap (V3 pin, 3.3V mode) | `PCM_JLCPCB-Capacitors:0603,100nF` | 0603 |

---

## CH340C vs CH340G

Use **CH340C** (LCSC C2609442, SOIC-16) if available at JLCPCB as a basic part.  
Otherwise use **CH340G** (LCSC C94953) and leave XI/XO unconnected (no crystal): CH340C has an internal 12 MHz oscillator; CH340G needs 12 MHz crystal on XI/XO.

If using `Interface_USB:CH340G` symbol for CH340C:
- Connect **VCC** and **V3** as shown below
- Leave **XI** and **XO** unconnected (add no-connect flags)

---

## Net Assignments

### CH340C / CH340G Pin-to-Net Mapping

| CH340 Pin | Pin Name | Net | Notes |
|-----------|----------|-----|-------|
| 1 | GND | `GND` | Power ground |
| 2 | TXD | `UART1_RX` | CH340 transmits → STM32 receives |
| 3 | RXD | `UART1_TX` | CH340 receives ← STM32 transmits |
| 4 | V3 | `+3.3V Boot` | 3.3V mode: tie V3 to VCC; add 100 nF to GND |
| 5 | UD+ | `USB_DP` | USB D+ → J20 |
| 6 | UD− | `USB_DM` | USB D− → J20 |
| 7 | XI | no_connect | CH340C has internal clock; CH340G needs 12 MHz crystal here |
| 8 | XO | no_connect | CH340G crystal out; CH340C: NC |
| 9 | VCC | `+3.3V Boot` | 3.3V operation; add 100 nF to GND |
| 10 | CTS# | no_connect | Optional; NC for now |
| 11 | DSR# | no_connect | |
| 12 | RI# | no_connect | |
| 13 | DCD# | no_connect | |
| 14 | DTR# | no_connect | |
| 15 | RTS# | no_connect | |
| 16 | NC | no_connect | |

> **Note:** CH340G SOIC-16 pinout above. If actual CH340C has a different package/pinout, verify against LCSC C2609442 datasheet before placing.

---

## USB Connector (J20) Net Assignments

| J20 Pin | Signal | Net | Notes |
|---------|--------|-----|-------|
| VBUS | +5V from host | `USB_VBUS` | Add 100 nF + 10 µF bypass to GND; do **not** power the STM32/HAT from this — informational only |
| D+ | USB data+ | `USB_DP` | Route with 90 Ω differential impedance if possible |
| D− | USB data− | `USB_DM` | Same trace pair as D+ |
| GND/Shield | Ground | `GND` | |
| CC1, CC2 (USB-C only) | Configuration channel | 5.1 kΩ to GND each | Marks device as USB 2.0 Full Speed, no power delivery |

> **USB-C CC resistors:** Add two 5.1 kΩ resistors: CC1 → GND and CC2 → GND. This tells any USB-C host that this is a standard downstream device (not a power role). Use 0603 package. Net names: `USB_CC1`, `USB_CC2` (or just local labels).

---

## Suggested Placement

Place near J13 (UART1 area on HAT), roughly X=380-410, Y=250-280 (schematic units).  
Keep USB_DP / USB_DM traces short and as a matched pair.

---

## KiCad GUI Steps

1. **Add `Interface_USB:CH340G` symbol** (Add Symbol → search "CH340G")
   - Reference: **U12**
   - Value: **CH340C** (or CH340G if using that part)
   - Footprint: `Package_SO:SOIC-16_3.9x9.9mm_P1.27mm`

2. **Wire U12 pins** using global labels and net labels as listed above.
   - Pins 4 (V3) and 9 (VCC) → `+3.3V Boot` global label  
   - Pin 1 (GND) → `GND` global label  
   - Pin 2 (TXD) → net label `UART1_RX`  
   - Pin 3 (RXD) → net label `UART1_TX`  
   - Pins 5–6 → local net labels `USB_DP` / `USB_DM`  
   - Pins 7, 8, 10–16 → `X` no-connect flags

3. **Add bypass caps:**
   - 100 nF from U12 pin 9 (VCC) to GND — use `PCM_JLCPCB-Capacitors:0603,100nF`
   - 100 nF from U12 pin 4 (V3) to GND — same footprint

4. **Add USB connector J20:**
   - Symbol: `Connector_USB:USB_C_Receptacle_USB2.0_HorizMid_AssyType` or search for USB_C
   - Reference: **J20**, Value: **USB_C**
   - Wire VBUS → `USB_VBUS`, D+/D− → `USB_DP`/`USB_DM`, GND → `GND`
   - Wire CC1 and CC2 each through 5.1 kΩ resistor to GND

5. **Add 5.1 kΩ CC resistors** (×2):
   - Use `PCM_JLCPCB-Resistors:0805,4.7kΩ` as closest available, or add a new 5.1 kΩ entry
   - Net from USB-C CC1 → R_CC1 → GND; same for CC2

6. **Run ERC** and confirm:
   - UART1_TX / UART1_RX warnings clear (both now have 2 instances: U10 side + U12 side)
   - No floating pins on U12

---

## UART1_TX/UART1_RX ERC Note

Currently ERC shows "Global label not connected" for `UART1_TX` and `UART1_RX` because each only has one instance (on U10). After wiring CH340C with those same net labels, ERC should clear those 2 warnings. Expected final ERC: ≤4 warnings (library missing × 4, possibly a few SWO/KEY no-connect).

---

## Summary of Net Names

| Net | Source | Sink |
|-----|--------|------|
| `UART1_TX` | U10 PA9 (pin 6 stm32_BLUE) | U12 RXD (pin 3 CH340) |
| `UART1_RX` | U10 PA10 (pin 7 stm32_BLUE) | U12 TXD (pin 2 CH340) |
| `USB_DP` | U12 UD+ (pin 5) | J20 D+ |
| `USB_DM` | U12 UD− (pin 6) | J20 D− |
| `+3.3V Boot` | U9 AMS1117 VO | U12 VCC + V3 |
