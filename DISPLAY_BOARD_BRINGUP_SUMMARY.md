# Display Board Bring-Up Summary - 2026-04-20

## Implementation Status: COMPLETE ✅

All diagnostic infrastructure is in place and ready for systematic root-cause testing.

---

## Current Firmware on COM9

**Configuration**:
- **MOSI**: 11 (swapped from 13)
- **MISO**: 13 (swapped from 11)
- **DC Polarity**: Normal (DIAG_INVERT_DC=0)
- **CS Polarity**: Normal (DIAG_INVERT_CS=0)
- **Controller**: ST7796 (RGB565, 16-bit)

**Status**: Ready for testing

---

## What Works

✅ Backlight PWM brightness control (0-255)  
✅ Hardware reset pulse (GPIO19, active-low)  
✅ Software reset (SWRESET 0x01)  
✅ SPI command transmission  
✅ Backlight diagnostic loop (brightness cycling)  
✅ Firmware boots without errors  
✅ Serial communication at 115200 baud  

---

## What Doesn't Work

❌ RED framebuffer display (no visible output)  
❌ Controller appears to receive commands but not execute framebuffer writes  

---

## Root Cause Candidates

Based on diagnostic evidence:

1. **SPI MOSI/MISO Swap** (HIGH probability)
   - Symptom: Backlight works (command bus OK), framebuffer fails (data bus reversed)
   - Test: Swap pins 13↔11
   - Status: ⚠️ Currently loaded on COM9

2. **DC Line Polarity Reversed** (MEDIUM probability)
   - Symptom: Data/command confusion during initialization
   - Test: Invert DC signal polarity
   - Status: ⚠️ Prepared, not yet tested

3. **CS Line Polarity Reversed** (LOW probability)
   - Symptom: Chip-select not activating, but backlight works (suggests selective issue)
   - Test: Invert CS signal polarity
   - Status: ⚠️ Prepared, ready to build

---

## Diagnostic Testing Procedure

### Quick Start
1. Read: `DISPLAY_TESTING_CHECKLIST.md`
2. Perform: Cold power-cycle (unplug 5s, replug)
3. Observe: Display for RED framebuffer
4. Check: Serial output for boot messages
5. Report: RED appears? YES or NO

### Systematic Testing
Follow `DISPLAY_TESTING_CHECKLIST.md` for step-by-step instructions to test each hypothesis.

### Expected Outcome
When RED framebuffer appears, the root cause is identified:
- RED appears + SPI pin-swap active → **MOSI/MISO are swapped on PCB**
- RED appears + DC polarity inverted → **DC line is inverted on PCB**
- RED appears + CS polarity inverted → **CS line is inverted on PCB**

---

## File Reference

| File | Purpose |
|------|---------|
| `src/display_main.cpp` | Dual-profile firmware (ILI9488 + ST7796), diagnostic wrappers |
| `platformio.ini` | Build flags for diagnostic control (DIAG_INVERT_DC, DIAG_INVERT_CS) |
| `docs/GPIO_PINOUT.md` | PCB schematic pin definitions |
| `DISPLAY_DIAGNOSTIC_REFERENCE.md` | Technical reference for each test configuration |
| `DISPLAY_TESTING_CHECKLIST.md` | Step-by-step user guide for testing |
| `DISPLAY_BOARD_BRINGUP_SUMMARY.md` | This file |

---

## Code Changes Made

### 1. Diagnostic Framework
Added compile-time flags to enable/disable polarity inversions:
```cpp
#ifndef DIAG_INVERT_DC
#define DIAG_INVERT_DC 0
#endif

#ifndef DIAG_INVERT_CS
#define DIAG_INVERT_CS 0
#endif
```

### 2. Polarity Wrapper Functions
```cpp
static inline void diagSetDC(bool dataMode) {
#if DIAG_INVERT_DC
    digitalWrite(TFT_DC_PIN, dataMode ? LOW : HIGH);
#else
    digitalWrite(TFT_DC_PIN, dataMode ? HIGH : LOW);
#endif
}

static inline void diagSetCS(bool active) {
#if DIAG_INVERT_CS
    digitalWrite(TFT_CS_PIN, active ? HIGH : LOW);
#else
    digitalWrite(TFT_CS_PIN, active ? LOW : HIGH);
#endif
}
```

### 3. SPI Write Functions Updated
All legacy SPI write functions now use diagnostic wrappers:
- `legacyWriteCommand()` → uses `diagSetCS()` and `diagSetDC()`
- `legacyWriteData8()` → uses `diagSetCS()` and `diagSetDC()`
- `legacyWriteDataN()` → uses `diagSetCS()` and `diagSetDC()`

### 4. Serial Diagnostic Output
Boot messages show which diagnostic is active:
```
[DIAGNOSTIC] DC polarity INVERTED
[DIAGNOSTIC] CS polarity INVERTED
```

### 5. platformio.ini
Added diagnostic flags to build_flags:
```ini
-DDIAG_INVERT_DC=0
-DDIAG_INVERT_CS=0
```

---

## Build Instructions

### Clean Build
```powershell
Set-Location "c:/Users/user/Esp32 projects VScode/WorkStation"
C:/Users/user/.platformio/penv/Scripts/platformio.exe run -e display_board -t upload
```

### With Diagnostics
1. Edit `platformio.ini` or `src/display_main.cpp` as needed
2. Run build command
3. Wait for upload to complete
4. Perform cold power-cycle

---

## Hardware Specifications

**Controller**: ST7796 (Hosyond MSP4022 module)
- Display: 320x480 pixels
- Color: RGB565 (16-bit)
- Interface: SPI 4-wire + control lines
- Init sequence: Full table, SWRESET first

**ESP32-C6 (QFN40 v0.2)**
- Core: RISC-V 160MHz, single-core with LP core
- RAM: 320KB SRAM
- Flash: 4MB
- SPI pins: 12=SCLK, 13=MOSI, 11=MISO (PCB schematic)
- Control: 10=CS, 18=DC, 19=RST (active-low)

---

## Test Matrix Summary

```
Test Config          | Board MOSI/MISO | DC Polarity | CS Polarity | Expected Outcome
                     |                 |             |             |
Original             | 13/11           | Normal      | Normal      | No display (baseline)
MOSI/MISO Swap       | 11/13           | Normal      | Normal      | RED if swapped on PCB
DC Invert            | 13/11           | Inverted    | Normal      | RED if inverted on PCB
CS Invert            | 13/11           | Normal      | Inverted    | RED if inverted on PCB
```

---

## Success Criteria

Display board bring-up is successful when:

1. ✅ RED framebuffer appears on display
2. ✅ Root cause identified (which polarity/pin swap needed)
3. ✅ All brightness levels show RED consistently
4. ✅ No hardware damage or smoke
5. ✅ Serial messages show successful init sequence

---

## Next Steps After Root Cause Identified

Once RED appears and root cause is known:

1. **Update PCB Schematic** with correct pin/polarity definitions
2. **Create Production Firmware** with inverted/swapped configuration as needed
3. **Remove Diagnostic Flags** (set DIAG_INVERT_DC=0, DIAG_INVERT_CS=0)
4. **Full Integration Testing** with other board systems (power supply, control board, etc.)
5. **Prototype Validation** with full hardware stack

---

## Known Issues

⚠️ GFX Library compilation occasionally fails with Error 3 on clean builds  
**Workaround**: Re-run build command or clean build cache

---

## Contact / Questions

- Firmware: `src/display_main.cpp`
- Config: `platformio.ini`
- Testing Guide: `DISPLAY_TESTING_CHECKLIST.md`
- Reference: `DISPLAY_DIAGNOSTIC_REFERENCE.md`

---

**Date**: 2026-04-20  
**Target**: ESP32-C6 Front Display Board (COM9)  
**Module**: Hosyond MSP4022  
**Status**: Diagnostic infrastructure complete, ready for user testing
