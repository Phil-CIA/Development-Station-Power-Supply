# Phase 5: Parameter Integration Design (2-Channel Power Supply)

**Date:** 2026-07-03  
**Status:** Design Phase — Ready for Implementation  
**Scope:** Bind real system parameters to CrowPanel UI for 2-fixed-channel power supply  
**Reference:** IPS3608 dark-theme adaptation, Phase 4 visual baseline, UDI UART contract

---

## 1. Boot Sequence

```
Power On
  ↓
[Splash Screen] 3–5 seconds
  - Brand logo / product name
  - No user interaction
  ↓
[Setup Screen] User-configurable once per boot
  - Feature discovery & control mapping
  - User confirms defaults or adjusts
  - Sets: Current limits, protection thresholds, display preferences
  ↓
[Main Telemetry Screen] Live operation
  - Dual-channel V/I/P display
  - Status indicators
  - Protection state
  - User navigates between Main → Graph → Settings via encoder knob
```

**Gate to Main:** User must confirm Setup OR timeout (auto-proceed with defaults) after 30 seconds.

---

## 2. Parameter Contract

### Real System Data (from STM32 HAT via UART)

**Current telemetry frame (binary, 10 bytes):**
```
[0xAA][0x55][0x06][T][seq][v12_mV_lo][v12_mV_hi][i12_mA_lo][i12_mA_hi][CRC8]
```

**Proposed extended frame (Phase 5):**
```
[0xAA][0x55][LEN][T][seq][v5_mV_lo][v5_mV_hi][i5_mA_lo][i5_mA_hi]
                          [v3v3_mV_lo][v3v3_mV_hi][i3v3_mA_lo][i3v3_mA_hi]
                          [status][protection_flags][temp_C][CRC8]
```

**Status byte breakdown:**
- Bit 7: CH1 output enabled (1=on, 0=off)
- Bit 6: CH2 output enabled
- Bit 5: CH1 in CV mode (constant voltage)
- Bit 4: CH2 in CV mode
- Bit 3: Thermal warning (temp > threshold)
- Bit 2: Reserved
- Bit 1: Reserved
- Bit 0: Reserved

**Protection flags byte:**
- Bit 7: CH1 OVP triggered
- Bit 6: CH1 OCP triggered
- Bit 5: CH2 OVP triggered
- Bit 4: CH2 OCP triggered
- Bit 3: CH1 OTP triggered
- Bit 2: CH2 OTP triggered
- Bit 1: Communication link stale (no frame for > 1 second)
- Bit 0: Reserved

**Frame rate:** 4 Hz (250 ms interval)

### Display-Local Parameters (UI State)

**Setup Configuration (stored in flash or session):**
- CH1 current limit setpoint (0–3A, default TBD)
- CH2 current limit setpoint (0–2A, default TBD)
- CH1 OCP threshold (50–3000 mA, step 10 mA)
- CH2 OCP threshold (50–2000 mA, step 10 mA)
- CH1 OVP threshold (4.5–5.5V, step 0.1V)
- CH2 OVP threshold (3.0–3.6V, step 0.1V)
- OTP threshold (global, 45–80°C, step 1°C)
- Dark theme enabled (1=yes, 0=no — default: yes)
- Brightness (5–100%, step 5)

**User Controls (transient, not stored unless saved to preset):**
- CH1 output enable/disable toggle
- CH2 output enable/disable toggle
- Data group selector (preset 1–6 for quick parameter swap)
- Graph page timebase (0.1s, 0.2s, 0.5s)
- Screen: Main / Graph / Settings selector

---

## 3. Screen Layouts

### 3.1 Setup Screen (Boot Flow)

**Layout:**
```
╔════════════════════════════════════╗
║  Setup: Control Configuration      ║  ← Title
╠════════════════════════════════════╣
║                                    ║
║  CH1: +5V  Current Limit: 3.0 A   ║
║  CH1 OCP:  3000 mA  OVP: 5.5V     ║
║                                    ║
║  CH2: +3.3V  Current Limit: 2.0 A ║
║  CH2 OCP:  2000 mA  OVP: 3.6V     ║
║                                    ║
║  OTP Threshold: 70°C               ║
║  Theme: Dark   Brightness: 80%     ║
║                                    ║
╠════════════════════════════════════╣
║ ↑↓ Scroll  OK Confirm  ESC Cancel  ║  ← Instructions
╚════════════════════════════════════╝
```

**Encoder behavior:**
- Rotate: scroll through parameters
- Press: enter edit mode for selected parameter
- Long press: apply defaults and proceed to Main

**Expected parameters in order:**
1. CH1 current limit
2. CH1 OCP threshold
3. CH1 OVP threshold
4. CH2 current limit
5. CH2 OCP threshold
6. CH2 OVP threshold
7. OTP threshold
8. Brightness
9. Confirm & Start

---

### 3.2 Main Telemetry Screen

**Layout (IPS3608-inspired, dark theme):**

```
╔════════════════════════════════════╗
║ 🔊 ❄️  🔒       M1      CV   ⏹     ║  ← Status chips
╠════════════════════════════════════╣
║                                    ║
║  [CH1: +5V]                        ║
║  V 5.234 v  ← V set: 5.20v        ║  ← Yellow accent
║  A 1.234 A  ← I set: 1.60A        ║  ← Blue accent
║  P 6.3 W    Temp: 28°C            ║  ← Gray accent
║                                    ║
║  [CH2: +3.3V]                      ║
║  V 3.285 v  ← V set: 3.30v        ║  ← Yellow accent
║  A 0.542 A  ← I set: 0.50A        ║  ← Blue accent
║  P 1.8 W                           ║  ← Gray accent
║                                    ║
║  Capacity: 2456.3 Ah              ║
║  Energy: 14523.3 Wh               ║
║  Time: 13:06:17                    ║
║                                    ║
╠════════════════════════════════════╣
║ ↑↓ CH Select  RUN/STOP  ⚙ Settings ║  ← Navigation
╚════════════════════════════════════╝
```

**Status chips (top-right):**
- Volume icon (black if on, white if off)
- Fan icon (black if running, white if idle)
- Lock icon (black if locked, white if unlocked)
- Data group indicator (M1–M6)
- CV indicator (always on for fixed rails)
- RUN/STOP indicator (green if running, red if stopped)

**Encoder behavior:**
- Rotate (no input): toggle between CH1 and CH2 detail focus
- Rotate (after V/A press): scroll pages (Main → Graph → Settings)
- Press RUN/STOP button: toggle output enable for selected channel
- Press V/A: enter current-limit edit mode for selected channel
- Press encoder knob: confirm parameter edit or lock/unlock
- Long press encoder: clear statistics

---

### 3.3 Graph (VI Curve) Screen

**Layout:**

```
╔════════════════════════════════════╗
║ 🔊 ❄️  🔒       M1      CV   ⏹     ║  ← Status chips (same)
╠════════════════════════════════════╣
║  MAX: 5.20v (CH1)  MAX: 0.55A     ║  ← Yellow/Blue labels
║  MIN: 5.18v (CH1)  MIN: 0.10A     ║
║                                    ║
║  ┌────────────────────────────┐    ║
║  │ CH1 ↑   ╱╲╱╲  ← Voltage    │    ║  ← Yellow trace (CH1 V)
║  │     │  ╱  ╲                │    ║
║  │ CH2 ↓ ╱    ╲  ← Current    │    ║  ← Blue trace (CH1 I)
║  │     │╱      ╲              │    ║
║  │     ├────────┤             │    ║
║  │     T: 0.5s  ← Timebase    │    ║
║  └────────────────────────────┘    ║
║                                    ║
║  CH1 Data: V=5.23v I=1.24A        ║
║  CH2 Data: V=3.28v I=0.54A        ║
║                                    ║
╠════════════════════════════════════╣
║ ← TB Time Base      RUN/STOP  ⚙    ║  ← Timebase adjust / nav
╚════════════════════════════════════╝
```

**Legend:**
- **Yellow line:** Voltage trace (normalized to display width)
- **Blue line:** Current trace (normalized to display height)
- **Timebase:** 0.1s, 0.2s, 0.5s per division (left/right buttons adjust)

**Encoder behavior:**
- Rotate: scroll timebase (0.1s ↔ 0.2s ↔ 0.5s)
- Press: return to Main screen
- Long press: clear trend buffer

---

### 3.4 Settings Screen

**Layout:**

```
╔════════════════════════════════════╗
║  Settings: System Configuration    ║
╠════════════════════════════════════╣
║                                    ║
║  ✓ System                          ║  ← Current selection
║    Language: English               ║
║    Brightness: 80%                 ║
║    Volume: 40%                     ║
║    Style: Dark                     ║
║                                    ║
║  DataSet                           ║
║    Group 1–6 parameter presets     ║
║                                    ║
║  About                             ║
║    Model: WorkStation PSU          ║
║    Version: 1.0.0                  ║
║                                    ║
╠════════════════════════════════════╣
║ ↑↓ Menu   ► Enter   ← Back / ESC   ║
╚════════════════════════════════════╝
```

**Encoder behavior:**
- Rotate: select menu item (System / DataSet / About)
- Press: enter submenu
- Left arrow / ESC: return to Main

---

## 4. Operational State Machine

```
BOOT
  ↓
SPLASH (3–5 sec)
  ↓
SETUP (user interactive, 30 sec timeout)
  ├─→ [OK] → RUNNING / IDLE (Main screen, outputs OFF by default)
  └─→ [Timeout] → RUNNING / IDLE (Main screen with defaults)

RUNNING / IDLE
  ├─→ RUN/STOP press → toggle CH1 or CH2 output
  ├─→ V/A press → edit current limit for selected channel
  ├─→ Encoder rotate → select Main / Graph / Settings
  ├─→ Encoder press → navigate within current screen
  └─→ Power off → SHUTDOWN (save state, zero V/I)

FAULT STATE (if protection triggered)
  ├─→ OVP/OCP/OTP sets protection_flags bit
  ├─→ UI shows warning indicator on status chip
  ├─→ Output disabled automatically (hardware + firmware)
  └─→ User must acknowledge (Settings → clear fault)
```

---

## 5. Parameter Mapping: Display ↔ STM32 HAT

| Display Element | Source | Update Rate | Notes |
|-----------------|--------|-------------|-------|
| CH1 voltage | STM32 ADC (12-bit) | 4 Hz (250 ms) | Measures main +5V rail |
| CH1 current | STM32 ADC (12-bit) | 4 Hz | INA3221 or equivalent |
| CH2 voltage | STM32 ADC (12-bit) | 4 Hz | Measures main +3.3V rail |
| CH2 current | STM32 ADC (12-bit) | 4 Hz | INA3221 or equivalent |
| Temperature | STM32 internal sensor | 4 Hz | MCU junction or external NTC |
| CH1 output enable | STM32 GPIO (PA0/equivalent) | Immediate | User toggle via UI |
| CH2 output enable | STM32 GPIO (PA1/equivalent) | Immediate | User toggle via UI |
| CH1 current limit | CrowPanel flash or UI state | On change | User-set threshold (compare against ADC) |
| CH2 current limit | CrowPanel flash or UI state | On change | User-set threshold |
| OVP/OCP/OTP flags | STM32 firmware logic | 4 Hz | Comparator + firmware trip |
| Protection flags byte | STM32 UART frame | 4 Hz | Bits 7–2 encode all active faults |
| Thermal warning | STM32 firmware | 4 Hz | Set if temp exceeds OTP threshold |
| Data statistics | CrowPanel accumulator | Real-time | Ah/Wh/time updated locally |

---

## 6. Telemetry Frame Evolution

### Current Frame (Phase 4 — Binary, HAT → CrowPanel only)
```
Byte [0]: 0xAA (SOF1)
Byte [1]: 0x55 (SOF2)
Byte [2]: 0x06 (LEN: TAG + seq + v12 + i12)
Byte [3]: 'T'  (TAG: Telemetry)
Byte [4]: seq  (u8, increments per transmission)
Byte [5–6]: v12_mV (uint16_le, main +12V rail estimate or +5V)
Byte [7–8]: i12_mA (int16_le, main rail current)
Byte [9]: CRC8 (poly 0x07, init 0x00)
```

### Proposed Extended Frame (Phase 5+)
```
Byte [0]: 0xAA (SOF1)
Byte [1]: 0x55 (SOF2)
Byte [2]: 0x14 (LEN: 20 bytes of payload)
Byte [3]: 'T'  (TAG: Extended Telemetry)
Byte [4]: seq  (u8)
Byte [5–6]: v5_mV (uint16_le, +5V channel)
Byte [7–8]: i5_mA (int16_le, +5V channel current)
Byte [9–10]: v3v3_mV (uint16_le, +3.3V channel)
Byte [11–12]: i3v3_mA (int16_le, +3.3V channel current)
Byte [13]: temp_C (u8, 0–125°C)
Byte [14]: status (u8: [CH1_en, CH2_en, CH1_CV, CH2_CV, Thermal_warn, -, -, -])
Byte [15]: protection_flags (u8: [CH1_OVP, CH1_OCP, CH2_OVP, CH2_OCP, CH1_OTP, CH2_OTP, Link_stale, -])
Byte [16–19]: Reserved for future expansion
Byte [20]: CRC8
```

**Backward compatibility:** Old 10-byte frame remains valid. CrowPanel detects frame length and parses accordingly.

**Gate:** Only deploy extended frame after both CrowPanel and STM32 firmware are ready to handle it. Until then, Phase 5 uses 10-byte frame with derived/placeholder values.

---

## 7. Implementation Roadmap (Next Steps)

### 7.1 Immediate (Days 1–2)
1. ✓ Capture IPS3608 manual reference (done)
2. ✓ Create this Phase 5 design doc (done)
3. Commit both to repo
4. Schedule STM32 breadboard bring-up (parallel track, Phase A bench runsheet)

### 7.2 Phase 5a — UI Skeleton (Days 3–5)
1. Add Setup screen template to `main.cpp` (no functional parameter storage yet)
2. Update Main screen to show 2-channel layout (CH1 top, CH2 bottom)
3. Update Graph screen to dual-trace (CH1 + CH2)
4. Update Settings menu structure
5. Build + test on CrowPanel hardware (visual layout only, hardcoded demo data)

### 7.3 Phase 5b — Parameter Binding (Days 6–10)
1. Define flash storage for Setup parameters (on CrowPanel or off-device)
2. Add V/A button handler for current-limit editing
3. Add RUN/STOP toggle for per-channel output control
4. Implement telemetry frame parser (handle both 10-byte and 20-byte formats)
5. Bind status flags and protection warnings to UI indicators

### 7.4 Phase 5c — End-to-End (Days 11–14)
1. Flash updated CrowPanel firmware to hardware
2. Verify STM32 breadboard bring-up results (Phase A completion)
3. Cross-connect STM32 UART → CrowPanel UART0-IN per bench runsheet
4. Test parameter flow: STM32 ADC → UART frame → CrowPanel display
5. Verify output enable/disable commands (if reverse channel added)
6. Capture scope traces for boot-to-run sequence

---

## 8. Definition of Done (Phase 5 Complete)

1. ✓ Setup screen appears at boot, allows current-limit adjustment, proceeds to Main
2. ✓ Main screen displays real V/I/P for both channels (4 Hz update)
3. ✓ Protection flags displayed as visual indicators (red/yellow chips if triggered)
4. ✓ Graph page shows dual traces (V and I) with adjustable timebase
5. ✓ RUN/STOP button toggles CH1 output enable/disable (or per-channel toggle)
6. ✓ V/A button allows current-limit editing (stored in Setup, used for OCP compare)
7. ✓ Guarded build/flash workflow passes for CrowPanel target
8. ✓ No regression in screen navigation or render stability
9. ✓ STM32 ↔ CrowPanel UART link confirmed with bench-captured data

---

**Document Version:** 1.0  
**Last Updated:** 2026-07-03  
**Status:** Ready for Development Sprint  
**Next Review:** After Phase 5a UI skeleton checkpoint (Day 5)
