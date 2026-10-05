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
| D5a | `phil-cia-crowpanel-strip-recorder-core` | Strip-recorder re-skin of the Graph screen: bezel, 4-pen trend (CH1 V/I, CH2 V/I), PSRAM ring buffer (4500 samples). Supersedes the prior window-selector-only D5 scope. See `strip-recorder-graph-spec.md` (Rev 2). | [#89](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/89) | 🟢 | part of #26 |
| D5b | `phil-cia-crowpanel-strip-recorder-feedrate` | Feed-rate selector (10 s–1 hr/div), pause/resume, per-pen show/hide. Time-based resample of the PSRAM ring. Bench-reviewed by the user (no changes requested); see the 2026-10-05 note. | [#91](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/91) | ✅ merged 2026-10-05 (squash `e1624a9`); user panel review only, checks not itemised | part of #26 |
| D5c | `phil-cia-crowpanel-strip-recorder-microview` | Per-channel micro-view (one `Micro` screen, CH1/CH2 switch), entered from a new Detail `MICRO` button or the Graph header; Back returns to the opening screen. One tall V+I plot from the selected channel only: V upper band / left axis, I lower band / right axis, unit-labelled axes, independent-scale caption, autoscale (100 mV / 10 mA snap, 2-step minimum span, grow-at-once / shrink-at-2x hysteresis, never clips). Link chip + plot note for LIVE/STALE/DEMO/UNKNOWN/no data; pause-rollover note. Feed rate and pause shared with Graph; pen show/hide does not apply. Uptime axis (time of day waits for #82). Spec: strip-recorder-graph-spec.md section 5a. | [#94](https://github.com/Phil-CIA/Development-Station-Power-Supply/pull/94) | 🟡 (builds; flashed and user-reviewed as good enough for this iteration; individual checks not itemised; PR not merged) | part of #26 |
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

**First flash findings (2026-10-02, panel on COM12; serial only, panel visuals
not yet reviewed):**

- The first build boot-looped (`LoadProhibited` in `lv_label_create`): the
  default 48 KB LVGL heap ran out while creating the last screen. Fixed by
  `-DLV_MEM_SIZE=131072` in `crowpanel-43-bringup/platformio.ini`; at boot the
  firmware now logs `lvgl heap: total=131072 free=76060 used=42%`. `crowpanel43`
  RAM use rose from about 116 KB to 198 KB.
- With the HAT attached, the console shares UART0 with the host link, so the
  display's own `udi ack/err/evt:` echo lines drew `ERR:FORMAT need CMD:` replies
  from the STM32 and re-triggered themselves (about 90 lines/s) and polluted
  Setup's host-error text. The echo is now skipped while the console is the
  host link (`telemetryOnConsoleSerial()`); after the fix the boot log is quiet
  and shows only the expected `CMD:GET OUTPUT` / `GET ILIM CH1` / `GET ILIM CH2`.
  Consequence: in UART0 mode, ACK/ERR evidence must come from the STM32 side or
  the UART1 transport, not the display console.

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

**Panel review round 1 (2026-10-02, from the user's Main and Detail photos;
build-verified, NOT yet re-flashed or re-photographed):**

- Naming: `CH1` / `FIXED +5 V` merged into `+5V Supply` and `+3.3V Supply`
  on Main cards, the Detail chip and trend title, the top-bar Detail chip and
  the Graph screen value labels. Not renamed (by scope): Setup labels (#71 owns
  Setup; draft, conflicting) and the Graph fault row (D5).
- CH1 color: pink/purple `0xFF79C6` replaced by yellow-green `0x84D82E` (tint
  `0x28361C`). Chosen to stay apart from CH2 teal `0x2DD4BF`, the yellow
  voltage text and the LIVE/OK green. Easy to change in `UiTheme::kCh1`.
- OUTPUT widget: wider (240 px), sub-line `BOTH CHANNELS` /
  `BOTH - tap to turn ON|OFF`. OFF is one tap. ON now opens a confirmation
  dialog (`TURN OUTPUT ON?`, CANCEL / TURN ON, 10 s timeout) over a touch-blocking
  scrim; it closes with a notice if the link stops being live, closes silently
  if the output is already on or a command is pending, and re-checks the link
  before sending. This answers open question 3 for the dashboard (modal, per user).
  The protocol only has the shared `OUTPUT ON|OFF`; per-channel control is a
  protocol change and is NOT part of this PR.
- Bottom nav (Main / Setup / Graph / Settings) is now a shared builder used on
  Main and Detail (Detail highlights Main; Back stays). To fit it, Detail panels
  were compacted (276 px high, chart plot 348 x 176, footer 56 px, Back button
  110 px). Setup / Graph / Settings keep their top-right nav buttons for now:
  Setup waits for #71, Graph/Settings for D5/D6.
- Detail trend panel is tappable and opens the Graph screen (hint
  `Tap for Graphs >`). The Graph screen has no per-channel selection yet.
- Clock: `CLOCK NOT SET` replaced by `UPTIME H:MM:SS` (CrowPanel `millis()`),
  so it is never read as time of day. WiFi setup and network time are tracked
  in #82 and are not part of this PR.
- Stale data finding (NOT changed here): the STM32 loop publishes telemetry
  every 5000 ms (`stm32-bluepill-bringup/src/main.cpp`, `now - lastMs >= 5000`)
  while the display marks STALE after `kLinkStaleMs = 1500` ms, so the panel
  shows STALE between every frame. The Detail caption in the user's photo
  (`last 594 s, 120 samples`) is consistent with about 4.95 s per sample. The user
  wants a 500-1000 ms exchange rate, to be settled together with the Graph
  chart-speed work; that is an STM32/protocol change (out of scope for #81).
  The display stale threshold must then be derived from the chosen cadence.

**Panel review round 1 evidence (pending until re-flashed and photographed):**
Main / CH1 detail / CH2 detail clipping and touch navigation, both traces,
graph-tap navigation, ON confirmation (cancel, timeout, confirm), stale/demo/
unknown/off appearance, confirmed-limit refresh. No upload has been done for
this round. The host-side text-fit check above predates round 1 and was not
re-run for the new names, dialog and compacted Detail layout.

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

**Session 5: strip-recorder core re-skin + CH2 data model (D5a)**

Recommended model: **Claude Sonnet 5.5**, reasoning effort **medium**. This
is mostly mechanical LVGL/data-structure work in one large existing file;
a mid-effort coding-focused model is sufficient. Use **high** effort (or
Claude Opus 5.5) only if the PSRAM buffer-sizing math or chart-series
plumbing needs deeper review.

```text
Work in Development-Station-Power-Supply on
phil-cia-crowpanel-strip-recorder-core, branched off main (not off any
other feature branch). Read README.md, docs/SYSTEM_DEVELOPMENT_WORKFLOW.md,
docs/display-project/README.md, this tracker, and
docs/display-project/strip-recorder-graph-spec.md (Rev 2) in full before
editing. Read issue #26. Issue #84 (CSV export) is separate and not part of
this session's scope — do not touch LOG_DUMP_CSV beyond what D5a's data
model change requires it to still compile.
Implement only bucket D5a from strip-recorder-graph-spec.md: recorder-bezel
re-skin of the Graph screen (header bar, group label, grid/time-axis
styling) in crowpanel-43-bringup/src/main.cpp. Extend TrendSample with
v2_mV/i2_mA for CH2, captured each tick from the existing
t.last_v3v3_mV/t.last_i3v3_mA telemetry fields (already read live for the
Main screen — do not add new UART/UDI messages). Move trend_buf from its
static SRAM array to a heap_caps_malloc(..., MALLOC_CAP_SPIRAM) allocation;
pick a concrete capacity sized for roughly 10-15 minutes at the current
sample rate, show your sizing math, and record the exact number chosen in
this tracker. Keep two stacked chart areas (voltage pair, current pair) per
the spec's decided layout; add a second lv_chart_series_t pair for CH2 to
both existing chart objects. Add a pen legend showing live value and color
swatch per channel for all 4 real channels.
Do not implement a feed-rate selector, pause/resume, or show/hide toggles —
those are D5b, a separate later PR. Do not add alarm markers, relay
functions, history scroll-back/pan, or bar gauges; these are permanently
out of scope per spec section 4, not deferred work to stub out.
Build with: pio run -d crowpanel-43-bringup -e crowpanel43 (use the local
PlatformIO executable if not on PATH). Bench-test on the physical CrowPanel:
confirm the CH2 trace renders against known reference values, confirm the
PSRAM allocation succeeds (check the serial boot log for any allocation
failure message), confirm the existing CH1 trace and Main-screen behavior
are unchanged. Update this tracker's D5a row to the bench-tested status
with evidence, record the final PSRAM buffer capacity chosen, and open one
PR referencing #26 and strip-recorder-graph-spec.md. Stop before D5b.
```

**Session 6: strip-recorder feed-rate control (D5b)**

Recommended model: **Claude Sonnet 5.5**, reasoning effort **high** (or
**GPT-5.3-Codex** as an alternative). The resample-on-feed-rate-change logic
and honest-blank-when-buffer-is-short behavior are more algorithmically
fiddly than D5a and benefit from the extra reasoning effort.

```text
Work on phil-cia-crowpanel-strip-recorder-feedrate, branched off main, only
after D5a's PR has merged. Read this tracker and
docs/display-project/strip-recorder-graph-spec.md (Rev 2), then review the
actual merged D5a diff before changing it — do not assume unmerged code or
a different buffer capacity than what D5a's PR recorded in this tracker.
Implement only bucket D5b: a feed-rate selector offering the full list
(10 s/30 s/1 min/5 min/15 min/1 hr per div) per the spec's chosen Option B.
On every feed-rate change, immediately resample the PSRAM ring buffer
(written by D5a) into the chart's point array rather than only continuing
circular live updates. When the selected window exceeds real buffer depth
(15 min/div, 1 hr/div with a short buffer), render the unfilled portion of
the chart as visibly blank — never fabricate or extrapolate data to fill it.
Add pause/resume and return-to-live controls, and a simple independent
show/hide toggle per channel (CH1 V, CH1 I, CH2 V, CH2 I) — no further pen
framework, no per-pen color/scale editing.
Do not add alarm markers, relay functions, history scroll-back/pan, or bar
gauges; these remain out of scope per spec section 4.
Build with: pio run -d crowpanel-43-bringup -e crowpanel43 (use the local
PlatformIO executable if not on PATH). Bench-test on the physical CrowPanel:
verify each feed-rate setting visually scales the trend correctly, verify
pause freezes the displayed trace without losing buffered samples, verify
resume reconnects to live data, verify show/hide toggles the correct series
without disturbing the others, and verify the blank-when-short-buffer
behavior for the slowest settings. Update this tracker's D5b row with bench
evidence. Open one PR referencing #26 and strip-recorder-graph-spec.md.
Close #26 only if this PR plus D5a together satisfy every acceptance
criterion in strip-recorder-graph-spec.md section 7 — otherwise leave #26
open and say what remains.
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
  and `crowpanel43` build verified (PR #81). Flashed to the panel with the
  user's permission; see the first-flash findings in the #78 section. The
  Iset slider/keypad editor is intentionally not started; it waits for the
  user's physical-panel review and #71 reconciliation.
- 2026-10-02 RESUME POINT (#78, PR #81, branch
  `display/fnirsi-dual-rail-dashboard`, head `4807589`, open, mergeable, not
  merged): firmware on the panel boots with no panic (serial only). NOT yet
  done: the user's visual review of the panel, and bench evidence (photos of
  Main/Detail in live/off/stale/demo/unknown states, touch navigation,
  per-rail traces, limit-only update after a Setup change, OUTPUT
  success/ERR/no-ACK). Next steps in order: (1) get the user's panel
  notes/photos and fix any clipping or layout bugs; (2) fill the PR's bench
  section from real evidence only; (3) user merges #81; (4) reconcile #71
  (draft; conflicts expected in Main and `update_telemetry_labels()`); (5)
  only then start the Iset slider/keypad editor (#74/#75), which must also
  settle the open questions on zero limit, slider step and output-on
  confirmation. Flash rules: guarded script only
  (`scripts/guarded-flash.ps1 -Target crowpanel -Action upload`, expects COM12
  / ESP32-S3 / MAC 80:B5:4E:E2:E4:08), set `PYTHONUTF8=1` and log to a file;
  ask before every upload. In UART0 mode the console is the host link, so do
  not add non-`CMD:` prints. Useful serial aids: `SCREEN DETAIL1|DETAIL2`,
  `DEMO ON|OFF` (console commands only work when the transport is not UART0).
- 2026-10-04 RESUME POINT (#78, PR #81, branch `display/fnirsi-dual-rail-dashboard`,
  head `5404611` plus this note; open, mergeable, not merged): panel review round 1
  (see above) was flashed by the user and reported verbally as "the screens look
  great". No photos, touch, graph-tap, ON-dialog, stale/demo/unknown, limit-refresh
  or OUTPUT command evidence was captured, so all of it stays PENDING. The user
  wants operational experience with the panel before continuing. Not started, by
  decision: Iset slider/keypad editor, STM32 telemetry cadence (user target
  500-1000 ms, to be settled with the Graph chart-speed work; the 5000 ms vs
  1500 ms stale mismatch is still in place), WiFi/network time (#82), bottom nav
  on Setup/Graph/Settings, #71 reconciliation. Flash notes: the guard's
  `read-mac` right after its `chip-id` probe failed intermittently; a 2 s wait
  with up to 3 retries in `scripts/guarded-flash.ps1` fixed it (local, uncommitted).
  One guarded upload then failed loading the esptool stub (`Checksum error`, no
  flash written); the user's successful flash used `upload_speed = 460800` in
  `crowpanel-43-bringup/platformio.ini` (local, uncommitted, not made by Copilot).
  Next steps: (1) collect operating notes/photos and fix any defects; (2) fill the
  PR's bench section from real evidence only; (3) user decides on committing the
  guard and `upload_speed` changes and on merging #81; (4) reconcile #71; (5) Iset
  editor (#74/#75).
- 2026-10-04 review checkpoint (#78, PR #81, no firmware change): user supplied
  panel photos and reported no layout/touch defects to fix; the only issue noted
  is the telemetry refresh rate (STM32 cadence, deferred to the next effort).
  Photo evidence reviewed by Copilot: CH2 (+3.3V Supply) detail only - LIVE,
  OUTPUT OFF, V/I/W readings, TRIP with "STM32 flags: OCP", FIXED +3.30 V,
  Iset LIMIT 1.500 A CONFIRMED, trend with limit trace, bottom nav; no clipped
  text. Observation, not changed: the readings panel's lower edge sits behind the
  FIXED/Iset cards (minor overlap). Main and CH1 detail photos could not be
  viewed in this session, so they stay PENDING. Still PENDING: touch navigation,
  graph tap, ON dialog, stale/demo/unknown, limit-refresh, OUTPUT success/ERR/no-ACK.
  No upload this session. Stopped at the panel-review checkpoint, before Iset editor work.
- 2026-10-04 END-OF-SESSION RESUME POINT / HANDOFF (#78, PR #81, branch
  `display/fnirsi-dual-rail-dashboard`; open, not merged): the guard `read-mac`
  retry (`scripts/guarded-flash.ps1`) and `upload_speed = 460800`
  (`crowpanel-43-bringup/platformio.ini`) are now committed with this note. No firmware
  change this session. State: user reports no layout/touch defects; wants
  operating time on the panel before further changes. Next agent: (1) wait for
  the user's operating notes/photos (Main and CH1 detail photos still unreviewed;
  they are in the user's OneDrive exchange folder) and fix only reported defects;
  optional minor overlap of the Detail readings panel behind the FIXED/Iset cards
  if the user approves; (2) next effort: STM32 telemetry cadence 500-1000 ms vs the
  1500 ms stale threshold (needs STM32/protocol scope approval); (3) fill the PR
  bench section from real evidence only; (4) user decides on merging #81; (5)
  reconcile draft #71 only after that; (6) Iset editor (#74/#75) last. Rules: ask
  before every upload and use `.\scripts\guarded-flash.ps1 -Target crowpanel -Action upload`
  (expects COM12 / ESP32-S3 / MAC 80:B5:4E:E2:E4:08, `PYTHONUTF8=1`, log to file); never
  write non-`CMD:` text to UART0; do not toggle real outputs without permission.
- 2026-10-04 RESUME POINT (D5a strip-recorder Graph, branch
  `phil-cia-crowpanel-strip-recorder-core`, off `main`; PR #89 open (refs #26, #83); plan doc is
  open PR #83 on `phil-cia-strip-recorder-graph-plan`, now including proposed D5c):
  - Done in `crowpanel-43-bringup/src/main.cpp`: trend ring buffer moved to PSRAM,
    capacity 4500 samples (28 B each = 126,000 B; sized for 15 min at the 5 Hz cap, but the
    STM32 really sends every 5 s so it holds about 6 h). `TrendSample` already carried CH2
    (`v3v3_mV`/`i3v3_mA`/`has_ch2`), so no new fields. Graph screen re-skinned: header bezel,
    4-pen legend with live values, stacked V (0-6 V) and I (0-4 A) charts with CH1+CH2 traces in
    SHIFT (right-to-left) mode, 3 horizontal + 5 vertical grid lines, 7 uptime `H:MM:SS` axis
    labels (real time of day waits for #82), fault row listing only active trips (`NO FAULTS`
    green, red per-trip list, explicit stale/unknown/demo/legacy text, no CC/CV claim, #77),
    inferred CV/CC/RUN status chips removed (right column left empty for D5b). Fixed a
    decimation/stats bug that used the saturating `trend_count`. `crowpanel43` build passes.
  - Bench: first build flashed (guarded, COM12) and photographed by the user; layout good, only
    height of the graph panels noted. The grid/time-axis and fault-row commits are build-verified
    only, NOT flashed. Console is the UDI link in UART0 mode, so no serial log evidence exists.
  - Observed: window spans about 20 min per screen at the 5 s STM32 cadence (axis showed
    `-1190s`); `OCP` flags were reported set on both channels in the photo (STM32 data, not
    investigated). User wants the 500-1000 ms cadence settled before D5b so feed rates match data.
  - Next, in order: (1) STM32 branch `phil-cia-stm32-telemetry-cadence` (local only, NOT pushed,
    2 commits: 500 ms telemetry timer decoupled from the 5 s heartbeat; `Uart` -> `HardwareSerial`
    build fix for core 2.12, #3) is already flashed to the Blue Pill via ST-Link; user to confirm on
    the panel that the link stays LIVE and the axis spans about 4 min, then OK push + PR;
    (2) user merges #89 and #83; (3) D5b only after D5a merges; (4) D5c after D5b.
  - Late 2026-10-04: CrowPanel upload at 460800 baud failed (bad data checksum), recovered by
    replugging USB and BOOT+RESET; `upload_speed` is now 115200 permanently. The latest D5a build
    IS flashed and was reviewed OK (grid, axis, fault row). STM32 flash is 98.1% of 64 KB.
  - Open: `OCP` flags on both channels come from the shared FAULT_CRITICAL_SUM path, which the
    STM32 code says is unrouted on Rev-C; not investigated.
  - Rules: ask before every upload and push; guarded script only (`PYTHONUTF8=1`, log to file);
    local PlatformIO at `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe`; read user
    feedback .docx from the OneDrive exchange folder only after the user closes it in Word.
- 2026-10-05 END-OF-SESSION RESUME POINT (D5a merged, D5b open; supersedes the 2026-10-04 D5a
  resume point above). Merged into `main` this session (all squashed): #83 (plan doc), #88
  (Detail panel overlap fix), #89 (D5a Graph), #90 (STM32 500 ms telemetry + `HardwareSerial`
  build fix + `ststm32@19.7.0` platform pin, because unpinned CI pulled core 3.0 where `Uart`
  exists). Still open elsewhere: #63, #61 (older bench-evidence docs; #61 conflicts), #71 (D4, draft).
  - D5b (branch `phil-cia-crowpanel-strip-recorder-feedrate`, `crowpanel-43-bringup/src/main.cpp`
    only): the chart is no longer fed incrementally; `refreshGraphChart()` resamples the PSRAM trend
    ring by time on every loop into per-pen static arrays (`lv_chart_set_ext_y_array`), bucketed on
    absolute time so points do not jitter. Feed button cycles 10 s / 30 s / 1 min / 5 min / 15 min /
    1 hr per division (default 30 s; window = 6 divisions); bucket is at least 1 s, so 10 s/div uses
    60 points. Empty buckets are blank (no synthetic history); the label reads `win <window>  hist
    <recorded span>`. Pause freezes the right edge (samples keep recording; RESUME returns to live;
    button turns orange). Tap a legend cell to hide/show that pen (dims, `[OFF]`). Demo and live
    samples are never mixed. Removed `kChartDecimation`/`appendCharts`. No history pan, alarms, or
    autoscale (cut by spec). `crowpanel43` builds; STM32 CI green.
  - Bench: flashed to the panel with the guarded script (COM12 / ESP32-S3 / MAC
    80:B5:4E:E2:E4:08). The user reviewed it and asked for no changes. No photos or logs were captured
    by Copilot and the individual checks (all six rates, pause/resume, pen hide, no gaps at 10 s/div)
    were not itemised, so treat those details as user-observed, not documented evidence.
  - Observed: at 500 ms telemetry the default 30 s/div window is 3 min; the previous fixed view
    (about 2 min) was 120 points x decimation 2 x 500 ms. 15 min and 1 hr/div can show at most
    about 37 min of history (4500 samples at 500 ms); the rest is blank by design.
  - ROOT CAUSE of repeated CrowPanel flash failures ("Checksum error" on the stub, "serial data
    stream stopped", 460800 "bad checksum"): the host link is UART0 (IO44/IO43), the same UART as the
    CH340K on COM12, so the Blue Pill's 500 ms telemetry corrupts the flash stream. Confirmed
    2026-10-05: with the Blue Pill powered off the unchanged guarded script passed first try. Not the
    cable. **Power the Blue Pill off (or hold NRST / unplug the UART link) before every CrowPanel
    upload, then reconnect.** Longer-term options: move the link to UART1 (`DISP_LINK_SLAVE_USE_UART0 =
    0`, needs wiring) or have the STM32 hold telemetry until the display speaks first.
  - Next, in order: (1) user merges the D5b PR (#91); (2) D5c micro-views (separate branch off `main`);
    (3) Setup/Settings/fault modal/self-test (D6-D8); (4) #71 reconciliation and the Iset editor
    (#74/#75); (5) WiFi time (#82), CSV export (#84, needs CH2 columns). Open from before: `OCP` flags on
    both channels come from the unrouted-on-Rev-C FAULT_CRITICAL_SUM path (not investigated); STM32 flash
    is 98.1% of 64 KB, so little room for more STM32 firmware.
  - Rules: ask before every upload and push; guarded script only (`PYTHONUTF8=1`, log to file); power
    the Blue Pill off first; never write non-`CMD:` text to UART0.
- 2026-10-05 SESSION 2 (D5c micro-view; adds to the resume point above; supersedes its "Next" list):
  #91 squash-merged as `e1624a9` (user-authorized); the D5c commits were rebased onto it. D5c is `main.cpp`
  plus docs only; design in strip-recorder-graph-spec.md section 5a.
  - Implemented: `UiScreen::Micro` (one screen, `micro_channel` 0/1) entered from a new Detail `MICRO`
    button or the Graph header CH1/CH2; Back returns to the opening screen (Detail is re-selected to the
    channel last viewed). V in the 55-92 % band / left axis, I in the 8-45 % band / right axis, unit-labelled
    ticks, headers and an independent-scale caption. Autoscale: 100 mV / 10 mA snap, 2-step minimum span,
    grow-at-once / shrink-below-half hysteresis, no clipping, held spans reset on channel/rate/demo change.
    Link chip plus plot-gap note for LIVE/STALE/DEMO/UNKNOWN/no data; paused-window rollover note. Shared with
    Graph: feed rate, pause, time axis (uptime), fault row. Pen show/hide does not apply to Micro. Detail
    footer was re-flowed to fit the button (Edit 208 -> 136 px wide); header buttons are 44 px tall.
  - Build: `pio run -d crowpanel-43-bringup -e crowpanel43` succeeds (flash 3.7 %).
  - Bench: flashed with the guarded script (COM12 / ESP32-S3 / MAC 80:B5:4E:E2:E4:08, Blue Pill off,
    log `crowpanel-43-bringup/upload-d5c-2026-10-05.log`, not committed). The user looked at the panel and
    judged it good enough for this iteration. No photos or serial logs were captured and the individual
    checks were NOT itemised, so these remain unrecorded: both channels' source, distinct scales, flat-data
    readability, scale stability, gaps, STALE/UNKNOWN/DEMO states, Graph controls still working, no
    clipping and 44x44 touch targets (incl. the narrowed Detail footer). The paused-window rollover note
    was not exercised.
  - D5c is NOT complete until its PR is merged; the acceptance checks above are open follow-ups, not passed.
    #26 and the broader screen issues stay open.
  - NEXT SESSION: (1) #93 UART0 handshake / quiet mode is the next objective; it needs its own design and
    firmware/flash-flow session (STM32 + `guarded-flash.ps1`). (2) Until it lands the workaround is: power
    the Blue Pill off (or hold NRST / unplug the UART link) before every CrowPanel upload and reconnect
    after; the guarded script probes every serial port with `esptool chip-id`; never write non-`CMD:` text
    to UART0. (3) STM32 flash headroom is tight (last reported 98.1 % of 64 KB), so #93 must be small or
    host-side. (4) #92 and #93 have identical titles and bodies, so #92 duplicates #93; propose closing
    one as a duplicate of the other once the user decides (neither closed). (5) After that: D6-D8, #71
    reconciliation and Iset editor (#74/#75), #82 time, #84 CSV.
- 2026-10-05 SESSION 3 (#93, branch `firmware/crowpanel-flash-quiet-mode`, NOT pushed, NO hardware touched):
  - Design (user-chosen, replaces the earlier lease-by-host proposal): the PC sends `QUIET <s>` to the STM32
    console on COM7 (HAT CH340 on USART1; DTR# is not connected, so opening it cannot reset the STM32) and the
    STM32 acknowledges with `ACK QUIET ON rem=.. up=..` before the script runs any esptool call. `guarded-flash.ps1`
    then renews the 15 s lease every 3 s from a background runspace while esptool/PlatformIO run, aborts the
    uploader if two renewals go unacknowledged, the STM32 uptime goes backwards (STM32 reset) or the lease may have
    lapsed, and sends `QUIET OFF` only after the uploader has exited. A lease that is never renewed expires on its
    own, so telemetry is never disabled permanently. The command exists only on the console, not on the UDI link.
  - STM32 (`main.cpp`): while quiet, `sendUdiAck/Err/Evt`, `publishTelemetry` and UDI RX are suppressed, USART3 is
    ended and PB10 is set to input (idle-high push-pull would contend with the CH340K if the nets are joined; that
    net topology is still UNCONFIRMED). The `usart3: ready` boot line on the UDI link was removed (non-CMD text).
    Resume re-announces fault state. Output/limit/fault/calibration code is untouched.
  - Build: `bluepill_f103c8` 65212 / 65536 B (99.5 %), +916 B vs 64296 B; 324 B headroom left. CrowPanel code is
    unchanged. `guarded-flash.ps1` parses cleanly; hardware-free failure paths checked (absent quiet port, quiet
    port = programming port, wrong target): all exit 2 before any esptool or serial traffic.
  - Script changes: crowpanel target requires an acknowledged quiet unless `-NoQuiet`; `-QuietPort` overrides COM7
    (identity is proved by the ACK, not the port number); with the MAC lock set, only the expected port is probed
    (other serial devices are no longer reset). Existing port/chip/MAC guards are unchanged.
  - OPERATOR FLOW: (1) ST-Link flash the STM32 first (it needs the new firmware). (2) Run the CrowPanel upload
    task with the Blue Pill powered and both cables connected. (3) If the script reports a quiet failure, nothing
    was assumed quiet: re-run; the CrowPanel may be in download mode, press RESET if it stays blank.
    FALLBACK (unchanged, until the lease is bench-proven): power off or isolate the Blue Pill and run with `-NoQuiet`.
  - NOT PROVEN: no upload, no bench test and no capture have been done. Pending: powered-Blue-Pill upload, repeated
    uploads, STALE-free telemetry afterwards, aborted-upload behaviour, CrowPanel-only / STM32-only / cold-boot
    resets, existing CMD/ACK/ERR/EVT behaviour, and output/limit state unchanged. Do not close #93 until then.
