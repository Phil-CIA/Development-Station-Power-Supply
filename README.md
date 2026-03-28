# Development-Station-Power-Supply
3 channel bench supply with current and voltage plotting.

## Current architecture status

Hardware topology now tracked as:

- External supply (12V) -> Regulator board -> Hat board -> auxiliary boards (display and USB hub)
- Regulator board performs buck conversion for:
  - 5V rail
  - 3.3V rail
  - Adjustable rail (relay-selected 3.3V or 5V)
- Hat board performs measurement/control:
  - Voltage and current measurement for all rails
  - Incoming 12V measurement path
  - High/low range shunt paths
  - Feedback and current-limit control signaling to regulator board

## Firmware stopping point (March 28, 2026)

This repository is paused at a stable checkpoint.

Completed and pushed to main:

- Added logical rail mapping layer for hat telemetry in src/power_telemetry_map.h
- Added runtime topology and rail snapshot commands
- Added automatic range selection (high/low) with hysteresis
- Added per-rail/per-range calibration support
- Added persistent storage (NVS) for:
  - autorange enabled state
  - range thresholds
  - calibration values

Latest commits on main:

- 9590dbe Persist rail auto-range and calibration config in NVS
- fcad256 Add hat rail auto-range and calibration controls

## Runtime command quick reference

Telemetry and mapping:

- HWMAP
- RAILSNAP
- RCALSHOW

Range control:

- AUTORANGE ON
- AUTORANGE OFF
- RTHR <low_mA> <high_mA>

Calibration:

- RCAL <rail> <range> <vgain> <voff> <igain> <ioff>
- Rail options: 5V, 3V3, ADJ, IN12
- Range options: HIGH, LOW, SINGLE

Persistent config:

- CFGSAVE
- CFGLOAD
- CFGRESET
- CFGERASE

## Resume checklist

When returning to work:

1. Build firmware: platformio run
2. Flash board: platformio run -t upload
3. Verify mapping: HWMAP
4. Verify live readings: RAILSNAP
5. Tune thresholds and calibration as needed:
   - RTHR 250 900
   - RCAL ...
   - CFGSAVE

## Display board note

Display-board details and pinout references remain in docs/GPIO_PINOUT.md.
