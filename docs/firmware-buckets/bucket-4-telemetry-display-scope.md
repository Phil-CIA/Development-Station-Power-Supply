# Bucket 4: Telemetry and display-link contract scope

Status: active Rev-C control document; end-to-end bench acceptance remains
separate from compilation or command-parser success.

Recovered from PR #52, which merged into `phil-cia-firmware-scope-buckets-alignment`
after base PR #50 merged to main. Updated for the current fixed-rail UI and UART1 path.

## In scope now

- Keep STM32 telemetry publishing and CrowPanel parsing synchronized for the current schema and units.
- Maintain the current UART contract: `CMD:` requests, `ACK:` acceptance,
  `ERR:` rejection, and asynchronous `EVT:` reports.
- Validate read/edit/write paths for shared `OUTPUT`, CH1/CH2 `ILIM`, and state readback.
- Keep `docs/DISPLAY_INTERFACE_STANDARD.md`, display trackers, and shipped behavior aligned.

## Out of scope / blocked by hardware

- Incompatible protocol/schema changes without an explicit migration and scope decision.
- Claims of end-to-end acceptance from an unstable or unidentified wiring configuration.
- Adjustable voltage, manual CV/CC selection, or CH3 setpoint controls.
- Restarting or redesigning the custom ESP32-C6 path (#17). Both display paths
  remain retained; CrowPanel is the active target and the custom panel is paused
  under `docs/display-project/README.md`.
- Hardware ISET/persistence/regulation claims based only on an ILIM ACK.

## CMD/ACK/ERR/EVT contract boundaries

| Surface | Bucket 4 ownership | Outside this scope |
|---|---|---|
| Grammar | Current implemented verbs and payloads | Unreviewed incompatible changes |
| ACK/ERR | Parseable acceptance/rejection and explicit errors | Implicit success or silent failure |
| Events | Reflect asynchronous state/fault reports without losing queued lines | Unrelated event-taxonomy expansion |
| Binary telemetry | Field order, encoding, units, and meaning parity | Unrelated schema expansion |
| UI bindings | Current Setup/Main/Detail/Graph consumers | Hardware coverage inferred from labels alone |

## Current integration constraints

- CH1 +5V and CH2 +3.3V are fixed rails, with one shared output control.
  Current limits are editable; voltage and recovery modes are not UI setpoints.
- Use the existing shared ILIM transaction/parser (`ilim_core.h` and
  `disp_link_slave.*`), not the superseded #71 ACK gate. Only a matching
  report/readback confirms a write; timeout/link loss means unconfirmed.
- The host accepts CH1 0..3000 mA and CH2 0..2000 mA; the current touch editor
  blocks zero writes. ILIM is a RAM value, not persisted or wired to hardware
  ISET. See the #98 command-path checkpoint in the screen tracker.
- The normal CrowPanel data path is UART1-OUT (IO19 RX / IO20 TX); UART0-IN
  supplies power only. Verify K1, shared ground, polarity, and the interim
  connector mapping in `docs/DISPLAY_INTERFACE_STANDARD.md`. UART0 data is
  recovery-only and requires host isolation during upload.
- UDI framing remains UART 115200 8N1. Fault-source coverage is governed by
  Bucket 3 and the authoritative pin table, not by receipt of an event alone.

## Exit criteria (bench measurable)

1. Capture successful `CMD` -> `ACK` round-trips for output and current-limit control.
2. Capture a rejection with the expected `ERR:` reason and consistent UI state.
3. Parse telemetry end-to-end and reflect values for a stable bench interval.
4. Reflect an asynchronous event without manual refresh or reconnect.
5. Exercise ILIM timeout/link loss and readback; never show an unconfirmed draft as confirmed.
6. Keep protocol docs consistent with observed behavior in the implementing PR.

## Evidence required

- Timestamped host/display logs, including commands, responses, and events.
- Per-run bundle: command, response, UI outcome, and telemetry/event excerpt.
- Hardware revision/source identity, firmware commits, UART settings, and wiring/K1 notes.
- Matching-ACK, rejection, and timeout/readback evidence for tracked ILIM writes.
- Explicit separation of command acceptance from physical output/regulation measurements.

## Issue mapping

| Issue | Role | Disposition |
|---|---|---|
| #26 Display screens | Current telemetry/command consumers | In scope |
| #40 CrowPanel startup | Link-readiness evidence | In scope |
| #74 / #75 Touch interaction | Current editor and remaining Setup affordances | Existing editor merged in #97; remaining work is not implemented here |
| #77 CV/CC validation | Physical-regulation evidence dependency | Separate Bucket 3 gate |
| #17 Custom-panel rewrite | Secondary-path compatibility | Paused unless explicitly reactivated |

## Bench execution checklist

- [ ] Record both endpoint revisions and verify the current connector/UART contract.
- [ ] Capture startup and stable telemetry logs on both sides.
- [ ] Record OUTPUT and ILIM command/ACK/UI outcomes.
- [ ] Record one rejected command and its visible error.
- [ ] Exercise timeout/link loss and readback confirmation.
- [ ] Capture a source-qualified asynchronous event and UI response.
- [ ] Attach evidence without claiming ISET/regulation or persistence from ACK alone.
