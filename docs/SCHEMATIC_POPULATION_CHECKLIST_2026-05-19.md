# Schematic Population Checklist (2026-05-19)

Purpose: execute schematic/netlist population in small, verifiable batches while preserving the current 3-channel selector topology baseline.

## Scope Lock (Do Before Editing)

1. Confirm active scope is regulator/HAT Rev-B schematic and netlist updates only.
2. Confirm firmware remains scaffold-only (no runtime behavior changes).
3. Confirm bench execution is deferred unless explicitly requested.

## Baseline Snapshot (Read-Only)

1. Verify regulator Rev-B baseline in netlist:
   - U5/U6/U7 = LMV358IDR
   - D4/D5/D6 = BAT54C
   - R31/R33/R35 = 100 ohm
   - R32/R34/R36 = 33k
2. Verify HAT Rev-B Sense-B baseline:
   - 5V path includes R59/R60 and VSENSE_5V+/VSENSE_5V-
   - 3.3V path includes R61/R62 and VSENSE_3V3+/VSENSE_3V3-
   - Adj path includes R63/R64 and VSENSE_ADJ+/VSENSE_ADJ-
3. Verify feedback net names remain present:
   - Feedback 5V
   - Feedback 3.3
   - Feedback Adj Channel

## Edit Batch Workflow (Repeat Per Change Group)

1. Make one focused schematic change group only.
2. Re-export corresponding netlist.
3. Run targeted text verification for edited refs/nets.
4. Record pass/fail summary in handoff notes.
5. If mismatch appears, stop and resolve before any additional topology edits.

## Guardrails During Population

1. Do not alter selector architecture unless bring-up evidence requires it.
2. Prefer value or annotation updates over topology rewrites.
3. Keep connector and net naming consistent across regulator and HAT projects.
4. Avoid mixed changes across unrelated issue IDs in one batch.

## Per-Batch Verification Checklist

1. Edited references exist in schematic and netlist with expected values.
2. Expected nets still exist and are attached to intended components.
3. No unexpected deletions of baseline references (U5/U6/U7, D4/D5/D6, R31-R36, R59-R64).
4. Handoff docs updated with what changed and why.

## Stop Criteria And Escalation

1. Stop immediately if baseline references disappear unexpectedly.
2. Stop if net naming drifts between HAT and regulator connector interfaces.
3. Stop if multiple alternate topologies are introduced without measured evidence.

## End-Of-Session Documentation

1. Update HANDOFF.md with completed batch summary.
2. Update NEW_CHAT_HANDOFF.rmd and NEW_CHAT_HANDOFF_SHORT.rmd only if priorities changed.
3. Add a dated dev-station handoff note when the checkpoint materially advances.

## Suggested Command Snippets (Optional)

1. Targeted ref/value check patterns:
   - U5|U6|U7|LMV358IDR
   - D4|D5|D6|BAT54
   - R31|R32|R33|R34|R35|R36
   - R59|R60|R61|R62|R63|R64
   - VSENSE_5V\+|VSENSE_3V3\+|VSENSE_ADJ\+
2. Keep checks narrow to avoid noisy matches from unrelated passive components.
