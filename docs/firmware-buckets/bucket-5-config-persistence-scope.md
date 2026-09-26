# Bucket 5: Config/Persistence/Calibration Scope

Status: active control document for Rev-C firmware execution under Bucket 5.

Primary issue ownership: **#38**  
Explicit out-of-scope issue: **#25** (unless formally re-scoped)

## In scope now

- Versioned configuration load/save/reset/erase behavior on STM32 using the current external flash path (W25Q128 integration in the active firmware target).
- Persistence of calibration coefficients required for rail telemetry/control correctness (including per-rail coefficients already represented in current firmware structures).
- Deterministic defaulting behavior on first boot (no valid config present) and after explicit operator reset-to-defaults.
- Safe recovery behavior for unsupported config version, malformed payload, checksum/header mismatch, or read failure.
- Operator-visible logging that differentiates:
  - normal persisted-load path,
  - default-init path,
  - corruption/version-recovery path.

## Out of scope / blocked by hardware

- **#25 Add OTA and Wi-Fi** is explicitly out of scope for this bucket and Rev-C bench bring-up unless project scope is formally updated.
- Any broad persistence data-model redesign that lacks a migration/recovery strategy and validation evidence.
- Security-hardening beyond current bring-up requirements (e.g., encrypted config blobs, secure key storage), unless explicitly re-scoped.
- Hardware changes to add/replace nonvolatile storage devices; this bucket operates on currently routed and assembled hardware.

## Exit criteria (bench measurable)

1. **Cold-boot restore:** After writing non-default config/calibration values, power-cycle and verify restored values match expected persisted state.
2. **Deterministic reset path:** Execute reset-to-defaults, power-cycle, and verify defaults are restored exactly (no stale persisted residue).
3. **Corruption/version recovery:** Inject one invalid-config condition (bad version or integrity failure), boot, and verify safe recovery to defaults with explicit reason logged.
4. **Calibration persistence:** At least one changed per-rail calibration coefficient survives save + cold boot and is reported correctly in runtime telemetry/config readback.
5. **No silent fallback:** Recovery and default-init paths are distinguishable in logs from nominal persisted-load path.

## Evidence required

- Serial boot logs for:
  - successful persisted load after cold boot,
  - reset-to-defaults flow,
  - intentional invalid-config recovery run.
- Before/after config snapshots (or equivalent readback logs) proving persisted values and restored defaults.
- Bench run notes with timestamped procedure steps and observed outcomes for each exit criterion.
- PR description mapping each exit criterion to exact log excerpt(s) and test step references.

## Config versioning/recovery boundaries

- **Version contract:** Config payload must carry an explicit schema/config version. Unsupported versions must never be interpreted as compatible.
- **Integrity boundary:** Any structural or integrity failure (header mismatch, invalid length/layout, checksum/signature mismatch if present) is treated as invalid config.
- **Recovery action boundary:** Invalid/unsupported config triggers safe default initialization and optional overwrite/repair path; runtime must not continue with partially trusted values.
- **Migration boundary:** Cross-version migration is allowed only when an explicit migration path is implemented and validated; otherwise fall back to defaults.
- **Calibration safety boundary:** Missing/invalid calibration fields must resolve to safe baseline coefficients with explicit log annotation.
- **Operator visibility boundary:** Recovery cause (version mismatch vs corruption vs missing config) must be externally observable in logs for bench diagnosis.

## Issue mapping

| Issue | Bucket role | Scope disposition |
|---|---|---|
| #38 SPI memory test | Primary implementation/evidence issue for Bucket 5 persistence path validation | In scope now |
| #25 Add OTA and Wi-Fi | Explicitly excluded from Rev-C Bucket 5 execution unless formally re-scoped | Out of scope |

## Bench execution checklist

- [ ] Record firmware build identity (branch/commit) and target board revision before testing.
- [ ] Start from known defaults; capture baseline boot log.
- [ ] Write at least one non-default config value and one non-default calibration coefficient.
- [ ] Save config, power-cycle hardware, and capture cold-boot restore log.
- [ ] Verify readback matches written non-default values after reboot.
- [ ] Execute reset-to-defaults command/path; power-cycle; verify default values restored.
- [ ] Inject one invalid-config case (version mismatch or corruption) and capture recovery log.
- [ ] Confirm recovery returns system to safe defaults and does not retain invalid fields.
- [ ] Attach all logs/readbacks to PR and map evidence to exit criteria.

