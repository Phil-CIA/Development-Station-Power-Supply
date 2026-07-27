# Development Station Power Supply - Handoff

Latest active handoff: [docs/DEV_STATION_HANDOFF_2026-07-20-session2.md](docs/DEV_STATION_HANDOFF_2026-07-20-session2.md)

Previous handoff: [docs/DEV_STATION_HANDOFF_2026-07-20.md](docs/DEV_STATION_HANDOFF_2026-07-20.md)

Session status: resumed on replacement CrowPanel and validated live-frame operation on UART0 temp wiring.

## 2026-07-27 Replacement Panel Bring-Up Update

1. Replacement CrowPanel detected and locked by guarded flash identity on `COM12`.
2. New immutable MAC recorded for CrowPanel target:
	- `80:B5:4E:E2:E4:08`
3. CrowPanel firmware updated to compile-time selectable transport mode and built for UART0 telemetry intake.
4. Guarded upload succeeded end-to-end on replacement panel after MAC update.
5. Live frame updates are confirmed on-screen with replacement hardware.
6. CrowPanel switch position during successful run:
	- `S1=0, S2=0`
7. Bench power/connection intent remains:
	- CrowPanel powered by USB-C
	- UART0 temp wiring for data path testing
	- HAT +5V injection to panel remains avoided in this phase
8. UART0 mapping status after this run:
	- Logical mapping confirmed: `UART0 RX=IO44`, `UART0 TX=IO43`
	- Physical connector pin-3/pin-4 numbering still needs direct schematic pin-number closure in docs

## Current Session Goal

1. Confirm the CrowPanel dashboard is visibly showing the live counts.
2. Keep the HAT-to-CrowPanel UART link as the known-good baseline.
3. Promote the validated live telemetry state to the main branch before any hardware interruption.

## Session Outcome

1. STM32 Blue Pill firmware is built and flashed successfully over ST-Link.
2. The CrowPanel UART path is working and receiving frames on IO19 RX / IO20 TX.
3. The CrowPanel main screen was updated to keep frame/status counters live and to display extra source counters.
4. The STM32 source firmware was updated from placeholder values to live INA/AHT-derived telemetry.
5. Verified CrowPanel serial output after STM32 flash showed real measured values instead of placeholders:
	- `V5=4920 mV`
	- `V3=3256..3264 mV`
	- `I5=0 mA`
	- `I3=0 mA`
6. Validated baseline was committed on `main` as `7f0d266` with message `Establish CrowPanel live telemetry baseline`.
7. Validated baseline was pushed to GitHub and tagged as `crowpanel-live-telemetry-baseline-2026-07-20`.

## Hardware Incident

1. During bench work, `5V` was accidentally applied to the CrowPanel `3.3V` pin.
2. Current expectation is that this panel should not be trusted for further hardware validation.
3. Work is paused until a replacement CrowPanel arrives.

## Verified Firmware / Repo State

1. CrowPanel UI/receiver side:
	- `crowpanel-43-bringup/src/disp_link_slave.cpp`
	- `crowpanel-43-bringup/src/main.cpp`
2. STM32 source side:
	- `stm32-bluepill-bringup/src/main.cpp`
3. Branch/tag state:
	- branch: `main`
	- commit: `7f0d266`
	- tag: `crowpanel-live-telemetry-baseline-2026-07-20`

## What Was Confirmed

1. CrowPanel guarded upload succeeded on `COM12` with ESP32-S3 MAC/chip precheck passing.
2. STM32 Blue Pill build and ST-Link upload succeeded after switching telemetry publish from constants to live measurements.
3. The CrowPanel UI issue was partly a render/update-path problem and partly a source-data problem:
	- UI counters were initially blocked by value-gated refresh logic.
	- Source rail values were initially fixed placeholders on the STM32 side.
4. After both fixes, the displayed values matched live UART telemetry.

## Resume Plan When Replacement Panel Arrives

1. Start from `main` or tag `crowpanel-live-telemetry-baseline-2026-07-20`.
2. Flash the replacement CrowPanel using the guarded task:
	- `WorkStation: Upload (crowpanel43)`
3. Confirm CrowPanel UART path first:
	- expected target `COM12`
	- expected chip `ESP32-S3`
4. If needed, flash/reconfirm the STM32 Blue Pill image from the current `main` baseline.
5. Verify on-screen values against CrowPanel serial output before making any new UI changes.
6. Only after replacement hardware is stable should new display/UI work continue.

## Immediate Next Steps

1. Keep current UART0 temp-wire setup unchanged and run short stability checks before any rewiring.
2. Capture final confirmed UART0 pin-3/pin-4 mapping from vendor schematic into docs if not already closed.
3. If stability holds, promote this replacement-panel state as the next baseline commit/tag.

## Suggested Prompt For New Chat

Continue from the 2026-07-20 session2 handoff. Use tag `crowpanel-live-telemetry-baseline-2026-07-20` or current `main` as the restart point. The CrowPanel/STM32 live telemetry baseline is validated and pushed, but the original panel was likely damaged after 5V was applied to its 3.3V pin, so resume by bringing up the replacement CrowPanel hardware first and revalidating the known-good UART/UI path before any new development.