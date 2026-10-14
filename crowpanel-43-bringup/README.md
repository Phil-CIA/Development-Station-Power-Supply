# CrowPanel 4.3 Bring-up (WorkStation)

This is a standalone PlatformIO starter target for the Elecrow CrowPanel Advance 4.3 inch board.

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

## Export trend data

Install the PC-side serial dependency once:

```powershell
python -m pip install pyserial
```

With the CrowPanel connected over USB, capture up to 4,500 trend samples to
a timestamped CSV. The elapsed history depends on actual telemetry arrival
cadence:

```powershell
python .\tools\dump_log_csv.py COM12
```

Pass `--limit N` to capture only the newest N samples or `--output path.csv`
to choose the destination. The script waits for the complete `csv_begin` /
`csv_end` block and does not write a file on timeout. Existing CH1 columns
retain their order; CH2 millivolt/amp values and `has_ch2` validity plus the
`demo` marker are appended. CH2 value cells are empty when unavailable. The
capture is validated before write: malformed rows, console noise, exported
count mismatch, invalid/duplicate metadata, non-contiguous idx values, and
timestamp ordering outside uint32 rollover-safe bounds are rejected.

## Notes

- This is a bring-up baseline, not full LVGL UI firmware yet.
- RGB panel init is intentionally deferred in this baseline to keep first compile/flash cycle reliable.
- Next step is enabling panel init after board revision confirmation and then layering LVGL.
