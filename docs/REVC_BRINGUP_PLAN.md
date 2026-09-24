# Rev-C Board Bring-Up Plan

**Status:** Active bench bring-up
**Scope:** `DSP-Regulator-RevC` and `DSP-Regulator-HAT-RevC`
**Boards ordered:** 2026-08-28
**Bench snapshot:** 2026-09-23

This plan tracks the actual physical bring-up of the received Rev-C boards. It is
separate from any fabrication-readiness (ERC/DRC) gate work tracked for a future
Rev-C respin — this document only records what has been measured and validated on
the bench with the boards in hand.

## Current status

| Area | Status | Evidence |
|---|---|---|
| HAT Rev-C standalone power | PASS | +5V_Boot ~5.0423V, +3.3V ~3.2926V, stable, no anomalies — `docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md` |
| HAT Rev-C MCU/telemetry bring-up | PASS | STM32 Blue Pill boots, serial console responsive, AHT20 and AW9523 (0x58) confirmed — `docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md` |
| Range-MOSFET gate-path control (AW9523-driven) | PASS (2026-09-23) | All six paths (Q1–Q5, Q9) verified: commanded state matches read-back, correlated INA directional response — `docs/DEV_STATION_HANDOFF_2026-09-23.md` |
| Regulator Rev-C standalone power | HOLD | Buck cores regulate correctly at LM2596 IC pins when force-enabled, but board output rails collapse downstream (5V rail ~4.01V, 3.3V rail ~0.06V) — `docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md`, RB-011 in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md` |
| RB-010 open-remote-sense fallback check | Deferred | Not yet run; pending regulator rail-collapse root cause |
| Stacked regulator + HAT test | Not started | Blocked until regulator-only rails are stable |
| OCP (INA2180A2/TLV1702) calibration | Not started | Blocked until stacked test is possible |
| Rail/telemetry calibration | In progress (opening objective) | Next session scope — `docs/CALIBRATION_WORKFLOW_2026-09-23.md` |

## Bring-up rules

1. Use the external bench-supply current limit for every first-power and fault test.
2. Do not stack the HAT on the regulator board until regulator-only checks pass.
3. STM32 Blue Pill is the controller-of-record for the current bench branch; do not use ESP32-C6 flash/build tasks on this branch.
4. Record measured values, instrument settings, board population, jumper state, and firmware revision for every run.
5. Stop on unexpected current limiting, rail collapse, excessive heating, oscillation, smoke, or repeated resets.

## What has been done

1. **HAT Rev-C standalone bring-up (2026-09-21):** Powered, rails measured stable, MCU/telemetry/AW9523 confirmed healthy. See `docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md`.
2. **Regulator Rev-C standalone bring-up (2026-09-21):** Powered with a conservative current limit; rails found out of range at the board output nodes. A forced-enable diagnostic (grounding U2/U4 pin 5) showed both buck cores regulate correctly at the IC pins, isolating the fault to the downstream distribution/select path rather than the primary buck loops. See `docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md` and the RB-011 entry in `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`.
3. **Range-MOSFET control bring-up (2026-09-22 to 2026-09-23):** Migrated range-switch control from a legacy shift-register path to the AW9523 I/O expander, added per-path ON/OFF bench commands, and validated all six range paths with matching commanded/read-back state and INA correlation. Session closed **PASS**. See `docs/DEV_STATION_HANDOFF_2026-09-21.md` (continuation entries) and `docs/DEV_STATION_HANDOFF_2026-09-23.md`.

## Open blockers

1. **Regulator Rev-C output rail collapse** — buck cores are functional, but 5V/3.3V distribution downstream of the cores does not reach the board output nodes at spec. This is the current gating issue before any stacked test (RB-011, `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`).
2. **RB-010 fallback check** — open-remote-sense fallback behavior for `VSENSE_5V+` has not yet been re-verified on Rev-C; deferred pending the rail-collapse fix.
3. **OCP calibration** — the HAT Rev-C differential OCP (INA2180A2/TLV1702) has not been bench-calibrated; this requires a stable stacked configuration first.

## Next steps

1. Continue isolating the regulator downstream rail-distribution/select-path fault (trace from U2/U4 pin 2 output nodes toward the board output connectors).
2. Once regulator-only rails are stable, run the RB-010 open-sense fallback check.
3. In parallel, begin rail/telemetry calibration on the HAT/Blue Pill path following `docs/CALIBRATION_WORKFLOW_2026-09-23.md`.
4. Re-attempt stacked regulator + HAT bring-up only after regulator-only phases pass.
5. Proceed to OCP calibration and fault validation once stacked operation is stable.

## Reference documents

- `docs/DEV_STATION_HANDOFF_2026-09-21.md` — bench session log, HAT/regulator standalone results, range-control pivot
- `docs/DEV_STATION_HANDOFF_2026-09-23.md` — AW9523 range-MOSFET control closure, next-session calibration scope
- `docs/HAT_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md` — HAT standalone runsheet and results
- `docs/REGULATOR_FIRST_POWER_RUNSHEET_REVC_2026-09-21.md` — regulator standalone runsheet and results
- `docs/REGULATOR_BOARD_CHANGE_TRACKER.md` — RB-010/RB-011 issue tracking
- `docs/CALIBRATION_WORKFLOW_2026-09-23.md` — active calibration procedure
