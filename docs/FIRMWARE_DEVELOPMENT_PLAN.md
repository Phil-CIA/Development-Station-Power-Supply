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

## Rev-C firmware scope buckets (hardware-aligned)

Use these buckets as the planning unit. Issues and PRs are implementation
artifacts under a bucket, not the scope definition themselves.

### Bucket 1: Pin/net contract conformance

| Item | Definition |
|---|---|
| In scope now | Keep STM32 firmware pin constants aligned with current Rev-C routed nets; keep explicit "not routed" handling for unavailable nets (for example `PIN_FAULT_CRITICAL_SUM = -1`). |
| Out of scope now | Inventing virtual mappings to unrouted hardware, or documenting speculative pin assignments not present in current netlist. |
| Exit criteria | 1) `docs/STM32_BLUEPILL_PIN_TABLE.md` status is accurate for every active STM32 signal. 2) `stm32-bluepill-bringup/src/main.cpp` pin constants match that table. 3) Any unrouted signal is represented explicitly in code and does not attempt GPIO reads/writes. |
| Required evidence | PR includes netlist references (`hardware/kicad/dsp-regulator-hat-rev-c/*.net`), firmware line references, and updated pin-table rows. |
| Base artifacts | `docs/STM32_BLUEPILL_PIN_TABLE.md`, `docs/GPIO_PINOUT.md`, `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`, `stm32-bluepill-bringup/src/main.cpp` |

### Bucket 2: Rail-control behavior (5V/3V3/CH3)

| Item | Definition |
|---|---|
| In scope now | Deterministic boot-safe output states, correct ISET output control on `PA0/PA1/PA2`, and validated enable/disable behavior through actual control path hardware. |
| Out of scope now | New rail architecture redesign and unrelated regulator-board feature expansion. |
| Exit criteria | 1) Power-on defaults leave outputs in safe inactive state until firmware enables paths. 2) Rail enable/disable commands act on intended rail only. 3) Bench checks confirm expected gate/control transitions per rail. |
| Required evidence | Bench procedure + observed results attached in PR (scope captures, logs, or measured rail state table). |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `src/rev1/main.cpp` (reference behavior), Rev-C HAT netlist |

### Bucket 3: Fault and protection reporting (routed-signal reality)

| Item | Definition |
|---|---|
| In scope now | Replace placeholder status/protection bytes with real bits only for signals that actually exist in Rev-C; explicitly mark unavailable fault sources as unavailable. |
| Out of scope now | Claiming OVP/OCP/OTP coverage not backed by routed net + firmware read path. |
| Exit criteria | 1) `status` and `protection_flags` are computed from real runtime state (not constants). 2) Each reported fault bit maps to a documented source and polarity. 3) Unavailable signals are represented as "not observed on this revision" rather than fake values. |
| Required evidence | Bit-level mapping table in PR description + validation log excerpt showing asserted and cleared states. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `docs/FIRMWARE_DEVELOPMENT_PLAN.md` feature table, `docs/STM32_BLUEPILL_PIN_TABLE.md` |

### Bucket 4: Telemetry + display-link contract

| Item | Definition |
|---|---|
| In scope now | Keep STM32 telemetry frame and CrowPanel parser/UI bindings synchronized with `CMD:`/`ACK:`/`ERR:`/`EVT:` behavior and binary frame schema used today. |
| Out of scope now | New protocol families or incompatible framing changes without migration plan. |
| Exit criteria | 1) End-to-end telemetry frame parse passes on bench. 2) Setup-screen controls round-trip through command channel (read, edit, write, ACK/ERR handling). 3) Protocol docs stay aligned with shipped behavior. |
| Required evidence | Host/display serial logs for at least one successful command round-trip and one error case; UI behavior notes. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `crowpanel-43-bringup/src/disp_link_slave.*`, `crowpanel-43-bringup/src/main.cpp`, `docs/DISPLAY_INTERFACE_STANDARD.md` |

### Bucket 5: Persistent config and calibration

| Item | Definition |
|---|---|
| In scope now | Versioned load/save/reset/erase config flow on STM32 external flash, calibration coefficient persistence, safe defaults on missing/corrupt config. |
| Out of scope now | Broad data-model redesign without migration handling. |
| Exit criteria | 1) Cold boot restores expected persisted values. 2) Reset-to-defaults path is deterministic. 3) Version mismatch/corruption path recovers safely and logs reason. |
| Required evidence | Before/after persistence logs and one intentional invalid-config recovery run. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `src/rev1/main.cpp` (reference), W25Q128 handling paths |

### Bucket 6: Bring-up diagnostics and recovery

| Item | Definition |
|---|---|
| In scope now | Boot diagnostics, explicit warnings for unavailable hardware contracts, and operator-visible recovery hints for common bring-up failure modes. |
| Out of scope now | Silent fallback behavior that hides hardware/contract problems. |
| Exit criteria | 1) Startup log clearly states target revision assumptions and unavailable paths. 2) Major initialization failures produce actionable messages. 3) Bench operator can distinguish wiring/contract fault from firmware crash using logs alone. |
| Required evidence | Boot log snippets covering normal boot and at least one induced failure path. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, PR logs/screenshots, this plan doc |

## Bucket execution model (same going forward)

This is the standard process for future firmware work in this repo:

1. Pick a bucket first, then define/update issues under that bucket.
2. Every firmware PR must name its primary bucket in the PR summary.
3. Every firmware PR must include bucket exit-criteria evidence (or explicitly
   mark which criteria remain open and why).
4. Update this plan's feature inventory status in the same PR when behavior
   changes.
5. If hardware routing changes, update `docs/STM32_BLUEPILL_PIN_TABLE.md`
   before merging firmware behavior that depends on those new routes.

## Feature inventory

| Feature | Status | Where | Notes |
|---|---|---|---|
| Rail V/I telemetry read (INA3221) | ✅ Done | `stm32-bluepill-bringup/src/main.cpp` | Real hardware reads for +5V and +3.3V rails. |
| Extended UART telemetry frame, STM32 → display | ✅ Implemented both ends, ⛔ blocked | `stm32-bluepill-bringup/src/main.cpp` (`publishTelemetry`) + `crowpanel-43-bringup/src/disp_link_slave.cpp` (`parseFrame`) | Frame layouts match on both ends. Can't prove it end-to-end until issue #3 is fixed. |
| CrowPanel LVGL UI skeleton (Splash/Setup/Main/Graph/Settings) | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | All five screens exist and navigate. |
| CrowPanel Main/Graph screens bound to live telemetry | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | Real V/I/P per channel, OVP/OCP/OTP indicators, CV/CC state, dual-trace graph. Has a synthetic-data fallback generator so it's demoable without a live STM32. |
| Fault/status reporting (OVP/OCP/OTP, output-enable, CV/CC) | ❌ Hardcoded placeholder | `stm32-bluepill-bringup/src/main.cpp:965-966` | `status = 0xF0` and `protection_flags = 0x00` are constants — always "everything on, no faults." Rev-C note: `FAULT_CRITICAL_SUM` is not routed to an STM32 GPIO; firmware now sets `PIN_FAULT_CRITICAL_SUM = -1` and disables direct GPIO fault monitoring until hardware adds a dedicated fault-input net. |
| Fan control + tach contract (Rev-C) | ⚠️ Hardware control path only, no tach net | `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net` + `docs/STM32_BLUEPILL_PIN_TABLE.md` | Current Rev-C J7 is a 2-pin fan control connector and does not expose a dedicated tach signal. Treat fan as open-loop for this revision unless hardware adds a tach net. |
| Rail auto-range (low/high shunt path) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | Exists and works on the legacy ESP32 firmware. Needs a design decision before porting (see Milestone 3). |
| Per-rail calibration + persistent config storage | ✅ Basic STM32 persistence wired | `src/rev1/main.cpp` (`Preferences`/NVS) + `stm32-bluepill-bringup/src/main.cpp` (W25Q128) | STM32 now has versioned external-flash config load/save/reset/erase wiring plus persisted per-rail calibration coefficients and default D9 path state. |
| Current-limit operating modes (LATCH/HICCUP/MONITOR) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | HAT Rev-C implements OCP trip in hardware (dual INA2180A2 + TLV1702 comparators per README), so this isn't a straight port — firmware's job becomes reading/latching a hardware trip, not computing one. Needs a design pass, not a copy-paste. |
| CrowPanel Setup screen parameter editing | ⚠️ Partially implemented | `crowpanel-43-bringup/src/main.cpp` | Setup now binds real host values for Output Enable and CH1/CH2 current limits (`CMD:GET ...` read on entry, `CMD:OUTPUT`/`CMD:ILIM` write on apply). The select/edit/commit flow now follows encoder semantics (rotate/press/long-press), currently mapped to Setup controls and `SETUP_ENC` serial commands for bring-up validation. |
| CrowPanel Settings screen submenus | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | Settings now has working System/DataSet/About submenus with touch navigation, live status detail panes, and submenu-specific actions (demo/tour toggle, dataset log control/clear, about/build info refresh). |
| Display → host command channel (`CMD:`/`ACK:`/`ERR:`/`EVT:`) | ⚠️ Expanded and in use | `stm32-bluepill-bringup/src/main.cpp` + `crowpanel-43-bringup/src/disp_link_slave.*` + `crowpanel-43-bringup/src/main.cpp` | Parser/framing is now wired to Setup UI flow: `OUTPUT`/`ILIM` write commands plus `GET OUTPUT` and `GET ILIM CH1|CH2` readback commands with ACK/ERR/EVT parsing on the display. Bench validation remains open. |
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
