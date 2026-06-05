# Export Path Checklist (2026-05-31)

## Purpose
Capture high-level data-export hardware direction for the stabilized CrowPanel/HAT telemetry baseline.

## Scope
1. Short-term path using current hardware.
2. Medium-term optional logging enhancement.
3. Long-term dedicated USB-UART path on HAT hardware.

## Short-Term (Now)
Use CrowPanel COM12 serial as the export path.

Checklist:
1. Use CrowPanel serial commands for CSV export (LOG_START/LOG_STOP/LOG_DUMP_CSV/LOG_STATUS).
2. Keep UART telemetry path stable and unchanged during export sessions.
3. Keep OTA disabled during bench export runs.
4. Record sample interval and units in CSV headers for parser stability.

## Medium-Term (Optional)
Add local buffering/storage option if longer autonomous logging is needed.

Checklist:
1. Evaluate SD-card or larger on-device ring buffer only if COM12 streaming is insufficient.
2. Keep CSV schema backward-compatible with current CrowPanel export.
3. Avoid changing telemetry frame format unless absolutely required.
4. Gate any added storage writes to avoid UI/render regressions.

## Long-Term (HAT Dedicated USB Path)
Implement dedicated HAT USB-UART export path around CH340C/STM32 when hardware revision resumes.

Checklist:
1. Base schematic/placement decisions on CH340C guidance in docs/CH340C_KICAD_PLACEMENT_SPEC.md.
2. Preserve UART net naming consistency (UART1_TX/UART1_RX) between STM32 and bridge path.
3. Keep USB-C CC resistor and basic USB2 wiring requirements explicit in schematic review.
4. Treat PCB file as hardware truth for manufactured boards; library mismatches are secondary.

## Decision Snapshot
1. Current recommended export path: CrowPanel COM12.
2. Hardware redesign work for dedicated HAT USB export is deferred.
3. Existing CH340C placement spec remains the source document for the deferred hardware path.
