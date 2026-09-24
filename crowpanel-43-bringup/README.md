# CrowPanel 4.3 Bring-up (WorkStation)

This is a standalone PlatformIO starter target for the Elecrow CrowPanel Advance 4.3 inch board.

Current transport baseline for the active bench branch:
- Incoming telemetry: dedicated UART1 on `IO19` (RX) / `IO20` (TX)
- Source controller: STM32F103 Blue Pill USART3 (`PB10`/`PB11`)
- USB Serial remains the console path for CrowPanel commands and logs
- The temporary UART0 shared-console intake path is historical only

## Why this folder exists

- The official Elecrow repo is Arduino-sketch oriented.
- WorkStation uses PlatformIO for repeatable builds and handoff.
- This folder provides a clean baseline we can iterate without touching existing C6 display bring-up code.

## What this includes

- ESP32-S3 PlatformIO target (`esp32-s3-devkitc-1`)
- UART + I2C board-probe baseline (0x30 board controller and 0x5D touch probe)
- Official V1.2 CrowPanel 4.3 RGB pin mapping captured in serial boot output for verification
- Minimal serial command stub for host-display contract experiments

## Build

```powershell
C:/Users/user/.platformio/penv/Scripts/platformio.exe run -d "c:/Users/user/Esp32 projects VScode/WorkStation/crowpanel-43-bringup"
```

## Upload

```powershell
C:/Users/user/.platformio/penv/Scripts/platformio.exe run -d "c:/Users/user/Esp32 projects VScode/WorkStation/crowpanel-43-bringup" -t upload
```

## Serial monitor

```powershell
C:/Users/user/.platformio/penv/Scripts/platformio.exe device monitor -b 115200
```

## Serial commands

- `HELP`
- `PING`
- `STATUS`
- `RX`
- `UDI_STATUS`
- `UDI_OUTPUT ON|OFF` (sends `CMD:OUTPUT ...` over UDI UART)
- `UDI_ILIM CH1|CH2 <mA>` (sends `CMD:ILIM ...` over UDI UART)

## Notes

- This is a bring-up baseline, not full LVGL UI firmware yet.
- RGB panel init is intentionally deferred in this baseline to keep first compile/flash cycle reliable.
- Next step is enabling panel init after board revision confirmation and then layering LVGL.
