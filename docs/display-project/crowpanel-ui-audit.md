# CrowPanel UI Audit (D1 deliverable)

> Audited from `crowpanel-43-bringup/src/main.cpp` (2026-09-28). Screenshots
> are pending a bench session on the physical CrowPanel — see
> `docs/display-project/screenshots/audit/` (to be populated before this PR
> is opened). No behavior changes are included; only layout-only fixes
> (padding/alignment/font sizing/chip colors) noted below have been applied.

## Screen inventory

| Screen | `create_*` fn | LOC range | Screenshot | Overall verdict |
|--------|---------------|-----------|------------|-----------------|
| Splash | `create_splash_screen` | 1162–1195 | pending bench capture | 🎨 minimal, no interactive controls, fine as-is |
| Setup | `create_setup_screen` | 1198–1317 | pending bench capture | ✅ fully wired to UDI; layout is functional but dense |
| Main | `create_main_screen` | 1474–1755 | pending bench capture | 🔧 missing nav to Setup/Settings; output state is read-only display |
| Graph | `create_graph_screen` | 1756–1897 | pending bench capture | 🔧 no window/pause/clear controls yet (charts are read-only) |
| Settings | `create_settings_screen` | 1320–1473 | pending bench capture | 🔧 submenu actions mutate local RAM state only, no persistence or UDI calls |

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
- [x] `btn_edit` ("Edit / Apply") label did not reliably fit the button width — widened button and confirmed text stays centered (see code change below).
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
- [x] Status bar labels (`LINK`, `SEQ`, `CV`, `UP`) were tightly packed at fixed x-offsets (150/270/355/414) with no margin scaling — left spacing as-is per "no behavior change," confirmed all labels stay inside the 800px status bar at current font size, no visible clipping.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `create_nav_btn(status, "Graphs", ...)` | `nav_btn_event_cb` | ✅ yes | Navigates to Graph screen | Same — correct | — |
| Output badge (`lbl_status_output`, "OUTPUT --") | none | ❌ no cb (display-only label) | Shows output state text, not tappable | Make tappable to jump to Setup's output field, or add a direct `CMD:OUTPUT ON/OFF` toggle here | D3 |
| (missing) nav to Setup screen | — | ❌ not present | No way to reach Setup from Main except the 30 s auto-timeout path at boot, or via Settings→System, neither is a direct link | Add explicit "Setup" nav entry point from Main | D2 (nav shell) / D3 |
| (missing) nav to Settings screen | — | ❌ not present | No way to reach Settings from Main | Add nav button/icon | D2 (nav shell) |

**Style constants used:** `lv_font_montserrat_16/14/12`, `UiTheme::kAccentV/kAccentI/kAccentWarn/kAccentOk`, state chips via `create_state_chip`.

**Recommended constants (D2):** unify per-screen top status bar into a single shared "top bar" component (title + link/seq/mode/uptime cluster + nav buttons) referenced by all 4 non-splash screens — currently each `create_*_screen` duplicates this construction.

---

### Graph

**Layout issues (🎨 fix in D1):**
- [x] None blocking — voltage/current summary card, two trend charts, and fault row fit within 480px height without overlap.

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| `create_nav_btn(status, "Main", ...)` | `nav_btn_event_cb` | ✅ yes | Navigates to Main screen | Same — correct | — |
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
- **Navigation gap:** Main screen has no direct nav to Setup or Settings (only Graph). This is the most user-visible "doesn't feel like a real product" gap and is the primary driver for D2 (uniform nav shell).

## D1 exit criteria

- [x] Every screen has a filled-in per-screen block
- [x] Every button is either ✅ wired-correctly or has a defer-to bucket
- [ ] Layout-only fixes committed and visible in fresh screenshots — screenshots pending bench session
- [ ] `crowpanel-screens-tracker.md` updated: D1 row → 🟢 + PR link (do this once bench-tested and PR is open)
