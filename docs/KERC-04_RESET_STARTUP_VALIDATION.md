# KERC-04 Reset-Safe Startup Validation (Blue Pill Migration)

Purpose: close Issue 15 blocker KERC-04 with repeatable bench evidence.

## Pass Criteria

All three control outputs must satisfy both conditions:
1. Stay inactive during power-on reset and manual reset window.
2. Assert only after explicit firmware initialization point.

Signals under test:
- ISET_MPU_5V
- ISET_MPU_3V3
- ISET_MPU_Channel_3

Reference timing markers:
- NRST (reset line)
- Optional firmware marker GPIO pulse or STATUS_LED transition

## Bench Setup (10-minute target)

If bench is unavailable today:
1. Keep KERC-04 in HOLD state.
2. Do not modify schematic net assignments solely to clear KERC-04.
3. Resume with the Test Sequence section at next bench window.

Equipment:
- 4-channel scope minimum (recommended 100 MHz or higher)
- Probe ground spring or short ground lead
- Stable bench supply

Channel assignment:
- CH1: NRST
- CH2: ISET_MPU_5V
- CH3: ISET_MPU_3V3
- CH4: ISET_MPU_Channel_3

Scope defaults:
- Trigger: rising edge on NRST deassertion
- Timebase: 5 ms/div start, then 500 us/div for zoom
- Vertical: logic-level scale for 3.3 V domain
- Acquisition: normal, then single-shot captures

## Test Sequence

1. Power-on capture
- Arm single-shot.
- Apply power from 0 V.
- Capture NRST and all three ISET lines.

Expected:
- ISET lines remain inactive through reset and early boot interval.
- No high pulse glitches before firmware init marker.

2. Manual reset capture
- Keep board powered.
- Arm single-shot.
- Assert and release reset.
- Capture NRST and all three ISET lines.

Expected:
- Same behavior as power-on: no premature assertion/glitch.

3. Brownout-style quick power cycle (optional but recommended)
- Reduce input below operational threshold briefly, then restore.
- Capture startup again.

Expected:
- No unintended control-line assertion during recovery.

## Evidence Table (Fill In)

| Run ID | Condition | ISET_MPU_5V before init | ISET_MPU_3V3 before init | ISET_MPU_Channel_3 before init | Earliest assertion point | Pass/Fail | Notes |
|--------|-----------|-------------------------|---------------------------|----------------------------------|--------------------------|-----------|-------|
| R1 | Power-on | [inactive/active] | [inactive/active] | [inactive/active] | [time or marker] | [PASS/FAIL] | |
| R2 | Manual reset | [inactive/active] | [inactive/active] | [inactive/active] | [time or marker] | [PASS/FAIL] | |
| R3 | Brownout cycle | [inactive/active] | [inactive/active] | [inactive/active] | [time or marker] | [PASS/FAIL] | |

## Closure Rule

Promote KERC-04 to PASS only when:
1. All required runs are PASS.
2. No pre-init high glitch is observed on any ISET line.
3. Results are summarized in the active handoff doc with screenshot filenames.

If any run fails:
1. Keep KERC-04 as HOLD.
2. Record failing waveform details.
3. Add mitigation action (pull direction, external bias, or firmware init sequence change).

## Handoff Entry Template (Copy/Paste)

Paste this block into the active daily handoff doc after bench capture.

```markdown
### KERC-04 Reset-Safe Startup Validation (YYYY-MM-DD)

Scope setup:
- CH1: NRST
- CH2: ISET_MPU_5V
- CH3: ISET_MPU_3V3
- CH4: ISET_MPU_Channel_3
- Trigger: NRST deassertion rising edge

Capture files:
1. [power-on capture filename]
2. [manual-reset capture filename]
3. [brownout capture filename or N/A]

Results table:

| Run ID | Condition | ISET_MPU_5V before init | ISET_MPU_3V3 before init | ISET_MPU_Channel_3 before init | Earliest assertion point | Pass/Fail | Notes |
|--------|-----------|-------------------------|---------------------------|----------------------------------|--------------------------|-----------|-------|
| R1 | Power-on | [inactive/active] | [inactive/active] | [inactive/active] | [time or marker] | [PASS/FAIL] | |
| R2 | Manual reset | [inactive/active] | [inactive/active] | [inactive/active] | [time or marker] | [PASS/FAIL] | |
| R3 | Brownout cycle | [inactive/active/N/A] | [inactive/active/N/A] | [inactive/active/N/A] | [time or marker] | [PASS/FAIL] | |

Decision:
- KERC-04: [PASS/HOLD]
- Rationale: [one-sentence summary]
- Next action: [if HOLD, specify exact mitigation and retest plan]
```
