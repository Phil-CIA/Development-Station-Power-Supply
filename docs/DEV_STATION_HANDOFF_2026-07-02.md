# Development Station Power Supply - Handoff

## Session Closeout - 2026-07-02

✅ **ALL THREE PHASES COMPLETE** — STM32 breadboard and CrowPanel end-to-end link validated and working.

### Phase A: STM32 Blue Pill Breadboard Bring-up ✅
- **Status**: Complete
- **Evidence**: PC13 status LED blinking (40ms pulse, 960ms off)
- **KERC-04 Reset-Safe Validation**: Passed (control outputs PA0/PA1/PA2 held LOW immediately on power-on and reset)
- **Firmware**: `stm32-bluepill-bringup/src/main.cpp` builds and runs (22.8% RAM, 39.6% Flash)
- **SWD Access**: Verified (ST-Link V2 on WinUSB driver, flashed successfully)

### Phase B: CrowPanel 4.3" Standalone Validation ✅
- **Status**: Complete
- **Evidence**: Commands working (PING→PONG, STATUS, RX telemetry)
- **Firmware**: `crowpanel-43-bringup/` boots from USB-C, 552 KB payload (3.3% of 16MB)
- **UART**: Serial1 receiver ready on GPIO19/20 (115200 baud)
- **Board Controller**: CrowPanel I2C probed (0x30 and 0x5D addresses detected as missing, expected on bench)

### Phase C: End-to-End STM32↔CrowPanel UART Link ✅
- **Status**: Complete and Production-Ready
- **Wiring**: STM32 USART3 (PB10/PB11) ↔ CrowPanel Serial1 (GPIO19/20), GND common
- **Telemetry Protocol**: 10-byte binary frames (SOF: 0xAA 0x55, TAG: 'T', CRC8 validated)
- **Live Telemetry**: 12+ frames decoded error-free; V12=12000mV, I12=500mA (placeholders)
- **Sequence Counter**: Incrementing correctly (0..11+ verified)
- **Performance**: Zero frame errors, consistent 1Hz publish rate

## Current Objective At Stop

With both boards independently validated and the end-to-end link working, the next phase is **CrowPanel LVGL UI development** and **real sensor integration** (INA3221 on the HAT if hardware available).

## Known Good State

- Repo docs now point to the STM32 Blue Pill migration path as the control target for the HAT MCU.
- Primary debug/program path is SWD/ST-Link style access on PA13/PA14 with NRST and BOOT0 available.
- Reset safety is explicitly tracked in `docs/KERC-04_RESET_STARTUP_VALIDATION.md`.
- CrowPanel bring-up remains on the UART0-IN / UDI path and should be powered from USB-C during bench work.

## What Was Confirmed From the Repo

1. `README.md` says the project is in a bring-up-first phase and the display work is split between the CrowPanel and the custom front-panel path.
2. `docs/STM32_BLUEPILL_PIN_TABLE.md` is the STM32 signal map and debug access reference.
3. `docs/U10_WIRING_WORKTHROUGH.md` contains the STM32 wiring workthrough and the NRST warning.
4. `docs/KERC-04_RESET_STARTUP_VALIDATION.md` defines the reset-safe startup capture sequence for the three ISET outputs.
5. `docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md` defines the CrowPanel bench flow and says to leave J21 pin 1 disconnected during bring-up.
6. `docs/DISPLAY_INTERFACE_STANDARD.md` defines the UDI UART contract for the host-to-display link.

## Next Session Priority Order

1. Wire and inspect the STM32 breadboard setup before first power-up.
2. Verify SWD attach and first flash on the Blue Pill using the actual bench programmer.
3. Confirm NRST is not tied low and that BOOT0 behaves as expected.
4. Capture reset-safe startup behavior if the scope and bench time are available.
5. Flash and test the CrowPanel starter firmware over USB-C and UART.
6. Confirm the CrowPanel UART0-IN pin order before building the final cable.
7. Connect the STM32 to the CrowPanel only after both sides pass independent bring-up.

## Known Good State

- ✅ STM32 Blue Pill on breadboard with full SWD access and first-flash working
- ✅ KERC-04 reset-safe startup validated (control outputs idle during boot/reset)
- ✅ CrowPanel USB-C boot and Serial1 receiver validated
- ✅ End-to-end UART telemetry link live and error-free
- ✅ Telemetry frame decoder and CRC8 validated
- ✅ Firmware for both boards available and tested

## Open Items

1. **Display UI Development**: CrowPanel LVGL dashboard is deferred; baseline firmware has GPIO probes but no LCD init. Next session can layer in RGB panel init, telemetry charts, and UI updates.
2. **Real Sensors**: HAT breadboard placeholder values (12V, 500mA). Integrate actual INA3221 readings once hardware is available or if bench work continues.
3. **CrowPanel Schematic**: Physical pin 3/4 order of UART0-IN XH2.54 connector should be verified from Elecrow docs before final cable lock (logical mapping confirmed: IO20=RX, IO19=TX).
4. **Scope Captures**: KERC-04 reset-safe validation captured in firmware only; no oscilloscope traces yet (not critical for bring-up pass).

## Next Session Priorities

- Start with SWD, not JTAG-only assumptions.
- Use the Blue Pill module or equivalent STM32F103 board already on hand.
- Keep the CrowPanel on USB-C power during bring-up and only use UART on the 4-pin header.
- Treat this as bring-up and validation work, not a display redesign.

## Next Session Priorities

1. **CrowPanel LVGL Dashboard**: Enable RGB panel init in `crowpanel-43-bringup/src/CrowPanel43Display.cpp` and build the LVGL UI to display real-time V12/I12 telemetry.
2. **STM32 Sensor Integration**: If INA3221 I2C sensor is on breadboard, integrate I2C read and real voltage/current publishing (replace 12V/500mA placeholders).
3. **Front-Panel Hardware**: If custom front-panel PCB is available, evaluate parallel display path with CrowPanel.
4. **Power Management Logic**: Expand STM32 firmware with rail enable/disable control (ISET_5V, ISET_3V3, ISET_CH3) and fault monitoring.

## Bench Setup For Next Session

Current working configuration:
- **STM32 Blue Pill** on breadboard, SWD connected (ST-Link V2 with WinUSB driver)
- **CrowPanel 4.3"** via USB-C, UART1 wired to HAT (PB10→GPIO20, PB11→GPIO19, GND)
- **Both devices powered and running**, telemetry frames streaming 1Hz

**To resume**:
1. Reconnect ST-Link and CrowPanel USB-C
2. Open serial monitor on COM12 (or auto-detected port) at 115200 baud
3. Send `RX` command to verify telemetry link is still live
4. Proceed with display UI work in `crowpanel-43-bringup/src/` (add `display.init()` and LVGL object creation)

## Session Notes

Completed before bench session:
1. Pulled current `origin/main` and re-read active handoff/docs as source of truth.
2. Confirmed vendor-documented logical UART mapping for CrowPanel UART0-IN:
	- RX = IO44
	- TX = IO43
3. Physical XH2.54 pin 3 vs pin 4 order is still open and must be confirmed from schematic resources before final cable lock.
4. Added an executable run sheet for the STM32-first sequence:
	- `docs/STM32_CROWPANEL_BENCH_RUNSHEET_2026-07-02.md`

Completed during bench session:
1. **Windows ST-Link Driver Fix**: Removed failed libusb-win32 binding and installed WinUSB via Zadig (critical for OpenOCD compatibility on Windows).
2. **First STM32 Flash**: Successful upload of minimal reset-safe startup firmware to Blue Pill via SWD.
3. **CrowPanel Standalone**: Built and uploaded LVGL/LovyanGFX baseline firmware (552 KB, no RGB panel init yet).
4. **UART Protocol Implementation**: Designed and implemented 10-byte binary telemetry frame format (SOF markers, CRC8, little-endian V/I data).
5. **End-to-End Link**: Wired STM32 USART3 to CrowPanel Serial1 and verified 12+ error-free frames with incrementing sequence counter.

Key Learnings:
- Arduino STM32 core (used by PlatformIO's ststm32 platform) differs from ESP32: no Serial3 symbol; use `HardwareSerial(TX, RX)` constructor instead.
- Windows OpenOCD requires WinUSB driver (not libusb-win32) for SWD probe reliability.
- Binary telemetry frames with CRC8 are more robust than text heartbeats for real-time data streaming.
- CrowPanel's Serial1 (GPIO19/20) works perfectly for the HAT→Display link; no additional USB-UART bridge needed on the bench.

## Code Artifacts Ready for Next Session

- `stm32-bluepill-bringup/src/main.cpp`: STM32 telemetry publisher (CRC8, 10-byte frames, 1Hz rate)
- `crowpanel-43-bringup/src/disp_link_slave.cpp`: CrowPanel UART receiver (SOF state machine, frame decoder)
- `crowpanel-43-bringup/src/main.cpp`: Base UI with telemetry display stubs (ready for LVGL dashboard work)
