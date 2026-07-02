# Development Station Power Supply - Handoff

## Session Closeout - 2026-07-02

The current next step is STM32F103 breadboard bring-up first, then CrowPanel bench validation.

## Current Objective At Stop

Bring the STM32 Blue Pill path up on a breadboard with SWD first-flash/debug access, verify reset-safe startup behavior, and then move to the CrowPanel UART bring-up using the documented UDI contract.

## Verified State At Stop

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

## Known Good / Open Items

- Known good: the repo already contains the STM32 pin table, the reset validation procedure, the CrowPanel bring-up stub, and the UDI contract.
- Open item: the exact CrowPanel UART0-IN pin 3/4 order still needs schematic confirmation before cable assembly.
- Open item: no bench capture has been recorded yet for the STM32 reset-safe startup sequence.

## Bench Notes For The Next Person

- Start with SWD, not JTAG-only assumptions.
- Use the Blue Pill module or equivalent STM32F103 board already on hand.
- Keep the CrowPanel on USB-C power during bring-up and only use UART on the 4-pin header.
- Treat this as bring-up and validation work, not a display redesign.
