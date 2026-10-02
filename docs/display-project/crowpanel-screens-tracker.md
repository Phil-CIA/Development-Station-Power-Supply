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
  #77 gates definitive CV/CC claims. Use explicit unavailable/unknown or
  limit/trip wording where regulation cannot be observed or proven.

### Visual direction to approve before LVGL implementation

**User-selected direction (2026-10-02):** Blend DPS-150 simplicity with
IPS3608 colors and status indicators. This selects the design direction,
not approval of final pixel dimensions/fonts. The user supplied two reference
images and selected the overview/detail structure below. A current-edit
reference and final detailed layout approval remain open.

The existing [IPS3608 reference](../IPS3608_REFERENCE_MANUAL_KEY_SPECS.md)
documents yellow voltage, blue current, a dark background, neutral power
readouts, and instrument-style status chips. The supplied first image
(user-described DPS-150 or a variant) shows large left-aligned, vertically
stacked voltage/current/power digits: yellow voltage, cyan current, and pale
neutral power. Its exact product identity is unverified; the visual preference
does not depend on that identity. The supplied second image shows a single
channel's readings on the left and yellow/cyan V/I traces on the right,
with status above and setpoints below. These are visual references, not
proof of supported device features. Create an original layout without
copied logos/assets or a pixel-identical clone.

**User-directed screen structure:**

- Main is a two-channel overview: CH1 on the left, CH2 on the right
  (side-by-side explicitly selected by the user). Each card has a large
  vertical V/A/W stack, following the first reference's reading hierarchy.
- No Energy/Ah/Wh/runtime statistics panel or Input panel on Main.
- Tap either channel card to open its own single-channel detail screen.
  Keep readings on the left and a V/I graph for that channel on the right,
  following the second reference's composition.
- Detail includes clearly labeled channel identity, read-only nominal
  voltage, confirmed current limit, status, and a visible Back to Main
  touch action. Switching channel changes both readings and graph source;
  do not silently mix CH1 voltage with CH2 current.
- The graph timebase and supported interactions remain D5 work. A reference
  image's 0.1 s label is not a requirement to invent a faster sample rate.
- Shared OUTPUT remains shared even on a single-channel screen; label it
  as affecting both outputs. There is no editable Vset.

800x480 composition (structure selected; exact geometry/fonts pending):

```text
+----------------------------------------------------------------+
| WORKSTATION PSU     LIVE / STALE / DEMO     OUTPUT ON / OFF      |
+-------------------------------+--------------------------------+
| CH1  FIXED +5 V               | CH2  FIXED +3.3 V               |
| large voltage value       V   | large voltage value        V   |
| large current value       A   | large current value        A   |
| large power value         W   | large power value          W   |
| I LIMIT: confirmed value      | I LIMIT: confirmed value       |
| regulation / fault / unknown  | regulation / fault / unknown   |
| tap for CH1 detail            | tap for CH2 detail             |
+-------------------------------+--------------------------------+
| Main           Setup            Graph             Settings     |
+----------------------------------------------------------------+
```

Single-channel detail composition:

```text
+----------------------------------------------------------------+
| Back to Main   CH1 / CH2   LINK STATUS   SHARED OUTPUT (BOTH)    |
+--------------------+-------------------------------------------+
| measured voltage V | selected channel's V / I graph            |
| measured current A | yellow V trace / cyan-blue A trace        |
| measured power   W | labeled axes, units, truthful timebase    |
| channel status     | stale/demo/unknown indication             |
+--------------------+-------------------------------------------+
| FIXED: 5.00 / 3.30 V (read-only) | I LIMIT: confirmed value     |
+----------------------------------------------------------------+
```

Both rail cards have equal priority. Debug sequence counters and detailed
uptime belong in diagnostics, not the main measurement hierarchy. Require
at least 44x44 px touch targets, persistent units, text plus color for
status, and visible Select/Edit/Pending/Error states. Final dimensions,
fonts, spacing, and error examples are outputs of #76, not implied by this
sketch. Do not add decorative fan/protection/statistics indicators unless
their data source is actually supported.

### User-selected Iset popup interaction

**Decision (2026-10-02):** On a channel's detail screen, tap its **Iset**
panel to open a current-limit popup for that channel. Iset means the
configured current limit, not measured output current. Label units in A
and show channel identity throughout.

1. Open with the host-confirmed limit as the initial draft, a slider, a
   tappable numeric value field, and Apply / Cancel actions.
2. Dragging the slider changes the draft and numeric field only.
3. Tap the numeric field inside the popup to open a numeric keypad for
   exact entry. Keypad completion validates and updates the same draft
   and slider; it does **not** send a command. Keypad dismissal preserves
   the preceding draft. Support decimal entry and backspace.
4. Apply sends the existing channel-specific `ILIM` command once, then
   shows Pending while awaiting matching host confirmation. Disable
   duplicate Apply and channel switching while the write is pending.
5. Matching ACK/readback updates the confirmed limit. Send failure, ERR,
   timeout, or link loss must be visible; do not claim success or silently
   replace the confirmed value. On an uncertain timeout, refresh from the
   host before retrying, because the command may already have applied.
6. Cancel before sending discards the draft without a command. After
   sending, dismissal cannot undo the host write: retain pending/result
   visibility and refresh on re-entry rather than promising rollback.

The user explicitly selected **draft editing plus Apply/Cancel**, not live
updates while dragging. Opening/editing never enables an output. Missing
confirmed values must trigger a host refresh and explicit loading/error
state, not a fabricated default.

Use integer mA as the canonical draft and convert displayed A without
floating-point rounding surprises. Respect actual host limits/precision
(currently CH1 0..3000 mA and CH2 0..2000 mA); protocol ranges are not
validated safe hardware ratings. Reject malformed, negative, out-of-range,
or unsupported-precision keypad input visibly rather than silently
clamping it. Determine slider step and permitted decimal precision in the
visual specification against the actual command contract.

The popup/keypad must fit 800x480 with >=44x44 px touch targets, preserve
the selected channel, and prevent touches from reaching the underlying
output/channel controls. Switching between slider and keypad must not
reset the draft. Use the display's existing pending/ACK/error machinery,
not a second competing command path.

### Visual mockup and specification (#76) - BASELINE VISUAL APPROVAL RECORDED

**Status (2026-10-02): baseline visual appearance and screen flow approved
(user decision, PR #80 comment).** The approval covers the baseline appearance
and screen flow for the first LVGL implementation (#78) only. It does NOT
cover bench validation or the unresolved behavior questions below (zero-limit
semantics, slider step, output-on confirmation, status validity), which stay
open until their implementation phase. Network date/time (WiFi/NTP) remains
later scope and is not part of the dashboard port. The Iset command-writing
editor stays a separate phase until overlapping PR #71 is reconciled. The
mockup is a simulated browser prototype;
firmware, protocol, and hardware unchanged; not bench-tested; not an LVGL
rendering. Browser text differs slightly from LVGL Montserrat, so final
clipping must be re-checked on the panel (#78).

Open it: double-click
[`mockups/crowpanel-fixed-rail-mockup.html`](mockups/crowpanel-fixed-rail-mockup.html)
(single file, no network or dependencies; any current browser). Click the
display to "touch" it. The right-hand column (outside the 800x480 area)
selects the example state (live, output off, stale, demo, unknown, pending,
error, limit/trip), jumps to any view, picks the simulated Apply result
(confirmed / ERR / timeout), toggles 44x44 target outlines and zoom, and runs
a layout audit. Deep link: `...mockup.html#state=trip&view=d1`
(views `main d1 d2 p1 p2 k1 k2`). Everything shown is simulated.

Layout audit (in-page, all 8 states x main, both details, both popups, both
keypads, error banners): no touch target under 44x44, no clipped text,
nothing outside 800x480. This checks the browser mockup only.

**Proposed dimensions (px, 800x480):**

| Region | Geometry |
|--------|----------|
| Top bar | y0 h52. Title/Back 150x44; channel chip on detail; link chip (LIVE/STALE/DEMO/UNKNOWN); shared OUTPUT button 200x46 at x588 |
| Info strip | y52 h28, all screens: date and time (left), unit temperature (center), SIMULATED tag (mockup-only, right) |
| Main cards | CH1 x8 / CH2 x404, y84, 388x336, 10 px radius, 2 px channel-color border, 6 px channel-color top accent, whole card is the tap target |
| Bottom nav | y428 h52, four 200x52 buttons (Main/Setup/Graph/Settings; only Main is live in the mockup) |
| Detail readings | x8 y84 320x316 panel, left, channel-tinted with channel-color border and accent |
| Detail graph | x336 y84 456x316 panel, right; plot area 456x244, V axis left (yellow), A axis right (blue) |
| Detail footer | y408 h64: FIXED read-only box 230 wide (not a button); Iset button 330x64 (tappable); last ILIM result 208 wide |
| Iset popup | x60 y36 680x408, scrim over the full 800x480 |
| Popup controls | value field 300x76 (tap = keypad); slider track 648x48 (44 px thumb); Cancel 200x56; Apply 220x56 |
| Keypad | Back 130x44; keys 76x60 in a 4x3 grid, wide 0 key 160x60; OK 76x196; entry box 300x84 |

**Proposed fonts (all already enabled in `crowpanel-43-bringup/include/lv_conf.h`):**
Montserrat 48 for V/A/W digits and popup values; 28 for units, CH label,
Iset value, keypad keys, fixed-voltage text; 20 for status chips, buttons and
nav; 16 for captions, labels and small chips. No new font sizes required.

**Proposed colors:** existing `UiTheme` values: background `#14181D`, panel
`#2A2F36`, soft panel `#22272D`, bar `#1B2026`, border `#3B434D`, text
`#F2F4F7`, muted `#B4BDC8`, voltage yellow `#F5C316`, current cyan-blue
`#2EA5F9`, OK green `#30C95E`, warn orange `#FF8C3A`. New proposed additions:
neutral power `#D5DAE0`, error red `#FF5A5F`, demo violet `#B48CFF`, unknown
gray `#8A94A3` (dashed outline). Status is always text plus color plus outline
style; no status is color-only.

**Channel identity colors (proposed, 2026-10-02 tweak):** CH1 pink `#FF79C6`
with card tint `#35262F`; CH2 teal `#2DD4BF` with card tint `#1E3837`. Applied
to the card/panel border, top accent, filled CH chip, graph panel border and
the Iset popup border and chip. Reading colors stay yellow/cyan-blue/neutral
for both channels, and the CH1/CH2 text is always shown, so channels are not
distinguished by color alone. Pink and teal were chosen to avoid the status
colors (green, orange, red, violet, gray).

**Unit temperature and date/time (proposed, 2026-10-02 tweak):** shown in the
info strip on every screen. Unit temperature is one integer value in degrees C
from the existing telemetry field `last_temp_C` (already parsed by the
CrowPanel firmware); it shows `--` when no telemetry exists and is dimmed when
stale. No temperature warning thresholds are invented. This revision has only
one temperature sensor, which is effectively the enclosure temperature (user
confirmation, 2026-10-02), so the value is not per-channel or regulator
temperature. Date/time is shown as
`YYYY-MM-DD HH:MM:SS` and as `CLOCK NOT SET` when unset. **Clock source
(user direction, 2026-10-02):** a later step will connect the CrowPanel to
WiFi and take the time from the router (network time). Until then, and until a
sync succeeds, the display shows `CLOCK NOT SET`. The current firmware has no
WiFi/time code and the UDI has no time message, so the mockup clock is
simulated and no protocol change is implied. WiFi/time-sync firmware is a
separate future PR, not part of this design-only PR.

**Proposed numeric precision and slider:** voltage 2 decimals (V), current
3 decimals (A), power 2 decimals (W), I LIMIT 3 decimals (A). Canonical draft
is integer mA. Slider step 10 mA (0.010 A); range 0..3000 mA (CH1) and
0..2000 mA (CH2). Keypad accepts up to 3 decimals (1 mA) and up to 2 integer
digits; extra digits/decimals, a second decimal point, and values above the
channel maximum are rejected with a visible message, never clamped. A keypad
value off the 10 mA grid (for example 1.678 A) stays exact in the draft; the
slider handle shows the nearest step and the draft only changes to a grid
value if the slider is moved afterwards. Protocol ranges, not hardware ratings.

**Status wording shown (no inferred CC as proven):** `ON - BELOW LIMIT` with
note "CV/CC not verified (#77)"; `AT LIMIT` with "I >= limit - CC not
verified"; `TRIP` with "cause not reported"; `OUTPUT OFF`; `NO DATA - STALE`;
`UNKNOWN`. Link chip: `LIVE`, `STALE n s`, `DEMO DATA`, `UNKNOWN`. On Main,
demo additionally tags each channel `DEMO`. The shared OUTPUT button appears in the
top bar on every screen and is labeled "CH1 + CH2 (shared)"; it is disabled
with explanatory text when the link is stale or unknown.

**Popup behavior demonstrated:** opens on the channel's confirmed limit;
slider drag edits the draft only ("DRAFT - NOT APPLIED"); tapping the value
opens the keypad (digits, `.`, backspace, OK, Back); OK returns to the popup
with the same draft and slider position and does not apply; Back keeps the
prior draft; Cancel discards; Apply shows PENDING with Apply and slider
disabled, then CONFIRMED, ERR (confirmed value unchanged), or TIMEOUT
(readback, retry allowed). Apply is disabled for no change, demo, stale, and
unknown states. The underlying screen is inert and the scrim swallows touches
while a modal is open.

**Open questions for the reviewer** (baseline visual approval does not answer
these; items 1-4 stay UNRESOLVED until their implementation phase):

1. UNRESOLVED (slider/editor phase): slider step 10 mA vs coarser (for
   example 50 mA)? Keypad covers exact values.
2. UNRESOLVED (slider/editor phase, gated on #77): is a 0.000 A limit allowed
   from the UI? Its firmware meaning is part of #77.
3. UNRESOLVED: keep the top-bar OUTPUT button identical on Main and detail
   (current), or add a confirm step before turning OUTPUT ON? The first
   dashboard PR preserves existing OUTPUT confirmation/error handling as-is.
4. UNRESOLVED (gated on #77): status labels `AT LIMIT` / `TRIP` acceptable
   until #77 gives evidence? The dashboard uses explicit unknown/limit wording.
5. Setup/Graph/Settings tabs are placeholders here; their layouts are D4-D6.
6. The original reference photos were not available to this session; the
   look follows the written reference notes above.
7. Date/time source: RESOLVED by user direction (WiFi network time, later).
   Open detail: time zone and daylight-saving handling, and wording when WiFi
   is connected but the sync has failed.
8. Temperature: RESOLVED. One sensor this revision (enclosure temperature);
   the label stays `UNIT TEMP` unless you prefer `ENCL TEMP`.

### First LVGL dashboard (#78) - build-verified, NOT bench-tested

Branch `display/fnirsi-dual-rail-dashboard`, Bucket 4, `crowpanel-43-bringup/src/main.cpp`
only. No STM32, protocol, hardware, WiFi/NTP, persistence, or recovery-policy
change. The Iset slider/keypad write flow is **not** in this PR (next phase,
after #71 reconciliation). Build: `pio run -d crowpanel-43-bringup -e crowpanel43`
(success). Not uploaded; no bench evidence yet.

**Implemented (LVGL 8.3, existing fonts 12/16/20/28/48, no new dependency):**

- Main: CH1 fixed +5 V left, CH2 fixed +3.3 V right; V/A/W stacks (yellow,
  cyan-blue, neutral) with persistent units; per-channel confirmed `I LIMIT`;
  per-channel status chip; whole card taps to that channel's detail; bottom
  nav Main/Setup/Graph/Settings (closes D3's missing Main-to-Settings hop). No
  Energy or Input panel.
- Detail: Back to Main, channel chip, readings left, selected-rail V/I chart
  right (V on the left axis, I and the confirmed limit on the right axis),
  read-only `FIXED` nominal voltage, read-only confirmed `Iset`, and an
  `EDIT LIMIT in Setup` button that opens the **existing** Setup editor on that
  channel's ILIM field (no dead Iset control, no new write path).
- One shared OUTPUT button in the top bar of both screens, labelled
  `CH1 + CH2 (shared)`. Existing `OUTPUT ON/OFF` command, no per-channel
  toggle. A send failure, host `ERR`, or a missing `ACK` within 1.5 s now shows
  a notice in the info strip; success is only shown by telemetry. The button is
  disabled (with a reason) for stale, unknown, demo, and legacy-frame states.
- Link state is LIVE / STALE n s / DEMO DATA / UNKNOWN, text plus color.
- The dashboard refreshes every loop (not only when a telemetry frame
  arrives), so STALE appears and host-confirmed limits redraw even if V/I are
  unchanged. Missing limits are requested one at a time with `GET ILIM CHn`
  while the link is live and the Main/Detail screen is shown.
- Status never asserts CC: `OUTPUT OFF`, `TRIP` (STM32 OVP/OCP/OTP flags),
  `ON - LIMIT UNKNOWN`, `LIMIT 0.000 A` (zero-limit meaning unverified),
  `AT LIMIT` (measured I >= confirmed limit, "CC not verified"), and
  `ON - BELOW LIMIT` ("CV/CC not verified (#77)"). The STM32 CV status bit is
  not displayed on Main/Detail.
- Trend history now stores CH2 as well as CH1 (and a demo flag). The detail
  chart plots only the selected rail's samples and never mixes demo with live
  samples. Missing data is explicit: `NO DATA`, `NO SAMPLES YET`, `NO CH2 DATA
  (legacy frame)`, `STALE - no new samples`, `DEMO DATA - SIMULATED`. The
  x axis is per received sample; the caption shows the real span (`last N s,
  M samples`). Timebase/window selection/pause/clear/autoscale remain D5.
- Demo mode values now sit on the fixed-rail nominals (+5 V / +3.3 V) instead
  of ~12 V, and are always labelled DEMO.
- Serial test aids: `SCREEN DETAIL1` / `SCREEN DETAIL2` (plus the existing
  `DEMO ON|OFF`, `UDI_STATUS`, `RX`).

**Deliberate differences from the approved mockup (LVGL/font limits):**

- Built-in Montserrat has no `U+00B7` middle dot, degree sign, `>=` glyph, or
  `U+203A`: separators are ` - `, temperature is `31 C`, the card chevron is
  `LV_SYMBOL_RIGHT`, the Back arrow is `LV_SYMBOL_LEFT`.
- LVGL 8.3 has no dashed borders or dashed chart lines: the unknown status chip
  uses a solid gray outline (text still says UNKNOWN) and the limit trace is a
  solid light-blue line instead of dashed.
- Back button is 180 px wide (mockup 150) so `Back to Main` fits at font 20;
  the CH chip and link chip shift right accordingly. Digits are Montserrat, not
  the browser font.
- Demo status text is `DEMO DATA` (violet) instead of the simulated `ON - BELOW
  LIMIT`, so demo values are never read as hardware state.
- The mockup-only `SIMULATED` tag is absent; the info strip right side shows
  command notices instead. The date/time field stays `CLOCK NOT SET` (WiFi time
  is later scope). Unit temperature is `last_temp_C`, `--` when unavailable.
- Detail footer: the mockup's `LAST ILIM RESULT` box is replaced by `EDIT LIMIT
  in Setup` until the dedicated editor (which owns ILIM results) exists. The
  Iset box is read-only here (`CONFIRMED` / `NO VALUE`), not a tap target.

**Known limitations (not fixed here, by scope):**

- The STM32 telemetry frame cannot flag a failed INA3221/AHT20 read: on a
  failed read it substitutes 5.000 V / 0.500 A (CH1), 3.300 V / 0.320 A (CH2),
  and 31 C, which the display cannot distinguish from real values. This needs
  a protocol/STM32 change and belongs with #77.
- The display keeps only the last ACK/ERR/EVT line, so back-to-back replies
  can overwrite each other; the dashboard therefore requests one missing limit
  at a time. Setup's three-command burst on entry has the same exposure.
- Output "on" means CH1 or CH2 enabled bit set in the extended status byte
  (previously CH1 only). The STM32 sets that bit only when the output is on and
  the rail voltage is above its minimum, so `OUTPUT OFF` can also mean "rail
  below enable voltage".
- The Graph screen (D5) is unchanged: its axes are still 9-15 V and its
  CV/CC chips still use the STM32 CV bit.
- Output-on confirmation, zero-limit semantics, and slider step remain
  UNRESOLVED (see open questions); nothing in this PR decides them.

**Layout/text-fit check (host-side, not a photo):** every string and worst-case
reading used on Main/Detail was measured against the Montserrat 12/16/20/28/48
glyph advances in LVGL 8.3.11 and fits its box (the only overflow is the
scrolling notice, by design). All touch targets are >= 44 x 44 (`static_assert`
in the firmware). Geometry is the mockup's 800 x 480 layout. Actual clipping and
touch behavior still need the physical-panel photos.

### Issues, dependencies, and PR sequence

These extend #65's D buckets rather than creating a competing roadmap.
All firmware work names its primary scope bucket and includes its exit
evidence. Do not open empty implementation PRs ahead of work.

| Step | Owner / branch | Dependency | Deliverable |
|------|----------------|------------|-------------|
| P0 | This documentation PR | None | Fixed-rail decisions, issue links, VS Code prompts; no firmware changes |
| P1 | [#76](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/76), `display/fnirsi-visual-spec` | P0 merged; user reference photos and wireframe approval | Documentation PR with approved 800x480 visual/interaction specification |
| P2 | [#77](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/77), `firmware/fixed-rail-regulation-status` | P0 merged; hardware-safe bench conditions | Regulation/status evidence and truth table; behavior PR only if justified. Can run independently of P1 |
| P3 | [#78](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/78), `display/fnirsi-dual-rail-dashboard` | P1 merged (#80); reconcile overlapping #71 work | Side-by-side V/A/W overview and channel-detail navigation/readout shell PR. P2 evidence required for definitive CV/CC labels; unresolved states must remain explicit. **Status: implemented and build-verified; not bench-tested** (see the #78 notes above) |
| P4 | [#74](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/74) / [#75](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/75), `display/touch-current-limit-editor` | #71 disposition resolved; P1 approved; P3 merged where shared layout is used | Touch-only current editor and UX polish PR, reusing D4 command/ACK/error flow |
| P5 | Existing D5-D8 | Accepted common UI patterns; relevant data contracts | Separate Graph, Settings, fault, and self-test PRs; no new duplicate issues |

D5 owns the selected-channel V/I graph on the detail screen, adapting the
existing Graph implementation rather than adding a competing graph family.
P3 can reuse the existing chart for a basic selected-channel view; history,
timebase/window selection, pause/resume, clear, and autoscale remain D5.
An unfinished detail chart must be marked unavailable, not filled with
unlabeled synthetic history.

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
Use my selected blend: DPS-150 simplicity with IPS3608 colors and status
indicators. Use the two supplied reference descriptions and the selected
screen structure in the tracker; do not re-ask settled layout decisions.
Request the original images if they are not available in your session.
Create an original 800x480 wireframe/spec in the existing screen tracker:
side-by-side CH1 +5 V / CH2 +3.3 V cards, each with a vertical stack of
large yellow V / cyan-blue A / neutral W; no Energy or Input panels.
Tap each card to open its channel detail: readings left, own V/I graph
right, clear channel identity and Back to Main. Include
read-only nominal voltages, confirmed I LIMIT, per-channel status,
one shared OUTPUT control, persistent link/demo state, touch navigation.
Specify the selected Iset popup: slider first, tap its numeric field for
a keypad; both edit one draft. Keypad completion returns to the popup;
only Apply sends ILIM, Cancel discards unsent edits. Include channel/units,
slider step/decimal precision, validation and pending/error examples.
Specify dimensions/fonts/spacing, >=44x44 px targets, edit/pending/error
states, and live/off/stale/demo/fault/unknown examples. No voltage editor,
manual CV/CC selector, new UDI commands, or firmware/hardware edits.
Get my approval before marking the design accepted. Open a documentation
PR referencing #76 and #65; state firmware unchanged/not bench-tested.
Stop after the specification PR; do not start LVGL implementation.
```

**Session 2: regulation/status evidence (#77)**

```text
Work on firmware/fixed-rail-regulation-status. Read the project/workflow/
firmware plan, screen tracker, DISPLAY_INTERFACE_STANDARD,
STM32_BLUEPILL_PIN_TABLE, and relevant hardware change trackers.
Read #77, #14, #62, and #73. Trace routed ISET/OCP/control paths and
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

**Session 3: dashboard (#78)**

```text
Work on display/fnirsi-dual-rail-dashboard after #76's spec PR merges.
Read the project/workflow/firmware plan and display tracker, then #78 and
the accepted spec. Review #71's disposition before editing shared code.
Implement the approved LVGL 8.3 layout in crowpanel-43-bringup, reusing
theme/helpers and existing telemetry/UDI. No STM32/protocol/hardware edits.
Main has CH1 left / CH2 right with large vertical V/A/W stacks and no
Energy or Input panels. Tap each card to open the selected-channel detail
shell: readings left, existing selected-channel V/I chart right, visible
Back to Main. Bind both traces to the selected rail, preserve its history
identity, and mark missing chart data explicitly. Expanded graph controls
and history behavior stay in D5; do not invent faster telemetry.
Show both rails' measured V/I/P, read-only nominal voltages, confirmed
GET ILIM CH1/CH2 values, and individual status. Redraw setpoints even if
measurements have not changed. Preserve shared OUTPUT semantics and label
that action as affecting both rails even on a single-channel detail view.
Never guess CC from missing CV bits/current thresholds: use #77 evidence
or explicit unknown/limit wording. Distinguish live/stale/demo/off/pending/
fault states; preserve ACK/ERR handling. All navigation is touch-only.
Build with pio run -d crowpanel-43-bringup -e crowpanel43 (using local
PlatformIO executable if not on PATH). Request my bench photos/logs for
the acceptance states; do not fabricate evidence. Update the tracker and
affected firmware inventory. Open one Bucket 4 PR referencing #78/#65;
state exact bench-tested status. Stop before Setup/Graph feature expansion.
```

**Session 4: current-limit editor (#74 / #75)**

```text
Work on display/touch-current-limit-editor after reconciling #71 and the
approved shared layout. Read #74/#75, the screen tracker, and actual
merged D4 command/ACK/error state machine before changing it.
Implement the user-selected Iset interaction on channel detail: tap Iset
to open a modal with that channel's confirmed limit, slider, numeric field,
Apply and Cancel. Drag edits a draft only; tap the numeric field to open
a decimal numeric keypad with backspace. Keypad completion validates and
updates the same draft/slider without sending; keypad dismissal preserves
the previous draft. Apply alone sends ILIM; Cancel discards unsent edits.
Follow the full popup contract in this tracker, including pending writes,
uncertain-timeout readback, visible errors, and modal touch isolation.
Reuse existing shared OUTPUT handling without changing its semantics.
Highlight the active channel/field and adapt action labels by state.
Use host-supported ILIM validation (currently CH1 0..3000 mA, CH2
0..2000 mA); these are protocol ranges, not proof of safe bench ratings.
No editable voltage, manual CV/CC, recovery-mode policy, CH3 controls,
independent output toggles, or new UDI commands. Keep confirmed values
separate from drafts; show send failure/ERR/timeout, refresh host values,
and prevent duplicate pending writes. Do not auto-enable outputs.
Build crowpanel43; exercise slider/keypad draft parity, channel isolation,
decimal/backspace/range validation, Apply/Cancel, and success/ERR/timeout/
stale paths. Verify no write before Apply, no duplicate pending writes,
and no implicit output enable. Request bench evidence. Update the tracker
and affected inventory, then open one Bucket 4 PR referencing #74/#75/#65.
```

### Review/evidence checklist for this UI effort

- [x] Mockup approved as the baseline visual appearance and screen flow (#76,
  PR #80 comment, 2026-10-02). Not bench validation; behavior questions open.
- [x] Reference compositions supplied; side-by-side overview and channel-detail
  navigation selected by the user. Mockup geometry/fonts are the approved
  baseline; LVGL clipping must still be re-checked on the panel (#78).
- [ ] Main has two large V/A/W stacks, with no Energy or Input panels.
  (Implemented, `crowpanel43` build passes; physical-panel photo pending.)
- [ ] Each card opens its own detail view; both V/I traces use that channel,
  and Back to Main works by touch. (Implemented and build-verified; touch and
  per-rail traces not yet exercised on hardware.)
- [ ] Iset opens the slider popup; tapping its numeric field opens the
  keypad, with one synchronized draft and no command before Apply.
- [ ] Cancel preserves the confirmed value; pending/error/timeout handling
  never promises an unsent rollback or unconfirmed success.
- [ ] Hardware CC/limit/trip behavior and status validity documented (#77).
- [ ] Main and current editor usable by touch without prior LVGL knowledge.
- [ ] Both confirmed limits refresh independently of measured V/I changes.
  (Implemented via a per-loop refresh; limit-only update not yet seen on
  hardware.)
- [ ] No misleading live/demo, healthy/fault, off/CV, or unknown/CC states.
  (Status wording implemented without asserting CC; photo matrix pending.)
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
  overlapping work. #74/#75 own touch/edit clarity, and #77 gates proven
  CV/CC semantics. D3's navigation follow-up is included in #78.
- 2026-10-02: #79 merged by user decision. Opened the #76 visual-spec branch
  with a standalone simulated mockup under `mockups/` and the proposed spec
  above. Awaiting user visual approval; design not marked approved, no LVGL
  work started. #71 remains open and unmerged; its scope was read but not
  reused. Issue-number correction: the merged #79 text had #77/#78 swapped;
  per the GitHub issue titles #77 is regulation/CV-CC validation and #78 is
  the dashboard, and the tracker and firmware plan now say so.
- 2026-10-02: User approved the #80 mockup as the baseline visual appearance
  and screen flow for the first LVGL implementation (#78). Recorded here;
  bench validation and the unresolved behavior questions (zero-limit
  semantics, slider step, output-on confirmation, status validity) are NOT
  approved or resolved. Iset editing stays a separate phase after #71
  reconciliation; WiFi/NTP remains later scope.
- 2026-10-02: #80 merged by user authorization. Started
  `display/fnirsi-dual-rail-dashboard` from updated `main`; PR #71 reviewed
  (draft, unmerged, touches Setup and Main ILIM in the same `main.cpp`) and not
  reused. First LVGL dashboard (Main overview + channel detail) implemented
  and `crowpanel43` build verified; not uploaded, not bench-tested. The
  Iset slider/keypad editor is intentionally not started; it waits for the
  user's physical-panel review and #71 reconciliation.
