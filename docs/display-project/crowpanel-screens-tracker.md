# CrowPanel Screens & UX Tracker

Coordination doc for the "make the CrowPanel feel like a real product" work.
This file is the source of truth for status across sessions. Update it in each
merged PR so the next session can rebase / continue cleanly.

- Scope target: `crowpanel-43-bringup/` (ESP32-S3, LVGL, UDI UART to STM32 HAT)
- Coordination: this tracker lives on `main` after its documentation PR merges.
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
| D4 | `phil-cia-crowpanel-setup-screen` | Setup wizard: shared output and CH1/CH2 current limits, commit via UDI, cancel/back, validation. No adjustable voltage or manual CV/CC selector. | [#71](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/71) | 🟡 (open PR; not merged) | part of #26 |
| D5 | `phil-cia-crowpanel-graph-screen` | Trend: window selector (30 s / 5 min / 30 min), pause/resume, clear, per-channel visibility, autoscale. | — | ⬜ | #26 |
| D6 | `phil-cia-crowpanel-settings-screen` | Settings submenus fully functional: System (brightness, sleep, units), Dataset (save/load/reset cal), About (versions, uptime, UDI stats). | — | ⬜ | #26 |
| D7 | `phil-cia-crowpanel-fault-modal` | Global fault/alert modal (OVP/OCP/OTP/UVLO) driven by `EVT:` frames, ack + clear. | — | ⬜ | #26 + Bucket 3 tie-in |
| D8 | `phil-cia-crowpanel-startup-selftest` | Boot self-test screen: RGB, touch, I2C (0x30, 0x5D), SPI flash, PSRAM, UDI handshake — pass/fail chips before Main. | — | ⬜ | #27, #37, #38, #39, #40 |

When #26 is fully covered by D2–D6, close it in the D6 PR.
When #27/#37/#38/#39/#40 are covered by D8, close them in the D8 PR.

## FNIRSI-inspired fixed-rail UI plan

**Decision (2026-10-02):** Use FNIRSI DPS-150 / IPS3608 as visual and
interaction references, adapted to this supply rather than copied as an
adjustable-voltage product. Implementation stays in VS Code; this planning
session changes documentation only. The custom display path stays paused.

### Product contract

- CH1 is fixed +5 V; CH2 is fixed +3.3 V. Show nominal voltage read-only,
  separately from measured output voltage. No voltage editor or `VSET`.
- Users set a current limit per rail. CV/CC is automatic status, not a manual
  mode selector. The user chose this behavior **if supported by hardware**.
  LATCH/HICCUP/MONITOR are fault-recovery policies, not CV/CC choices.
- There is one shared master OUTPUT control. Do not invent independent
  per-channel ON/OFF buttons. CH3 is not an adjustable output.
- Touch alone must support every action on current bench hardware (#74).
  Encoder support may remain optional, never an acceptance prerequisite.
- Host-confirmed values are authoritative. Pending edits, rejected commands,
  missing values, stale telemetry, and demo data must be visibly distinct.
- Current STM32 code infers CC from measured current reaching the limit;
  this is not evidence that the hardware sustains constant-current regulation.
  #78 gates definitive CV/CC claims. Use explicit unavailable/unknown or
  limit/trip wording where regulation cannot be observed or proven.

### Visual direction to approve before LVGL implementation

The existing [IPS3608 reference](../IPS3608_REFERENCE_MANUAL_KEY_SPECS.md)
documents yellow voltage, blue current, a dark background, neutral power
readouts, and instrument-style status chips. DPS-150-specific visual details
are not yet verified here. Ask the user for preferred Main and current-edit
photos from either reference; record which elements they want. Create an
original layout without copied logos/assets or a pixel-identical clone.

Proposed 800x480 structure (not yet user-approved):

```text
+----------------------------------------------------------------+
| WORKSTATION PSU     LIVE / STALE / DEMO     OUTPUT ON / OFF      |
+-------------------------------+--------------------------------+
| CH1  FIXED +5 V               | CH2  FIXED +3.3 V               |
| measured voltage (yellow)     | measured voltage (yellow)      |
| measured current (blue)       | measured current (blue)        |
| measured power (neutral)      | measured power (neutral)       |
| I LIMIT: confirmed value      | I LIMIT: confirmed value       |
| regulation / fault / unknown  | regulation / fault / unknown   |
+-------------------------------+--------------------------------+
| Main           Setup            Graph             Settings     |
+----------------------------------------------------------------+
```

Both rail cards have equal priority. Debug sequence counters and detailed
uptime belong in diagnostics, not the main measurement hierarchy. Require
at least 44x44 px touch targets, persistent units, text plus color for
status, and visible Select/Edit/Pending/Error states. Final dimensions,
fonts, spacing, and error examples are outputs of #76, not implied by this
sketch. Do not add decorative fan/protection/statistics indicators unless
their data source is actually supported.

### Issues, dependencies, and PR sequence

These extend #65's D buckets rather than creating a competing roadmap.
All firmware work names its primary scope bucket and includes its exit
evidence. Do not open empty implementation PRs ahead of work.

| Step | Owner / branch | Dependency | Deliverable |
|------|----------------|------------|-------------|
| P0 | This documentation PR | None | Fixed-rail decisions, issue links, VS Code prompts; no firmware changes |
| P1 | [#76](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/76), `display/fnirsi-visual-spec` | P0 merged; user reference photos and wireframe approval | Documentation PR with approved 800x480 visual/interaction specification |
| P2 | [#78](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/78), `firmware/fixed-rail-regulation-status` | P0 merged; hardware-safe bench conditions | Regulation/status evidence and truth table; behavior PR only if justified. Can run independently of P1 |
| P3 | [#77](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/77), `display/fnirsi-dual-rail-dashboard` | P1 merged; reconcile overlapping #71 work | Main dashboard and navigation PR. P2 evidence required for definitive CV/CC labels; unresolved states must remain explicit |
| P4 | [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74) / [#75](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/75), `display/touch-current-limit-editor` | #71 disposition resolved; P1 approved; P3 merged where shared layout is used | Touch-only current editor and UX polish PR, reusing D4 command/ACK/error flow |
| P5 | Existing D5-D8 | Accepted common UI patterns; relevant data contracts | Separate Graph, Settings, fault, and self-test PRs; no new duplicate issues |

**Scope conflict:** #73 still proposes `VSET` and recovery `MODE` commands.
The user-directed fixed-rail UI plan removes the VSET requirement from this
effort. Recovery-policy work stays separate under #14; do not implement
#73 merely to make this visual plan work. Re-scope the existing issue with
these findings before any protocol expansion. Do not silently close it.

**Current checkout vs open work:** #71 is open, not part of this checkout's
merged baseline. Its referenced `D4-setup-screen.md` is absent here. Review
that PR before reusing its wizard; do not assume unmerged files or behavior
are present, and do not mix overlapping layout changes without reconciliation.

### Copy-paste VS Code Copilot session instructions

Use one prompt per session/branch. Start from updated `main` only after the
dependency PRs merge. If the checkout is dirty or on someone else's active
branch, stop before switching it. Do not merge or close issues without
review and the evidence required by the workflow.

**Session 1: visual specification (#76)**

```text
Work in Development-Station-Power-Supply on display/fnirsi-visual-spec.
Read README.md, docs/SYSTEM_DEVELOPMENT_WORKFLOW.md,
docs/FIRMWARE_DEVELOPMENT_PLAN.md, docs/display-project/README.md,
docs/display-project/crowpanel-screens-tracker.md, and
docs/IPS3608_REFERENCE_MANUAL_KEY_SPECS.md. Read issue #76 and review #71,
#74, #75, and #73 for conflicts. Inspect current CrowPanel source and audit
photos; do not assume open PR code has merged.
Ask me for the DPS-150 / IPS3608 photos and visual elements I prefer.
Create an original 800x480 wireframe/spec in the existing screen tracker:
equal CH1 +5 V / CH2 +3.3 V cards, large yellow V / blue A, neutral W,
read-only nominal voltages, confirmed I LIMIT, per-channel status,
one shared OUTPUT control, persistent link/demo state, touch navigation.
Specify dimensions/fonts/spacing, >=44x44 px targets, edit/pending/error
states, and live/off/stale/demo/fault/unknown examples. No voltage editor,
manual CV/CC selector, new UDI commands, or firmware/hardware edits.
Get my approval before marking the design accepted. Open a documentation
PR referencing #76 and #65; state firmware unchanged/not bench-tested.
Stop after the specification PR; do not start LVGL implementation.
```

**Session 2: regulation/status evidence (#78)**

```text
Work on firmware/fixed-rail-regulation-status. Read the project/workflow/
firmware plan, screen tracker, DISPLAY_INTERFACE_STANDARD,
STM32_BLUEPILL_PIN_TABLE, and relevant hardware change trackers.
Read #78, #14, #62, and #73. Trace routed ISET/OCP/control paths and
publishTelemetry CV bits against the CrowPanel parser/status bindings.
Determine what is observed versus inferred. Write a safe operator-run
bench procedure for both fixed rails below/near/at their current limits;
do not enable outputs or claim physical measurements without me.
Cover invalid sensors, output off, zero limit, stale/legacy telemetry,
threshold noise, and trip/clear. Record CV/confirmed CC/limit/trip/unknown
truth table and evidence in the existing tracker. If CC is not proven,
specify truthful labels, not a guessed regulation state. No hardware
redesign or adjustable voltage. Propose contract changes before incompatible
edits; keep Bucket 3 primary with Buckets 2/4 dependencies explicit.
Open a focused PR with actual evidence or clearly stated remaining gates.
```

**Session 3: dashboard (#77)**

```text
Work on display/fnirsi-dual-rail-dashboard after #76's spec PR merges.
Read the project/workflow/firmware plan and display tracker, then #77 and
the accepted spec. Review #71's disposition before editing shared code.
Implement the approved LVGL 8.3 layout in crowpanel-43-bringup, reusing
theme/helpers and existing telemetry/UDI. No STM32/protocol/hardware edits.
Show both rails' measured V/I/P, read-only nominal voltages, confirmed
GET ILIM CH1/CH2 values, and individual status. Redraw setpoints even if
measurements have not changed. Preserve shared OUTPUT semantics.
Never guess CC from missing CV bits/current thresholds: use #78 evidence
or explicit unknown/limit wording. Distinguish live/stale/demo/off/pending/
fault states; preserve ACK/ERR handling. All navigation is touch-only.
Build with pio run -d crowpanel-43-bringup -e crowpanel43 (using local
PlatformIO executable if not on PATH). Request my bench photos/logs for
the acceptance states; do not fabricate evidence. Update the tracker and
affected firmware inventory. Open one Bucket 4 PR referencing #77/#65;
state exact bench-tested status. Stop before Setup/Graph feature expansion.
```

**Session 4: current-limit editor (#74 / #75)**

```text
Work on display/touch-current-limit-editor after reconciling #71 and the
approved shared layout. Read #74/#75, the screen tracker, and actual
merged D4 command/ACK/error state machine before changing it.
Implement touch-only Select/Edit/Pending/Error flow for shared OUTPUT and
CH1/CH2 current limits: select target, enter edit, adjust, apply, cancel,
exit. Highlight the active channel/field and adapt action labels by state.
Use host-supported ILIM validation (currently CH1 0..3000 mA, CH2
0..2000 mA); these are protocol ranges, not proof of safe bench ratings.
No editable voltage, manual CV/CC, recovery-mode policy, CH3 controls,
independent output toggles, or new UDI commands. Keep confirmed values
separate from drafts; show send failure/ERR/timeout, refresh host values,
and prevent duplicate pending writes. Do not auto-enable outputs.
Build crowpanel43, exercise touch select/edit/apply/cancel and success/
ERR/timeout/stale paths, and request bench evidence. Update the tracker
and affected inventory, then open one Bucket 4 PR referencing #74/#75/#65.
```

### Review/evidence checklist for this UI effort

- [ ] User-selected reference images and original wireframe approved (#76).
- [ ] Hardware CC/limit/trip behavior and status validity documented (#78).
- [ ] Main and current editor usable by touch without prior LVGL knowledge.
- [ ] Both confirmed limits refresh independently of measured V/I changes.
- [ ] No misleading live/demo, healthy/fault, off/CV, or unknown/CC states.
- [ ] Physical photos show no clipping across the required state matrix.
- [ ] Serial evidence covers a successful command and ERR/timeout recovery.
- [ ] Each PR states bench-tested status, updates this tracker, and preserves
  existing UDI compatibility; only affected firmware targets are built.

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
- 2026-10-02: Next visual session starts with #76 reference/wireframe approval
  under the fixed-rail plan above. D4 remains open in #71; reconcile it before
  overlapping work. #74/#75 own touch/edit clarity, and #78 gates proven
  CV/CC semantics. D3's navigation follow-up is included in #77.
