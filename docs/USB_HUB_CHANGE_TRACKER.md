# USB Hub Board — Change Tracker

**Current State:** Rev 1 board built and in inventory, ready for assembly testing.  
**Purpose of this doc:** Track all known issues and planned changes so nothing is lost between sessions.  
**Workflow:** All items below are scoped to the **next PCB revision**. Do not modify the current Rev 1 files until a new revision branch/folder is started.

---

## Status Legend
| Symbol | Meaning |
|--------|---------|
| 🔴 | Open — not yet addressed |
| 🟡 | In progress / needs decision |
| 🟢 | Resolved — pending next rev commit |

---

## Schematic Changes

### SCH-001 — Net alias conflict: `VCC_12V` vs `+12V`
**Status:** 🟢 Resolved in next-iter files  
**Severity:** Low (cosmetic/ERC noise)  
**Found by:** KiCad ERC — `multiple_net_names`  
**Description:** Two global labels (`+12V` and `VCC_12V`) are attached to the same net. KiCad resolves to `+12V` in the netlist but the duplicate alias causes confusion and ERC noise on every run.  
**Fix:** Rename all `VCC_12V` labels to `+12V` in the schematic.  
**Progress:** Done in `hardware/kicad/usb-hub-next-iter/USB Hub Next Iter.kicad_sch`; ERC dropped from 124 to 123 by removing the `multiple_net_names` violation.

---

### SCH-002 — Missing PWR_FLAG on 5 power rails
**Status:** 🟢 Resolved in next-iter files  
**Severity:** Low (ERC housekeeping)  
**Found by:** KiCad ERC — `power_pin_not_driven` × 5  
**Affected nets:** U3/OUT, J1 VBUS, J3 VBUS, U1/VS, #PWR01  
**Description:** ERC cannot confirm these rails are driven because no `PWR_FLAG` symbol is present. Not an electrical fault — the rails are driven — but causes 5 ERC errors on every run.  
**Fix:** Add a `PWR_FLAG` symbol to each of the 5 nets listed above.
**Progress:** Done in `hardware/kicad/usb-hub-next-iter/USB Hub Next Iter.kicad_sch`; `power_pin_not_driven` is now 0. Remaining ERC: **118 violations** (58 lib_symbol_issues + 55 footprint_link_issues + 3 lib_symbol_mismatch + 2 pin_to_pin). No `erc-after-sch002.json` capture on file — counts derived from `erc-after-sch001.json` minus the 5 removed violations.

---

### SCH-003 — Symbol library link cleanup (58 issues)
**Status:** 🔴 Open  
**Severity:** Low (library housekeeping)  
**Found by:** KiCad ERC — `lib_symbol_issues` (58) + `lib_symbol_mismatch` (3)  
**Description:** Many schematic symbols reference libraries that are not configured in the project `sym-lib-table`. Symbols still work (they are embedded in the schematic) but ERC flags every instance and the project is not portable.  
**Fix (next rev):**
- Audit `sym-lib-table` and add any missing libraries, OR
- For custom/one-off symbols, copy them into a project-local symbol library (`USB-Hub.kicad_sym`) and re-link all symbols to it.

---

### SCH-004 — Footprint link inconsistency (55 issues)
**Status:** 🔴 Open  
**Severity:** Low (library housekeeping)  
**Found by:** KiCad ERC — `footprint_link_issues` (55)  
**Description:** Symbol footprint fields reference libraries not present in the project `fp-lib-table`. Same root cause as SCH-003 — project libraries are not self-contained.  
**Fix:** Resolved together with SCH-003 / PCB-002 cleanup pass.

---

### SCH-005 — U3 pin type warnings (ON#/OFF tied to GND; FB to inductor)
**Status:** 🟡 Confirm intentional  
**Severity:** Informational  
**Found by:** KiCad ERC — `pin_to_pin` (2)  
**Description:**  
- `U3 Pin 5 (ON#/OFF, Unspecified)` connected to `GND` — typical enable tie-down, ERC confused by Unspecified pin type.  
- `U3 Pin 4 (FB, Unspecified)` connected to `L1 Pin 1 (Passive)` — normal feedback node in boost/buck topology.  
**Fix:** Either change the pin types in the U3 symbol to resolve ERC warnings, or add a schematic note confirming these connections are intentional. Decide at next rev.

---

## PCB Changes

### PCB-001 — Silkscreen clipped by solder mask (3 locations)
**Status:** 🟢 Resolved in next-iter files  
**Severity:** Low (cosmetic, does not affect assembly)  
**Found by:** KiCad DRC — `silk_over_copper`  
**Locations:**
- `J4` F.Silkscreen segment @ (159.8025, 85.18)
- `J9` B.Silkscreen rectangle @ (160.06, 80.04)
- `U8` designator text @ (129.7584, 84.1226)  
**Fix:** Nudge the affected silkscreen elements inward so they clear the solder mask boundary.
**Progress:** Done in `hardware/kicad/usb-hub-next-iter/USB Hub Next Iter.kicad_pcb`; `silk_over_copper` is now 0.

---

### PCB-002 — Missing footprint library: `PCM_JLCPCB` (54 DRC warnings)
**Status:** � Resolved in next-iter files  
**Severity:** Low (library housekeeping — footprints are embedded and board files are correct)  
**Found by:** KiCad DRC — `lib_footprint_issues` (54)  
**Description:** The `PCM_JLCPCB` footprint library was not registered in the project `fp-lib-table`. Affects decoupling caps, resistors, and many passives. Footprints in the PCB file are intact; this is a library-path-portability issue only.  
**Fix:** Set `KICAD9_3RD_PARTY` environment variable and add the `PCM_JLCPCB` entry to the project `fp-lib-table` in the next-iter workspace.  
**Progress:** Done — `lib_footprint_issues` dropped from 54 → **0** in `drc-after-pcb001.json`. Side effect: with the library now linked, KiCad can compare footprints and `lib_footprint_mismatch` count increased (see PCB-003).

---

### PCB-003 — Footprint mismatch vs. library: 14 components
**Status:** 🔴 Open  
**Severity:** Low (footprints in PCB are what was used to fabricate — the issue is library drift)  
**Found by:** KiCad DRC — `lib_footprint_mismatch` (baseline: 11; current: **14** after PCM_JLCPCB was linked)  
**Note on count change:** Baseline showed 11 because PCM_JLCPCB was missing — KiCad could not compare those footprints. Once the library was linked (PCB-002 fix), 3 additional mismatches became detectable. All 14 remain open.  
**Affected parts (baseline):**  
| Ref | Footprint | Library |
|-----|-----------|---------|
| U1, U9 | SOT65P280X145-8N | 1My_Parts_ ICs |
| U2, U4, U5, U6, U10 | SOT-23-6 | Package_TO_SOT_SMD |
| J2, J5, J10 | USB_C_Receptacle_G-Switch_GT-USB-7051x | Connector_USB |
| J7 | JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical | Connector_JST |  

**Fix:** Decide for each: update PCB footprint from library (if library is the authority) or update library from PCB (if PCB is the correct version). Do this as part of the library cleanup pass.

---

## Future / Nice-to-Have

### FUT-001 — J4 and J6 are power-only USB-A ports
**Status:** 🟡 Intentional — document clearly  
**Description:** `J4` and `J6` D+/D- pins are explicitly no-connected. These connectors provide 5V power only, not data. Currently there is no silkscreen or BOM note distinguishing them from data ports.  
**Action:** Add a silkscreen label or assembly note marking J4/J6 as `5V POWER ONLY` to prevent confusion during use.

---

## Revision History
| Date | Item | Notes |
|------|------|-------|
| 2026-04-14 | PCB-002 resolved; PCB-003 count corrected | Tracker updated to reflect actual JSON report state. PCB-002 lib_footprint_issues confirmed 0 in drc-after-pcb001.json. PCB-003 count corrected 11→14. |
| 2026-04-12 | SCH-002 completed | Added PWR_FLAG coverage in next-iter schematic; `power_pin_not_driven` now 0; remaining ERC: 118 (library/link warnings only). |
| 2026-04-12 | PCB-002 resolved (library path fix) | Set `KICAD9_3RD_PARTY` env var and wired `PCM_JLCPCB` into project `fp-lib-table`; `lib_footprint_issues` dropped 54→0. DRC at 17 before PCB-001 silk fix. |
| 2026-04-12 | PCB-001 completed | Cleared all 3 silkscreen-over-copper warnings in next-iter PCB; DRC final: **14** (`lib_footprint_mismatch` only). Snapshot saved as `drc-after-pcb001.json`. |
| 2026-04-12 | SCH-001 completed | Isolated next-iteration workspace created at `hardware/kicad/usb-hub-next-iter`; net-label alias cleanup verified by ERC rerun (124→123). Snapshot saved as `erc-after-sch001.json`. |
| 2026-04-12 | All items added | Initial capture from ERC/DRC runs on Rev 1 files. Board is assembled/ready — all fixes deferred to next PCB revision. Baseline snapshots: `erc-baseline.json` (124), `drc-baseline.json` (68). |
