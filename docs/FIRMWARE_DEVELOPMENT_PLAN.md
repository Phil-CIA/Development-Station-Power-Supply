# Firmware Development Plan

Status: active. This is the feature inventory and branch plan for firmware
work going forward, replacing "figure it out from old handoff docs" with a
single tracked list. Follow `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md` for how to
branch, PR, and merge each item below.

This was built by reading the actual firmware source in all four targets
(not just the design docs), so the status column reflects what the code
does today, not what a doc says it should do.

## The four firmware targets

| Target | Path | MCU | Role |
|---|---|---|---|
| HAT reference implementation (legacy) | `src/rev1/main.cpp` (`env:esp32dev`) | ESP32 (generic dev board) | Mature prototype of measurement/control logic. Wrong MCU for the current board — being superseded by STM32 — but it's the most complete implementation of several features and is the porting source for them. |
| HAT controller-of-record | `stm32-bluepill-bringup/` | STM32F103C8T6 "Blue Pill" | The actual bench controller going forward per `platformio.ini`. Currently the least complete and doesn't build (issue #3). |
| Front-panel display, primary path | `crowpanel-43-bringup/` | ESP32-S3 (CrowPanel Advance 4.3") | Active development target for the display. Furthest along of the display paths. |
| Front-panel display, secondary path | `src/rev1/display_main.cpp` (`env:display_board`) | ESP32-C6 (custom board) | Paused. Still at raw hardware bring-up (byte-swap/DC-invert diagnostics), predates the UDI UART standard. |

## Feature inventory

| Feature | Status | Where | Notes |
|---|---|---|---|
| Rail V/I telemetry read (INA3221) | ✅ Done | `stm32-bluepill-bringup/src/main.cpp` | Real hardware reads for +5V and +3.3V rails. |
| Extended UART telemetry frame, STM32 → display | ✅ Implemented both ends, ⛔ blocked | `stm32-bluepill-bringup/src/main.cpp` (`publishTelemetry`) + `crowpanel-43-bringup/src/disp_link_slave.cpp` (`parseFrame`) | Frame layouts match on both ends. Can't prove it end-to-end until issue #3 is fixed. |
| CrowPanel LVGL UI skeleton (Splash/Setup/Main/Graph/Settings) | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | All five screens exist and navigate. |
| CrowPanel Main/Graph screens bound to live telemetry | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | Real V/I/P per channel, OVP/OCP/OTP indicators, CV/CC state, dual-trace graph. Has a synthetic-data fallback generator so it's demoable without a live STM32. |
| Fault/status reporting (OVP/OCP/OTP, output-enable, CV/CC) | ❌ Hardcoded placeholder | `stm32-bluepill-bringup/src/main.cpp:963-964` | `status = 0xF0` and `protection_flags = 0x00` are constants — always "everything on, no faults." No real fault detection is wired yet. |
| Rail auto-range (low/high shunt path) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | Exists and works on the legacy ESP32 firmware. Needs a design decision before porting (see Milestone 3). |
| Per-rail calibration + persistent config storage | ✅ Basic STM32 persistence wired | `src/rev1/main.cpp` (`Preferences`/NVS) + `stm32-bluepill-bringup/src/main.cpp` (W25Q128) | STM32 now has versioned external-flash config load/save/reset/erase wiring plus persisted per-rail calibration coefficients and default D9 path state. |
| Current-limit operating modes (LATCH/HICCUP/MONITOR) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | HAT Rev-C implements OCP trip in hardware (dual INA2180A2 + TLV1702 comparators per README), so this isn't a straight port — firmware's job becomes reading/latching a hardware trip, not computing one. Needs a design pass, not a copy-paste. |
| CrowPanel Setup screen parameter editing | ❌ Not implemented | `crowpanel-43-bringup/src/main.cpp` | Screen exists but shows hardcoded placeholder text (e.g. `"CH1 OCP Threshold ... 1.000 A"`). No read/write binding, no encoder edit logic. |
| CrowPanel Settings screen submenus | ❌ Not implemented | `crowpanel-43-bringup/src/main.cpp` | System/DataSet/About headers exist; no working submenu content. |
| Display → host command channel (`CMD:`/`ACK:`/`ERR:`/`EVT:`) | ⚠️ Minimal path implemented | `stm32-bluepill-bringup/src/main.cpp` + `crowpanel-43-bringup/src/disp_link_slave.*` + `crowpanel-43-bringup/src/main.cpp` | Parser/framing now implemented both ends with `OUTPUT ON/OFF` and `ILIM CH1|CH2 <mA>` plus `ACK:`/`ERR:` handling and `EVT:` parsing. Full UI binding and bench validation remain open. |
| Custom front-panel board (ESP32-C6) UDI + LVGL rewrite | ❌ Not started | `src/rev1/display_main.cpp` | Paused/secondary path. Needs the Phase 3 rewrite described in `docs/display-project/README.md` before it reaches parity with CrowPanel. |
| STM32 build | ❌ Broken | `stm32-bluepill-bringup/` | Issue #3 — `HardwareSerial(rx, tx)` constructor mismatch. Blocks everything else on this target. |

## Milestones and branches

Work in this order — each milestone unblocks the next. Branch names follow
`docs/SYSTEM_DEVELOPMENT_WORKFLOW.md`'s `firmware/<topic>` / `display/<topic>`
convention. Open the branch when you actually start the work, not before.

### Milestone 0 — Unblock the STM32 build
- `firmware/stm32-fix-hardwareserial` — fix issue #3 so `stm32-bluepill-bringup` compiles again. Small, well-scoped, blocks everything below.

### Milestone 1 — Make the telemetry loop real end-to-end
- `firmware/stm32-fault-status-bits` — replace the hardcoded `status`/`protection_flags` bytes with real values: output-enable tracking, CV/CC detection, OVP/OTP threshold checks in firmware, and reading the Rev-C hardware OCP comparator outputs.
- `firmware/stm32-persistent-config` — port the calibration/threshold/config storage pattern from `src/rev1/main.cpp` (Preferences/NVS) onto STM32, using the already-bring-up-tested W25Q128 external flash (or STM32 internal flash emulation as a fallback).

### Milestone 2 — Close the loop: let the display control the HAT
- `firmware/udi-command-channel` — implement the `CMD:`/`ACK:`/`ERR:`/`EVT:` framing from `docs/DISPLAY_INTERFACE_STANDARD.md` on both ends. Start with the two actions already drawn in the UI: output enable/disable and current-limit set.
- `display/crowpanel-setup-screen-binding` — wire the Setup screen's fields to real config values: read on entry, edit via encoder, write back over the new command channel.
- `display/crowpanel-settings-submenus` — implement working System/DataSet/About submenus.

### Milestone 3 — Auto-range and current-limit modes (design pass first)
- `firmware/stm32-current-limit-modes` — after confirming how Rev-C's hardware OCP comparators actually behave on the bench, port or redesign the LATCH/HICCUP/MONITOR handling from the ESP32 reference onto STM32. Do this after Milestone 1, since it depends on real fault-bit reporting existing first.

### Milestone 4 — Secondary display path (do last, only if bench results say it's needed)
- `display/custom-panel-udi-port` — Phase 3 rewrite of the ESP32-C6 custom board firmware to LVGL + UDI UART, per `docs/display-project/README.md`. Paused by design; revisit only if the CrowPanel path stalls.

## Definition of done per item

Same checklist as `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md`, plus for firmware
specifically:
- [ ] Builds via the CI PlatformIO check for the affected environment(s).
- [ ] Bench-verified on real hardware where hardware is available, stated
      explicitly true/false in the PR — don't leave it implied.
- [ ] This doc's feature inventory table updated to reflect the new status.
