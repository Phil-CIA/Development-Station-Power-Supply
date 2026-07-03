#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <cstring>
#include <cmath>
#include <driver/gpio.h>
#include <soc/usb_serial_jtag_reg.h>

#include "CrowPanel43Display.h"
#include "disp_link_slave.h"

namespace {

// ── constants ──────────────────────────────────────────────────────────────
constexpr uint8_t kBoardCtrlAddr  = 0x30;
constexpr uint8_t kTouchAddr      = 0x5D;
constexpr int     kDisplayWidth   = 800;
constexpr int     kDisplayHeight  = 480;
constexpr bool    kOtaEnabled     = false;
constexpr bool    kLiveTelemetryUiEnabled = true;
constexpr bool    kRxSerialLogEnabled = false;
constexpr uint32_t kTrendSampleMs = 200;
constexpr uint32_t kUiUpdateMinMs = 50;
constexpr uint32_t kDetailUiUpdateMinMs = 250;
constexpr bool    kLiveChartsEnabled = true;
constexpr bool    kMinimalUiLabelsOnly = false;
constexpr uint8_t kChartDecimation = 2;
constexpr uint16_t kVoltageUpdateDeadband_mV = 5;
constexpr int16_t  kCurrentUpdateDeadband_mA = 2;
constexpr uint32_t kValueKeepaliveMs = 0;
constexpr uint32_t kSplashDurationMs = 1800;
constexpr uint32_t kDemoFrameMs = 50;
constexpr uint32_t kDemoTourSwitchMs = 8000;
constexpr size_t   kTrendCapacity = 1800;  // 7.5 minutes at 4 Hz
constexpr uint16_t kChartPoints   = 120;   // 2 minutes visible at 1 Hz (decimated)

enum class UiScreen : uint8_t {
  Splash = 0,
  Main = 1,
  Graph = 2,
};

struct UiTheme {
  static constexpr uint32_t kBg = 0x14181D;
  static constexpr uint32_t kPanel = 0x2A2F36;
  static constexpr uint32_t kPanelSoft = 0x22272D;
  static constexpr uint32_t kStatusBar = 0x1B2026;
  static constexpr uint32_t kBadge = 0x202C3A;
  static constexpr uint32_t kTextPrimary = 0xF2F4F7;
  static constexpr uint32_t kTextMuted = 0xB4BDC8;
  static constexpr uint32_t kAccentV = 0xF5C316;
  static constexpr uint32_t kAccentI = 0x2EA5F9;
  static constexpr uint32_t kAccentWarn = 0xFF8C3A;
  static constexpr uint32_t kAccentOk = 0x30C95E;
  static constexpr uint32_t kBorder = 0x3B434D;
};

// ── hardware ───────────────────────────────────────────────────────────────
CrowPanel43Display display;
String serialLine;

// ── LVGL driver state (full-frame double-buffer from PSRAM) ───────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lvgl_fb1 = nullptr;
static lv_color_t* lvgl_fb2 = nullptr;

// ── LVGL UI handles ────────────────────────────────────────────────────────
static lv_obj_t* screen_splash = nullptr;
static lv_obj_t* screen_main = nullptr;
static lv_obj_t* screen_graph = nullptr;

static lv_obj_t* lbl_main_voltage = nullptr;
static lv_obj_t* lbl_main_current = nullptr;
static lv_obj_t* lbl_main_stats = nullptr;

static lv_obj_t* lbl_graph_voltage = nullptr;
static lv_obj_t* lbl_graph_current = nullptr;
static lv_obj_t* lbl_graph_stats = nullptr;
static lv_obj_t* lbl_window = nullptr;
static lv_obj_t* lbl_graph_window_v = nullptr;
static lv_obj_t* lbl_graph_window_i = nullptr;

static lv_obj_t* lbl_status_link = nullptr;
static lv_obj_t* lbl_status_seq = nullptr;
static lv_obj_t* lbl_status_mode = nullptr;
static lv_obj_t* lbl_status_uptime = nullptr;
static lv_obj_t* lbl_splash_hint = nullptr;
static lv_obj_t* lbl_fault_main = nullptr;
static lv_obj_t* lbl_fault_graph = nullptr;

static lv_obj_t* chip_main_ok = nullptr;
static lv_obj_t* chip_main_mode = nullptr;
static lv_obj_t* chip_main_limit = nullptr;
static lv_obj_t* chip_main_run = nullptr;

static lv_obj_t* chip_graph_ok = nullptr;
static lv_obj_t* chip_graph_mode = nullptr;
static lv_obj_t* chip_graph_limit = nullptr;
static lv_obj_t* chip_graph_run = nullptr;

static lv_obj_t* bar_main_voltage = nullptr;
static lv_obj_t* bar_main_current = nullptr;

static lv_obj_t* chart_v     = nullptr;
static lv_obj_t* chart_i     = nullptr;
static lv_chart_series_t* chart_v_series = nullptr;
static lv_chart_series_t* chart_i_series = nullptr;

static UiScreen active_screen = UiScreen::Splash;
static bool splash_done = false;
static uint32_t splash_start_ms = 0;

// ── telemetry change-detection ─────────────────────────────────────────────
static uint32_t last_drawn_seq   = 0xFFFFFFFFu;
static uint32_t last_drawn_count = 0;
static uint32_t last_ui_update_ms = 0;
static uint16_t last_v12_drawn_mV = 0;
static int16_t  last_i12_drawn_mA = 0;
static bool     have_drawn_values = false;
static uint32_t last_value_draw_ms = 0;
static char     last_voltage_text[32] = "";
static char     last_current_text[32] = "";
static size_t   last_drawn_samples = static_cast<size_t>(-1);
static uint16_t chart_write_idx = 0;
static uint32_t last_detail_ui_update_ms = 0;
static bool demo_mode = true;
static bool demo_tour = true;
static uint32_t demo_start_ms = 0;
static uint32_t demo_last_tour_switch_ms = 0;

struct DisplayTelemetry {
  uint32_t rx_count;
  uint32_t err_count;
  uint32_t i2c_rx_count;
  uint32_t uart_bytes;
  uint8_t last_seq;
  uint16_t last_v12_mV;
  int16_t last_i12_mA;
  uint32_t last_rx_ms;
};

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int32_t map_i32(int32_t value, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
  if (in_max <= in_min) return out_min;
  const int32_t clamped = clamp_i32(value, in_min, in_max);
  const int32_t num = (clamped - in_min) * (out_max - out_min);
  return out_min + (num / (in_max - in_min));
}

struct TrendSample {
  uint32_t t_ms;
  uint32_t rx_count;
  uint32_t err_count;
  uint32_t uart_count;
  uint8_t seq;
  uint16_t v12_mV;
  int16_t i12_mA;
};

static TrendSample trend_buf[kTrendCapacity] = {};
static size_t trend_head = 0;
static size_t trend_count = 0;
static bool trend_logging_enabled = true;

size_t trendStartIndex() {
  if (trend_count == 0) return 0;
  return (trend_head + kTrendCapacity - trend_count) % kTrendCapacity;
}

const TrendSample& trendAt(size_t logical_idx) {
  const size_t idx = (trendStartIndex() + logical_idx) % kTrendCapacity;
  return trend_buf[idx];
}

void trendClear() {
  trend_head = 0;
  trend_count = 0;
  chart_write_idx = 0;
  if (chart_v && chart_v_series) {
    lv_chart_set_all_value(chart_v, chart_v_series, LV_CHART_POINT_NONE);
    lv_chart_refresh(chart_v);
  }
  if (chart_i && chart_i_series) {
    lv_chart_set_all_value(chart_i, chart_i_series, LV_CHART_POINT_NONE);
    lv_chart_refresh(chart_i);
  }
}

struct TrendWindowStats {
  bool has_data;
  uint16_t min_v12_mV;
  uint16_t max_v12_mV;
  int16_t min_i12_mA;
  int16_t max_i12_mA;
  size_t samples;
};

TrendWindowStats getTrendWindowStats(size_t window_samples) {
  TrendWindowStats stats = {};
  if (trend_count == 0 || window_samples == 0) {
    return stats;
  }

  const size_t start = (trend_count > window_samples) ? (trend_count - window_samples) : 0;
  const TrendSample& first = trendAt(start);
  stats.has_data = true;
  stats.min_v12_mV = first.v12_mV;
  stats.max_v12_mV = first.v12_mV;
  stats.min_i12_mA = first.i12_mA;
  stats.max_i12_mA = first.i12_mA;
  stats.samples = trend_count - start;

  for (size_t i = start + 1; i < trend_count; ++i) {
    const TrendSample& sample = trendAt(i);
    if (sample.v12_mV < stats.min_v12_mV) stats.min_v12_mV = sample.v12_mV;
    if (sample.v12_mV > stats.max_v12_mV) stats.max_v12_mV = sample.v12_mV;
    if (sample.i12_mA < stats.min_i12_mA) stats.min_i12_mA = sample.i12_mA;
    if (sample.i12_mA > stats.max_i12_mA) stats.max_i12_mA = sample.i12_mA;
  }

  return stats;
}

void appendCharts(const TrendSample& s) {
  if (!kLiveChartsEnabled) return;
  if (!chart_v || !chart_i || !chart_v_series || !chart_i_series) return;

  // Use LVGL's incremental circular update path to reduce redraw artifacts.
  lv_chart_set_next_value(chart_v, chart_v_series, static_cast<lv_coord_t>(s.v12_mV));
  lv_chart_set_next_value(chart_i, chart_i_series, static_cast<lv_coord_t>(s.i12_mA));
}

void trendPushSample(const disp_link_slave::Telemetry& t, uint32_t now_ms) {
  TrendSample s = {};
  s.t_ms = now_ms;
  s.rx_count = t.rx_count;
  s.err_count = t.err_count;
  s.uart_count = t.i2c_rx_count;
  s.seq = t.last_seq;
  s.v12_mV = t.last_v12_mV;
  s.i12_mA = t.last_i12_mA;

  trend_buf[trend_head] = s;
  trend_head = (trend_head + 1) % kTrendCapacity;
  if (trend_count < kTrendCapacity) trend_count++;

  // Decimate chart writes to limit redraw pressure while labels stay snappy.
  if ((trend_count % kChartDecimation) == 0) {
    appendCharts(s);
  }
}

DisplayTelemetry get_display_telemetry() {
  const auto raw = disp_link_slave::snapshot();
  DisplayTelemetry out = {
    raw.rx_count,
    raw.err_count,
    raw.i2c_rx_count,
    raw.uart_bytes,
    raw.last_seq,
    raw.last_v12_mV,
    raw.last_i12_mA,
    raw.last_rx_ms,
  };

  if (!demo_mode) {
    return out;
  }

  const uint32_t now_ms = millis();
  const float t = (now_ms - demo_start_ms) * 0.001f;
  const float v = 12100.0f + 850.0f * sinf(t * 1.12f) + 180.0f * sinf(t * 0.31f);
  const float i = 830.0f + 620.0f * sinf(t * 0.87f + 1.35f) + 140.0f * sinf(t * 2.2f);

  out.rx_count = out.rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.i2c_rx_count = out.i2c_rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.last_seq = static_cast<uint8_t>((now_ms - demo_start_ms) / 200);
  out.last_v12_mV = static_cast<uint16_t>(clamp_i32(static_cast<int32_t>(v), 9000, 15000));
  out.last_i12_mA = static_cast<int16_t>(clamp_i32(static_cast<int32_t>(i), -500, 3000));
  out.last_rx_ms = now_ms;
  return out;
}

// ── LVGL flush callback ────────────────────────────────────────────────────
// Called by LVGL when a screen region has been rendered into color_p.
// pushImage() writes into the RGB panel's PSRAM framebuffer; LCD_CAM DMA
// streams it continuously to the display.  startWrite/endWrite manage the
// bus mutex so this is safe to call from the main-loop context.
void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  display.startWrite();
  display.pushImage(area->x1, area->y1,
                    area->x2 - area->x1 + 1,
                    area->y2 - area->y1 + 1,
                    (lgfx::rgb565_t*)color_p);
  display.endWrite();
  lv_disp_flush_ready(drv);
}

// ── LVGL touch callback ────────────────────────────────────────────────────
void lvgl_touch_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  uint16_t x = 0, y = 0;
  if (display.getTouch(&x, &y)) {
    data->state   = LV_INDEV_STATE_PR;
    data->point.x = static_cast<lv_coord_t>(x);
    data->point.y = static_cast<lv_coord_t>(y);
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void set_active_screen(UiScreen screen) {
  active_screen = screen;
  if (screen_splash) {
    if (screen == UiScreen::Splash) lv_obj_clear_flag(screen_splash, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_splash, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_main) {
    if (screen == UiScreen::Main) lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_graph) {
    if (screen == UiScreen::Graph) lv_obj_clear_flag(screen_graph, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_graph, LV_OBJ_FLAG_HIDDEN);
  }
}

void nav_btn_event_cb(lv_event_t* e) {
  const uintptr_t target = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (target == static_cast<uintptr_t>(UiScreen::Main)) {
    set_active_screen(UiScreen::Main);
    return;
  }
  if (target == static_cast<uintptr_t>(UiScreen::Graph)) {
    set_active_screen(UiScreen::Graph);
  }
}

lv_obj_t* create_nav_btn(lv_obj_t* parent, const char* text, UiScreen target, int x_ofs) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 102, 32);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, x_ofs, 6);
  lv_obj_set_style_bg_color(btn, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(target)));

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(label);
  return btn;
}

lv_obj_t* create_state_chip(lv_obj_t* parent, const char* text, uint32_t bg_hex, uint32_t text_hex, int y_ofs) {
  lv_obj_t* chip = lv_obj_create(parent);
  lv_obj_set_size(chip, 74, 34);
  lv_obj_align(chip, LV_ALIGN_TOP_MID, 0, y_ofs);
  lv_obj_set_style_bg_color(chip, lv_color_hex(bg_hex), LV_PART_MAIN);
  lv_obj_set_style_border_width(chip, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(chip, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_radius(chip, 8, LV_PART_MAIN);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* lbl = lv_label_create(chip);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_color(lbl, lv_color_hex(text_hex), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl);
  return lbl;
}

// ── LVGL init ──────────────────────────────────────────────────────────────
void init_lvgl() {
  lv_init();

  // Allocate two full-frame buffers from PSRAM (2 × 800×480×2 B ≈ 1.5 MB).
  const size_t fb_bytes = static_cast<size_t>(kDisplayWidth) * kDisplayHeight * sizeof(lv_color_t);
  lvgl_fb1 = static_cast<lv_color_t*>(heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lvgl_fb2 = static_cast<lv_color_t*>(heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!lvgl_fb1 || !lvgl_fb2) {
    Serial.println("LVGL: PSRAM framebuffer alloc failed!");
  }
  lv_disp_draw_buf_init(&draw_buf, lvgl_fb1, lvgl_fb2, kDisplayWidth * kDisplayHeight);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = static_cast<lv_coord_t>(kDisplayWidth);
  disp_drv.ver_res  = static_cast<lv_coord_t>(kDisplayHeight);
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch_cb;
  lv_indev_drv_register(&indev_drv);
}

// ── Dashboard UI ───────────────────────────────────────────────────────────
void create_splash_screen(lv_obj_t* root) {
  screen_splash = lv_obj_create(root);
  lv_obj_set_size(screen_splash, kDisplayWidth, kDisplayHeight);
  lv_obj_clear_flag(screen_splash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(screen_splash, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(screen_splash, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(screen_splash, 0, LV_PART_MAIN);

  lv_obj_t* hero = lv_obj_create(screen_splash);
  lv_obj_set_size(hero, 720, 300);
  lv_obj_center(hero);
  lv_obj_set_style_radius(hero, 22, LV_PART_MAIN);
  lv_obj_set_style_bg_color(hero, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_border_width(hero, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(hero, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(hero);
  lv_label_set_text(title, "Development Station");
  lv_obj_set_style_text_color(title, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 44);

  lv_obj_t* subtitle = lv_label_create(hero);
  lv_label_set_text(subtitle, "FNIRSI-inspired visual prototype");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 114);

  lbl_splash_hint = lv_label_create(hero);
  lv_label_set_text(lbl_splash_hint, "Synchronizing telemetry link...");
  lv_obj_set_style_text_color(lbl_splash_hint, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_splash_hint, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_splash_hint, LV_ALIGN_BOTTOM_MID, 0, -40);
}

void create_main_screen(lv_obj_t* root) {
  screen_main = lv_obj_create(root);
  lv_obj_set_size(screen_main, kDisplayWidth, kDisplayHeight);
  lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(screen_main, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(screen_main, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(screen_main, 0, LV_PART_MAIN);

  lv_obj_t* status = lv_obj_create(screen_main);
  lv_obj_set_size(status, kDisplayWidth - 20, 50);
  lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_color(status, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(status, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(status, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(status, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* mode = lv_label_create(status);
  lv_label_set_text(mode, "LIVE OUTPUT 12V BUS");
  lv_obj_set_style_text_color(mode, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(mode, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(mode, LV_ALIGN_LEFT_MID, 14, 0);

  lbl_status_link = lv_label_create(status);
  lv_label_set_text(lbl_status_link, "LINK --");
  lv_obj_set_style_text_color(lbl_status_link, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_status_link, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_status_link, LV_ALIGN_LEFT_MID, 150, 0);

  lbl_status_seq = lv_label_create(status);
  lv_label_set_text(lbl_status_seq, "SEQ --");
  lv_obj_set_style_text_color(lbl_status_seq, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_status_seq, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_status_seq, LV_ALIGN_LEFT_MID, 270, 0);

  lbl_status_mode = lv_label_create(status);
  lv_label_set_text(lbl_status_mode, "CV");
  lv_obj_set_style_text_color(lbl_status_mode, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_status_mode, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_status_mode, LV_ALIGN_LEFT_MID, 355, 0);

  lbl_status_uptime = lv_label_create(status);
  lv_label_set_text(lbl_status_uptime, "UP 0s");
  lv_obj_set_style_text_color(lbl_status_uptime, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_status_uptime, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_status_uptime, LV_ALIGN_LEFT_MID, 414, 0);

  lv_obj_t* badge = lv_obj_create(status);
  lv_obj_set_size(badge, 96, 30);
  lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -118, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(UiTheme::kBadge), LV_PART_MAIN);
  lv_obj_set_style_radius(badge, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(badge, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(badge, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* badge_lbl = lv_label_create(badge);
  lv_label_set_text(badge_lbl, "OUTPUT ON");
  lv_obj_set_style_text_color(badge_lbl, lv_color_hex(UiTheme::kAccentOk), LV_PART_MAIN);
  lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_center(badge_lbl);

  create_nav_btn(status, "Graphs", UiScreen::Graph, -10);

  lv_obj_t* panel_v = lv_obj_create(screen_main);
  lv_obj_set_size(panel_v, 368, 182);
  lv_obj_align(panel_v, LV_ALIGN_TOP_LEFT, 18, 68);
  lv_obj_set_style_bg_color(panel_v, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(panel_v, 16, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_v, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel_v, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* hdr_v = lv_label_create(panel_v);
  lv_label_set_text(hdr_v, "INPUT VOLTAGE");
  lv_obj_set_style_text_color(hdr_v, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_v, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(hdr_v, LV_ALIGN_TOP_LEFT, 18, 16);

  lv_obj_t* hdr_v_set = lv_label_create(panel_v);
  lv_label_set_text(hdr_v_set, "SET 12.00V");
  lv_obj_set_style_text_color(hdr_v_set, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_v_set, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_v_set, LV_ALIGN_TOP_RIGHT, -16, 18);

  lbl_main_voltage = lv_label_create(panel_v);
  lv_label_set_text(lbl_main_voltage, "--.-- V");
  lv_obj_set_style_text_color(lbl_main_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_voltage, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_voltage, 330);
  lv_obj_set_style_text_align(lbl_main_voltage, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_align(lbl_main_voltage, LV_ALIGN_TOP_LEFT, 18, 58);

  bar_main_voltage = lv_bar_create(panel_v);
  lv_obj_set_size(bar_main_voltage, 330, 20);
  lv_obj_align(bar_main_voltage, LV_ALIGN_BOTTOM_LEFT, 18, -20);
  lv_bar_set_range(bar_main_voltage, 0, 1000);
  lv_bar_set_value(bar_main_voltage, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_main_voltage, lv_color_hex(0x1A2735), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_main_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(bar_main_voltage, lv_color_hex(0xFFD95A), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(bar_main_voltage, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_main_voltage, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_main_voltage, 4, LV_PART_INDICATOR);

  lv_obj_t* bar_v_lo = lv_label_create(panel_v);
  lv_label_set_text(bar_v_lo, "9V");
  lv_obj_set_style_text_color(bar_v_lo, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_v_lo, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_v_lo, LV_ALIGN_BOTTOM_LEFT, 18, -2);

  lv_obj_t* bar_v_mid = lv_label_create(panel_v);
  lv_label_set_text(bar_v_mid, "12V");
  lv_obj_set_style_text_color(bar_v_mid, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_v_mid, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_v_mid, LV_ALIGN_BOTTOM_MID, 0, -2);

  lv_obj_t* bar_v_hi = lv_label_create(panel_v);
  lv_label_set_text(bar_v_hi, "15V");
  lv_obj_set_style_text_color(bar_v_hi, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_v_hi, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_v_hi, LV_ALIGN_BOTTOM_RIGHT, -18, -2);

  lv_obj_t* panel_i = lv_obj_create(screen_main);
  lv_obj_set_size(panel_i, 368, 182);
  lv_obj_align(panel_i, LV_ALIGN_TOP_LEFT, 18, 270);
  lv_obj_set_style_bg_color(panel_i, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(panel_i, 16, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_i, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel_i, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* hdr_i = lv_label_create(panel_i);
  lv_label_set_text(hdr_i, "INPUT CURRENT");
  lv_obj_set_style_text_color(hdr_i, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_i, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(hdr_i, LV_ALIGN_TOP_LEFT, 18, 16);

  lv_obj_t* hdr_i_set = lv_label_create(panel_i);
  lv_label_set_text(hdr_i_set, "LIM 2.000A");
  lv_obj_set_style_text_color(hdr_i_set, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_i_set, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_i_set, LV_ALIGN_TOP_RIGHT, -16, 18);

  lbl_main_current = lv_label_create(panel_i);
  lv_label_set_text(lbl_main_current, "--.--- A");
  lv_obj_set_style_text_color(lbl_main_current, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_current, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_current, 330);
  lv_obj_set_style_text_align(lbl_main_current, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_align(lbl_main_current, LV_ALIGN_TOP_LEFT, 18, 58);

  bar_main_current = lv_bar_create(panel_i);
  lv_obj_set_size(bar_main_current, 330, 20);
  lv_obj_align(bar_main_current, LV_ALIGN_BOTTOM_LEFT, 18, -20);
  lv_bar_set_range(bar_main_current, 0, 1000);
  lv_bar_set_value(bar_main_current, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_main_current, lv_color_hex(0x1A2735), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_main_current, lv_color_hex(UiTheme::kAccentI), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(bar_main_current, lv_color_hex(0x73C8FF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(bar_main_current, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_main_current, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_main_current, 4, LV_PART_INDICATOR);

  lv_obj_t* bar_i_lo = lv_label_create(panel_i);
  lv_label_set_text(bar_i_lo, "-0.5A");
  lv_obj_set_style_text_color(bar_i_lo, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_i_lo, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_i_lo, LV_ALIGN_BOTTOM_LEFT, 18, -2);

  lv_obj_t* bar_i_mid = lv_label_create(panel_i);
  lv_label_set_text(bar_i_mid, "1.0A");
  lv_obj_set_style_text_color(bar_i_mid, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_i_mid, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_i_mid, LV_ALIGN_BOTTOM_MID, 0, -2);

  lv_obj_t* bar_i_hi = lv_label_create(panel_i);
  lv_label_set_text(bar_i_hi, "3.0A");
  lv_obj_set_style_text_color(bar_i_hi, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_i_hi, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_i_hi, LV_ALIGN_BOTTOM_RIGHT, -18, -2);

  lv_obj_t* panel_meta = lv_obj_create(screen_main);
  lv_obj_set_size(panel_meta, 390, 384);
  lv_obj_align(panel_meta, LV_ALIGN_TOP_RIGHT, -18, 68);
  lv_obj_set_style_bg_color(panel_meta, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_radius(panel_meta, 16, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_meta, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel_meta, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* panel_meta_hdr = lv_label_create(panel_meta);
  lv_label_set_text(panel_meta_hdr, "SYSTEM SNAPSHOT");
  lv_obj_set_style_text_color(panel_meta_hdr, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(panel_meta_hdr, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(panel_meta_hdr, LV_ALIGN_TOP_LEFT, 16, 16);

  lbl_main_stats = lv_label_create(panel_meta);
  lv_label_set_text(lbl_main_stats, "rx=0 err=0 uart=0 bytes=0");
  lv_obj_set_style_text_color(lbl_main_stats, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_stats, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_stats, 250);
  lv_obj_align(lbl_main_stats, LV_ALIGN_TOP_LEFT, 16, 56);

  lv_obj_t* chip_col = lv_obj_create(panel_meta);
  lv_obj_set_size(chip_col, 92, 250);
  lv_obj_align(chip_col, LV_ALIGN_TOP_RIGHT, -12, 50);
  lv_obj_set_style_bg_opa(chip_col, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(chip_col, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(chip_col, 0, LV_PART_MAIN);
  lv_obj_clear_flag(chip_col, LV_OBJ_FLAG_SCROLLABLE);

  chip_main_ok = create_state_chip(chip_col, "OK", 0x193425, UiTheme::kAccentOk, 0);
  chip_main_mode = create_state_chip(chip_col, "M1", 0x24364A, UiTheme::kAccentI, 52);
  chip_main_limit = create_state_chip(chip_col, "CV", 0x1F3A27, UiTheme::kAccentOk, 104);
  chip_main_run = create_state_chip(chip_col, "RUN", 0x1F3A27, UiTheme::kAccentOk, 156);

  lv_obj_t* footer = lv_label_create(panel_meta);
  lv_label_set_text(footer, "Read-only visual mode");
  lv_obj_set_style_text_color(footer, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
  lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 16, -16);

  lv_obj_t* fault_row = lv_obj_create(screen_main);
  lv_obj_set_size(fault_row, kDisplayWidth - 36, 34);
  lv_obj_align(fault_row, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(fault_row, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(fault_row, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(fault_row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(fault_row, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_fault_main = lv_label_create(fault_row);
  lv_label_set_text(lbl_fault_main, "OVP OK   OCP OK   OTP OK   SCP OK   LIM CV");
  lv_obj_set_style_text_color(lbl_fault_main, lv_color_hex(UiTheme::kAccentOk), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_fault_main, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_fault_main);
}

void create_graph_screen(lv_obj_t* root) {
  screen_graph = lv_obj_create(root);
  lv_obj_set_size(screen_graph, kDisplayWidth, kDisplayHeight);
  lv_obj_clear_flag(screen_graph, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(screen_graph, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(screen_graph, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(screen_graph, 0, LV_PART_MAIN);

  lv_obj_t* status = lv_obj_create(screen_graph);
  lv_obj_set_size(status, kDisplayWidth - 20, 44);
  lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_color(status, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(status, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(status, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(status, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* mode = lv_label_create(status);
  lv_label_set_text(mode, "TREND HISTORY");
  lv_obj_set_style_text_color(mode, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(mode, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(mode, LV_ALIGN_LEFT_MID, 14, 0);

  create_nav_btn(status, "Main", UiScreen::Main, -10);

  lv_obj_t* card = lv_obj_create(screen_graph);
  lv_obj_set_size(card, kDisplayWidth - 36, 84);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 68);
  lv_obj_set_style_bg_color(card, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_graph_voltage = lv_label_create(card);
  lv_label_set_text(lbl_graph_voltage, "V --.--");
  lv_obj_set_style_text_color(lbl_graph_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_voltage, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_set_width(lbl_graph_voltage, 210);
  lv_obj_set_style_text_align(lbl_graph_voltage, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_graph_voltage, LV_ALIGN_LEFT_MID, 20, 0);

  lbl_graph_current = lv_label_create(card);
  lv_label_set_text(lbl_graph_current, "I --.---");
  lv_obj_set_style_text_color(lbl_graph_current, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_current, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_set_width(lbl_graph_current, 210);
  lv_obj_set_style_text_align(lbl_graph_current, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_graph_current, LV_ALIGN_LEFT_MID, 260, 0);

  lbl_graph_stats = lv_label_create(card);
  lv_label_set_text(lbl_graph_stats, "samples=0");
  lv_obj_set_style_text_color(lbl_graph_stats, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_stats, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_stats, LV_ALIGN_RIGHT_MID, -20, 0);

  lv_obj_t* lbl_v_trend = lv_label_create(screen_graph);
  lv_label_set_text(lbl_v_trend, "Voltage trend (mV)");
  lv_obj_set_style_text_color(lbl_v_trend, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_v_trend, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_v_trend, LV_ALIGN_TOP_LEFT, 22, 164);

  chart_v = lv_chart_create(screen_graph);
  lv_obj_set_size(chart_v, 758, 120);
  lv_obj_align(chart_v, LV_ALIGN_TOP_MID, 0, 194);
  lv_chart_set_type(chart_v, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart_v, LV_CHART_UPDATE_MODE_CIRCULAR);
  lv_chart_set_point_count(chart_v, kChartPoints);
  lv_chart_set_div_line_count(chart_v, 6, 8);
  lv_chart_set_range(chart_v, LV_CHART_AXIS_PRIMARY_Y, 9000, 15000);
  lv_obj_set_style_bg_color(chart_v, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart_v, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(chart_v, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart_v, lv_color_hex(0x4A525D), LV_PART_ITEMS);
  lv_obj_set_style_line_opa(chart_v, LV_OPA_30, LV_PART_ITEMS);
  chart_v_series = lv_chart_add_series(chart_v, lv_color_hex(UiTheme::kAccentV), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_v, chart_v_series, LV_CHART_POINT_NONE);

  lv_obj_t* lbl_i_trend = lv_label_create(screen_graph);
  lv_label_set_text(lbl_i_trend, "Current trend (mA)");
  lv_obj_set_style_text_color(lbl_i_trend, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_i_trend, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_i_trend, LV_ALIGN_TOP_LEFT, 22, 314);

  chart_i = lv_chart_create(screen_graph);
  lv_obj_set_size(chart_i, 758, 120);
  lv_obj_align(chart_i, LV_ALIGN_TOP_MID, 0, 344);
  lv_chart_set_type(chart_i, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart_i, LV_CHART_UPDATE_MODE_CIRCULAR);
  lv_chart_set_point_count(chart_i, kChartPoints);
  lv_chart_set_div_line_count(chart_i, 6, 8);
  lv_chart_set_range(chart_i, LV_CHART_AXIS_PRIMARY_Y, -500, 3000);
  lv_obj_set_style_bg_color(chart_i, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart_i, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(chart_i, 1, LV_PART_MAIN);
  lv_obj_set_style_line_color(chart_i, lv_color_hex(0x4A525D), LV_PART_ITEMS);
  lv_obj_set_style_line_opa(chart_i, LV_OPA_30, LV_PART_ITEMS);
  chart_i_series = lv_chart_add_series(chart_i, lv_color_hex(UiTheme::kAccentI), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_i, chart_i_series, LV_CHART_POINT_NONE);

  lbl_window = lv_label_create(screen_graph);
  lv_label_set_text(lbl_window, "win: waiting for samples");
  lv_obj_set_style_text_color(lbl_window, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_window, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_window, LV_ALIGN_BOTTOM_LEFT, 24, -14);

  lbl_graph_window_v = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_v, "V min/max --.-- / --.--");
  lv_obj_set_style_text_color(lbl_graph_window_v, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_v, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_v, LV_ALIGN_BOTTOM_MID, -130, -14);

  lbl_graph_window_i = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_i, "I min/max --.--- / --.---");
  lv_obj_set_style_text_color(lbl_graph_window_i, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_i, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_i, LV_ALIGN_BOTTOM_RIGHT, -22, -14);

  lv_obj_t* chip_col = lv_obj_create(screen_graph);
  lv_obj_set_size(chip_col, 92, 246);
  lv_obj_align(chip_col, LV_ALIGN_TOP_RIGHT, -24, 190);
  lv_obj_set_style_bg_opa(chip_col, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(chip_col, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(chip_col, 0, LV_PART_MAIN);
  lv_obj_clear_flag(chip_col, LV_OBJ_FLAG_SCROLLABLE);

  chip_graph_ok = create_state_chip(chip_col, "OK", 0x193425, UiTheme::kAccentOk, 0);
  chip_graph_mode = create_state_chip(chip_col, "M1", 0x24364A, UiTheme::kAccentI, 52);
  chip_graph_limit = create_state_chip(chip_col, "CV", 0x1F3A27, UiTheme::kAccentOk, 104);
  chip_graph_run = create_state_chip(chip_col, "RUN", 0x1F3A27, UiTheme::kAccentOk, 156);

  lv_obj_t* fault_row = lv_obj_create(screen_graph);
  lv_obj_set_size(fault_row, kDisplayWidth - 36, 30);
  lv_obj_align(fault_row, LV_ALIGN_BOTTOM_MID, 0, -44);
  lv_obj_set_style_bg_color(fault_row, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(fault_row, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(fault_row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(fault_row, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_fault_graph = lv_label_create(fault_row);
  lv_label_set_text(lbl_fault_graph, "OVP OK   OCP OK   OTP OK   SCP OK   LIM CV");
  lv_obj_set_style_text_color(lbl_fault_graph, lv_color_hex(UiTheme::kAccentOk), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_fault_graph, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_fault_graph);
}

void create_dashboard() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  create_splash_screen(scr);
  create_main_screen(scr);
  create_graph_screen(scr);
  set_active_screen(UiScreen::Splash);
}

// ── Telemetry label updates ────────────────────────────────────────────────
void update_telemetry_labels() {
  if (!kLiveTelemetryUiEnabled) return;

  const uint32_t now_ms = millis();
  if (now_ms - last_ui_update_ms < kUiUpdateMinMs) return;

  const DisplayTelemetry t = get_display_telemetry();
  if (t.rx_count == last_drawn_count &&
      t.last_seq == last_drawn_seq) return;

  last_ui_update_ms = now_ms;
  last_drawn_count = t.rx_count;
  last_drawn_seq   = t.last_seq;

    const uint16_t dv_mV = have_drawn_values
      ? static_cast<uint16_t>(abs(static_cast<int>(t.last_v12_mV) - static_cast<int>(last_v12_drawn_mV)))
      : 0;
    const int16_t di_mA = have_drawn_values
      ? static_cast<int16_t>(abs(static_cast<int>(t.last_i12_mA) - static_cast<int>(last_i12_drawn_mA)))
      : 0;
    const bool value_keepalive_due =
        (kValueKeepaliveMs > 0) && ((now_ms - last_value_draw_ms) >= kValueKeepaliveMs);
    const bool should_redraw_values = !have_drawn_values ||
                    dv_mV >= kVoltageUpdateDeadband_mV ||
                    di_mA >= kCurrentUpdateDeadband_mA ||
                    value_keepalive_due;
    if (!should_redraw_values) return;

    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), "%06.2f V", t.last_v12_mV / 1000.0f);
    if (strcmp(vbuf, last_voltage_text) != 0) {
      if (lbl_main_voltage) lv_label_set_text(lbl_main_voltage, vbuf);
      if (lbl_graph_voltage) {
        char v_graph[32];
        snprintf(v_graph, sizeof(v_graph), "V %s", vbuf);
        lv_label_set_text(lbl_graph_voltage, v_graph);
      }
      strncpy(last_voltage_text, vbuf, sizeof(last_voltage_text) - 1);
      last_voltage_text[sizeof(last_voltage_text) - 1] = '\0';
    }

    char ibuf[32];
    snprintf(ibuf, sizeof(ibuf), "%06.3f A", t.last_i12_mA / 1000.0f);
    if (strcmp(ibuf, last_current_text) != 0) {
      if (lbl_main_current) lv_label_set_text(lbl_main_current, ibuf);
      if (lbl_graph_current) {
        char i_graph[32];
        snprintf(i_graph, sizeof(i_graph), "I %s", ibuf);
        lv_label_set_text(lbl_graph_current, i_graph);
      }
      strncpy(last_current_text, ibuf, sizeof(last_current_text) - 1);
      last_current_text[sizeof(last_current_text) - 1] = '\0';
    }
  last_v12_drawn_mV = t.last_v12_mV;
  last_i12_drawn_mA = t.last_i12_mA;
  last_value_draw_ms = now_ms;
  have_drawn_values = true;

  if (kMinimalUiLabelsOnly) return;

  if (now_ms - last_detail_ui_update_ms < kDetailUiUpdateMinMs) return;
  last_detail_ui_update_ms = now_ms;

  char sbuf[112];
  snprintf(sbuf, sizeof(sbuf), "rx=%lu err=%lu uart=%lu bytes=%lu",
           static_cast<unsigned long>(t.rx_count),
           static_cast<unsigned long>(t.err_count),
           static_cast<unsigned long>(t.i2c_rx_count),
           static_cast<unsigned long>(t.uart_bytes));
  if (lbl_main_stats) lv_label_set_text(lbl_main_stats, sbuf);
  if (lbl_graph_stats) {
    char gbuf[112];
    snprintf(gbuf, sizeof(gbuf), "samples=%lu rx=%lu err=%lu",
             static_cast<unsigned long>(trend_count),
             static_cast<unsigned long>(t.rx_count),
             static_cast<unsigned long>(t.err_count));
    lv_label_set_text(lbl_graph_stats, gbuf);
  }

  if (lbl_status_link) {
    if (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500)) {
      lv_label_set_text(lbl_status_link, "LINK STALE");
      lv_obj_set_style_text_color(lbl_status_link, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
    } else if (demo_mode) {
      lv_label_set_text(lbl_status_link, "DEMO");
      lv_obj_set_style_text_color(lbl_status_link, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
    } else {
      lv_label_set_text(lbl_status_link, "LINK OK");
      lv_obj_set_style_text_color(lbl_status_link, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
    }
  }

  if (lbl_status_seq) {
    char qbuf[32];
    snprintf(qbuf, sizeof(qbuf), "SEQ %u", static_cast<unsigned>(t.last_seq));
    lv_label_set_text(lbl_status_seq, qbuf);
  }

  if (lbl_status_mode) {
    if (t.last_i12_mA >= 1500) {
      lv_label_set_text(lbl_status_mode, "CC");
      lv_obj_set_style_text_color(lbl_status_mode, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
    } else {
      lv_label_set_text(lbl_status_mode, "CV");
      lv_obj_set_style_text_color(lbl_status_mode, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
    }
  }

  if (lbl_fault_main || lbl_fault_graph) {
    const bool link_stale = (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500));
    const bool cc_mode = (t.last_i12_mA >= 1500);
    char fbuf[96];
    snprintf(fbuf, sizeof(fbuf), "OVP OK   OCP OK   OTP OK   SCP OK   LIM %s%s",
             cc_mode ? "CC" : "CV",
             link_stale ? "   COMM WARN" : "");
    if (lbl_fault_main) {
      lv_label_set_text(lbl_fault_main, fbuf);
      lv_obj_set_style_text_color(lbl_fault_main,
                                  link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                  LV_PART_MAIN);
    }
    if (lbl_fault_graph) {
      lv_label_set_text(lbl_fault_graph, fbuf);
      lv_obj_set_style_text_color(lbl_fault_graph,
                                  link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                  LV_PART_MAIN);
    }
  }

  const bool link_stale = (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500));
  const bool cc_mode = (t.last_i12_mA >= 1500);

  if (chip_main_ok) {
    lv_label_set_text(chip_main_ok, link_stale ? "WARN" : "OK");
    lv_obj_set_style_text_color(chip_main_ok,
                                link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }
  if (chip_graph_ok) {
    lv_label_set_text(chip_graph_ok, link_stale ? "WARN" : "OK");
    lv_obj_set_style_text_color(chip_graph_ok,
                                link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }

  if (chip_main_limit) {
    lv_label_set_text(chip_main_limit, cc_mode ? "CC" : "CV");
    lv_obj_set_style_text_color(chip_main_limit,
                                cc_mode ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }
  if (chip_graph_limit) {
    lv_label_set_text(chip_graph_limit, cc_mode ? "CC" : "CV");
    lv_obj_set_style_text_color(chip_graph_limit,
                                cc_mode ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }

  if (chip_main_run) {
    lv_label_set_text(chip_main_run, link_stale ? "WAIT" : "RUN");
    lv_obj_set_style_text_color(chip_main_run,
                                link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }
  if (chip_graph_run) {
    lv_label_set_text(chip_graph_run, link_stale ? "WAIT" : "RUN");
    lv_obj_set_style_text_color(chip_graph_run,
                                link_stale ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }

  if (lbl_status_uptime) {
    char ubuf[32];
    snprintf(ubuf, sizeof(ubuf), "UP %lus", static_cast<unsigned long>(millis() / 1000UL));
    lv_label_set_text(lbl_status_uptime, ubuf);
  }

  if (bar_main_voltage) {
    const int32_t v_scaled = map_i32(static_cast<int32_t>(t.last_v12_mV), 9000, 15000, 0, 1000);
    lv_bar_set_value(bar_main_voltage, static_cast<lv_coord_t>(v_scaled), LV_ANIM_OFF);
  }
  if (bar_main_current) {
    const int32_t i_scaled = map_i32(static_cast<int32_t>(t.last_i12_mA), -500, 3000, 0, 1000);
    lv_bar_set_value(bar_main_current, static_cast<lv_coord_t>(i_scaled), LV_ANIM_OFF);
  }

  if (trend_count != last_drawn_samples) {
    last_drawn_samples = trend_count;
    const TrendWindowStats stats = getTrendWindowStats(kChartPoints);
    char wbuf[128];
    if (!stats.has_data) {
      snprintf(wbuf, sizeof(wbuf), "win: waiting for samples");
      if (lbl_graph_window_v) lv_label_set_text(lbl_graph_window_v, "V min/max --.-- / --.--");
      if (lbl_graph_window_i) lv_label_set_text(lbl_graph_window_i, "I min/max --.--- / --.---");
    } else {
      snprintf(wbuf, sizeof(wbuf), "win %u s  V %.2f..%.2f  I %.3f..%.3f",
               static_cast<unsigned>(stats.samples),
               stats.min_v12_mV / 1000.0f,
               stats.max_v12_mV / 1000.0f,
               stats.min_i12_mA / 1000.0f,
               stats.max_i12_mA / 1000.0f);

      if (lbl_graph_window_v) {
        char v_win[64];
        snprintf(v_win, sizeof(v_win), "V min/max %.2f / %.2f",
                 stats.min_v12_mV / 1000.0f,
                 stats.max_v12_mV / 1000.0f);
        lv_label_set_text(lbl_graph_window_v, v_win);
      }
      if (lbl_graph_window_i) {
        char i_win[64];
        snprintf(i_win, sizeof(i_win), "I min/max %.3f / %.3f",
                 stats.min_i12_mA / 1000.0f,
                 stats.max_i12_mA / 1000.0f);
        lv_label_set_text(lbl_graph_window_i, i_win);
      }
    }
    if (lbl_window) lv_label_set_text(lbl_window, wbuf);
  }
}

// ── I2C helpers ────────────────────────────────────────────────────────────
bool i2cAddressResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// ── Serial command support ─────────────────────────────────────────────────
void printStatus() {
  Serial.printf("status board=0x30:%s touch=0x5D:%s demo=%s\n",
                i2cAddressResponds(kBoardCtrlAddr) ? "ok" : "missing",
                i2cAddressResponds(kTouchAddr)     ? "ok" : "missing",
                demo_mode ? "on" : "off");
}

void printRxStatus() {
  const auto t = disp_link_slave::snapshot();
  const uint32_t age_ms = (t.last_rx_ms == 0) ? 0 : (millis() - t.last_rx_ms);
  Serial.printf("rx frames=%lu errs=%lu uart=%lu seq=%u V12=%u mV I12=%d mA age=%lu ms uartB=%lu\n",
                static_cast<unsigned long>(t.rx_count),
                static_cast<unsigned long>(t.err_count),
                static_cast<unsigned long>(t.i2c_rx_count),
                static_cast<unsigned>(t.last_seq),
                static_cast<unsigned>(t.last_v12_mV),
                static_cast<int>(t.last_i12_mA),
                static_cast<unsigned long>(age_ms),
                static_cast<unsigned long>(t.uart_bytes));
}

void printOtaStatus() {
  Serial.printf("ota enabled=%s policy=forced-off\n", kOtaEnabled ? "yes" : "no");
}

void printLogStatus() {
  Serial.printf("log enabled=%s samples=%lu capacity=%lu sample_ms=%lu\n",
                trend_logging_enabled ? "yes" : "no",
                static_cast<unsigned long>(trend_count),
                static_cast<unsigned long>(kTrendCapacity),
                static_cast<unsigned long>(kTrendSampleMs));
  if (trend_count == 0) {
    Serial.println("log window: empty");
    return;
  }
  const TrendSample& oldest = trendAt(0);
  const TrendSample& newest = trendAt(trend_count - 1);
  Serial.printf("log window: oldest=%lu ms newest=%lu ms span=%lu ms\n",
                static_cast<unsigned long>(oldest.t_ms),
                static_cast<unsigned long>(newest.t_ms),
                static_cast<unsigned long>(newest.t_ms - oldest.t_ms));
}

void dumpLogCsv(int limit) {
  if (trend_count == 0) {
    Serial.println("csv: no samples");
    return;
  }

  size_t start_logical = 0;
  if (limit > 0 && static_cast<size_t>(limit) < trend_count) {
    start_logical = trend_count - static_cast<size_t>(limit);
  }

  Serial.println("csv_begin");
  Serial.printf("# target=%s\n", "crowpanel43");
  Serial.printf("# sample_period_ms=%lu\n", static_cast<unsigned long>(kTrendSampleMs));
  Serial.printf("# buffer_capacity=%lu\n", static_cast<unsigned long>(kTrendCapacity));
  Serial.printf("# exported_samples=%lu\n", static_cast<unsigned long>(trend_count - start_logical));
  Serial.printf("# ota_enabled=%s\n", kOtaEnabled ? "true" : "false");
  Serial.println("idx,t_ms,seq,v12_mV,v12_V,i12_mA,i12_A,rx_count,err_count,uart_frames");
  size_t out_idx = 0;
  for (size_t i = start_logical; i < trend_count; ++i) {
    const TrendSample& s = trendAt(i);
    Serial.printf("%lu,%lu,%u,%u,%.3f,%d,%.3f,%lu,%lu,%lu\n",
                  static_cast<unsigned long>(out_idx++),
                  static_cast<unsigned long>(s.t_ms),
                  static_cast<unsigned>(s.seq),
                  static_cast<unsigned>(s.v12_mV),
                  s.v12_mV / 1000.0f,
                  static_cast<int>(s.i12_mA),
                  s.i12_mA / 1000.0f,
                  static_cast<unsigned long>(s.rx_count),
                  static_cast<unsigned long>(s.err_count),
            static_cast<unsigned long>(s.uart_count));
  }
  Serial.println("csv_end");
}

// PROBE <pin> [ms]  — raw digitalRead loop on the given GPIO pin.
// Use 'PROBE 20 7000' to verify the HAT UART TX signal arrives on IO20.
void handleProbe(const String& rawArgs) {
  String args = rawArgs;
  args.trim();
  int spaceIdx = args.indexOf(' ');
  int pin = -1;
  uint32_t ms = 6000;
  if (spaceIdx < 0) {
    pin = args.toInt();
  } else {
    pin = args.substring(0, spaceIdx).toInt();
    ms  = static_cast<uint32_t>(args.substring(spaceIdx + 1).toInt());
  }
  if (pin < 0 || pin > 48) { Serial.println("ERR PROBE: bad pin"); return; }
  if (ms == 0) ms = 6000;
  Serial.printf("PROBE IO%d for %u ms...\n", pin, ms);
  gpio_reset_pin(static_cast<gpio_num_t>(pin));
  pinMode(pin, INPUT);
  uint32_t hi = 0, lo = 0, trans = 0;
  bool last = digitalRead(pin);
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    bool v = digitalRead(pin);
    if (v != last) { trans++; last = v; }
    if (v) hi++; else lo++;
  }
  Serial.printf("PROBE IO%d done: hi=%u lo=%u trans=%u\n", pin, hi, lo, trans);
}

void handleCommand(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.isEmpty()) return;
  if (line.equalsIgnoreCase("HELP")) {
    Serial.println("Commands: HELP, PING, STATUS, RX, OTA, SCREEN <MAIN|GRAPH|SPLASH>, SPLASH <ON|OFF>, DEMO <ON|OFF>, TOUR <ON|OFF>, PROBE <pin> [ms], LOG_START, LOG_STOP, LOG_STATUS, LOG_CLEAR, LOG_DUMP_CSV [N]");
    return;
  }
  if (line.equalsIgnoreCase("PING"))         { Serial.println("PONG"); return; }
  if (line.equalsIgnoreCase("STATUS"))       { printStatus();           return; }
  if (line.equalsIgnoreCase("RX"))           { printRxStatus();         return; }
  if (line.equalsIgnoreCase("OTA") )         { printOtaStatus();        return; }
  if (line.equalsIgnoreCase("LOG_START"))    { trend_logging_enabled = true;  Serial.println("ACK LOG_START"); return; }
  if (line.equalsIgnoreCase("LOG_STOP"))     { trend_logging_enabled = false; Serial.println("ACK LOG_STOP");  return; }
  if (line.equalsIgnoreCase("LOG_STATUS"))   { printLogStatus(); return; }
  if (line.equalsIgnoreCase("LOG_CLEAR"))    { trendClear(); Serial.println("ACK LOG_CLEAR"); return; }
  if (line.startsWith("SCREEN") || line.startsWith("screen")) {
    String arg = line.substring(6);
    arg.trim();
    if (arg.equalsIgnoreCase("MAIN")) {
      set_active_screen(UiScreen::Main);
      Serial.println("ACK SCREEN MAIN");
      return;
    }
    if (arg.equalsIgnoreCase("GRAPH")) {
      set_active_screen(UiScreen::Graph);
      Serial.println("ACK SCREEN GRAPH");
      return;
    }
    if (arg.equalsIgnoreCase("SPLASH")) {
      set_active_screen(UiScreen::Splash);
      splash_done = false;
      splash_start_ms = millis();
      Serial.println("ACK SCREEN SPLASH");
      return;
    }
    Serial.println("ERR SCREEN: use MAIN|GRAPH|SPLASH");
    return;
  }
  if (line.startsWith("SPLASH") || line.startsWith("splash")) {
    String arg = line.substring(6);
    arg.trim();
    if (arg.equalsIgnoreCase("ON")) {
      set_active_screen(UiScreen::Splash);
      splash_done = false;
      splash_start_ms = millis();
      Serial.println("ACK SPLASH ON");
      return;
    }
    if (arg.equalsIgnoreCase("OFF")) {
      splash_done = true;
      set_active_screen(UiScreen::Main);
      Serial.println("ACK SPLASH OFF");
      return;
    }
    Serial.println("ERR SPLASH: use ON|OFF");
    return;
  }
  if (line.startsWith("DEMO") || line.startsWith("demo")) {
    String arg = line.substring(4);
    arg.trim();
    if (arg.equalsIgnoreCase("ON")) {
      demo_mode = true;
      demo_tour = true;
      demo_start_ms = millis();
      demo_last_tour_switch_ms = demo_start_ms;
      Serial.println("ACK DEMO ON");
      return;
    }
    if (arg.equalsIgnoreCase("OFF")) {
      demo_mode = false;
      Serial.println("ACK DEMO OFF");
      return;
    }
    Serial.println("ERR DEMO: use ON|OFF");
    return;
  }
  if (line.startsWith("TOUR") || line.startsWith("tour")) {
    String arg = line.substring(4);
    arg.trim();
    if (arg.equalsIgnoreCase("ON")) {
      demo_tour = true;
      demo_last_tour_switch_ms = millis();
      Serial.println("ACK TOUR ON");
      return;
    }
    if (arg.equalsIgnoreCase("OFF")) {
      demo_tour = false;
      Serial.println("ACK TOUR OFF");
      return;
    }
    Serial.println("ERR TOUR: use ON|OFF");
    return;
  }
  if (line.startsWith("LOG_DUMP_CSV") || line.startsWith("log_dump_csv")) {
    int limit = 0;
    const int sp = line.indexOf(' ');
    if (sp > 0) {
      limit = line.substring(sp + 1).toInt();
    }
    dumpLogCsv(limit);
    return;
  }
  if (line.startsWith("PROBE") || line.startsWith("probe")) {
    handleProbe(line.substring(5));
    return;
  }
  Serial.printf("ERR unknown: %s\n", line.c_str());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("policy: OTA disabled");

  // IO19/IO20 are S3 USB-JTAG D-/D+ pads. Release them so Serial1 can own
  // them as UART1 TX/RX.  Must happen before Wire.begin() and Serial1.begin().
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
  gpio_reset_pin(GPIO_NUM_19);
  gpio_reset_pin(GPIO_NUM_20);

  Wire.begin(15, 16);
  delay(50);

  while (!i2cAddressResponds(kBoardCtrlAddr) || !i2cAddressResponds(kTouchAddr)) {
    Serial.println("waiting for I2C devices 0x30 + 0x5D...");
    Wire.beginTransmission(kBoardCtrlAddr);
    Wire.write(250);
    Wire.endTransmission();
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delay(120);
    pinMode(1, INPUT);
    delay(100);
  }
  Serial.println("I2C devices ready");

  // Backlight: 0 = max brightness, 245 = off (STC8H1K28 at 0x30).
  Wire.beginTransmission(kBoardCtrlAddr);
  Wire.write(0);
  Wire.endTransmission();

  printStatus();

  display.init();
  display.initDMA();
  display.setRotation(0);
  // Clear to black; keep write-context open so the first LVGL flush_cb
  // finds getStartCount() > 0 and calls endWrite() before pushImageDMA().
  display.fillScreen(TFT_BLACK);

  Serial.println("display init: OK");

  disp_link_slave::begin();

  init_lvgl();
  create_dashboard();
  splash_start_ms = millis();
  demo_start_ms = splash_start_ms;
  demo_last_tour_switch_ms = splash_start_ms;
  splash_done = false;

  Serial.println("ready");
}

void loop() {
  disp_link_slave::poll();

  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (!serialLine.isEmpty()) {
        handleCommand(serialLine);
        serialLine = "";
      }
    } else {
      serialLine += ch;
      if (serialLine.length() > 240) {
        serialLine = "";
        Serial.println("ERR line too long");
      }
    }
  }

  // Echo new telemetry frames to serial once per second.
  static uint32_t last_log_ms     = 0;
  static uint32_t last_logged_cnt = 0;
  const uint32_t  now             = millis();
  if (kRxSerialLogEnabled && now - last_log_ms >= 1000) {
    last_log_ms = now;
    const auto t = disp_link_slave::snapshot();
    if (t.rx_count != last_logged_cnt) {
      last_logged_cnt = t.rx_count;
      printRxStatus();
    }
  }

  static uint32_t last_sample_ms = 0;
  static uint32_t last_trend_rx_count = 0;
  if (now - last_sample_ms >= kTrendSampleMs) {
    last_sample_ms = now;
    if (trend_logging_enabled) {
      const DisplayTelemetry t = get_display_telemetry();
      if (t.rx_count != last_trend_rx_count) {
        disp_link_slave::Telemetry sampled = {};
        sampled.rx_count = t.rx_count;
        sampled.err_count = t.err_count;
        sampled.i2c_rx_count = t.i2c_rx_count;
        sampled.last_seq = t.last_seq;
        sampled.last_v12_mV = t.last_v12_mV;
        sampled.last_i12_mA = t.last_i12_mA;
        sampled.last_rx_ms = t.last_rx_ms;
        sampled.uart_bytes = t.uart_bytes;
        last_trend_rx_count = t.rx_count;
        trendPushSample(sampled, now);
      }
    }
  }

  update_telemetry_labels();  // updates LVGL labels if telemetry changed

  if (!splash_done && (now - splash_start_ms) >= kSplashDurationMs) {
    splash_done = true;
    set_active_screen(UiScreen::Main);
    if (lbl_splash_hint) {
      lv_label_set_text(lbl_splash_hint, "Telemetry synchronized");
    }
  }

  if (demo_mode && demo_tour && splash_done && (now - demo_last_tour_switch_ms) >= kDemoTourSwitchMs) {
    demo_last_tour_switch_ms = now;
    if (active_screen == UiScreen::Main) {
      set_active_screen(UiScreen::Graph);
    } else if (active_screen == UiScreen::Graph) {
      set_active_screen(UiScreen::Main);
    }
  }

  // Advance the LVGL tick counter.  LV_TICK_CUSTOM in lv_conf.h may not reach
  // the library compilation unit (GCC __has_include silent-fail), so we call
  // lv_tick_inc() explicitly to keep the internal counter correct.
  static uint32_t lv_last_tick_ms = 0;
  const uint32_t  lv_now_ms       = millis();
  lv_tick_inc(lv_now_ms - lv_last_tick_ms);
  lv_last_tick_ms = lv_now_ms;

  lv_timer_handler();         // runs LVGL internal tasks and triggers flush
  delay(2);
}
