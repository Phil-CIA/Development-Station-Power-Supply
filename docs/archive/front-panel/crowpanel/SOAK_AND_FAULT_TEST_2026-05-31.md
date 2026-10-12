# CrowPanel/HAT Soak and Fault Test (2026-05-31)

## Purpose
Close remaining validation items for the current UART-only, no-flicker display baseline.

## Current Recorded Outcome
1. Soak PASS (user report): multiple hours of continuous operation with stable behavior.
2. No new instability observed during extended use.

## Firmware Baseline Required
1. HAT flashed from env hat_c6_i2c_probe.
2. CrowPanel flashed from env crowpanel43.
3. CrowPanel UI should show live values and trend charts.

## Soak Test (30-60 min)

### Start
1. Power both boards and let UI stabilize for 60 seconds.
2. Connect serial monitor to CrowPanel COM12 at 115200.
3. On CrowPanel monitor, run:
   - LOG_CLEAR
   - LOG_START
   - LOG_STATUS

### During Run
1. Every 5-10 minutes run:
   - RX
   - LOG_STATUS
2. Observe on-screen criteria:
   - no visible flicker
   - values continue changing
   - chart moves continuously

### Pass Criteria
1. rx frames monotonic increase.
2. err count remains near zero (no sustained growth pattern).
3. UART byte/frame counters keep increasing.
4. No reset loop observed on either board.
5. Display remains stable (no tear/flicker return).

## CSV Capture Check
1. At end of soak, run:
   - LOG_STOP
   - LOG_DUMP_CSV
2. Confirm output includes:
   - csv_begin/csv_end markers
   - header row with fields
   - rows with increasing t_ms

## Fault Injection (UART)

### Procedure
1. With system running, temporarily disconnect UART signal path (keep power).
2. Wait 10-20 seconds.
3. Reconnect UART.
4. Run RX and LOG_STATUS before, during, and after reconnection.

### Expected Behavior
1. During disconnect: frame growth pauses or slows.
2. After reconnect: frame growth resumes.
3. System remains responsive; no reboot loop.

## Record Template
- Start time:
- End time:
- Duration:
- Max err_count observed:
- Any visible flicker:
- Any reset events:
- Fault test resume behavior:
- Final verdict: PASS/FAIL
