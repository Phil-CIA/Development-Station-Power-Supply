# CrowPanel Screens & UX Tracker

Coordination doc for the "make the CrowPanel feel like a real product" work.
This file is the source of truth for status across sessions. Update it in each
merged PR so the next session can rebase / continue cleanly.

- Scope target: `crowpanel-43-bringup/` (ESP32-S3, LVGL, UDI UART to STM32 HAT)
- Coordination branch (this file lives here): `phil-cia-crowpanel-screens-focus`
- Related issues: #26 (crowpanel display screens), #27 (Bootup log and testing),
  #37 (ESP32 startup test), #38 (SPI memory test), #39 (I2C startup test),
  #40 (CrowPanel startup test)
- Related spec: `docs/DISPLAY_INTERFACE_STANDARD.md` (UDI)

## Rules of engagement

- One branch per bucket, all branched off `main` (not off this branch).
- One PR per bucket, small and reviewable, bench-tested on the physical CrowPanel.
- Never commit to `main` directly (see `docs/SYSTEM_DEVELOPMENT_WORKFLOW.md`).
- Do **not** stack D2–D8 on this branch. The audit doc lives here; behavior work
  starts fresh off `main` after this tracker PR merges.
- No new root-level HANDOFF/SUMMARY files. Update this tracker instead.
- All host↔display messaging must conform to the UDI framing
  (`CMD:` / `ACK:` / `EVT:` / `ERR:`).

## Bucket status

Legend: ⬜ not started · 🟡 in progress · 🟢 merged · 🔴 blocked

| ID | Branch (off `main`) | Scope | PR | Status | Closes |
|----|---------------------|-------|----|--------|--------|
| D0 | `phil-cia-crowpanel-screens-focus` | This tracker doc + audit skeleton | — | 🟡 | — |
| D1 | `phil-cia-crowpanel-ui-audit` | Screen-by-screen audit + layout-only fixes (padding, alignment, font, chip colors). No behavior changes. | [#66](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/66) | 🟢 | prep for #26 |
| D2 | `phil-cia-crowpanel-nav-shell` | Uniform top bar + bottom nav across all screens, consistent back/home, state chip system unified. | [#67](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/67) | 🟢 | part of #26 |
| D3 | `phil-cia-crowpanel-main-screen` | Main telemetry: live V/I/P per channel, output ON/OFF wired to UDI, channel selector, big numerics, fault/ILIM chips. | [#68](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/68) | 🟢 | #26 |
| D4 | `phil-cia-crowpanel-setup-screen` | Setup wizard (UI-subset): edit/commit/cancel-revert for the UDI-supported fields — Output enable + CH1/CH2 I_limit — with await-ACK gating, inline ERR, and touch-first flow (encoder optional when populated). V_set / mode (LATCH/HICCUP/MONITOR) / CH3 deferred to [#73](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/73) (STM32 UDI lacks those commands). Encoderless operation follow-up tracked in [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74). | [#71](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/71) | 🟡 | #26 |
| D5 | `phil-cia-crowpanel-graph-screen` | Trend: window selector (30 s / 5 min / 30 min), pause/resume, clear, per-channel visibility, autoscale. For current hardware rev, all controls must be touch-first; no required rotary-encoder dependency (see [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74)). | — | ⬜ | #26 |
| D6 | `phil-cia-crowpanel-settings-screen` | Settings submenus fully functional: System (brightness, sleep, units), Dataset (save/load/reset cal), About (versions, uptime, UDI stats). For current hardware rev, all controls must be touch-first; no required rotary-encoder dependency (see [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74)). | — | ⬜ | #26 |
| D7 | `phil-cia-crowpanel-fault-modal` | Global fault/alert modal (OVP/OCP/OTP/UVLO) driven by `EVT:` frames, ack + clear. For current hardware rev, acknowledge/clear flows must be touch-first; no required rotary-encoder dependency (see [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74)). | — | ⬜ | #26 + Bucket 3 tie-in |
| D8 | `phil-cia-crowpanel-startup-selftest` | Boot self-test screen: RGB, touch, I2C (0x30, 0x5D), SPI flash, PSRAM, UDI handshake — pass/fail chips before Main. For current hardware rev, navigation and pass/fail actions must be touch-first; no required rotary-encoder dependency (see [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74)). | — | ⬜ | #27, #37, #38, #39, #40 |

When #26 is fully covered by D2–D6, close it in the D6 PR.
When #27/#37/#38/#39/#40 are covered by D8, close them in the D8 PR.

## Close-out checklist (return here to run)

- [ ] All D1–D8 PRs merged into `main`
- [ ] `main` rebased/pulled locally before starting any follow-on
- [ ] This tracker updated with PR numbers and 🟢 status per row
- [ ] Screenshots archived under `docs/display-project/screenshots/` (per bucket)
- [ ] Issue #26 closed
- [ ] Issues #27, #37, #38, #39, #40 closed (or re-scoped)
- [ ] `docs/FIRMWARE_DEVELOPMENT_PLAN.md` cross-referenced with any bucket work
  that changed UDI behavior

## Rebase / update protocol

When returning to this session for a status sync:

1. `git fetch origin && git log --oneline origin/main -20` — see what merged.
2. Update the **Status** and **PR** columns in the table above.
3. If any bucket depended on another (e.g. D3 uses nav shell from D2), note
   the rebase order in the notes section below before spawning the next agent.
4. Re-open the VSCode agent with the next ⬜ row as its objective.

## Notes / decisions log

- 2026-09-28: Tracker created. VSCode agent instructed to start with D1
  (audit + layout-only). Do **not** merge D1 into behavior branches — each
  D2–D8 branches off `main` after D1 merges.
- 2026-09-28: D1 bench-tested on physical CrowPanel across two flash/photo
  rounds. Real bugs found and fixed (layout-only): unsupported arrow glyphs
  on Setup, Main status-bar label overlap, Main CH1/CH2 power-label/bar
  overlap, Main CH2/SET-STATUS panels overlapping the fault row, and a
  duplicated/overlapping label on Graph. Known open item: the last Main-panel
  fix is build-verified but not yet re-photographed; no Settings screenshot
  captured yet. See `crowpanel-ui-audit.md` for details.
- 2026-09-28: D2 merged (#67), bench-verified. Main's top status bar is
  already at full width capacity (labels + OUTPUT badge + one nav button)
  so scope was contained to what fit safely: OUTPUT badge on Main is now
  tappable → Setup, and Graph ↔ Settings got mutual nav buttons. Every
  screen now reaches every other screen in at most one hop. Still open:
  Main has no direct button to Settings; a real shared top-bar/bottom-nav
  component (replacing the per-screen duplicated header code) is the
  correct long-term fix — noted as a D2 follow-up, not a blocker for D3.
- 2026-09-28: D3 (#68, merged) wired Main's RUN/WAIT chip to
  toggle `CMD:OUTPUT ON/OFF` directly. While scoping this, went down a
  research path on independent per-channel (CH1/CH2) output control that
  turned into real hardware/firmware data conflicts — spun out to issue #62
  instead of blocking this bucket, since it's a hardware-verification
  problem, not a CrowPanel UI problem. Key resolved finding: CH1/CH2 already
  share one master output enable by hardware design (not a gap) — PR #68's
  chip toggle already covers both correctly. Key unresolved finding (issue
  #62): the current `.net` file's `Q9` (AO3400A, wired to the fan connector)
  doesn't match a schematic screenshot's `Q9` (BSS138, ISET_MPU_3V3 gate),
  and `Q3`/`Q12`/`U4` don't appear in the current netlist at all — needs a
  fresh netlist export before any per-channel range-control firmware work
  proceeds. A related pure-comment fix (stale/self-contradictory AW9523
  P0.x pin comments) landed separately as PR #69, not gated on that
  resolution.
- **Next session should resume at D4** (rebase `main` first, D3/#68 is now
  merged). D3's open item (Main has no direct Settings button) and issue
  #62 are tracked separately and don't block D4–D8.
