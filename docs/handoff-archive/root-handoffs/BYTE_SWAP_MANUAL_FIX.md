# Manual Byte-Swap Fix (Build System Workaround)

## Problem
Build system having issues with GFX library compilation. PlatformIO flag approach blocked.  
**Solution**: Manually edit the color byte-send order in C++ code directly.

## Quick Fix (60 seconds)

### Step 1: Open File
Open `src/display_main.cpp`

### Step 2: Find the Function
Search for `legacyFillScreenRaw565` (around line 407)

### Step 3: Swap the Bytes

**Current (WRONG - causing B&W pattern):**
```cpp
for (uint32_t i = 0; i < px; ++i) {
    SPI.transfer(hi);  // HIGH BYTE FIRST
    SPI.transfer(lo);  // LOW BYTE SECOND
}
```

**Change To (TEST THIS):**
```cpp
for (uint32_t i = 0; i < px; ++i) {
    SPI.transfer(lo);  // LOW BYTE FIRST
    SPI.transfer(hi);  // HIGH BYTE SECOND
}
```

### Step 4: Build and Test
```bash
platformio run -e display_board -t upload
```

### Step 5: Observe
- Unplug display 5+ seconds
- Replug
- **If RED appears**: Byte-swap is correct ✅
- **If B&W continues**: Swap back and try alternatives

---

## Why This Matters

16-bit RGB565 color format requires bytes in specific order:
- **Hi-Lo** (current): Sends high byte first, low byte second
- **Lo-Hi** (test): Sends low byte first, high byte second

Display ST7796 controller may expect Lo-Hi order.

---

## Expected Result When Fixed

After byte-swap fix + upload + reboot:
- Backlight ramps brightness 0→255
- Screen fills with **RED** color
- Serial output shows diagnostics
- Display responds to subsequent color commands

---

## If Byte-Swap Fixes It

Commit the change:
```cpp
for (uint32_t i = 0; i < px; ++i) {
    SPI.transfer(lo);  // FINAL: Lo-Hi byte order
    SPI.transfer(hi);
}
```

This becomes the permanent fix for RED framebuffer.

---

## Build Command Quick Reference

```bash
# From WorkStation folder
cd "c:\Users\user\Esp32 projects VScode\WorkStation"

# Build only
platformio run -e display_board

# Build and upload
platformio run -e display_board -t upload
```

---

## Status

- ✅ SPI pins FIXED (11/13 instead of 13/11)
- ✅ B&W pattern CONFIRMED (data flowing)
- ⏳ Color byte order TESTING (next step)
- ⏳ RED framebuffer AWAITING (depends on byte-swap)

Once this fix works, RED will appear and diagnostic phase is COMPLETE.

---

**Action**: Edit `src/display_main.cpp` lines ~414-418, swap the SPI.transfer() call order, rebuild, and report if RED appears.
