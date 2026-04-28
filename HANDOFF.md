# Development Station Power Supply — Handoff

**Repo:** https://github.com/Phil-CIA/Development-Station-Power-Supply.git  
**Branch:** main  
**Last commit:** 4cf511f  
**Workspace:** `WorkStation/` (PlatformIO project)  
**Active file:** `src/main.cpp`

---

## What's Built and Working

- INA3221 dual-range current sensing — HIGH=20 mΩ, LOW=200 mΩ, Incoming 12V=10 mΩ (BOM values)
- Per-rail current-limit state machine: LATCH / HICCUP / MONITOR trip modes, NVS persistent
- Two operation modes: **NORMAL** and **CURRENT_LIMIT** (TFT-selectable via `tftRequestedOperationMode` volatile)
- NVS config namespace `hatpsu`, struct version 5
- Serial commands: `MODE`, `CLENABLE`, `CLSET`, `CLMODE`, `CLDELAY`, `CLSTAT`, `CLCLEAR`, `CFGSAVE`, `CFGSHOW`
- Build clean: RAM 6.9% (22724/327680), Flash 27.7% (362949/1310720)

---

## TFT Panel Integration Point

To trigger a mode change from a TFT button handler, write:

```cpp
tftRequestedOperationMode = static_cast<uint8_t>(OperationMode::CurrentLimit);
// or
tftRequestedOperationMode = static_cast<uint8_t>(OperationMode::Normal);
// loop() calls syncOperationModeFromTftRequest() automatically
```

---

## Safe Bring-up CLSET Values (serial commands before first power-on)

```
CLSET 0 1500 2000 50
CLSET 1 1500 2000 50
CLSET 2 1000 1500 50
CLMODE 0 HICCUP
CLMODE 1 HICCUP
CLMODE 2 HICCUP
CLDELAY 0 200 800
CLDELAY 1 200 800
CLDELAY 2 200 800
CFGSAVE
```

---

## Pending Next Steps (priority order)

1. **TFT mode button** — softkey to toggle NORMAL ↔ CURRENT_LIMIT + mode badge on dashboard
2. **Incoming 12V dashboard row** — add 4th row to TFT display
3. **Relay/GPIO expander** — adjustable channel voltage select
4. **Rail enable GPIO pins** — currently placeholder (-1); assign real pins on next PCB rev

---

## Display Board Bring-up Status (2026-04-19)

- Active env for the front display board is `display_board` on COM9 using ESP32-C6 + Arduino_GFX.
- `src/display_main.cpp` now contains a low-level legacy SPI path, shared-bus isolation for touch/SD, and a minimal TFT-only test mode.
- Full-library init paths ran and reported success, but did not produce trustworthy visible output on the Hosyond module.
- A possible breakthrough was suspected from a one-off visual event (pixelation/static and black/white pattern), but it did not become repeatable or actionable.
- Current flashed/tested state alternates pixel-format assumptions in minimal mode:
	- Mode A: RGB565 / `COLMOD=0x55`
	- Mode B: 18-bit / `COLMOD=0x66`
- User observation at stop point: panel behavior remained unstable and did not clearly accept intended image data.
- Final call for this session: no confirmed progress on this module; likely marginal/faulty behavior (possible ESD-related). Stop this path for now and move to alternative display direction.

## Display Bring-up Notes

- Keep touch and SD isolated during TFT-only work.
- COM9 often remains locked after monitor/upload activity; a full unplug/replug or closing all monitors may be required before reflashing.
- Manual power-cycle plus reset can be needed to observe panel behavior after flashing.
- If this module is revisited later, resume from `display_board` with `MINIMAL_TFT_ONLY=1` and the current `src/display_main.cpp` legacy path as the known baseline.

---

## NVS Config Version History

| Version | Change |
|---------|--------|
| v1 | Initial |
| v2 | Shunt ohms per rail |
| v3 | BOM shunt defaults (20/200/10 mΩ) |
| v4 | Per-rail current limit config |
| v5 | Operation mode |
