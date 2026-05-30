# Dev Station Handoff — 2026-05-30

## Session Goal
Dedicate Channel 3 (U2, LM2596S-ADJ) as a fixed 5V supply for the CrowPanel display,
isolating it from `+5V_Boot` which powers the HAT boot electronics (STM32, AMS1117, WS2812, POTs).

---

## What Was Completed This Session

### Rev-B HAT (`DSP-Regulator-HAT-RevB.kicad_sch` + `.net`)
- **J2 pin 06** split from `+5V_Boot` → new `+V Adj Channel` net
  - Wire (375.92→377.19) deleted; junction at (123.19, 375.92) removed
  - New `global_label "+V Adj Channel"` added at (123.19, 374.65)
  - PWR_FLAG at (123.19, 375.92) naturally migrates to `+V Adj Channel` via retained wire
- **J21 pin 1** (CrowPanel 4-pin JST XH display power) changed from `+5V_Boot` → `+V Adj Channel`
- J2 pins 08/10/12 remain on `+5V_Boot` (boot electronics: STM32, AMS1117, POTs, J13 expansion)

### Rev-B Regulator (`DSP-Regulator-RevB.kicad_sch` + `.net`)
- **J2 pin 06** split from `+5V_Boot` → `+V Adj Channel`
  - Wire (135.89→138.43) replaced with `global_label "+V Adj Channel"` at (387.35, 135.89)
  - Existing `+5V_Boot` label at (387.35, 138.43) retained for pins 08/10/12

### Rev-B Netlists (hand-edited to match schematics)
- `DSP-Regulator-RevB.net`: new net code 60 `+V Adj Channel` with J2 pin 06
- `DSP-Regulator-HAT-RevB.net`: new net code 164 `+V Adj Channel` with J2 pin 06 + J21 pin 1

### Connector Contract
- `scripts/verify-connector-contract.ps1 -Mode reduced` → **+V Adj Channel: PASS**
- 3 pre-existing failures remain (VSENSE_ADJ+/-, ISET_MPU_Channel_3) — out of scope

### Original DSP Regulator (`DSP Regulator.kicad_sch`) — Rev-C Prep
- R11 value: 1kΩ → **240Ω** (lower resistor, FB→GND)
- R41 value: 100Ω → **750Ω** (upper resistor, Vout→FB)
- U8 (TS5A3157 SPDT switch), R26 (0Ω jumper), R43 (2kΩ remote sense) → **DNP**

All changes committed on branch `copilot/channel3-boot-control-voltage-edits`:
- `2a770f2` — hw(channel3): split J2 pin 06 to +V Adj Channel for CrowPanel 5V supply
- `73473c1` — hw(kicad): add autosave files for DSP Regulator HAT and Rev B

---

## What Still Needs To Be Done

### PRIORITY 1 — Finish Original DSP Regulator Schematic (Rev-C topology)

R41 in `DSP Regulator.kicad_sch` is marked 750Ω and DNP flags are set on U8/R26/R43,
**but R41 is still wired into the old U8 NC path — not the correct upper-resistor position.**

The correct Rev-C topology is:
```
+V Adj Channel (Vout) ──── R41 (750Ω) ──┬── Net-(U2-FB) ──── R11 (240Ω) ──── GND
                                          └── U2 FB pin
```
Expected output: Vout = 1.23 × (1 + 750/240) = **5.07 V** ✓

**To complete this in KiCad GUI:**
1. Open `hardware/kicad/dsp-regulator/DSP Regulator.kicad_sch`
2. Locate R41 at position (224.79, 181.61, 90°)
3. Move R41 pin 1 wire → connect to `+V Adj Channel` label (or new label near Vout)
4. Move R41 pin 2 wire → connect to `Net-(U2-FB)` (same node as R11 pin 1 and U2 pin 4)
5. Delete or DNP R42 (the resistor between old R41 and R43 chain)
6. Verify ERC = 0 errors
7. Re-export netlist: Tools → Generate Netlist → save as `DSP-Regulator.net`

### PRIORITY 2 — Re-export Rev-B netlists from KiCad

The `.net` files were hand-edited this session. For full ERC/DRC correctness, re-export from KiCad GUI after visually verifying the schematics:
- `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.net`
- `hardware/kicad/dsp-regulator-rev-b/DSP-Regulator-RevB.net`

Then re-run: `PowerShell -ExecutionPolicy Bypass -File .\scripts\verify-connector-contract.ps1 -Mode reduced`
Expected: +V Adj Channel PASS (already passing with hand-edit).

### PRIORITY 3 — Open CrowPanel Bring-Up Items
- Verify CrowPanel XH2.54-4P connector pin 3/4 order (UART TX/RX) from Elecrow schematic
- Follow `docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md` for bench power-up sequence

---

## Architecture Reference

| Rail | Regulator | Output | J2 Pins (to HAT) | Purpose |
|------|-----------|--------|-------------------|---------|
| +5V_Boot | U2 (LM2596S-ADJ) | 5V fixed | 08, 10, 12 | STM32, AMS1117-3.3, WS2812, POTs, J13 |
| +V Adj Channel | U2 (LM2596S-ADJ) | 5V fixed | **06** + J21 pin 1 | CrowPanel display power |
| +5V_reg | U4 (LM2596-5.0) | 5V fixed | J1 pins 06/08/10/12 | MPU 5V |
| +3.3V_Reg | U3 (LM2596S-ADJ) | 3.3V fixed | J1 pins 11/13/15 | MPU 3.3V |

> Note: `+5V_Boot` and `+V Adj Channel` are both sourced from U2 in Rev-B (same physical output,
> split at J2 pin 06). Rev-C will separate them physically on the PCB with R41 as the dedicated
> upper feedback resistor.

---

## Branch
`copilot/channel3-boot-control-voltage-edits`

## Key Files
- `hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch`
- `hardware/kicad/dsp-regulator-rev-b/DSP-Regulator-RevB.kicad_sch`
- `hardware/kicad/dsp-regulator/DSP Regulator.kicad_sch` ← **INCOMPLETE: R41 topology**
- `scripts/verify-connector-contract.ps1`
