# CrowPanel UI Audit (D1 deliverable — skeleton)

> **Status: skeleton.** The VSCode agent working D1 (`phil-cia-crowpanel-ui-audit`)
> fills this in from `crowpanel-43-bringup/src/main.cpp`. Do **not** change
> screen behavior in D1 — only layout constants (padding, alignment, font
> sizing, chip colors). Behavior fixes happen in D2–D8.

## How to fill this in

1. Flash the current `crowpanel-43-bringup/` to a real CrowPanel.
2. Take an 800×480 screenshot (or clear photo) of every screen and save under
   `docs/display-project/screenshots/audit/<screen>.png`.
3. For each screen, enumerate every `lv_obj_t*` button / toggle / clickable
   region in `main.cpp` and fill the tables below. Grep starting point:
   `create_splash_screen`, `create_setup_screen`, `create_main_screen`,
   `create_graph_screen`, `create_settings_screen`.
4. Mark layout issues with 🎨 (fixable in D1) and behavior gaps with 🔧
   (defer to the correct D2–D8 bucket).

## Screen inventory

| Screen | `create_*` fn | LOC range | Screenshot | Overall verdict |
|--------|---------------|-----------|------------|-----------------|
| Splash | `create_splash_screen` | | | |
| Setup | `create_setup_screen` | | | |
| Main | `create_main_screen` | | | |
| Graph | `create_graph_screen` | | | |
| Settings | `create_settings_screen` | | | |

## Per-screen audit template

Copy this block once per screen.

### <Screen name>

**Layout issues (🎨 fix in D1):**
- [ ] …

**Behavior issues (🔧 defer):**

| Control | Event cb | Wired? | Current behavior | Expected behavior | Defer to |
|---------|----------|--------|------------------|-------------------|----------|
| e.g. "Output ON" btn | `main_output_btn_event_cb` | ❌ no cb | nothing | send `CMD:OUTPUT ON` via UDI, flip chip green | D3 |

**Style constants used** (font, padding, chip colors):
- …

**Recommended constants** (to unify across screens in D2):
- …

## Global findings

- Repeated magic numbers to hoist into a shared style header:
- Inconsistent chip color usage:
- Touch target sizes < 44 px:
- Font-size hierarchy proposal:

## D1 exit criteria

- [ ] Every screen has a filled-in per-screen block
- [ ] Every button is either ✅ wired-correctly or has a defer-to bucket
- [ ] Layout-only fixes committed and visible in fresh screenshots
- [ ] `crowpanel-screens-tracker.md` updated: D1 row → 🟢 + PR link
