# Display Board Diagnostic Testing Checklist

## Current Firmware Status
- **Uploaded to COM9**: SPI Diagnostic Probe (baseline - all diagnostics disabled)
- **MOSI Pin**: 13 (original)
- **MISO Pin**: 11 (original)
- **DC Polarity**: Normal (not inverted)
- **CS Polarity**: Normal (not inverted)

## Pre-Test Checklist
- [ ] Display board connected and powered
- [ ] COM9 connection verified
- [ ] Serial monitor ready (115200 baud)
- [ ] Read diagnostic reference: `DISPLAY_DIAGNOSTIC_REFERENCE.md`

## Test Plan

### Test 1: Baseline (Original Configuration)
**Status**: Firmware already uploaded to COM9

1. [ ] Unplug display board from power
2. [ ] Wait 5+ seconds
3. [ ] Plug back in
4. [ ] Wait 2 seconds for boot
5. [ ] **Observe**: Does RED framebuffer appear?
   - [ ] YES → Baseline works (unexpected - proceed with other tests for confirmation)
   - [ ] NO → Proceed to Test 2
6. [ ] **Serial Output**: Should show boot sequence and brightness diagnostic loop

**Result**: RED appears? __ YES __ NO

---

### Test 2: DC Polarity Invert
**Action Required**: Modify `platformio.ini`

1. [ ] Edit `platformio.ini` line 19: change `-DDIAG_INVERT_DC=0` to `-DDIAG_INVERT_DC=1`
2. [ ] Save file
3. [ ] In terminal: `platformio run -e display_board -t upload`
4. [ ] Wait for success message
5. [ ] Unplug display board
6. [ ] Wait 5+ seconds
7. [ ] Plug back in
8. [ ] Wait 2 seconds for boot
9. [ ] **Observe**: Does RED framebuffer appear?
   - [ ] YES → **DC POLARITY IS INVERTED on PCB** ← ROOT CAUSE FOUND
   - [ ] NO → Proceed to Test 3
10. [ ] **Serial Output**: Should show `[DIAGNOSTIC] DC polarity INVERTED`

**Result**: RED appears? __ YES __ NO

---

### Test 3: SPI Pin Swap (MOSI ↔ MISO)
**Action Required**: Modify `src/display_main.cpp`

1. [ ] Edit `src/display_main.cpp` lines 49-50:
   - Change `constexpr uint8_t TFT_MOSI_PIN   = 13;` to `constexpr uint8_t TFT_MOSI_PIN   = 11;`
   - Change `constexpr uint8_t TFT_MISO_PIN   = 11;` to `constexpr uint8_t TFT_MISO_PIN   = 13;`
2. [ ] Save file
3. [ ] Ensure `platformio.ini` has: `-DDIAG_INVERT_DC=0` (revert Test 2 change if needed)
4. [ ] In terminal: `platformio run -e display_board -t upload`
5. [ ] Wait for success message
6. [ ] Unplug display board
7. [ ] Wait 5+ seconds
8. [ ] Plug back in
9. [ ] Wait 2 seconds for boot
10. [ ] **Observe**: Does RED framebuffer appear?
    - [ ] YES → **MOSI/MISO PINS ARE SWAPPED on PCB** ← ROOT CAUSE FOUND
    - [ ] NO → Proceed to Test 4
11. [ ] **Serial Output**: Should show boot sequence (no diagnostic message)

**Result**: RED appears? __ YES __ NO

---

### Test 4: CS Polarity Invert
**Action Required**: Modify `platformio.ini` and revert `src/display_main.cpp`

1. [ ] Revert `src/display_main.cpp` lines 49-50 to original:
   - Change back to: `constexpr uint8_t TFT_MOSI_PIN   = 13;`
   - Change back to: `constexpr uint8_t TFT_MISO_PIN   = 11;`
2. [ ] Save file
3. [ ] Edit `platformio.ini` line 20: change `-DDIAG_INVERT_CS=0` to `-DDIAG_INVERT_CS=1`
4. [ ] Save file
5. [ ] In terminal: `platformio run -e display_board -t upload`
6. [ ] Wait for success message
7. [ ] Unplug display board
8. [ ] Wait 5+ seconds
9. [ ] Plug back in
10. [ ] Wait 2 seconds for boot
11. [ ] **Observe**: Does RED framebuffer appear?
    - [ ] YES → **CS POLARITY IS INVERTED on PCB** ← ROOT CAUSE FOUND
    - [ ] NO → Hardware issue or incompatibility (see Troubleshooting)
12. [ ] **Serial Output**: Should show `[DIAGNOSTIC] CS polarity INVERTED`

**Result**: RED appears? __ YES __ NO

---

## Summary of Findings

| Test | RED Appears? | Root Cause |
|------|--------------|-----------|
| Baseline | [ ] YES / [ ] NO | — |
| DC Polarity | [ ] YES / [ ] NO | DC line inverted on PCB |
| Pin Swap | [ ] YES / [ ] NO | MOSI/MISO swapped on PCB |
| CS Polarity | [ ] YES / [ ] NO | CS line inverted on PCB |

**Root Cause Identified**: ____________________________________________________

---

## If RED Appears in Multiple Tests

This is unlikely but possible if multiple issues exist. Note which tests show RED:
- Tests: _________________________________________________

**Recommendation**: Combine fixes (e.g., invert DC AND swap MOSI/MISO)

---

## If RED Never Appears

**Possible causes**:
1. Hardware defect on display module
2. Controller not recognized (wrong init sequence for this display)
3. SPI timing issue (speed too fast/slow)
4. Power supply insufficient for display
5. PCB has additional undocumented pin swaps

**Next steps**:
- Check backlight brightness cycles (proves SPI bus working)
- Verify reset pulse occurs (GPIO19)
- Test with hardware SPI diagnostic (DC/CS toggling with logic analyzer)
- Consult display module datasheet: Hosyond MSP4022

---

## Testing Notes

- Each test requires **cold power-cycle** (5+ seconds unplugged)
- Warm restart is insufficient; ROM needs time to de-energize
- Serial output shows init table, backlight ramp, then diagnostic loop
- Brightness cycles through {0, 50, 100, 150, 200, 255} every 3 seconds
- If RED appears at any brightness level, display IS responding

---

## Rollback to Baseline

After testing, to return to baseline (all diagnostics disabled):

```powershell
# Revert platformio.ini flags to 0:
platformio run -e display_board -t upload

# Revert src/display_main.cpp pins to original:
# MOSI=13, MISO=11
```

---

**Created**: 2026-04-20
**Target**: ESP32-C6 Front Display Board (COM9)
**Module**: Hosyond MSP4022 / ST7796 controller
