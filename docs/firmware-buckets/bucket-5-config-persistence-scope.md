# Bucket 5: Config/Persistence/Calibration Scope

Status: active Rev-C control document; persistence acceptance requires cold-boot
evidence, not a successful build.

Recovered from PR #53, which merged into `phil-cia-firmware-scope-buckets-alignment`
after base PR #50 merged to main.

Primary issue ownership: **#38**. **#25** remains out of scope unless formally re-scoped.

## In scope now

- Versioned config load/save/reset/erase through the STM32 external W25Q128 data flash.
- Persistence of existing per-rail calibration coefficients and configured D9 path state.
- Deterministic defaults on first boot and explicit reset.
- Safe recovery from unsupported version, malformed length/header, CRC failure,
  invalid gain, or unavailable flash.
- Operator-visible distinction between persisted load, defaults, and recovery.

## Out of scope / blocked by hardware

- OTA/Wi-Fi (#25), broad data-model redesign without migration, and unrelated security hardening.
- Hardware replacement/addition of storage or claims that external data flash increases MCU executable space.
- Persistence of ILIM or output state not represented in the existing persistent payload.
- Rev-D migration or hardware qualification; the F405 target remains build-only.

## Current command and recovery surfaces

- Operator/debug commands: `CFGSHOW`/`CALSHOW`, `CALSET`, `CFGSAVE`, `CFGLOAD`,
  `CFGRESET`, and `CFGERASE`. These are not new UDI verbs or display controls.
- Capture the startup `[CFG] REC r=<code> d=<0|1> s=<0|1>` summary and UDI
  `CMD:GET CFGREC` readback. Codes: `O` OK, `F` flash, `N` no signature,
  `V` version, `L` length, `C` CRC, `G` gain; `U` is the unknown/uninitialized code.
- Record defaults-used and save-success flags as observed. An attempted repair
  is not proof of durable storage.
- Existing config commands and reason reporting are implemented; their presence
  does not close the bench criteria below.

## Exit criteria (bench measurable)

1. Non-default persisted config/calibration survives save and cold boot with matching readback.
2. Reset-to-defaults and subsequent cold boot restore the documented defaults.
3. One deliberately invalid-config case recovers safely with the correct reason.
4. At least one changed calibration coefficient survives and is applied to telemetry.
5. Normal load, first-boot defaults, and corruption/version recovery remain distinguishable.

## Evidence required

- Boot logs for successful load, reset/defaults, and authorized invalid-config recovery.
- Before/after config snapshots proving written values, restored values, and defaults.
- Recovery summary and `GET CFGREC` response with observed flags.
- Timestamped procedures, hardware identity, firmware revision, and criterion-to-log mapping.

## Config versioning/recovery boundaries

- Unsupported versions must not be interpreted as compatible payloads.
- Header, length, CRC, and coefficient validity failures invalidate the config.
- Recovery uses safe defaults with an explicit reason; never retain partially trusted values.
- Migration requires an implemented, validated path; otherwise recover to defaults.
- Verify the outcome of any save/repair independently from default initialization.
- Invalid-config injection requires an authorized, bounded test on the dedicated
  config sector with saved readback and a recovery procedure; do not erase
  unrelated flash contents.

## Issue mapping

| Issue | Role | Disposition |
|---|---|---|
| #38 SPI memory test | Persistence-path evidence | In scope |
| #25 OTA/Wi-Fi | Separate feature family | Out of scope unless re-scoped |

## Bench execution checklist

- [ ] Record firmware and identified board/storage revision.
- [ ] Capture baseline config and recovery logs.
- [ ] Change and save a non-default calibration/config value.
- [ ] Cold boot and compare readback and calibrated telemetry.
- [ ] Reset defaults, cold boot, and verify exact default values.
- [ ] Run one authorized invalid-config case and capture the recovery reason.
- [ ] Verify no invalid residue or falsely reported save success.
- [ ] Attach logs/readbacks and map them to each exit criterion.
