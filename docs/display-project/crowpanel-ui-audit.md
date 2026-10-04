# CrowPanel UI Audit (D1 deliverable)

> Audited from `crowpanel-43-bringup/src/main.cpp` (2026-09-28), bench-tested
> on the physical CrowPanel across two flash/photo rounds. Screenshots live
> under `docs/display-project/screenshots/audit/`. No behavior changes are
> included; only layout-only fixes (padding/alignment/font sizing/chip
> colors, and two glyph/overlap bugs found on hardware) were applied.
>
> Update (D2): the Main fault-row overlap fix was re-photographed and
> confirmed on hardware. Nav-shell additions (Main OUTPUT badge → Setup,
> Graph ↔ Settings) are also confirmed on hardware. No Settings screenshot
> has been captured yet — deferred, not a blocker.


## Screen inventory

| Screen | `create_*` fn | LOC range | Screenshot | Overall verdict |
|--------|---------------|-----------|------------|-----------------|
| Splash | `create_splash_screen` | 1162–1195 | `screenshots/audit/splash.jpeg` | 🎨 minimal, no interactive controls, fine as-is |
| Setup | `create_setup_screen` | 1198–1317 | `screenshots/audit/setup.jpeg` (before: `setup-before-fix.jpeg`) | ✅ fully wired to UDI; layout fixed and confirmed on hardware |
| Main | `create_main_screen` | 1474–1755 | `screenshots/audit/main.jpeg` (pre-dates final fault-row overlap fix — see note above) | 🔧 missing nav to Setup/Settings; output state is read-only display |
| Graph | `create_graph_screen` | 1756–1897 | `screenshots/audit/graph.jpeg` (before: `graph-before-fix.jpeg`) | 🔧 no window/pause/clear controls yet (charts are read-only); layout fixed and confirmed on hardware |
| Settings | `create_settings_screen` | 1320–1473 | not yet captured | 🔧 submenu actions mutate local RAM state only, no persistence or UDI calls |

## Per-screen audit

### Splash

**Layout issues (🎨 fix in D1):**
- [x] None found — hero card, title, subtitle, and hint label are centered and legible at 800×480.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| (none — timed screen only) | — | n/a | Auto-advances to Setup after `kSplashDurationMs` (1800 ms) | Same, acceptable | — |

**Style constants used:** `lv_font_montserrat_48/20/16`, `UiTheme::kBg/kPanel/kTextPrimary/kTextMuted/kAccentV`, 22px card radius.

**Recommended constants (D2):** none — this screen is simple enough to leave as the pattern reference for hero-card styling.

---

### Setup

**Layout issues (🎨 fix in D1):**
- [x] `btn_edit` ("Edit / Apply") label did not reliably fit the button width — widened button 140→156px, confirmed on hardware (`setup-before-fix.jpeg` → `setup.jpeg`).
- [x] **Bench-found bug:** the `► … ◄` arrow glyphs in `lbl_setup_value` are not present in `lv_font_montserrat_48` and rendered as tofu boxes on real hardware. Replaced with plain ASCII `< … >`. Confirmed fixed in `setup.jpeg`.
- [x] Footer hint label used a smaller font (`montserrat_12`) than the equivalent footer treatment on Settings — left as-is (defer unification to D2 nav shell) since this is a font-hierarchy decision, not a bug.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `btn_prev` ("Prev") | `setup_prev_btn_event_cb` → `handleSetupEncoderRotate(-1)` | ✅ yes | Moves field selection backward (Output / CH1 limit / CH2 limit) | Same — correct | — |
| `btn_next` ("Next") | `setup_next_btn_event_cb` → `handleSetupEncoderRotate(1)` | ✅ yes | Moves field selection forward | Same — correct | — |
| `btn_edit` ("Edit / Apply") | `setup_edit_btn_event_cb` → `handleSetupEncoderPress` | ✅ yes | First press enters edit mode; second press sends `CMD:OUTPUT ON/OFF` or `CMD:ILIM CHx <mA>` via `disp_link_slave::sendCommand` (UDI-conformant) | Same — correct, no changes needed | — |
| `btn_done` ("Done") | `setup_done_btn_event_cb` → `handleSetupEncoderLongPress` | ✅ yes | Marks setup done, jumps to Main screen | Same — correct | — |
| Field value edit (rotate while editing) | `handleSetupEncoderRotate` | ✅ yes | Adjusts `ch1_limit_mA`/`ch2_limit_mA` by `kSetupStep_mA`, or toggles `output_enabled` | No validation against `kSetupCh1LimitMax_mA`/`kSetupCh2LimitMax_mA` visible at the touch-button layer (only the physical encoder path clamps) — confirm clamp applies to button path too | D4 |

**Style constants used:** `lv_font_montserrat_48` (value), `_20` (title/param), `_16` (list), `_12` (hint), `UiTheme::kAccentI/kAccentOk/kPanelSoft`, 8px button radius, 12px panel radius.

**Recommended constants (D2):** standardize button radius to 8px (Setup/Settings agree) vs 9–14px used elsewhere; hoist as `kUiButtonRadius`/`kUiPanelRadius`.

---

### Main

**Layout issues (🎨 fix in D1):**
- [x] **Bench-found bug:** status bar labels (`LINK`, `SEQ`, `CV`, `UP`) overlapped each other at their original fixed x-offsets (150/270/355/414) — real label widths (e.g. "WORKSTATION PSU") were wider than assumed. Re-spaced to 210/330/415/474. Confirmed fixed in `main.jpeg`.
- [x] **Bench-found bug:** in the CH1/CH2 panels, `lbl_main_ch1_power`/`lbl_main_ch2_power` ("P …W TEMP …C MODE …") shared the same y-position as the voltage bar drawn after them, hiding the label text under the bar and producing a visible strikethrough through the current-row text. Moved current label to a smaller font (`montserrat_20`→`16`) and re-flowed current/power/bar to non-overlapping rows (110/136/bar). Confirmed fixed in `main.jpeg`.
- [x] Removed the `4.5V/5.0V/5.5V` and `3.0V/3.3V/3.6V` bar scale-tick labels on both panels — the panel is only 182px tall and the full stack (header + big value + current + power + bar + ticks) does not fit without overlap; the tick values duplicate the already-visible SET voltage, so dropping them was the lowest-risk way to reclaim room (no info lost).
- [x] **Bench-found bug:** `panel_i` (CH2) and `panel_meta` (SET/STATUS) both extended 14px below the top of the bottom `fault_row`, overlapping it. Moved `panel_i` up (270→254) and shortened `panel_meta` (384→366px) so both clear the fault row with a small margin. **Fix is build-verified but not yet re-photographed** — see note at top of doc.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `create_nav_btn(status, "Graphs", ...)` | `nav_btn_event_cb` | ✅ yes | Navigates to Graph screen | Same — correct | — |
| Output badge (`lbl_status_output`, "OUTPUT --") | `nav_btn_event_cb` (D2) | ✅ yes (D2) | Badge is now clickable and navigates to Setup screen (top status bar had no free space left for a dedicated "Setup" button — see D2 PR) | Same — closes the Main→Setup nav gap | Superseded by #78: badge removed; Setup is reached from the Main bottom nav |
| (missing) nav to Settings screen | — | ✅ yes (#78 dashboard, build-verified; bench pending) | Main now has a bottom nav (Main/Setup/Graph/Settings); the old status bar and OUTPUT badge are gone | Same — closes the D2 follow-up | #78 |
| `chip_main_run` (replaced) | `main_output_toggle_event_cb` (D3, reworked in #78) | ✅ yes (#78, build-verified; bench pending) | The shared OUTPUT button in the top bar of Main and Detail sends the same `CMD:OUTPUT ON`/`OFF`, now disabled unless the link is live with an extended frame, and shows send failure / ERR / missing ACK. Output "on" is CH1 or CH2 enabled bit (was CH1 only) | Same | #78 |

**Style constants used:** `lv_font_montserrat_16/14/12`, `UiTheme::kAccentV/kAccentI/kAccentWarn/kAccentOk`, state chips via `create_state_chip`.

**Recommended constants (D2):** unify per-screen top status bar into a single shared "top bar" component (title + link/seq/mode/uptime cluster + nav buttons) referenced by all 4 non-splash screens — currently each `create_*_screen` duplicates this construction. Main's status bar is at capacity (labels + OUTPUT badge + one nav button fully use the available width) — a real fix requires this shared component, not more ad hoc buttons.

---

### Graph

**Layout issues (🎨 fix in D1):**
- [x] **Bench-found bug:** `lbl_window` ("win Ns V x..y I x..y") duplicated the range text already shown by the adjacent `lbl_graph_window_v`/`lbl_graph_window_i` labels and visually collided with them at the bottom of the screen. Shortened `lbl_window` to just "win Ns" since the V/I ranges are already shown by the two dedicated labels. Confirmed fixed in `graph.jpeg`.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `create_nav_btn(status, "Main", ...)` | `nav_btn_event_cb` | ✅ yes | Navigates to Main screen | Same — correct | — |
| `create_nav_btn(status, "Settings", ...)` (added D2) | `nav_btn_event_cb` | ✅ yes (D2) | Navigates to Settings screen | Same — closes the Graph↔Settings nav gap | — |
| `chart_v` / `chart_i` (trend charts) | none (data-driven only) | ❌ no touch controls | Read-only circular buffer chart, no window/pause/zoom | Add window selector (30 s/5 min/30 min), pause/resume, clear | D5 |
| Per-channel visibility | — | ❌ not present | Both channel traces always shown | Add per-channel show/hide toggle | D5 |

**Style constants used:** `lv_font_montserrat_16/12`, chart range `LV_CHART_AXIS_PRIMARY_Y` fixed at 9000–15000 (V) and -500–3000 (I) — not adaptive/autoscale.

**Recommended constants (D2/D5):** chart axis ranges are magic numbers duplicated from Main screen scaling assumptions; hoist to named constants and consider autoscale in D5.

---

### Settings

**Layout issues (🎨 fix in D1):**
- [x] Action row buttons (`action_primary`/`action_secondary`/`action_refresh`) — confirmed consistent 8px radius and spacing matching Setup screen buttons after the Setup button-width fix above.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `create_nav_btn(header, "Main", ...)` | `nav_btn_event_cb` | ✅ yes | Navigates to Main screen | Same — correct | — |
| `create_nav_btn(header, "Graph", ...)` (added D2) | `nav_btn_event_cb` | ✅ yes (D2) | Navigates to Graph screen | Same — closes the Settings↔Graph nav gap | — |
| `btn_settings_system` | `settings_system_btn_event_cb` → `selectSettingsMenu(System)` | ✅ yes | Switches submenu selection | Same — correct | — |
| `btn_settings_dataset` | `settings_dataset_btn_event_cb` → `selectSettingsMenu(DataSet)` | ✅ yes | Switches submenu selection | Same — correct | — |
| `btn_settings_about` | `settings_about_btn_event_cb` → `selectSettingsMenu(About)` | ✅ yes | Switches submenu selection | Same — correct | — |
| `action_primary` (System: "Value −" / Edit) | `settings_action_primary_event_cb` → `applySettingsPrimaryAction` | ⚠️ wired but local-only | Mutates `settings_state.brightness_pct`/`volume_pct`/`theme_dark`/`language_index` in RAM only — no backlight PWM call, no NVS/flash persistence, no UDI command sent | Brightness should drive actual display backlight; values should persist across reboot | D6 |
| `action_secondary` (System: "Value +" / Next Field) | `settings_action_secondary_event_cb` | ⚠️ wired but local-only | Same as above | Same as above | D6 |
| `action_refresh` ("Defaults"/"Apply Group"/"Refresh") | `settings_action_refresh_event_cb` → `applySettingsRefreshAction` | ⚠️ wired but local-only | System: resets in-RAM defaults only. DataSet: only sets a hint string, no actual save/load/reset-cal logic. About: no-op (labels are computed fresh each redraw) | Wire DataSet save/load/reset-cal to real calibration storage; About should show live UDI stats already available in `DisplayTelemetry` | D6 |
| DataSet group `-`/`+`/apply | same 3 action buttons, DataSet context | ⚠️ wired but local-only | `settings_state.dataset_group` incremented/decremented, "staged for preset usage" only | Should apply/save a real preset the host can also see | D6 |

**Style constants used:** `lv_font_montserrat_20/16/14/12`, `UiTheme::kPanel/kPanelSoft/kAccentOk`, menu button 8px radius.

**Recommended constants (D2):** the 3-tab submenu selector pattern (`btn_settings_system/dataset/about`) is a reusable "vertical tab list" component; worth extracting for reuse if other screens need multi-pane navigation later.

## Global findings

- **Repeated magic numbers to hoist into a shared style header:** button radius (8/9/12/14px used inconsistently), panel radius (12/16px), status-bar height (44/50/60px), and border color/width (`UiTheme::kBorder`, 1px) are re-declared per screen instead of shared helpers.
- **Inconsistent chip color usage:** `create_state_chip` colors are passed as raw hex literals per call site (e.g. `0x193425`, `0x24364A`) rather than named `UiTheme` members like the rest of the palette — should be added to `UiTheme` in D2.
- **Touch target sizes < 44 px:** all buttons audited are ≥32px tall (nav buttons) or ≥40px tall (Setup/Settings action buttons); nav buttons at 32px are borderline for gloved/imprecise touch — flag for D2 review, not a D1 blocker.
- **Font-size hierarchy proposal:** current usage is ad hoc (48/20/16/14/12 mixed per screen without a clear rule). Proposed hierarchy for D2: 48 = hero/primary value, 20 = screen title, 16 = section label/status text, 14 = body/data text, 12 = hint/footnote.
- **Navigation gap (D2 update):** Main originally had no direct nav to Setup or Settings (only Graph). D2 closed this by making the Main OUTPUT badge tap-navigate to Setup, and adding mutual Graph↔Settings nav buttons. Main still has no direct button to Settings (status bar is at capacity) — reachable via Main→Graph→Settings for now. A real shared top-bar/bottom-nav component (still recommended for D2 follow-up) would resolve this properly.

## D1 exit criteria

- [x] Every screen has a filled-in per-screen block
- [x] Every button is either ✅ wired-correctly or has a defer-to bucket
- [x] Layout-only fixes committed and visible in fresh screenshots (Splash/Setup/Graph/Main confirmed on hardware)
- [x] `crowpanel-screens-tracker.md` updated: D1 row → 🟢 + PR link (merged as #66)
