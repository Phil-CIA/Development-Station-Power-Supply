# STM32 -> CrowPanel Bench Run Sheet (2026-07-02)

Purpose: execute the current repo plan in the required order: STM32 breadboard bring-up first, then CrowPanel UART validation, then end-to-end link check.

## Preconditions

1. Use SWD first for STM32 bring-up.
2. Keep CrowPanel powered from USB-C during bring-up.
3. Leave J21 pin 1 (+5V_Boot) disconnected for bench UART tests.
4. Do not proceed to cable finalization until CrowPanel UART0-IN physical pin 3/4 order is confirmed from vendor schematic resources.

## Phase A - STM32 breadboard bring-up (independent)

### A1. Wiring sanity check (no power)

1. Verify SWD lines:
   - PA13 <-> SWDIO
   - PA14 <-> SWDCLK
   - NRST <-> programmer reset (if available)
   - GND common
2. Verify BOOT0 is default-low for normal boot.
3. Verify NRST is not tied low.

### A2. First attach and flash

1. Connect programmer and power.
2. Confirm SWD attach succeeds.
3. Flash minimal test firmware.
4. Record outcome (attach PASS/FAIL, flash PASS/FAIL).

### A3. Reset-safe validation (KERC-04)

Follow: `docs/KERC-04_RESET_STARTUP_VALIDATION.md`.

Required captures:
1. Power-on reset
2. Manual reset
3. Brownout-style cycle (recommended)

Signals:
1. NRST
2. ISET_MPU_5V
3. ISET_MPU_3V3
4. ISET_MPU_Channel_3

Gate to pass Phase A:
1. No pre-init assertion/glitch on any ISET signal.
2. Evidence table filled and screenshots saved.

## Phase B - CrowPanel standalone UART validation (independent)

Reference: `docs/CROWPANEL_BENCH_BRINGUP_2026-05-27.md`.

### B1. Flash and boot

1. Build/flash CrowPanel firmware in `crowpanel-43-bringup/`.
2. Power from USB-C.
3. Confirm stable boot/display.

### B2. UART command checks

1. Send `PING` -> expect `PONG`.
2. Send `STATUS` -> expect valid status payload.
3. Send `CMD:LABEL ...` -> expect ACK and visible label change.
4. Send `CMD:COLOR ...` -> expect ACK and color fill.

### B3. Pin-order closure item

Known logical mapping from Elecrow docs:
1. UART0-IN RX = IO44
2. UART0-IN TX = IO43

Still required before final cable lock:
1. Confirm physical XH2.54 pin 3 vs pin 4 assignment from schematic resource.

## Phase C - End-to-end STM32 <-> CrowPanel UART

Proceed only after Phase A and Phase B pass.

1. Cross-connect TX/RX using confirmed physical pin order.
2. Keep CrowPanel powered by USB-C.
3. Keep J21 pin 1 disconnected.
4. Run PING/PONG and command path through STM32 UART.
5. Record PASS/FAIL with timestamps.

## Evidence Log Template

| Item | Result | Evidence file / note |
|---|---|---|
| SWD attach | [PASS/FAIL] | |
| First flash | [PASS/FAIL] | |
| KERC-04 power-on run | [PASS/FAIL] | |
| KERC-04 manual reset run | [PASS/FAIL] | |
| KERC-04 brownout run | [PASS/FAIL/N/A] | |
| CrowPanel PING | [PASS/FAIL] | |
| CrowPanel STATUS | [PASS/FAIL] | |
| CrowPanel CMD:LABEL | [PASS/FAIL] | |
| CrowPanel CMD:COLOR | [PASS/FAIL] | |
| Physical pin 3/4 mapping closure | [DONE/OPEN] | |
| End-to-end STM32 <-> CrowPanel UART | [PASS/FAIL] | |

## Stop Rules

1. If SWD attach fails: stop and resolve wiring/power/debug access first.
2. If any KERC-04 run fails: keep KERC-04 on HOLD and do not advance to end-to-end link.
3. If physical pin 3/4 order remains unverified: do not finalize cable pinout.