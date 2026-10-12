# IPS3608 Reference Manual — Key Specifications

**Source:** IPS3608 AC-DC Intelligent Digital Control Power Supply User Manual V1.0

This document extracts key specifications from the IPS3608 manual for design adaptation to a 2-fixed-channel power supply system.

---

## Hardware Specifications

| Parameter | Value |
|-----------|-------|
| Output voltage range | DC 0–36V |
| Output current range | 0–8A |
| Output power range | 0–285W |
| Voltage accuracy | ±(0.3% + 3 bits) |
| Current accuracy | ±(0.15% + 5 bits) |
| Voltage resolution | 0.01V |
| Current resolution | 0.001A |
| Operating temperature | –10°C to +40°C, 0–75% RH |
| Display | 2.8-inch color screen |
| Product size | ≈138 × 214 × 115 mm |
| Product weight | ≈1539g |

### Protection Mechanisms
- Overvoltage protection (OVP)
- Overcurrent protection (OCP)
- Overpower protection (OPP)
- Overtemperature protection (OTP)
- Undervoltage protection (UVP)
- Output reverse injection protection
- Input reverse connection protection
- Short circuit protection

### Fast Charge Support
- **USB-C:** PD3.0, HUAWEI FCP, HUAWEI SCP
- **USB-A:** AFC, QC2.0, QC3.0

---

## UI/Display Features (IPS3608 Reference)

### Main Display Sections

**1. Voltage Display Area**
- Large yellow accent on measurement value
- Shows both measured (output) and set (target) voltage
- Format: `V set` and numerical values

**2. Current Display Area**
- Large blue accent on measurement value
- Shows both measured (output) and set (target) current
- Format: `I set` and numerical values

**3. Power Display Area**
- Gray accent, shows total power in watts
- Shows device temperature (°C) — not user-adjustable

**4. Data Statistics Area**
- Capacity (Ah): total charge delivered
- Energy (Wh): total energy delivered
- Time (HH:MM:SS): device runtime

**5. Status Indicators (Right Side Chips)**
- Volume on/off
- Cooling fan status
- Lock indicator
- OK button validity
- Data group selector (M1–M6)
- CV/CC status
- RUN/STOP state

**6. VI Curve Page**
- Real-time graph showing voltage (yellow) vs current (blue)
- MAX/MIN labels for both
- Timebase adjustment (0.1s–0.5s)

### Theme / Color Palette (Dark Theme)
- **Yellow accent:** Voltage measurements
- **Blue accent:** Current measurements
- **Gray accent:** Temperature, power, neutral elements
- **Dark background:** Primary display area
- **Green buttons:** Confirmation (OK, CV, etc.)
- **Red buttons:** Stop/error states

### Button Layout (Physical Controls)
1. **Encoder knob:** Page navigation, value adjustment
2. **Up/Down buttons:** Data group or menu selection
3. **Left/Right buttons:** Digit selection or parameter stepping
4. **V/A button:** Toggle between voltage and current input modes
5. **RUN/STOP button:** Output enable/disable
6. **Settings menu button:** Enter/exit system settings
7. **Long-press encoder:** Clear data statistics

### Data Group Storage
- 6 separate parameter sets (M1–M6)
- Each group stores:
  - Voltage setting (0–36V)
  - Current setting (0–8A)
  - OVP threshold (0V–36.10V)
  - OCP threshold (0–8.2A)
  - OPP threshold (0W–295.2W)
  - OTP threshold (0–99°C)

### Settings Menu Structure
```
Settings
├── System
│   ├── Language (Chinese, English)
│   ├── Brightness (5%–100%)
│   ├── Volume (0%–100%)
│   ├── Metering switch (ON/OFF)
│   ├── Device address (000–255)
│   └── Style switch (Light/Dark)
├── DataSet
│   ├── Group 1–6 (each with V, I, OVP, OCP, OPP, OTP)
└── About
    └── Model, version, factory reset
```

---

## Operational Flow (from manual Section 5.1)

1. Power on device → enter main interface
2. Press Settings to enter menu
3. Select DataSet group or System settings
4. Adjust parameters with buttons/encoder
5. Set voltage and current via V/A button
6. Confirm with OK button
7. Press RUN/STOP to enable output
8. Monitor V/I/P on main screen
9. Use encoder to switch to VI curve page
10. Power off: reduce V and I to minimum, then press RUN/STOP to disable output

---

## Design Adaptation for 2-Fixed-Channel System

### Channel-to-FNIRSI Mapping

| IPS3608 Element | 2-Channel Adaptation |
|-----------------|-------------------|
| Single voltage dial (0–36V) | Channel 1: Fixed +5V, Channel 2: Fixed +3.3V |
| Single current limit (0–8A) | CH1: User-configurable limit, CH2: User-configurable limit |
| CV indicator | Applies to both channels (always-on for fixed rail) |
| Data groups (M1–M6) | Retain for storing presets per channel pair or scenario |
| RUN/STOP button | Split to CH1 Enable / CH2 Enable (or single global toggle) |
| Status chips | Adapt to show CH1/CH2 status separately |
| VI curve | Show both channels on same graph (dual traces) or selector |
| Protection flags | OVP/OCP/OTP per channel |
| Temperature | Show PCB temperature or per-channel thermal state |

### Boot Sequence (2-Channel)
1. **Splash:** Brand/product screen (3–5 seconds)
2. **Setup:** Display features & control configuration (user-defined, then confirm)
3. **Main Telemetry:** Live V/I/P for both channels, status, protection state

### Key UI Changes vs IPS3608
- **Dual channel layout:** Main screen shows CH1 top, CH2 bottom (or side-by-side)
- **No voltage adjustment:** Voltage is fixed per channel; only current limit is user-configurable
- **Simplified output control:** Enable/disable per channel instead of single V/I dial
- **Simplified data groups:** Store current-limit presets instead of full V/I pairs
- **Graph page:** Show dual traces (one per channel) on same timebase

---

## Reference for Implementation

### Files Extracted from Manual
- Section 2.3: Main page layout and status chip definitions
- Section 2.4: VI curve page with dual-trace potential
- Section 2.5: Multi-output layout (USB-A/USB-C) — template for 2-channel display
- Section 3.2: Specification ranges and accuracy formulas
- Section 4.3: DataSet configuration structure
- Section 6: Troubleshooting and error conditions

### Next Steps for Phase 5
1. Define exact channel voltage/current limits (5V/3.3V specs from your HAT)
2. Create Setup screen flow (which parameters to configure at boot)
3. Map telemetry frame additions (if any extra channels needed beyond V/I/P)
4. Finalize status chip indicators (protection flags, thermal, link status)
5. Update `crowpanel-43-bringup/src/main.cpp` with 2-channel rendering

---

**Manual Version:** 1.0  
**Captured:** 2026-07-03  
**Adapter Context:** IPS3608 dark-theme reference for 2-fixed-channel development supply UI
