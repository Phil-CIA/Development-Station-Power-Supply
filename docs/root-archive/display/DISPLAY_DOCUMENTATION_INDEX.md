# Display Board Bring-Up Documentation Index

**Project**: ESP32-C6 Front Display Board (COM9)  
**Module**: Hosyond MSP4022  
**Status**: Diagnostic phase - Ready for testing  
**Date**: 2026-04-20  

---

## 🚀 Quick Start

1. **START HERE**: Read [`DISPLAY_TESTING_START_HERE.md`](DISPLAY_TESTING_START_HERE.md)
2. **Then**: Follow [`DISPLAY_TESTING_CHECKLIST.md`](DISPLAY_TESTING_CHECKLIST.md)
3. **Cold-cycle**: Unplug 5+ seconds, replug
4. **Report**: RED appears? YES or NO

---

## 📚 Documentation Files

### Primary (For Testing)
| File | Purpose | Audience |
|------|---------|----------|
| **DISPLAY_TESTING_START_HERE.md** | Overview and immediate next steps | End user / tester |
| **DISPLAY_TESTING_CHECKLIST.md** | Step-by-step testing procedures with checkbox workflow | Tester |
| **DISPLAY_DIAGNOSTIC_REFERENCE.md** | Technical reference for each test configuration | Developer / advanced user |

### Reference (For Context)
| File | Purpose | Audience |
|------|---------|----------|
| **DISPLAY_BOARD_BRINGUP_SUMMARY.md** | Complete implementation summary with code changes | Developer |
| **docs/GPIO_PINOUT.md** | PCB schematic pin definitions | Electrical engineer |
| **docs/display-project/README.md** | Project design and requirements | Project lead |

### Source Code
| File | Purpose | Status |
|------|---------|--------|
| **src/display_main.cpp** | Dual-profile firmware with diagnostic wrappers | ✅ Complete |
| **platformio.ini** | Build config with diagnostic flags | ✅ Complete |

---

## 🧪 Test Versions Available

All test versions are documented in the **DISPLAY_TESTING_CHECKLIST.md**

### Version 1: DC Polarity Invert
- **Config**: `-DDIAG_INVERT_DC=1` in platformio.ini
- **Pins**: MOSI=13, MISO=11 (original)
- **Expected**: If DC line polarity is reversed on PCB, RED appears

### Version 2: SPI Pin Swap ⚡ (Current on COM9)
- **Config**: Edit src/display_main.cpp lines 49-50
  - `TFT_MOSI_PIN = 11`
  - `TFT_MISO_PIN = 13`
- **Expected**: If MOSI/MISO swapped on PCB, RED appears

### Version 3: CS Polarity Invert
- **Config**: `-DDIAG_INVERT_CS=1` in platformio.ini
- **Pins**: MOSI=13, MISO=11 (original)
- **Expected**: If CS line polarity is reversed on PCB, RED appears

---

## 🔍 What's Working

✅ **Backlight PWM**: Brightness control 0-255 functional  
✅ **Reset Sequences**: Hardware and software resets executing  
✅ **Command Transmission**: SPI bus operational  
✅ **Controller Init**: Full init tables executing without errors  
✅ **Serial Communication**: 115200 baud logging active  

## ❌ What's Not Working

❌ **RED Framebuffer**: No display output despite all other systems working  
⚠️ **Root Cause**: Data transmission failure or signal polarity issue (to be diagnosed)

---

## 📊 Expected Test Outcomes

```
When RED framebuffer appears:
├─ After DC Polarity Test  → DC line is inverted on PCB
├─ After SPI Pin-Swap Test → MOSI/MISO pins are swapped on PCB  
└─ After CS Polarity Test  → CS line is inverted on PCB
```

---

## 🛠️ Build & Deploy Quick Reference

### Build Latest
```powershell
cd "c:/Users/user/Esp32 projects VScode/WorkStation"
C:/Users/user/.platformio/penv/Scripts/platformio.exe run -e display_board -t upload
```

### Edit Configuration

**For DC Polarity Test**:
```ini
# platformio.ini line 19:
-DDIAG_INVERT_DC=1
```

**For SPI Pin Swap Test**:
```cpp
// src/display_main.cpp lines 49-50:
constexpr uint8_t TFT_MOSI_PIN   = 11;
constexpr uint8_t TFT_MISO_PIN   = 13;
```

**For CS Polarity Test**:
```ini
# platformio.ini line 20:
-DDIAG_INVERT_CS=1
```

---

## 💡 Hardware Details

| Component | Value |
|-----------|-------|
| **Microcontroller** | ESP32-C6 (QFN40 v0.2) |
| **Core Speed** | 160 MHz |
| **RAM** | 320 KB |
| **Flash** | 4 MB |
| **Display Controller** | ST7796 |
| **Display Resolution** | 320×480 pixels |
| **Color Format** | RGB565 (16-bit) |
| **Interface** | 4-wire SPI |
| **Backlight Pin** | GPIO 20 (PWM) |

### SPI Pin Mapping (PCB Schematic)
| Signal | GPIO |
|--------|------|
| SCLK   | 12   |
| MOSI   | 13   |
| MISO   | 11   |
| CS     | 10   |
| DC     | 18   |
| RST    | 19   |

---

## 📋 Diagnostic Workflow

```
START
  ├─ Cold Power-Cycle
  ├─ Observe Display
  └─ Check RED Framebuffer?
     ├─ YES → Root Cause Identified
     │        └─ Update PCB Schematic & Deploy Fix
     └─ NO → Switch Test Version
        ├─ Modify Config (DC/CS/Pin-Swap)
        ├─ Build & Upload
        ├─ Cold Power-Cycle
        ├─ Observe Display
        └─ Check RED Framebuffer?
           ├─ YES → Root Cause Identified
           └─ NO → Try Next Test
```

---

## ✅ Success Criteria

Display board bring-up is complete when:

1. ✅ RED framebuffer appears consistently on display
2. ✅ Root cause identified and documented
3. ✅ All brightness levels show expected output
4. ✅ No hardware damage observed
5. ✅ PCB schematic updated with correct configuration

---

## 🎯 Next Milestones

- **Phase 1** (Now): Identify root cause via diagnostic testing
- **Phase 2**: Update PCB schematic with correct pin/polarity mappings
- **Phase 3**: Deploy production firmware with confirmed configuration
- **Phase 4**: Full system integration testing with power supply and control board
- **Phase 5**: Prototype validation and go/no-go decision

---

## 📞 Support / Questions

- **Testing Questions**: See `DISPLAY_TESTING_CHECKLIST.md`
- **Technical Details**: See `DISPLAY_DIAGNOSTIC_REFERENCE.md`
- **Implementation Details**: See `DISPLAY_BOARD_BRINGUP_SUMMARY.md`
- **GPIO Reference**: See `docs/GPIO_PINOUT.md`
- **Firmware Source**: See `src/display_main.cpp`

---

## 📝 Document History

| Date | Change |
|------|--------|
| 2026-04-20 | Diagnostic framework complete; documentation created; ready for user testing |
| 2026-04-19 | Display static/pixelation observed; dual-profile probe implemented |
| 2026-04-18 | Initial bring-up; display_board environment created |

---

**Ready to test?** Open [`DISPLAY_TESTING_START_HERE.md`](DISPLAY_TESTING_START_HERE.md) now!
