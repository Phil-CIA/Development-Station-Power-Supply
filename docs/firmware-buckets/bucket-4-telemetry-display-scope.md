# Bucket 4: Telemetry and display-link contract scope

Status: active control document for Rev-C firmware execution under Bucket 4.

## In scope now

- Keep STM32 telemetry publish behavior and CrowPanel telemetry parse behavior synchronized for the current frame schema and field semantics.
- Maintain and validate the UART text command channel contract used by setup/control flows:
  - `CMD:` requests from display to host
  - `ACK:` positive host response to accepted command
  - `ERR:` explicit host rejection with reason
  - `EVT:` host-originated asynchronous state/fault notifications
- Validate setup-screen read/edit/write paths for currently owned controls (`OUTPUT`, `ILIM`, state readback) using existing command grammar.
- Keep protocol-facing documentation aligned with shipped behavior (`docs/DISPLAY_INTERFACE_STANDARD.md` and this bucket document).

## Out of scope / blocked by hardware

- Protocol-family redesigns (new framing families, incompatible wire protocol replacement, transport swaps) without a migration plan and explicit re-scope approval.
- Claims of fully validated end-to-end telemetry/control on unreproducible bench setups (for example, missing or unstable UART link hardware path).
- Custom front-panel rewrite work on the ESP32-C6 secondary path (`#17`) beyond preserving compatibility notes; that path remains paused unless reactivated.

## CMD/ACK/ERR/EVT contract boundaries

| Surface | Bucket 4 ownership boundary | Explicitly outside Bucket 4 |
|---|---|---|
| Command grammar | Stability and correctness of currently implemented `CMD:` verbs and expected payload forms used by CrowPanel ↔ STM32 | New protocol families or unreviewed incompatible grammar changes |
| Positive/negative response semantics | Deterministic `ACK:` on accepted command and deterministic `ERR:` on reject/invalid input, with parseable reason text | Silent fallback, implicit success, or success-shaped failures |
| Event semantics | `EVT:` signaling for asynchronous state/fault transitions that the UI must reflect immediately | Event taxonomy expansion not required for current setup/telemetry contract |
| Telemetry frame schema | Field order/encoding/units parity between host publish path and display parse path | Schema expansion unrelated to current display control/telemetry needs |
| UI binding | Setup/Main/Graph behavior that consumes current contract exactly as documented | New UX feature families that do not validate the current contract |

## Exit criteria (bench measurable)

1. A bench run captures at least one successful command round-trip (`CMD`→`ACK`) for output control and one for current-limit control.
2. A bench run captures at least one command rejection (`CMD`→`ERR`) with an expected, parseable reason and no UI desynchronization.
3. Telemetry frames are parsed end-to-end on bench with values reflected on the display for at least one stable operating interval.
4. At least one asynchronous event (`EVT`) is observed and reflected in UI state without requiring a manual refresh/reconnect.
5. Protocol documentation remains consistent with observed behavior in the same PR that introduces behavior changes.

## Evidence required

- Raw or minimally filtered serial logs from both link sides (host + display), including timestamps and command/response/event lines.
- One short evidence bundle per run:
  - command issued
  - host response (`ACK`/`ERR`)
  - resulting UI state note
  - relevant telemetry/event excerpt
- Bench setup note (hardware revision, firmware commits/branches, UART settings).
- If behavior changed, doc diff references showing updated protocol documentation in the same PR.

## Issue mapping

| Issue | Ownership in this bucket | Current disposition |
|---|---|---|
| #26 CrowPanel display screens | Primary execution issue for display behavior consuming telemetry/command contract | In scope now |
| #40 CrowPanel startup test | Primary execution issue for startup/bring-up evidence of telemetry-link readiness | In scope now |
| #17 Milestone 4 custom panel UDI+LVGL rewrite | Secondary-path compatibility reference only | Paused (out of scope unless explicitly reactivated) |

## Bench execution checklist

- [ ] Confirm both firmware endpoints and UART settings match the active contract before testing.
- [ ] Capture baseline startup logs from host and display.
- [ ] Execute `OUTPUT` command round-trip and record `CMD` + `ACK` evidence.
- [ ] Execute `ILIM` command round-trip and record `CMD` + `ACK` evidence.
- [ ] Execute at least one invalid/unsupported command and record `CMD` + `ERR` evidence.
- [ ] Capture telemetry frame parse evidence during stable run window.
- [ ] Trigger or observe at least one relevant asynchronous state/fault event and capture `EVT` evidence.
- [ ] Verify display state consistency after each command/event (no stale or contradictory UI state).
- [ ] Attach evidence artifacts to the implementing PR and reference this bucket document.
