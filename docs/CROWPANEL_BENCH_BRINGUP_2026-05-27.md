# CrowPanel Advance 4.3" HMI — Bench Bring-Up Handoff

Date: 2026-05-27
Phase: 4 — CrowPanel bring-up (validates UART protocol before custom board redesign)
Parent doc: docs/DEV_STATION_HANDOFF_2026-05-27.md (afternoon closeout)

---

## Hardware Summary

| Property | Value |
|----------|-------|
| Board | Elecrow CrowPanel Advance 4.3" HMI |
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Display | 4.3" 800×480 RGB TFT |
| Host interface | UART (UART0-IN connector on board edge) |
| Host connector type | XH2.54-4P (JST XH 2.54 mm pitch, 4-pin) |
| Power input | 5V ±5%, 2A max |
| USB-C | Present — can power the board independently |
| IO44 | UART RX (receives data from host) |
| IO43 | UART TX (sends data to host) |
| Backlight controller | STC8H1K28 at I2C address 0x30 (CrowPanel-internal) |
| Touch controller | I2C address 0x5D (CrowPanel-internal) |

---

## STOP — Confirm Before Building Any Cable

**The physical pin 3/4 assignment of the XH2.54-4P connector for IO44 vs IO43 is NOT confirmed.**

Before building a cable between J21 and the CrowPanel:

1. Go to https://github.com/Elecrow-RD/CrowPanel-Advance-4.3--HMI-ESP32-800x480
2. Open the schematic (PDF or KiCad source)
3. Find the "UART0-IN" connector
4. Record which physical pin number is IO44 (RX) and which is IO43 (TX)
5. Update the table in this doc and in docs/DISPLAY_INTERFACE_STANDARD.md

---

## Power Strategy — Bench Bring-Up

**Do NOT connect J21 pin 1 (+5V_Boot) to the CrowPanel for bench testing.**

The +5V_Boot rail on the HAT Rev-B is sourced from an LDO that is likely insufficient for the
CrowPanel's 2A max draw. The production power solution is an open design decision.

**For bench bring-up:**
- Power the CrowPanel via its **USB-C port** using a separate 5V/2A USB supply or bench PSU
- On the UART0-IN XH2.54-4P connector: connect only **pins 2, 3, and 4** (GND + TX + RX)
- Leave pin 1 (+5V) **disconnected**

---

## HAT Rev-B — J21 Connector Pin Assignment

| J21 Pin | Signal | Net | STM32 Pin | Note |
|---------|--------|-----|-----------|------|
| 1 | +5V | +5V_Boot | — | **Leave disconnected for bench** |
| 2 | GND | GND | — | Connect to CrowPanel GND |
| 3 | DISP_UART_TX | via R78 ← PB10 | USART3_TX | Connect to CrowPanel IO44 (RX) |
| 4 | DISP_UART_RX | via R79 ← PB11 | USART3_RX | Connect to CrowPanel IO43 (TX) |

Series resistors R78 and R79 = 33Ω on TX and RX lines respectively.

---

## CrowPanel UART0-IN — Expected Wiring

Cross-connect rule (TX of one side → RX of other):

| CrowPanel UART0-IN Pin | Signal | From J21 Pin |
|------------------------|--------|-------------|
| 1 | +5V | Not connected (use USB-C for power) |
| 2 | GND | J21 pin 2 |
| 3 | IO44 or IO43 — **verify** | J21 pin 3 or 4 — depends on confirmed pin order |
| 4 | IO44 or IO43 — **verify** | J21 pin 4 or 3 — depends on confirmed pin order |

Final assignment (fill in after verifying Elecrow schematic):

| XH2.54-4P Pin 3 | = IO______ | Connected to J21 pin ______ |
| XH2.54-4P Pin 4 | = IO______ | Connected to J21 pin ______ |

---

## Firmware

Firmware is in the `crowpanel-43-bringup/` project folder.

- Build system: PlatformIO
- Target: see `crowpanel-43-bringup/platformio.ini`
- UART baud: 115200 8N1
- Flash via USB-C (native ESP32-S3 USB or USB-UART bridge on board)

### Supported Commands

| Command | Response | Effect |
|---------|----------|--------|
| `PING\r\n` | `PONG\r\n` | Basic connectivity test |
| `STATUS\r\n` | JSON with 0x30/0x5D status | Checks backlight and touch I2C |
| `CMD:LABEL <text>\r\n` | `ACK:LABEL\r\n` | Displays text on screen |
| `CMD:COLOR <hex>\r\n` | `ACK:COLOR\r\n` | Fills screen with color |

**Note:** The I2C in the STATUS command is CrowPanel-internal (backlight at 0x30, touch at 0x5D).
This I2C is NOT the host interface. Do not wire I2C from the HAT to the CrowPanel.

---

## Bench Bring-Up Procedure

### Step 1 — Power on CrowPanel standalone

1. Connect USB-C power supply (5V/2A rated) to the CrowPanel USB-C port
2. Observe boot — display should show Elecrow splash screen
3. Note board version on boot screen (V1.0 or V1.1 — affects internal I2C addresses)
4. **Measure and record current draw** at: boot peak, idle steady-state
5. Pass: display initializes and holds stable. Fail: no display, no response.

### Step 2 — Verify XH2.54-4P pin 3/4 order

1. Open Elecrow schematic: https://github.com/Elecrow-RD/CrowPanel-Advance-4.3--HMI-ESP32-800x480
2. Locate UART0-IN connector; identify which pin is IO44 and which is IO43
3. Fill in the table above ("CrowPanel UART0-IN — Expected Wiring")
4. Update docs/DISPLAY_INTERFACE_STANDARD.md with confirmed pin order

### Step 3 — UART validation with USB-serial adapter (recommended first)

Using a USB-serial adapter eliminates HAT variables for initial validation:

1. USB-serial adapter GND → CrowPanel UART0-IN pin 2 (GND) — shared ground required
2. USB-serial adapter TX → CrowPanel UART0-IN pin with IO44 (RX)
3. USB-serial adapter RX → CrowPanel UART0-IN pin with IO43 (TX)
4. Open serial terminal on PC at **115200 8N1**
5. Flash `crowpanel-43-bringup/` firmware via USB-C first
6. Send: `PING\r\n`
7. Expected: `PONG\r\n`

### Step 4 — Validate full firmware

1. Send `STATUS\r\n` — expect both 0x30 and 0x5D to respond
2. Send `CMD:LABEL Hello World\r\n` — expect text on screen + `ACK:LABEL\r\n`
3. Send `CMD:COLOR 0xFF0000\r\n` — expect red fill + `ACK:COLOR\r\n`

### Step 5 — Wire HAT J21 to CrowPanel

1. Build or source a JST XH 4-pin (XH2.54-4P) cable
2. Wire per the J21 table above — confirm pin 3/4 order before connecting
3. J21 pin 2 (GND) → CrowPanel UART0-IN pin 2 (GND)
4. J21 pin 3 (DISP_UART_TX) → CrowPanel IO44 (RX)
5. J21 pin 4 (DISP_UART_RX) → CrowPanel IO43 (TX)
6. Leave J21 pin 1 disconnected
7. Flash HAT STM32 with USART3 test firmware at 115200
8. Test PING/PONG end-to-end through the HAT

### Step 6 — Measure and record current draw

1. With firmware running and display active:
   - Measure 5V input current at boot peak
   - Measure at idle (static screen)
   - Measure under CMD:COLOR / CMD:LABEL activity
2. Record all values — these become the spec for the J21 pin 1 power solution

---

## Pass/Fail Criteria

| Test | Pass | Fail |
|------|------|------|
| Boot standalone | Splash screen appears, display stable | No display / hangs |
| UART PING | `PONG\r\n` received | No response, timeout, or garbage |
| STATUS | Both 0x30 and 0x5D respond OK | "Device not found" for either |
| CMD:LABEL | Text visible on screen + `ACK:LABEL\r\n` | No screen change |
| CMD:COLOR | Color fills screen + `ACK:COLOR\r\n` | No screen change |
| End-to-end (HAT → CrowPanel) | PING/PONG via STM32 USART3 | No response |

---

## Troubleshooting

| Symptom | First check |
|---------|-------------|
| No boot | USB-C cable is power+data? 5V/2A confirmed? |
| No UART response | Baud 115200? TX/RX crossed (not straight)? GND connected? |
| STATUS I2C fails | Board version V1.0 vs V1.1 — I2C addresses may differ |
| Garbage UART data | Baud rate mismatch? 8N1 confirmed? |
| Pin 3/4 wrong | Swap them and retry — one of two possibilities |

---

## After Bench Testing — Update These Docs

1. **This file** — fill in confirmed XH2.54-4P pin 3/4 order table
2. **docs/DISPLAY_INTERFACE_STANDARD.md** — add confirmed CrowPanel pin order in Compatible Devices section
3. **open-loops.md** — close J21 display power loop with measured current values
4. **Next session handoff doc** — record all pass/fail results and any wiring corrections made

---

*Firmware: crowpanel-43-bringup/src/main.cpp*
*Interface spec: docs/DISPLAY_INTERFACE_STANDARD.md*
*HAT schematic ref: hardware/kicad/dsp-regulator-hat-rev-b/DSP-Regulator-HAT-RevB.kicad_sch*
