# Firmware Development Plan

Status: active. This is the feature inventory and branch plan for firmware
work going forward, replacing "figure it out from old handoff docs" with a
single tracked list. Follow `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md` for how to
branch, PR, and merge each item below.

This was built by reading the actual firmware source in all four targets
(not just the design docs), so the status column reflects what the code
does today, not what a doc says it should do.

## Firmware scope buckets (Rev-C aligned)

This section is the control surface for firmware scope. Issues and PRs are
implementation artifacts under these buckets; they do not define scope on
their own.

- Pin/net contract conformance
- Rail control behavior (5V / 3V3 / CH3)
- Fault handling based on actual routed signals
- Telemetry and display-link contract
- Config/persistence/calibration
- Bring-up diagnostics and recovery paths

For every bucket, keep all four fields explicit:
- **In scope now**
- **Out of scope / blocked by hardware**
- **Exit criteria** (bench-measurable outcomes, not code task completion)
- **Evidence required** (logs, captures, and bench test steps)

This closes the gap between "tickets closed" and "current hardware proven."

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
| Base artifacts | `docs/firmware-buckets/bucket-2-rail-control-scope.md`, `stm32-bluepill-bringup/src/main.cpp`, `src/rev1/main.cpp` (reference behavior), Rev-C HAT netlist |

### Bucket 3: Fault handling based on actual routed signals

| Item | Definition |
|---|---|
| In scope now | Replace placeholder status/protection bytes with real bits only for signals that actually exist in Rev-C; explicitly mark unavailable fault sources as unavailable. |
| Out of scope now | Claiming OVP/OCP/OTP coverage not backed by routed net + firmware read path. |
| Exit criteria | 1) `status` and `protection_flags` are computed from real runtime state (not constants). 2) Each reported fault bit maps to a documented source and polarity. 3) Unavailable signals are represented as "not observed on this revision" rather than fake values. |
| Required evidence | Bit-level mapping table in PR description + validation log excerpt showing asserted and cleared states. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `docs/FIRMWARE_DEVELOPMENT_PLAN.md` feature table, `docs/STM32_BLUEPILL_PIN_TABLE.md` |

### Bucket 4: Telemetry and display-link contract

| Item | Definition |
|---|---|
| In scope now | Keep STM32 telemetry frame and CrowPanel parser/UI bindings synchronized with `CMD:`/`ACK:`/`ERR:`/`EVT:` behavior and binary frame schema used today. |
| Out of scope now | New protocol families or incompatible framing changes without migration plan. |
| Exit criteria | 1) End-to-end telemetry frame parse passes on bench. 2) Setup-screen controls round-trip through command channel (read, edit, write, ACK/ERR handling). 3) Protocol docs stay aligned with shipped behavior. |
| Required evidence | Host/display serial logs for at least one successful command round-trip and one error case; UI behavior notes. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `crowpanel-43-bringup/src/disp_link_slave.*`, `crowpanel-43-bringup/src/main.cpp`, `docs/DISPLAY_INTERFACE_STANDARD.md` |

### Bucket 5: Config/persistence/calibration

| Item | Definition |
|---|---|
| In scope now | Versioned load/save/reset/erase config flow on STM32 external flash, calibration coefficient persistence, safe defaults on missing/corrupt config. |
| Out of scope now | Broad data-model redesign without migration handling. |
| Exit criteria | 1) Cold boot restores expected persisted values. 2) Reset-to-defaults path is deterministic. 3) Version mismatch/corruption path recovers safely and logs reason. |
| Required evidence | Before/after persistence logs and one intentional invalid-config recovery run. |
| Base artifacts | `stm32-bluepill-bringup/src/main.cpp`, `src/rev1/main.cpp` (reference), W25Q128 handling paths |

### Bucket 6: Bring-up diagnostics and recovery paths

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

## Known issue mapping (bucket-owned implementation artifacts)

Map every known issue to a primary bucket before execution. If an issue spans
multiple buckets, keep one primary owner bucket and list the dependency in the
PR description.

| Issue | Primary bucket | Scope disposition |
|---|---|---|
| #30 Reconcile full STM32 pin contract across hardware, firmware, and docs | Bucket 1 | In scope now |
| #29 Reconcile fan pin mapping across schematic, docs, and firmware plan | Bucket 1 | In scope now |
| #28 Develop the cooling controls | Bucket 2 | In scope now |
| #37 ESP32 startup test | Bucket 6 | In scope now (bring-up evidence feed) |
| #39 I2C startup test | Bucket 6 | In scope now (bring-up evidence feed) |
| #14 Port/redesign current-limit modes to STM32 | Bucket 3 | In scope now |
| #31 Milestone A fan control driver foundation | Bucket 3 | In scope now |
| #32 Milestone B fan startup self-test | Bucket 3 | In scope now |
| #34 Milestone D fail-safe policy + telemetry/UDI integration | Bucket 3 | In scope now |
| #36 Fan-control milestone tracker | Bucket 3 | In scope now |
| #26 CrowPanel display screens | Bucket 4 | In scope now |
| #40 CrowPanel startup test | Bucket 4 | In scope now |
| #38 SPI memory test | Bucket 5 | In scope now |
| #27 Bootup log and testing | Bucket 6 | In scope now |
| #33 Milestone C AHT20 fan curve | Bucket 6 | In scope now (diagnostics + validation evidence) |
| #35 Milestone E bench fan-validation evidence capture | Bucket 6 | In scope now |
| #25 Add OTA and Wi-Fi | Bucket 5 | Out of scope for Rev-C bench bring-up unless explicitly re-scoped |
| #17 Milestone 4 custom panel UDI+LVGL rewrite | Bucket 4 | Out of scope now (secondary/paused path) |
| #3 STM32 HardwareSerial compile mismatch (closed) | Bucket 1 | Regression watch: reopen/new issue if compile break reappears |

## Six bucket scoping PRs (documentation-first, bench execution prep)

These PRs are scoping/control PRs only. Do not mix in firmware behavior
expansion until the scoping PR for the relevant bucket is merged.

| Priority | Bucket | Planned branch | PR scope | Required artifacts in PR description |
|---|---|---|---|---|
| 1 | Bucket 1 | `docs/firmware-bucket-1-pin-net-scope` | Contract tables, net ownership, and unrouted-signal policy | Netlist refs + pin-table diff + explicit unrouted list |
| 2 | Bucket 6 | `docs/firmware-bucket-6-bringup-recovery-scope` | Startup diagnostics, failure taxonomy, operator recovery expectations | Normal boot + induced-failure logs and recovery steps |
| 3 | Bucket 4 | `docs/firmware-bucket-4-telemetry-display-scope` | Command/telemetry contract limits and evidence matrix | Host/display command round-trip logs with one error case |
| 4 | Bucket 2 | `docs/firmware-bucket-2-rail-control-scope` | Rail enable/disable behavior boundaries and safe-state expectations | Bench state table + rail transition captures |
| 5 | Bucket 3 | `docs/firmware-bucket-3-fault-scope` | Fault-bit ownership tied to routed signals only | Bit-source map + asserted/cleared fault evidence |
| 6 | Bucket 5 | `docs/firmware-bucket-5-config-persistence-scope` | Persistence/calibration ownership, corruption behavior, reset semantics | Cold-boot persistence log + invalid-config recovery log |

## Feature inventory

| Feature | Status | Where | Notes |
|---|---|---|---|
| Rail V/I telemetry read (INA3221) | ✅ Done | `stm32-bluepill-bringup/src/main.cpp` | Real hardware reads for +5V and +3.3V rails. |
| Extended UART telemetry frame, STM32 → display | ✅ Implemented both ends | `stm32-bluepill-bringup/src/main.cpp` (`publishTelemetry`) + `crowpanel-43-bringup/src/disp_link_slave.cpp` (`parseFrame`) | Frame layouts match on both ends and STM32 target now builds. Bench end-to-end capture remains required for bucket exit evidence. |
| CrowPanel LVGL UI skeleton (Splash/Setup/Main/Graph/Settings) | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | All five screens exist and navigate. |
| CrowPanel Main/Graph screens bound to live telemetry | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | Real V/I/P per channel, OVP/OCP/OTP indicators, CV/CC state, dual-trace graph. Has a synthetic-data fallback generator so it's demoable without a live STM32. |
| Fault/status reporting (OVP/OCP/OTP, output-enable, CV/CC) | ⚠️ Runtime-derived, AW9523-interrupt sourced | `stm32-bluepill-bringup/src/main.cpp` (telemetry publish path) | `status` and `protection_flags` are computed from live state (channel enable/CV-CC, OVP thresholds, thermal warn/OTP). Rev-C fault path uses AW9523 input + `AW9523_INT` (`PB7`) to signal MCU fault handling; direct STM32 `FAULT_CRITICAL_SUM` GPIO remains disabled (`PIN_FAULT_CRITICAL_SUM = -1`). |
| Fan control + tach contract (Rev-C) | ⚠️ Hardware control path only, no tach net | `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net` + `docs/STM32_BLUEPILL_PIN_TABLE.md` | Current Rev-C J7 is a 2-pin fan control connector and does not expose a dedicated tach signal. Treat fan as open-loop for this revision unless hardware adds a tach net. |
| Rail auto-range (low/high shunt path) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | Exists and works on the legacy ESP32 firmware. Needs a design decision before porting (see Milestone 3). |
| Per-rail calibration + persistent config storage | ✅ STM32 persistence + explicit recovery reason surface | `src/rev1/main.cpp` (`Preferences`/NVS) + `stm32-bluepill-bringup/src/main.cpp` (W25Q128) | STM32 now has versioned external-flash config load/save/reset/erase wiring, persisted per-rail calibration coefficients/default D9 path state, deterministic boot recovery summary (`[CFG] REC r=<code> d=<0|1> s=<0|1>`), and additive `CMD:GET CFGREC` reason-code readback (`O/F/N/V/L/C/G` for OK/Flash/NoSig/Version/Length/CRC/Gain). |
| Current-limit operating modes (LATCH/HICCUP/MONITOR) | ⚠️ Reference only, not ported | `src/rev1/main.cpp` | HAT Rev-C implements OCP trip in hardware (dual INA2180A2 + TLV1702 comparators per README), so this isn't a straight port — firmware's job becomes reading/latching a hardware trip, not computing one. Needs a design pass, not a copy-paste. |
| CrowPanel Setup screen parameter editing | ⚠️ Partially implemented | `crowpanel-43-bringup/src/main.cpp` | Setup now binds real host values for Output Enable and CH1/CH2 current limits (`CMD:GET ...` read on entry, `CMD:OUTPUT`/`CMD:ILIM` write on apply). The select/edit/commit flow now follows encoder semantics (rotate/press/long-press), currently mapped to Setup controls and `SETUP_ENC` serial commands for bring-up validation. |
| CrowPanel Settings screen submenus | ✅ Done | `crowpanel-43-bringup/src/main.cpp` | Settings now has working System/DataSet/About submenus with live fields and interactions: System (language, brightness, volume, theme), DataSet (preset group M1–M6), About (model/build/uptime), plus touch navigation and `SETTINGS_ENC` rotate/press/long-press support for deterministic bring-up testing. |
| Display → host command channel (`CMD:`/`ACK:`/`ERR:`/`EVT:`) | ⚠️ Expanded and in use | `stm32-bluepill-bringup/src/main.cpp` + `crowpanel-43-bringup/src/disp_link_slave.*` + `crowpanel-43-bringup/src/main.cpp` | Parser/framing is now wired to Setup UI flow: `OUTPUT`/`ILIM` write commands plus `GET OUTPUT`, `GET ILIM CH1|CH2`, `GET STATE`, and additive `GET CFGREC` readback commands with ACK/ERR/EVT parsing on the display. Fault transitions now emit immediate `EVT:FAULT TRIP` / `EVT:FAULT CLEAR` from the AW9523 interrupt path. Bench validation remains open. |
| Bring-up diagnostics + recovery hints (Bucket 6) | ⚠️ In progress | `stm32-bluepill-bringup/src/main.cpp` | Startup and manual `DIAG` now emit compact contract/health snapshots (including Rev-C AW9523 fault-path mode) for repeatable bench log capture; richer recovery-path coverage remains open for follow-on work. |
| Custom front-panel board (ESP32-C6) UDI + LVGL rewrite | ❌ Not started | `src/rev1/display_main.cpp` | Paused/secondary path. Needs the Phase 3 rewrite described in `docs/display-project/README.md` before it reaches parity with CrowPanel. |
| STM32 build | ✅ Builds | `stm32-bluepill-bringup/` | `bluepill_f103c8` builds in the current tree. Flash headroom is tight (~99.5%), so new features should stay size-conscious. |

## Milestones and branches

Work in this order — each milestone unblocks the next. Branch names follow
`docs/SYSTEM_DEVELOPMENT_WORKFLOW.md`'s `firmware/<topic>` / `display/<topic>`
convention. Open the branch when you actually start the work, not before.

### Milestone 0 — Unblock the STM32 build (completed)
- `firmware/stm32-fix-hardwareserial` — done: STM32 target compiles again, so downstream milestones are no longer blocked on issue #3.

### Milestone 1 — Make the telemetry loop real end-to-end
- `firmware/stm32-fault-status-bits` — in progress: `status`/`protection_flags` now use real runtime values (output-enable, CV/CC, OVP, thermal warn/OTP). OCP/fault path is AW9523-input + `AW9523_INT` driven on Rev-C (non-polled), with direct STM32 `FAULT_CRITICAL_SUM` GPIO intentionally disabled.
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
