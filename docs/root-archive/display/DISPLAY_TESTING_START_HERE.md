# NEXT STEPS - Display Board Testing

**Status**: Diagnostic firmware is ready on COM9. Your turn to test and report results.

---

## What You Need to Do

1. **Read**: `DISPLAY_TESTING_CHECKLIST.md` (this will take 5 minutes)
2. **Perform**: Cold power-cycle test (unplug 5+ seconds, replug)
3. **Observe**: Does RED framebuffer appear on the display?
4. **Report**: YES or NO

---

## Quick Reference

| File | Purpose |
|------|---------|
| **DISPLAY_TESTING_CHECKLIST.md** | Step-by-step testing instructions (START HERE) |
| **DISPLAY_DIAGNOSTIC_REFERENCE.md** | Technical details on each test configuration |
| **DISPLAY_BOARD_BRINGUP_SUMMARY.md** | Complete implementation overview |

---

## Current Firmware

**Loaded on COM9**: SPI Pin-Swap test version
- **MOSI**: 11 (swapped from 13)
- **MISO**: 13 (swapped from 11)
- **Status**: Ready for first cold-cycle test

---

## What to Expect

**Boot Sequence** (watch serial output at 115200 baud):
```
[display] boot - Arduino_GFX ST7796 probe
[pins] using PCB schematic mapping (CS=10, DC=18, RST=19)
[tft] hardware reset done
[minimal] deterministic dual-profile probe mode
[minimal] profile = ST7796 (full table)
[minimal] ramping backlight 0->255 over 5 seconds...
[minimal] backlight at full brightness
[minimal] fill RED using RGB565 over 320x480
[done] probe complete; screen should be solid RED
[diag] entering brightness diagnostic loop
[diag] brightness = 0 (hold 3s)
[diag] brightness = 50 (hold 3s)
...
```

**Expected Visual**:
- Backlight comes on and ramps from dark to bright (0-255) over 5 seconds
- Screen should show solid RED color
- Brightness cycles through levels {0, 50, 100, 150, 200, 255} every 3 seconds

---

## Test Outcome Matrix

| Test | RED Appears? | What It Means |
|------|--------------|--------------|
| SPI Pin-Swap (current) | YES | ✅ **MOSI/MISO are swapped on PCB** → Fix confirmed |
| SPI Pin-Swap (current) | NO | Continue testing (follow checklist) |

---

## If RED Appears in Current Test

**Congratulations!** Root cause identified: MOSI and MISO pins are swapped on the PCB.

**Next Actions**:
1. Confirm result by testing again (repeat cold-cycle)
2. Note which brightness levels show RED (should be all of them)
3. Notify lead engineer with result
4. Update PCB schematic with correct pin mappings
5. Prepare production firmware with pins swapped (11↔13)

---

## If RED Does NOT Appear

**Follow the checklist** to test the next hypothesis (DC polarity invert).

The checklist will guide you through:
1. Modifying the firmware config
2. Rebuilding and uploading
3. Performing cold-cycle test
4. Observing results

---

## Important Notes

- ⚠️ **ALWAYS perform a cold power-cycle** (unplug 5+ seconds, not just restart)
- 🔍 Watch for backlight brightness ramp (proves SPI bus is working)
- 📊 Monitor serial output at 115200 baud for boot messages
- 🔄 Each test takes ~2 minutes (build + upload + cold-cycle)
- 💾 All test configurations are prepared; no coding needed, just config changes

---

## Questions?

Refer to:
- **Testing steps**: `DISPLAY_TESTING_CHECKLIST.md`
- **Technical details**: `DISPLAY_DIAGNOSTIC_REFERENCE.md`
- **Overview**: `DISPLAY_BOARD_BRINGUP_SUMMARY.md`
- **GPIO reference**: `docs/GPIO_PINOUT.md`

---

**Ready?** Open `DISPLAY_TESTING_CHECKLIST.md` and start testing!
