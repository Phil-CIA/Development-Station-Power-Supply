# Display Board Diagnostic Reference - 2026-04-20

## Current Status
- **Hardware**: ESP32-C6 (COM9) + Hosyond MSP4022 display module
- **Firmware**: SPI diagnostic probe with dual-profile controller support (ILI9488 + ST7796)
- **Working**: Backlight PWM brightness control (0→255), reset sequences, command transmission
- **Not Working**: RED framebuffer never appears (data transmission failure suspected)

## Test Versions Available

### How to Switch Between Test Versions

Each version requires modifying `platformio.ini` and/or `src/display_main.cpp`, then rebuilding.

#### Version 1: DC Polarity Invert Test
**Purpose**: Test if Data/Command (DC) line polarity is inverted on PCB

**Configuration**:
```ini
# In platformio.ini [env:display_board]:
build_flags = ... -DDIAG_INVERT_DC=1
```

**Pin Config**: MOSI=13, MISO=11 (original)

**Expected Result**: If PCB has DC polarity backwards, RED framebuffer appears

---

#### Version 2: SPI Pin Swap Test  
**Purpose**: Test if MOSI and MISO pins are swapped on PCB wiring

**Configuration**:
```cpp
// In src/display_main.cpp lines 48-49:
constexpr uint8_t TFT_MOSI_PIN   = 11;  // SWAPPED
constexpr uint8_t TFT_MISO_PIN   = 13;  // SWAPPED
```

**DC Polarity**: DIAG_INVERT_DC=0 (normal)

**Expected Result**: If PCB MOSI/MISO are reversed, RED framebuffer appears

---

#### Version 3: CS Polarity Invert Test
**Purpose**: Test if Chip-Select (CS) line polarity is inverted on PCB

**Configuration**:
```ini
# In platformio.ini [env:display_board]:
build_flags = ... -DDIAG_INVERT_CS=1
```

**Pin Config**: MOSI=13, MISO=11 (original)

**Expected Result**: If PCB has CS polarity backwards, RED framebuffer appears

---

## How to Test

### Prerequisites
1. Unplug display board from power for 5+ seconds
2. Plug back in
3. Wait 2 seconds for boot
4. Observe display for solid RED color
5. Monitor serial output (COM9, 115200 baud) for diagnostic messages

### Serial Output Indicators

```
[minimal] profile = ST7796 (full table)
[minimal] ramping backlight 0->255 over 5 seconds...
[minimal] backlight at full brightness
[minimal] fill RED using RGB565 over 320x480
[done] probe complete; screen should be solid RED
[diag] entering brightness diagnostic loop — observe at which level RED appears
[diag] brightness = 0 (hold 3s)
[diag] brightness = 50 (hold 3s)
...
```

### Diagnostic Message Indicators

If active:
- `[DIAGNOSTIC] DC polarity INVERTED` → Version 1 (DC-invert)
- `[DIAGNOSTIC] CS polarity INVERTED` → Version 3 (CS-invert)
- No diagnostic message + MOSI=11, MISO=13 → Version 2 (SPI pin-swap)

---

## Quick Build/Upload Steps

### Build and Upload Current Version
```powershell
Set-Location "c:/Users/user/Esp32 projects VScode/WorkStation"
C:/Users/user/.platformio/penv/Scripts/platformio.exe run -e display_board -t upload
```

### Switch to DC Polarity Test
1. Edit `platformio.ini`, change `build_flags` line to: `-DDIAG_INVERT_DC=1`
2. Ensure `src/display_main.cpp` has original pins: `MOSI=13, MISO=11`
3. Build and upload
4. Cold-cycle (unplug 5s, replug)
5. Observe RED framebuffer

### Switch to SPI Pin-Swap Test
1. Edit `src/display_main.cpp` lines 48-49:
   ```cpp
   constexpr uint8_t TFT_MOSI_PIN   = 11;
   constexpr uint8_t TFT_MISO_PIN   = 13;
   ```
2. Edit `platformio.ini`, ensure: `-DDIAG_INVERT_DC=0`
3. Build and upload
4. Cold-cycle (unplug 5s, replug)
5. Observe RED framebuffer

### Switch to CS Polarity Test
1. Edit `platformio.ini`, change `build_flags` line to: `-DDIAG_INVERT_CS=1`
2. Ensure `src/display_main.cpp` has original pins: `MOSI=13, MISO=11`
3. Build and upload
4. Cold-cycle (unplug 5s, replug)
5. Observe RED framebuffer

---

## Expected Outcomes

| Test | RED Appears | Likely Root Cause |
|------|-------------|-------------------|
| DC-Invert | YES | DC line polarity on PCB is inverted |
| DC-Invert | NO | Continue to next test |
| SPI Pin-Swap | YES | MOSI/MISO pins swapped on PCB schematic |
| SPI Pin-Swap | NO | Continue to next test |
| CS-Invert | YES | CS line polarity on PCB is inverted |
| CS-Invert | NO | Hardware defect or controller incompatibility |

---

## Additional Notes

- Backlight brightness cycles {0, 50, 100, 150, 200, 255} every 3 seconds after init
- If RED appears at any brightness level, the display IS working—just polarity/pin issue
- All three tests use ST7796 profile (RGB565, 16-bit color)
- Serial boot logs show which controller profile is active
- Reset pulse: active-low on GPIO19 (120ms low time)

---

## File Locations

- Firmware source: `WorkStation/src/display_main.cpp`
- Build config: `WorkStation/platformio.ini`
- PIN reference: `WorkStation/docs/GPIO_PINOUT.md`
- This guide: `WorkStation/DISPLAY_DIAGNOSTIC_REFERENCE.md`
