# Fan Control Definition (Issue #28)

Status: draft design baseline for implementation planning.  
Scope: HAT Rev-C 12V enclosure fan on J7, controlled from MCU GPIO through Q9, with tach feedback on `Fan_Tach`.

> **Caveat (tach not confirmed on Rev-C):** `docs/FIRMWARE_DEVELOPMENT_PLAN.md` records Rev-C J7 as a 2-pin
> fan connector with no dedicated tach net, so fan control is open-loop on this revision unless hardware adds one.
> The tach-dependent sections below (startup self-test, tach supervision) apply only once a tach path is confirmed;
> pin-contract reconciliation is tracked in #29. Implementation milestones are tracked as issues #31–#36.

## Goal

Implement a robust enclosure-cooling control loop that:
- verifies fan presence/response at startup,
- continuously adjusts fan speed from temperature,
- uses tach feedback to detect fan faults,
- fails safe when sensor or fan feedback is invalid.

This follows a **motherboard-style strategy** (fan curve + hysteresis + ramp limiting + tach supervision), adapted for enclosure thermal behavior instead of CPU junction spikes.

## Electrical Contract (Rev-C)

| Function | Signal | Direction | Notes |
|---|---|---|---|
| Fan drive control | `PB5` -> Q9 | MCU output | PWM-capable control output for fan command |
| Fan tach feedback | `Fan_Tach` -> `PA0` | MCU input | Pulse input from fan tachometer |
| Temperature input | AHT20 (`0x38`) | I2C input | Primary thermal source for enclosure control |

## Control Methodology

### 1) Startup self-test (fan detect + observability)
At boot, run a bounded startup test before normal closed-loop control:
1. Set fan command to 100% for spin-up (`T_spinup_ms`, default 700 ms).
2. Count tach pulses in a fixed observe window (`T_observe_ms`, default 1200 ms).
3. If RPM >= `RPM_start_min` (default 600 RPM), mark **startup PASS**.
4. If tach is missing/low, retry once at 100%.
5. If second attempt fails, mark **startup FAIL** and raise a latched fan fault flag.

Behavior after startup FAIL:
- keep fan command at 100% (safe attempt to cool anyway),
- surface fault over telemetry/logs/UI,
- keep power outputs policy-controlled (initial policy: warn/hold, not forced rail shutdown, until bench data defines stricter behavior).

### 2) Temperature-to-fan command (fan curve)
Use piecewise-linear curve points (editable later), with interpolation between points:

| AHT20 temp (°C) | Fan command (%) |
|---:|---:|
| <= 30 | 20 |
| 35 | 30 |
| 40 | 45 |
| 45 | 60 |
| 50 | 75 |
| >= 55 | 100 |

Why this shape:
- enclosure thermal mass is slower than CPU die temperature,
- keeps acoustics reasonable at low load,
- reaches full cooling before OTP region.

### 3) Hysteresis + ramp limiting (anti-hunt)
To avoid audible oscillation and rapid switching:
- Temperature hysteresis: `±1.0°C` equivalent deadband.
- Command ramp-up limit: max +8%/s.
- Command ramp-down limit: max -4%/s.

This mirrors motherboard fan-control stability principles while accounting for slower enclosure dynamics.

### 4) Runtime supervision and fail-safe policy

Fault conditions:
- **AHT20 stale/invalid**: no valid sample within timeout (default 10 s).
- **Tach stall**: command >= 45% but RPM < `RPM_run_min` (default 500 RPM) for >3 s.
- **Tach implausible**: RPM out of physical range (e.g., >7000 RPM) indicates signal fault.

Fail-safe actions:
- force fan command to 100%,
- set/retain fan fault bit(s) in status telemetry,
- emit explicit event/log message on fault assert/clear.

## Firmware State Model

States:
- `FAN_BOOT_TEST`
- `FAN_CLOSED_LOOP`
- `FAN_FAILSAFE`

Transitions:
- `BOOT_TEST -> CLOSED_LOOP` on startup PASS
- `BOOT_TEST -> FAILSAFE` on startup FAIL (after retry)
- `CLOSED_LOOP -> FAILSAFE` on sensor/tach fault
- `FAILSAFE -> CLOSED_LOOP` only after valid sensor+tach recovery for a qualified dwell time (default 15 s)

## PR / Milestone Plan

1. **PR-A: `firmware/fan-control-driver-foundation`**
   - Add fan-control pin/tach pin definitions and low-level driver scaffolding.
   - Add tach capture/counting and RPM calculation.
   - Add telemetry fields for fan command/RPM/fault bits.

2. **PR-B: `firmware/fan-startup-selftest`**
   - Implement startup spin/observe/retry flow.
   - Add PASS/FAIL health messages and latched startup fault state.

3. **PR-C: `firmware/fan-curve-ramping-hysteresis`**
   - Implement curve interpolation, hysteresis, and rate limiting.
   - Bind closed-loop control to AHT20 temperature.

4. **PR-D: `firmware/fan-failsafe-policy-and-udi`**
   - Implement stale-sensor and runtime tach-fault fail-safe transitions.
   - Expose fan status to UDI/status frame and display-side diagnostics.

5. **PR-E: `docs/bench-fan-validation-runsheet`**
   - Add a repeatable bench verification run sheet and acceptance thresholds.

## Bench Acceptance Criteria

- Startup PASS with known-good fan in >= 9/10 cold boots.
- Startup FAIL asserted with fan unplugged, with explicit fault telemetry.
- RPM tracks command monotonically across at least five setpoints.
- No fan-command oscillation at steady ambient conditions.
- Runtime fault detection triggers within 3 s for forced stall or tach disconnect.
- Fail-safe behavior forces 100% command on AHT20 timeout or tach fault.
