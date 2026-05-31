/**
 * lv_conf.h — LVGL 8.3.x configuration for CrowPanel 4.3" Advance V1.2
 *
 * Tick:   LV_TICK_CUSTOM stays 0 (default). loop() calls lv_tick_inc() manually.
 * Memory: LV_MEM_CUSTOM stays 0 (default 48 KB internal heap).  LVGL draw
 *         framebuffers are allocated from PSRAM explicitly in init_lvgl().
 * Fonts:  Montserrat 12/16/20/28/48 enabled via -D flags in platformio.ini
 *         (build_flags guarantees library compilation units see them too).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ── colour ───────────────────────────────────────────────────────── */
#define LV_COLOR_DEPTH     16   /* RGB565 — matches LovyanGFX framebuffer */
#define LV_COLOR_16_SWAP   0    /* byte order already correct for the panel */
#define LV_USE_LOG          0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* ── built-in fonts (Montserrat) ─────────────────────────────────── */
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_48   1

/* ── widgets: enable the ones we use ────────────────────────────── */
#define LV_USE_ARC          1
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     1
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         1
#define LV_USE_ROLLER       1
#define LV_USE_SLIDER       1
#define LV_USE_SWITCH       1
#define LV_USE_TEXTAREA     1
#define LV_USE_TABLE        1

#endif /* LV_CONF_H */
