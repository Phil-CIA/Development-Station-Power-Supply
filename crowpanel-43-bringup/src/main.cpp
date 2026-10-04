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
constexpr bool    kRxSerialLogEnabled = true;
constexpr uint32_t kTrendSampleMs = 200;
constexpr uint32_t kUiUpdateMinMs = 50;
constexpr uint32_t kDetailUiUpdateMinMs = 250;
constexpr uint32_t kSettingsUiUpdateMinMs = 300;
constexpr bool    kLiveChartsEnabled = true;
constexpr bool    kMinimalUiLabelsOnly = false;
constexpr uint8_t kChartDecimation = 2;
constexpr uint32_t kSplashDurationMs = 1800;
constexpr uint32_t kDemoFrameMs = 50;
constexpr uint32_t kDemoTourSwitchMs = 8000;
// 15 min at the 5 Hz cap (kTrendSampleMs). sizeof(TrendSample)=28 B -> 4500 * 28 = 126,000 B in PSRAM.
constexpr size_t   kTrendCapacity = 4500;
constexpr uint16_t kChartPoints   = 120;   // plotted points; each is every kChartDecimation-th sample
constexpr uint16_t kSetupCh1LimitMax_mA = 3000;
constexpr uint16_t kSetupCh2LimitMax_mA = 2000;
constexpr uint16_t kSetupStep_mA = 50;

// Dashboard (Main overview + channel detail)
constexpr uint32_t kLinkStaleMs = 1500;
constexpr uint32_t kDashboardRefreshMs = 100;
constexpr uint32_t kLimitGetRetryMs = 1200;
constexpr uint32_t kOutputAckTimeoutMs = 1500;
constexpr uint32_t kOutputConfirmMs = 10000;
constexpr uint32_t kNoticeMs = 4000;
constexpr uint16_t kDetailChartPoints = 120;
constexpr int kTouchMinPx = 44;

enum class UiScreen : uint8_t {
  Splash = 0,
  Setup = 1,
  Main = 2,
  Graph = 3,
  Settings = 4,
  Detail = 5,
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
  static constexpr uint32_t kPower = 0xD5DAE0;
  static constexpr uint32_t kError = 0xFF5A5F;
  static constexpr uint32_t kDemo = 0xB48CFF;
  static constexpr uint32_t kUnknown = 0x8A94A3;
  static constexpr uint32_t kInfoStrip = 0x171B20;
  static constexpr uint32_t kLimitTrace = 0x8FB4CC;
  static constexpr uint32_t kCh1 = 0x84D82E;
  static constexpr uint32_t kCh1Tint = 0x28361C;
  static constexpr uint32_t kCh2 = 0x2DD4BF;
  static constexpr uint32_t kCh2Tint = 0x1E3837;
  static constexpr uint32_t kScope = 0x0D1013;
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
static lv_obj_t* screen_setup = nullptr;
static lv_obj_t* screen_main = nullptr;
static lv_obj_t* screen_graph = nullptr;
static lv_obj_t* screen_settings = nullptr;

static lv_obj_t* lbl_setup_title = nullptr;
static lv_obj_t* lbl_setup_param = nullptr;
static lv_obj_t* lbl_setup_value = nullptr;
static lv_obj_t* lbl_setup_list = nullptr;
static lv_obj_t* lbl_setup_hint = nullptr;
static uint32_t setup_start_ms = 0;
static bool setup_done = false;
static lv_obj_t* btn_settings_system = nullptr;
static lv_obj_t* btn_settings_dataset = nullptr;
static lv_obj_t* btn_settings_about = nullptr;
static lv_obj_t* lbl_settings_detail_title = nullptr;
static lv_obj_t* lbl_settings_detail_body = nullptr;
static lv_obj_t* lbl_settings_hint = nullptr;
static lv_obj_t* lbl_settings_action_primary = nullptr;
static lv_obj_t* lbl_settings_action_secondary = nullptr;
static lv_obj_t* lbl_settings_action_refresh = nullptr;

enum class SetupField : uint8_t {
  Output = 0,
  Ch1CurrentLimit = 1,
  Ch2CurrentLimit = 2,
  Count = 3,
};

struct SetupBindingState {
  SetupField selected = SetupField::Output;
  bool editing = false;
  bool output_enabled = false;
  uint16_t ch1_limit_mA = kSetupCh1LimitMax_mA;
  uint16_t ch2_limit_mA = kSetupCh2LimitMax_mA;
  bool have_output = false;
  bool have_ch1_limit = false;
  bool have_ch2_limit = false;
  char last_error[96] = "";
};

static SetupBindingState setup_binding = {};

enum class SettingsMenu : uint8_t {
  System = 0,
  DataSet = 1,
  About = 2,
};

enum class SystemField : uint8_t {
  Language = 0,
  Brightness = 1,
  Volume = 2,
  Theme = 3,
  Count = 4,
};

struct SettingsState {
  SettingsMenu selected = SettingsMenu::System;
  SystemField system_field = SystemField::Language;
  bool editing = false;
  uint8_t language_index = 0;
  uint8_t brightness_pct = 80;
  uint8_t volume_pct = 40;
  bool theme_dark = true;
  uint8_t dataset_group = 1;
  char hint[128] = "Select a submenu to view and apply settings.";
};

static SettingsState settings_state = {};

// Dashboard (Main overview + single-channel detail, #78)
enum class LinkState : uint8_t { Live, Stale, Demo, Unknown };

struct Chip {
  lv_obj_t* box = nullptr;
  lv_obj_t* lbl = nullptr;
  uint32_t color = 0xFFFFFFFFu;
};

struct RailCard {
  lv_obj_t* panel = nullptr;
  lv_obj_t* accent = nullptr;
  lv_obj_t* row[3] = {};
  lv_obj_t* num[3] = {};
  Chip ch_chip;
  Chip status;
  lv_obj_t* note = nullptr;
  lv_obj_t* limit_val = nullptr;
  Chip limit_chip;
  Chip demo_tag;
};

struct TopBar {
  Chip link;
  lv_obj_t* out_btn = nullptr;
  lv_obj_t* out_main = nullptr;
  lv_obj_t* out_sub = nullptr;
  lv_obj_t* clock = nullptr;
  lv_obj_t* temp = nullptr;
  lv_obj_t* notice = nullptr;
  uint8_t out_mode = 0xFF;
};

struct OutputCommand {
  bool pending;
  uint32_t sent_ms;
  uint32_t ack_at_send;
  uint32_t err_at_send;
};

struct DashNotice {
  bool active;
  char text[80];
  uint32_t color;
  uint32_t until_ms;
};

static lv_obj_t* screen_detail = nullptr;
static lv_obj_t* out_confirm_layer = nullptr;
static uint32_t out_confirm_until_ms = 0;
static RailCard main_card[2];
static RailCard detail_card;
static TopBar main_bar;
static TopBar detail_bar;
static uint8_t detail_channel = 0;
static int8_t detail_styled_channel = -1;
static bool detail_chart_dirty = true;
static OutputCommand out_cmd = {};
static DashNotice dash_notice = {};

static lv_obj_t* detail_graph_panel = nullptr;
static lv_obj_t* detail_graph_accent = nullptr;
static lv_obj_t* detail_graph_title = nullptr;
static lv_obj_t* detail_graph_note = nullptr;
static lv_obj_t* detail_window_lbl = nullptr;
static lv_obj_t* detail_fixed_val = nullptr;
static lv_obj_t* detail_iset_val = nullptr;
static Chip detail_iset_chip;
static lv_obj_t* detail_axis_v[5] = {};
static lv_obj_t* detail_axis_i[5] = {};
static lv_obj_t* detail_chart = nullptr;
static lv_chart_series_t* detail_ser_v = nullptr;
static lv_chart_series_t* detail_ser_i = nullptr;
static lv_chart_series_t* detail_ser_lim = nullptr;
static size_t detail_last_trend_head = static_cast<size_t>(-1);
static size_t detail_last_trend_count = static_cast<size_t>(-1);
static uint32_t detail_last_chart_ms = 0;
static size_t detail_plotted = 0;

// Graph-screen pen order: CH1 V, CH1 I, CH2 V, CH2 I.
static lv_obj_t* lbl_pen_val[4] = {};
static lv_obj_t* lbl_graph_stats = nullptr;
static lv_obj_t* lbl_window = nullptr;
static lv_obj_t* lbl_graph_window_v = nullptr;
static lv_obj_t* lbl_graph_window_i = nullptr;
static lv_obj_t* lbl_graph_axis_v[5] = {};
static lv_obj_t* lbl_graph_axis_i[5] = {};
static lv_obj_t* lbl_graph_time[7] = {};

static lv_obj_t* lbl_splash_hint = nullptr;
static lv_obj_t* lbl_fault_graph = nullptr;

static lv_obj_t* chart_v     = nullptr;
static lv_obj_t* chart_i     = nullptr;
static lv_chart_series_t* chart_ch1_v_series = nullptr;
static lv_chart_series_t* chart_ch1_i_series = nullptr;
static lv_chart_series_t* chart_ch2_v_series = nullptr;
static lv_chart_series_t* chart_ch2_i_series = nullptr;

static UiScreen active_screen = UiScreen::Splash;
static bool splash_done = false;
static uint32_t splash_start_ms = 0;

// ── telemetry change-detection ─────────────────────────────────────────────
static uint32_t last_drawn_seq   = 0xFFFFFFFFu;
static uint32_t last_drawn_count = 0;
static uint32_t last_ui_update_ms = 0;
static size_t   last_drawn_samples = static_cast<size_t>(-1);
static uint16_t chart_write_idx = 0;
static uint32_t last_detail_ui_update_ms = 0;
static bool demo_mode = false;
static bool demo_tour = false;
static uint32_t demo_start_ms = 0;
static uint32_t demo_last_tour_switch_ms = 0;
static uint32_t last_settings_ui_update_ms = 0;

struct DisplayTelemetry {
  uint32_t rx_count;
  uint32_t err_count;
  uint32_t i2c_rx_count;
  uint32_t uart_bytes;
  uint8_t last_seq;
  uint16_t last_v12_mV;
  int16_t last_i12_mA;
  uint16_t last_v3v3_mV;
  int16_t last_i3v3_mA;
  uint8_t last_temp_C;
  uint8_t status;
  uint8_t protection_flags;
  bool has_extended;
  uint32_t last_rx_ms;
};

int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

struct TrendSample {
  uint32_t t_ms;
  uint32_t rx_count;
  uint32_t err_count;
  uint32_t uart_count;
  uint8_t seq;
  uint16_t v12_mV;
  int16_t i12_mA;
  uint16_t v3v3_mV;
  int16_t i3v3_mA;
  bool has_ch2;
  bool demo;
};

static TrendSample* trend_buf = nullptr;  // PSRAM, kTrendCapacity entries
static size_t trend_head = 0;
static size_t trend_count = 0;
static uint32_t trend_total = 0;  // monotonic; trend_count saturates when the ring is full
static bool trend_logging_enabled = true;

void setupRequestRefresh();
void updateSetupBindingsFromUdi();
void refreshSetupScreenLabels();
void refreshSettingsScreenLabels(bool force = false);
void enterSetupScreen();
void handleSetupEncoderRotate(int8_t detents);
void handleSetupEncoderPress();
void handleSetupEncoderLongPress();
void handleSettingsEncoderRotate(int8_t detents);
void handleSettingsEncoderPress();
void handleSettingsEncoderLongPress();
void settings_system_btn_event_cb(lv_event_t* e);
void settings_dataset_btn_event_cb(lv_event_t* e);
void settings_about_btn_event_cb(lv_event_t* e);
void settings_action_primary_event_cb(lv_event_t* e);
void settings_action_secondary_event_cb(lv_event_t* e);
void settings_action_refresh_event_cb(lv_event_t* e);
bool i2cAddressResponds(uint8_t address);
void printLogStatus();
void refreshDashboard(bool force);
void main_output_toggle_event_cb(lv_event_t* e);

void setup_prev_btn_event_cb(lv_event_t* e);
void setup_next_btn_event_cb(lv_event_t* e);
void setup_edit_btn_event_cb(lv_event_t* e);
void setup_done_btn_event_cb(lv_event_t* e);

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
  trend_total = 0;
  chart_write_idx = 0;
  lv_obj_t* const charts[4] = {chart_v, chart_v, chart_i, chart_i};
  lv_chart_series_t* const series[4] = {chart_ch1_v_series, chart_ch2_v_series,
                                        chart_ch1_i_series, chart_ch2_i_series};
  for (int k = 0; k < 4; ++k) {
    if (charts[k] && series[k]) lv_chart_set_all_value(charts[k], series[k], LV_CHART_POINT_NONE);
  }
  if (chart_v) lv_chart_refresh(chart_v);
  if (chart_i) lv_chart_refresh(chart_i);
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
  if (!chart_v || !chart_i || !chart_ch1_v_series || !chart_ch1_i_series ||
      !chart_ch2_v_series || !chart_ch2_i_series) return;

  // SHIFT mode scrolls right-to-left; CH2 stays blank for legacy frames that carry no CH2 data.
  lv_chart_set_next_value(chart_v, chart_ch1_v_series, static_cast<lv_coord_t>(s.v12_mV));
  lv_chart_set_next_value(chart_v, chart_ch2_v_series,
                          s.has_ch2 ? static_cast<lv_coord_t>(s.v3v3_mV) : LV_CHART_POINT_NONE);
  lv_chart_set_next_value(chart_i, chart_ch1_i_series, static_cast<lv_coord_t>(s.i12_mA));
  lv_chart_set_next_value(chart_i, chart_ch2_i_series,
                          s.has_ch2 ? static_cast<lv_coord_t>(s.i3v3_mA) : LV_CHART_POINT_NONE);
}

void trendInit() {
  trend_buf = static_cast<TrendSample*>(
      heap_caps_calloc(kTrendCapacity, sizeof(TrendSample), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!trend_buf) {
    Serial.println("trend: PSRAM buffer alloc failed, trend logging disabled");
    trend_logging_enabled = false;
  }
}

void trendPushSample(const disp_link_slave::Telemetry& t, uint32_t now_ms) {
  if (!trend_buf) return;
  TrendSample s = {};
  s.t_ms = now_ms;
  s.rx_count = t.rx_count;
  s.err_count = t.err_count;
  s.uart_count = t.i2c_rx_count;
  s.seq = t.last_seq;
  s.v12_mV = t.last_v12_mV;
  s.i12_mA = t.last_i12_mA;
  s.v3v3_mV = t.last_v3v3_mV;
  s.i3v3_mA = t.last_i3v3_mA;
  s.has_ch2 = t.has_extended;
  s.demo = demo_mode;
  trend_buf[trend_head] = s;
  trend_head = (trend_head + 1) % kTrendCapacity;
  if (trend_count < kTrendCapacity) trend_count++;
  trend_total++;

  // Decimate chart writes to limit redraw pressure while labels stay snappy.
  if ((trend_total % kChartDecimation) == 0) {
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
    raw.last_v3v3_mV,
    raw.last_i3v3_mA,
    raw.last_temp_C,
    raw.status,
    raw.protection_flags,
    raw.has_extended,
    raw.last_rx_ms,
  };

  if (!demo_mode) {
    return out;
  }

  const uint32_t now_ms = millis();
  const float t = (now_ms - demo_start_ms) * 0.001f;
  // Demo values sit on the fixed-rail nominals (+5 V / +3.3 V); the UI labels them DEMO.
  const float v = 5000.0f + 35.0f * sinf(t * 1.12f) + 12.0f * sinf(t * 0.31f);
  const float i = 900.0f + 620.0f * sinf(t * 0.87f + 1.35f) + 140.0f * sinf(t * 2.2f);

  out.rx_count = out.rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.i2c_rx_count = out.i2c_rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.last_seq = static_cast<uint8_t>((now_ms - demo_start_ms) / 200);
  out.last_v12_mV = static_cast<uint16_t>(clamp_i32(static_cast<int32_t>(v), 4800, 5200));
  out.last_i12_mA = static_cast<int16_t>(clamp_i32(static_cast<int32_t>(i), 0, 3000));
  out.last_v3v3_mV = static_cast<uint16_t>(clamp_i32(static_cast<int32_t>(3300.0f + 25.0f * sinf(t * 1.7f)), 3200, 3400));
  out.last_i3v3_mA = static_cast<int16_t>(clamp_i32(static_cast<int32_t>(420.0f + 130.0f * sinf(t * 1.9f + 0.6f)), 0, 2000));
  out.last_temp_C = static_cast<uint8_t>(clamp_i32(static_cast<int32_t>(31.0f + 5.0f * sinf(t * 0.22f)), 20, 95));
  out.status = 0xF0;  // CH1/CH2 enabled + CH1/CH2 in CV mode
  out.protection_flags = 0x00;
  out.has_extended = true;
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
  if (out_confirm_layer) lv_obj_add_flag(out_confirm_layer, LV_OBJ_FLAG_HIDDEN);
  if (screen_splash) {
    if (screen == UiScreen::Splash) lv_obj_clear_flag(screen_splash, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_splash, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_setup) {
    if (screen == UiScreen::Setup) lv_obj_clear_flag(screen_setup, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_setup, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_main) {
    if (screen == UiScreen::Main) lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_main, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_graph) {
    if (screen == UiScreen::Graph) lv_obj_clear_flag(screen_graph, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_graph, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_settings) {
    if (screen == UiScreen::Settings) lv_obj_clear_flag(screen_settings, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_settings, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen_detail) {
    if (screen == UiScreen::Detail) lv_obj_clear_flag(screen_detail, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_detail, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen == UiScreen::Detail) detail_chart_dirty = true;
  refreshDashboard(true);
}

const char* setupFieldLabel(SetupField field) {
  switch (field) {
    case SetupField::Output:
      return "Output Enable";
    case SetupField::Ch1CurrentLimit:
      return "CH1 Current Limit (I_max)";
    case SetupField::Ch2CurrentLimit:
      return "CH2 Current Limit (I_max)";
    default:
      return "--";
  }
}

bool parseOutputState(const char* payload, bool* enabled_out) {
  if (payload == nullptr || enabled_out == nullptr) return false;
  const char* text = payload;
  if (strncmp(text, "OUTPUT ", 7) == 0) {
    text += 7;
  }
  if (strncmp(text, "ON", 2) == 0) {
    *enabled_out = true;
    return true;
  }
  if (strncmp(text, "OFF", 3) == 0) {
    *enabled_out = false;
    return true;
  }
  return false;
}

bool parseIlimPayload(const char* payload, char* channel_out, size_t channel_len, uint16_t* limit_mA_out) {
  if (payload == nullptr || channel_out == nullptr || channel_len == 0 || limit_mA_out == nullptr) {
    return false;
  }
  char channel[8] = {0};
  unsigned int limit = 0;
  if (sscanf(payload, "ILIM %7s %u", channel, &limit) != 2) {
    return false;
  }
  strncpy(channel_out, channel, channel_len - 1);
  channel_out[channel_len - 1] = '\0';
  *limit_mA_out = static_cast<uint16_t>(limit);
  return true;
}

void applySetupAck(const char* ack_payload) {
  if (ack_payload == nullptr || ack_payload[0] == '\0') return;

  bool output_enabled = false;
  if (parseOutputState(ack_payload, &output_enabled)) {
    setup_binding.output_enabled = output_enabled;
    setup_binding.have_output = true;
    return;
  }

  char channel[8] = {0};
  uint16_t limit_mA = 0;
  if (parseIlimPayload(ack_payload, channel, sizeof(channel), &limit_mA)) {
    if (strcmp(channel, "CH1") == 0) {
      setup_binding.ch1_limit_mA = limit_mA;
      setup_binding.have_ch1_limit = true;
      return;
    }
    if (strcmp(channel, "CH2") == 0) {
      setup_binding.ch2_limit_mA = limit_mA;
      setup_binding.have_ch2_limit = true;
      return;
    }
  }
}

void applySetupEvent(const char* evt_payload) {
  // Reuse ACK parser for mirrored payloads (e.g. "OUTPUT ON", "ILIM CH1 3000 mA").
  applySetupAck(evt_payload);
}

void applySetupError(const char* err_payload) {
  if (err_payload == nullptr) return;
  strncpy(setup_binding.last_error, err_payload, sizeof(setup_binding.last_error) - 1);
  setup_binding.last_error[sizeof(setup_binding.last_error) - 1] = '\0';
}

void refreshSetupScreenLabels() {
  if (!lbl_setup_param || !lbl_setup_value || !lbl_setup_list || !lbl_setup_hint) return;

  lv_label_set_text(lbl_setup_param, setupFieldLabel(setup_binding.selected));

  char value_buf[48];
  if (setup_binding.selected == SetupField::Output) {
    snprintf(value_buf, sizeof(value_buf), "< %s >", setup_binding.output_enabled ? "ON" : "OFF");
  } else if (setup_binding.selected == SetupField::Ch1CurrentLimit) {
    snprintf(value_buf, sizeof(value_buf), "< %.3f A >", setup_binding.ch1_limit_mA / 1000.0f);
  } else {
    snprintf(value_buf, sizeof(value_buf), "< %.3f A >", setup_binding.ch2_limit_mA / 1000.0f);
  }
  lv_label_set_text(lbl_setup_value, value_buf);

  char list_buf[320];
  snprintf(list_buf,
           sizeof(list_buf),
           "%c Output Enable                 %s %s\n"
           "%c CH1 Current Limit (I_max)      %.3f A %s\n"
           "%c CH2 Current Limit (I_max)      %.3f A %s",
           setup_binding.selected == SetupField::Output ? '>' : ' ',
           setup_binding.output_enabled ? "ON " : "OFF",
           setup_binding.have_output ? "" : "(pending)",
           setup_binding.selected == SetupField::Ch1CurrentLimit ? '>' : ' ',
           setup_binding.ch1_limit_mA / 1000.0f,
           setup_binding.have_ch1_limit ? "" : "(pending)",
           setup_binding.selected == SetupField::Ch2CurrentLimit ? '>' : ' ',
           setup_binding.ch2_limit_mA / 1000.0f,
           setup_binding.have_ch2_limit ? "" : "(pending)");
  lv_label_set_text(lbl_setup_list, list_buf);

  if (setup_binding.last_error[0] != '\0') {
    char hint_buf[160];
    snprintf(hint_buf,
             sizeof(hint_buf),
             "Host error: %s",
             setup_binding.last_error);
    lv_label_set_text(lbl_setup_hint, hint_buf);
  } else if (setup_binding.editing) {
    lv_label_set_text(lbl_setup_hint, "Editing: rotate (Prev/Next) to adjust, press (Edit/Apply) to commit.");
  } else {
    lv_label_set_text(lbl_setup_hint, "Select with rotate (Prev/Next), press Edit/Apply to enter edit.");
  }
}

void setupSelectDelta(int8_t delta) {
  const int field_count = static_cast<int>(SetupField::Count);
  int idx = static_cast<int>(setup_binding.selected);
  idx += static_cast<int>(delta);
  if (idx < 0) idx = field_count - 1;
  if (idx >= field_count) idx = 0;
  setup_binding.selected = static_cast<SetupField>(idx);
}

void setupAdjustDelta(int32_t delta_mA) {
  if (setup_binding.selected == SetupField::Output) {
    setup_binding.output_enabled = !setup_binding.output_enabled;
    return;
  }
  if (setup_binding.selected == SetupField::Ch1CurrentLimit) {
    const int32_t next = clamp_i32(static_cast<int32_t>(setup_binding.ch1_limit_mA) + delta_mA, 0, kSetupCh1LimitMax_mA);
    setup_binding.ch1_limit_mA = static_cast<uint16_t>(next);
    return;
  }
  const int32_t next = clamp_i32(static_cast<int32_t>(setup_binding.ch2_limit_mA) + delta_mA, 0, kSetupCh2LimitMax_mA);
  setup_binding.ch2_limit_mA = static_cast<uint16_t>(next);
}

void setupRequestRefresh() {
  setup_binding.last_error[0] = '\0';
  const bool ok_output = disp_link_slave::sendCommand("GET OUTPUT");
  const bool ok_ch1 = disp_link_slave::sendCommand("GET ILIM CH1");
  const bool ok_ch2 = disp_link_slave::sendCommand("GET ILIM CH2");
  if (!ok_output || !ok_ch1 || !ok_ch2) {
    strncpy(setup_binding.last_error, "link not ready for GET refresh", sizeof(setup_binding.last_error) - 1);
    setup_binding.last_error[sizeof(setup_binding.last_error) - 1] = '\0';
  }
}

void updateSetupBindingsFromUdi() {
  static uint32_t last_udi_ack_count = 0;
  static uint32_t last_udi_err_count = 0;
  static uint32_t last_udi_evt_count = 0;

  const auto udi_link = disp_link_slave::commandSnapshot();
  // On UART0 the console is the host link; any non-CMD: line we print draws an ERR reply, which we would echo again.
  const bool echo = !disp_link_slave::telemetryOnConsoleSerial();
  if (udi_link.ack_count != last_udi_ack_count) {
    last_udi_ack_count = udi_link.ack_count;
    if (echo) Serial.printf("udi ack: %s\n", udi_link.last_ack[0] ? udi_link.last_ack : "(empty)");
    applySetupAck(udi_link.last_ack);
  }
  if (udi_link.err_count != last_udi_err_count) {
    last_udi_err_count = udi_link.err_count;
    if (echo) Serial.printf("udi err: %s\n", udi_link.last_err[0] ? udi_link.last_err : "(empty)");
    applySetupError(udi_link.last_err);
  }
  if (udi_link.evt_count != last_udi_evt_count) {
    last_udi_evt_count = udi_link.evt_count;
    if (echo) Serial.printf("udi evt: %s\n", udi_link.last_evt[0] ? udi_link.last_evt : "(empty)");
    applySetupEvent(udi_link.last_evt);
  }
}

const char* settingsMenuTitle(SettingsMenu menu) {
  switch (menu) {
    case SettingsMenu::System:
      return "SYSTEM";
    case SettingsMenu::DataSet:
      return "DATASET";
    case SettingsMenu::About:
      return "ABOUT";
    default:
      return "--";
  }
}

const char* settingsLanguageName(uint8_t language_index) {
  static constexpr const char* kLanguages[] = {
    "English",
    "Spanish",
  };
  const size_t idx = static_cast<size_t>(language_index) % (sizeof(kLanguages) / sizeof(kLanguages[0]));
  return kLanguages[idx];
}

const char* settingsSystemFieldName(SystemField field) {
  switch (field) {
    case SystemField::Language:
      return "Language";
    case SystemField::Brightness:
      return "Brightness";
    case SystemField::Volume:
      return "Volume";
    case SystemField::Theme:
      return "Theme";
    default:
      return "--";
  }
}

void settingsSelectMenuDelta(int8_t delta) {
  if (delta == 0) return;
  const int32_t count = static_cast<int32_t>(SettingsMenu::About) + 1;
  int32_t next = static_cast<int32_t>(settings_state.selected) + delta;
  while (next < 0) next += count;
  while (next >= count) next -= count;
  settings_state.selected = static_cast<SettingsMenu>(next);
}

void settingsSelectSystemFieldDelta(int8_t delta) {
  if (delta == 0) return;
  const int32_t count = static_cast<int32_t>(SystemField::Count);
  int32_t next = static_cast<int32_t>(settings_state.system_field) + delta;
  while (next < 0) next += count;
  while (next >= count) next -= count;
  settings_state.system_field = static_cast<SystemField>(next);
}

void settingsAdjustSystemFieldDelta(int8_t delta) {
  if (delta == 0) return;
  switch (settings_state.system_field) {
    case SystemField::Language: {
      const uint8_t count = 2;
      int16_t next = static_cast<int16_t>(settings_state.language_index) + delta;
      while (next < 0) next += count;
      while (next >= count) next -= count;
      settings_state.language_index = static_cast<uint8_t>(next);
      break;
    }
    case SystemField::Brightness: {
      const int32_t next = clamp_i32(static_cast<int32_t>(settings_state.brightness_pct) + (delta * 5), 5, 100);
      settings_state.brightness_pct = static_cast<uint8_t>(next);
      break;
    }
    case SystemField::Volume: {
      const int32_t next = clamp_i32(static_cast<int32_t>(settings_state.volume_pct) + (delta * 5), 0, 100);
      settings_state.volume_pct = static_cast<uint8_t>(next);
      break;
    }
    case SystemField::Theme:
      settings_state.theme_dark = !settings_state.theme_dark;
      break;
    default:
      break;
  }
}

void settingsAdjustDatasetGroupDelta(int8_t delta) {
  if (delta == 0) return;
  int16_t next = static_cast<int16_t>(settings_state.dataset_group) + delta;
  while (next < 1) next += 6;
  while (next > 6) next -= 6;
  settings_state.dataset_group = static_cast<uint8_t>(next);
}

void setSettingsHint(const char* hint) {
  if (hint == nullptr || hint[0] == '\0') return;
  strncpy(settings_state.hint, hint, sizeof(settings_state.hint) - 1);
  settings_state.hint[sizeof(settings_state.hint) - 1] = '\0';
}

void refreshSettingsMenuButtonStyle(lv_obj_t* btn, bool selected) {
  if (!btn) return;
  lv_obj_set_style_bg_color(btn,
                            lv_color_hex(selected ? UiTheme::kAccentI : UiTheme::kPanelSoft),
                            LV_PART_MAIN);
  lv_obj_set_style_border_color(btn,
                                lv_color_hex(selected ? UiTheme::kAccentI : UiTheme::kBorder),
                                LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, selected ? 2 : 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, selected ? 12 : 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(btn, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
}

void refreshSettingsMenuStyles() {
  refreshSettingsMenuButtonStyle(btn_settings_system, settings_state.selected == SettingsMenu::System);
  refreshSettingsMenuButtonStyle(btn_settings_dataset, settings_state.selected == SettingsMenu::DataSet);
  refreshSettingsMenuButtonStyle(btn_settings_about, settings_state.selected == SettingsMenu::About);
}

void applySettingsPrimaryAction() {
  if (settings_state.selected == SettingsMenu::System) {
    if (!settings_state.editing) {
      settings_state.editing = true;
      setSettingsHint("System edit enabled. Rotate to change value.");
      return;
    } else {
      settingsAdjustSystemFieldDelta(-1);
      setSettingsHint("System value decremented.");
    }
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    settingsAdjustDatasetGroupDelta(-1);
    setSettingsHint("DataSet group decremented.");
    return;
  }
  setSettingsHint("About is read-only.");
}

void applySettingsSecondaryAction() {
  if (settings_state.selected == SettingsMenu::System) {
    if (settings_state.editing) {
      settingsAdjustSystemFieldDelta(1);
      setSettingsHint("System value incremented.");
    } else {
      settingsSelectSystemFieldDelta(1);
      setSettingsHint("System field advanced.");
    }
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    settingsAdjustDatasetGroupDelta(1);
    setSettingsHint("DataSet group incremented.");
    return;
  }
  setSettingsHint("About is read-only.");
}

void applySettingsRefreshAction() {
  if (settings_state.selected == SettingsMenu::System) {
    settings_state.language_index = 0;
    settings_state.brightness_pct = 80;
    settings_state.volume_pct = 40;
    settings_state.theme_dark = true;
    setSettingsHint("System values reset to defaults.");
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    setSettingsHint("DataSet group staged for preset usage.");
    return;
  }
  setSettingsHint("About updated from live telemetry.");
}

void refreshSettingsScreenLabels(bool force) {
  if (!lbl_settings_detail_title || !lbl_settings_detail_body || !lbl_settings_hint) return;
  const uint32_t now_ms = millis();
  if (!force && (now_ms - last_settings_ui_update_ms) < kSettingsUiUpdateMinMs) return;
  last_settings_ui_update_ms = now_ms;

  refreshSettingsMenuStyles();
  lv_label_set_text(lbl_settings_detail_title, settingsMenuTitle(settings_state.selected));
  lv_label_set_text(lbl_settings_hint, settings_state.hint);

  const DisplayTelemetry t = get_display_telemetry();
  const bool link_live = demo_mode || (t.last_rx_ms != 0 && (now_ms - t.last_rx_ms) <= 1500);
  char body[420];

  if (settings_state.selected == SettingsMenu::System) {
    const bool editing = settings_state.editing;
    snprintf(body,
             sizeof(body),
             "Language:   %s\n"
             "Brightness: %u%%\n"
             "Volume:     %u%%\n"
             "Theme:      %s\n"
             "Selected:   %s  (%s)\n"
             "Telemetry:  %s (SEQ %u)",
             settingsLanguageName(settings_state.language_index),
             static_cast<unsigned>(settings_state.brightness_pct),
             static_cast<unsigned>(settings_state.volume_pct),
             settings_state.theme_dark ? "Dark" : "Light",
             settingsSystemFieldName(settings_state.system_field),
             editing ? "EDIT" : "VIEW",
             link_live ? "LIVE" : "STALE",
             static_cast<unsigned>(t.last_seq));
    lv_label_set_text(lbl_settings_action_primary, editing ? "Value -" : "Edit");
    lv_label_set_text(lbl_settings_action_secondary, editing ? "Value +" : "Next Field");
    lv_label_set_text(lbl_settings_action_refresh, "Defaults");
  } else if (settings_state.selected == SettingsMenu::DataSet) {
    const TrendWindowStats stats = getTrendWindowStats(kChartPoints);
    snprintf(body,
             sizeof(body),
             "Preset Group: M%u\n"
             "Samples:      %lu / %lu\n"
             "Latest RX:    %lu  ERR: %lu\n"
             "Window V:     %.2f .. %.2f V\n"
             "Window I:     %.3f .. %.3f A\n"
             "Status:       %s",
             static_cast<unsigned>(settings_state.dataset_group),
             static_cast<unsigned long>(trend_count),
             static_cast<unsigned long>(kTrendCapacity),
             static_cast<unsigned long>(t.rx_count),
             static_cast<unsigned long>(t.err_count),
             stats.has_data ? (stats.min_v12_mV / 1000.0f) : 0.0f,
             stats.has_data ? (stats.max_v12_mV / 1000.0f) : 0.0f,
             stats.has_data ? (stats.min_i12_mA / 1000.0f) : 0.0f,
             stats.has_data ? (stats.max_i12_mA / 1000.0f) : 0.0f,
             link_live ? "LINK OK" : "LINK STALE");
    lv_label_set_text(lbl_settings_action_primary, "Group -");
    lv_label_set_text(lbl_settings_action_secondary, "Group +");
    lv_label_set_text(lbl_settings_action_refresh, "Apply Group");
  } else {
    const unsigned long uptime_s = static_cast<unsigned long>(now_ms / 1000UL);
    snprintf(body,
             sizeof(body),
             "Model: Development Station PSU\n"
             "Display: CrowPanel 4.3\n"
             "FW: CrowPanel Firmware\n"
             "Build: %s %s\n"
             "Uptime: %lu s\n"
             "Transport: %s",
             __DATE__,
             __TIME__,
             uptime_s,
             disp_link_slave::telemetryOnConsoleSerial() ? "UART0 console-shared" : "UART1 dedicated");
    lv_label_set_text(lbl_settings_action_primary, "Read-Only");
    lv_label_set_text(lbl_settings_action_secondary, "Read-Only");
    lv_label_set_text(lbl_settings_action_refresh, "Refresh");
  }

  lv_label_set_text(lbl_settings_detail_body, body);
}

void selectSettingsMenu(SettingsMenu menu) {
  settings_state.selected = menu;
  settings_state.editing = false;
  setSettingsHint("Rotate=menu  Press=edit/select  Long=back.");
  refreshSettingsScreenLabels(true);
}

void settings_system_btn_event_cb(lv_event_t* /*e*/) {
  selectSettingsMenu(SettingsMenu::System);
}

void settings_dataset_btn_event_cb(lv_event_t* /*e*/) {
  selectSettingsMenu(SettingsMenu::DataSet);
}

void settings_about_btn_event_cb(lv_event_t* /*e*/) {
  selectSettingsMenu(SettingsMenu::About);
}

void settings_action_primary_event_cb(lv_event_t* /*e*/) {
  applySettingsPrimaryAction();
  refreshSettingsScreenLabels(true);
}

void settings_action_secondary_event_cb(lv_event_t* /*e*/) {
  applySettingsSecondaryAction();
  refreshSettingsScreenLabels(true);
}

void settings_action_refresh_event_cb(lv_event_t* /*e*/) {
  applySettingsRefreshAction();
  refreshSettingsScreenLabels(true);
}

void handleSettingsEncoderRotate(int8_t detents) {
  if (detents == 0 || active_screen != UiScreen::Settings) return;
  const int8_t direction = detents > 0 ? 1 : -1;
  if (!settings_state.editing) {
    settingsSelectMenuDelta(direction);
    setSettingsHint("Settings menu changed.");
  } else if (settings_state.selected == SettingsMenu::System) {
    settingsAdjustSystemFieldDelta(direction);
    setSettingsHint("System value changed.");
  } else if (settings_state.selected == SettingsMenu::DataSet) {
    settingsAdjustDatasetGroupDelta(direction);
    setSettingsHint("DataSet group changed.");
  } else {
    setSettingsHint("About is read-only.");
  }
  refreshSettingsScreenLabels(true);
}

void handleSettingsEncoderPress() {
  if (active_screen != UiScreen::Settings) return;
  if (settings_state.selected == SettingsMenu::About) {
    setSettingsHint("About refreshed.");
    refreshSettingsScreenLabels(true);
    return;
  }
  if (!settings_state.editing) {
    settings_state.editing = true;
    setSettingsHint("Edit mode enabled.");
    refreshSettingsScreenLabels(true);
    return;
  }

  if (settings_state.selected == SettingsMenu::System) {
    settingsSelectSystemFieldDelta(1);
    setSettingsHint("System field advanced.");
  } else {
    settings_state.editing = false;
    setSettingsHint("DataSet selection applied.");
  }
  refreshSettingsScreenLabels(true);
}

void handleSettingsEncoderLongPress() {
  if (active_screen != UiScreen::Settings) return;
  if (settings_state.editing) {
    settings_state.editing = false;
    setSettingsHint("Edit mode disabled.");
    refreshSettingsScreenLabels(true);
    return;
  }
  set_active_screen(UiScreen::Main);
}

void handleSetupEncoderRotate(int8_t detents) {
  if (detents == 0) return;
  const int8_t direction = detents > 0 ? 1 : -1;
  if (setup_binding.editing) {
    setupAdjustDelta(static_cast<int32_t>(direction) * static_cast<int32_t>(kSetupStep_mA));
  } else {
    setupSelectDelta(direction);
  }
  refreshSetupScreenLabels();
}

void handleSetupEncoderPress() {
  setup_binding.last_error[0] = '\0';
  if (!setup_binding.editing) {
    setup_binding.editing = true;
    refreshSetupScreenLabels();
    return;
  }

  bool sent = false;
  if (setup_binding.selected == SetupField::Output) {
    sent = disp_link_slave::sendCommand(setup_binding.output_enabled ? "OUTPUT ON" : "OUTPUT OFF");
  } else {
    char payload[40];
    snprintf(payload,
             sizeof(payload),
             "ILIM %s %u",
             setup_binding.selected == SetupField::Ch1CurrentLimit ? "CH1" : "CH2",
             static_cast<unsigned>(setup_binding.selected == SetupField::Ch1CurrentLimit
                                       ? setup_binding.ch1_limit_mA
                                       : setup_binding.ch2_limit_mA));
    sent = disp_link_slave::sendCommand(payload);
  }
  if (!sent) {
    strncpy(setup_binding.last_error, "link not ready", sizeof(setup_binding.last_error) - 1);
    setup_binding.last_error[sizeof(setup_binding.last_error) - 1] = '\0';
  }
  setup_binding.editing = false;
  refreshSetupScreenLabels();
}

void handleSetupEncoderLongPress() {
  setup_done = true;
  set_active_screen(UiScreen::Main);
}

void setup_prev_btn_event_cb(lv_event_t* /*e*/) {
  handleSetupEncoderRotate(-1);
}

void setup_next_btn_event_cb(lv_event_t* /*e*/) {
  handleSetupEncoderRotate(1);
}

void setup_edit_btn_event_cb(lv_event_t* /*e*/) {
  handleSetupEncoderPress();
}

void setup_done_btn_event_cb(lv_event_t* /*e*/) {
  handleSetupEncoderLongPress();
}

void enterSetupScreen() {
  set_active_screen(UiScreen::Setup);
  setup_done = false;
  setup_start_ms = millis();
  setup_binding.editing = false;
  setupRequestRefresh();
  refreshSetupScreenLabels();
}

void nav_btn_event_cb(lv_event_t* e) {
  const uintptr_t target = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (target == static_cast<uintptr_t>(UiScreen::Setup)) {
    enterSetupScreen();
    return;
  }
  if (target == static_cast<uintptr_t>(UiScreen::Main)) {
    set_active_screen(UiScreen::Main);
    return;
  }
  if (target == static_cast<uintptr_t>(UiScreen::Graph)) {
    set_active_screen(UiScreen::Graph);
    return;
  }
  if (target == static_cast<uintptr_t>(UiScreen::Settings)) {
    set_active_screen(UiScreen::Settings);
    refreshSettingsScreenLabels(true);
    return;
  }
}

lv_obj_t* create_nav_btn(lv_obj_t* parent, const char* text, UiScreen target, int x_ofs) {  lv_obj_t* btn = lv_btn_create(parent);
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

// ── Setup Screen (bound to host config over UDI command channel) ───────────
void create_setup_screen(lv_obj_t* root) {
  screen_setup = lv_obj_create(root);
  lv_obj_set_size(screen_setup, kDisplayWidth, kDisplayHeight);
  lv_obj_clear_flag(screen_setup, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(screen_setup, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(screen_setup, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(screen_setup, 0, LV_PART_MAIN);

  lv_obj_t* header = lv_obj_create(screen_setup);
  lv_obj_set_size(header, kDisplayWidth - 20, 60);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_color(header, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(header, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(header, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_setup_title = lv_label_create(header);
  lv_label_set_text(lbl_setup_title, "SETUP: Initial Configuration");
  lv_obj_set_style_text_color(lbl_setup_title, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_setup_title, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lbl_setup_title, LV_ALIGN_TOP_LEFT, 20, 10);

  lv_obj_t* hint = lv_label_create(header);
  lv_label_set_text(hint, "Encoder flow: rotate=Prev/Next, press=Edit/Apply, long-press=Done.");
  lv_obj_set_style_text_color(hint, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 20, -6);

  lv_obj_t* panel = lv_obj_create(screen_setup);
  lv_obj_set_size(panel, kDisplayWidth - 40, 340);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 88);
  lv_obj_set_style_bg_color(panel, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_setup_param = lv_label_create(panel);
  lv_label_set_text(lbl_setup_param, "CH1 Current Limit (I_max)");
  lv_obj_set_style_text_color(lbl_setup_param, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_setup_param, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lbl_setup_param, LV_ALIGN_TOP_LEFT, 20, 20);

  lbl_setup_value = lv_label_create(panel);
  lv_label_set_text(lbl_setup_value, "< -- >");
  lv_obj_set_style_text_color(lbl_setup_value, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_setup_value, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(lbl_setup_value, LV_ALIGN_TOP_MID, 0, 80);

  lbl_setup_list = lv_label_create(panel);
  lv_label_set_text(lbl_setup_list, "Loading setup values...");
  lv_obj_set_style_text_color(lbl_setup_list, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_setup_list, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_setup_list, LV_ALIGN_BOTTOM_LEFT, 20, -72);

  lv_obj_t* btn_prev = lv_btn_create(panel);
  lv_obj_set_size(btn_prev, 110, 40);
  lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 20, -16);
  lv_obj_set_style_bg_color(btn_prev, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_prev, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_prev, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_prev, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_prev, setup_prev_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_prev = lv_label_create(btn_prev);
  lv_label_set_text(lbl_prev, "Prev");
  lv_obj_center(lbl_prev);

  lv_obj_t* btn_next = lv_btn_create(panel);
  lv_obj_set_size(btn_next, 110, 40);
  lv_obj_align(btn_next, LV_ALIGN_BOTTOM_LEFT, 146, -16);
  lv_obj_set_style_bg_color(btn_next, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_next, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_next, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_next, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_next, setup_next_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_next = lv_label_create(btn_next);
  lv_label_set_text(lbl_next, "Next");
  lv_obj_center(lbl_next);

  lv_obj_t* btn_edit = lv_btn_create(panel);
  lv_obj_set_size(btn_edit, 156, 40);
  lv_obj_align(btn_edit, LV_ALIGN_BOTTOM_RIGHT, -176, -16);
  lv_obj_set_style_bg_color(btn_edit, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_edit, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_edit, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_edit, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_edit, setup_edit_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_edit = lv_label_create(btn_edit);
  lv_label_set_text(lbl_edit, "Edit / Apply");
  lv_obj_set_style_text_color(lbl_edit, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_center(lbl_edit);

  lv_obj_t* btn_done = lv_btn_create(panel);
  lv_obj_set_size(btn_done, 110, 40);
  lv_obj_align(btn_done, LV_ALIGN_BOTTOM_RIGHT, -20, -16);
  lv_obj_set_style_bg_color(btn_done, lv_color_hex(UiTheme::kAccentOk), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_done, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_done, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_done, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_done, setup_done_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_done = lv_label_create(btn_done);
  lv_label_set_text(lbl_done, "Done");
  lv_obj_set_style_text_color(lbl_done, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_center(lbl_done);

  lv_obj_t* footer = lv_obj_create(screen_setup);
  lv_obj_set_size(footer, kDisplayWidth - 40, 40);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(footer, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(footer, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(footer, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(footer, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_setup_hint = lv_label_create(footer);
  lv_label_set_text(lbl_setup_hint, "Refreshing host config via CMD:/ACK: ...");
  lv_obj_set_style_text_color(lbl_setup_hint, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_setup_hint, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_center(lbl_setup_hint);

  refreshSetupScreenLabels();
}

// ── Settings Screen (System/DataSet/About submenus) ─────────────────────────
void create_settings_screen(lv_obj_t* root) {
  screen_settings = lv_obj_create(root);
  lv_obj_set_size(screen_settings, kDisplayWidth, kDisplayHeight);
  lv_obj_clear_flag(screen_settings, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(screen_settings, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(screen_settings, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(screen_settings, 0, LV_PART_MAIN);

  lv_obj_t* header = lv_obj_create(screen_settings);
  lv_obj_set_size(header, kDisplayWidth - 20, 50);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_bg_color(header, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(header, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(header, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "SETTINGS");
  lv_obj_set_style_text_color(title, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

  create_nav_btn(header, "Graph", UiScreen::Graph, -120);
  create_nav_btn(header, "Main", UiScreen::Main, -10);

  lv_obj_t* menu_panel = lv_obj_create(screen_settings);
  lv_obj_set_size(menu_panel, 220, kDisplayHeight - 100);
  lv_obj_align(menu_panel, LV_ALIGN_TOP_LEFT, 20, 70);
  lv_obj_set_style_bg_color(menu_panel, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(menu_panel, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(menu_panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(menu_panel, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* menu_title = lv_label_create(menu_panel);
  lv_label_set_text(menu_title, "SUBMENU");
  lv_obj_set_style_text_color(menu_title, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(menu_title, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(menu_title, LV_ALIGN_TOP_LEFT, 14, 12);

  btn_settings_system = lv_btn_create(menu_panel);
  lv_obj_set_size(btn_settings_system, 188, 58);
  lv_obj_align(btn_settings_system, LV_ALIGN_TOP_MID, 0, 42);
  lv_obj_set_style_radius(btn_settings_system, 10, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_settings_system, settings_system_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_system = lv_label_create(btn_settings_system);
  lv_label_set_text(lbl_system, "System");
  lv_obj_set_style_text_font(lbl_system, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_system, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_center(lbl_system);

  btn_settings_dataset = lv_btn_create(menu_panel);
  lv_obj_set_size(btn_settings_dataset, 188, 58);
  lv_obj_align(btn_settings_dataset, LV_ALIGN_TOP_MID, 0, 112);
  lv_obj_set_style_radius(btn_settings_dataset, 10, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_settings_dataset, settings_dataset_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_dataset = lv_label_create(btn_settings_dataset);
  lv_label_set_text(lbl_dataset, "DataSet");
  lv_obj_set_style_text_font(lbl_dataset, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_dataset, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_center(lbl_dataset);

  btn_settings_about = lv_btn_create(menu_panel);
  lv_obj_set_size(btn_settings_about, 188, 58);
  lv_obj_align(btn_settings_about, LV_ALIGN_TOP_MID, 0, 182);
  lv_obj_set_style_radius(btn_settings_about, 10, LV_PART_MAIN);
  lv_obj_add_event_cb(btn_settings_about, settings_about_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_about = lv_label_create(btn_settings_about);
  lv_label_set_text(lbl_about, "About");
  lv_obj_set_style_text_font(lbl_about, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl_about, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_center(lbl_about);

  lv_obj_t* detail_panel = lv_obj_create(screen_settings);
  lv_obj_set_size(detail_panel, kDisplayWidth - 270, kDisplayHeight - 100);
  lv_obj_align(detail_panel, LV_ALIGN_TOP_RIGHT, -20, 70);
  lv_obj_set_style_bg_color(detail_panel, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(detail_panel, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(detail_panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(detail_panel, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_settings_detail_title = lv_label_create(detail_panel);
  lv_label_set_text(lbl_settings_detail_title, "SYSTEM");
  lv_obj_set_style_text_color(lbl_settings_detail_title, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_detail_title, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lbl_settings_detail_title, LV_ALIGN_TOP_LEFT, 18, 14);

  lbl_settings_detail_body = lv_label_create(detail_panel);
  lv_obj_set_width(lbl_settings_detail_body, kDisplayWidth - 320);
  lv_label_set_long_mode(lbl_settings_detail_body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(lbl_settings_detail_body, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_detail_body, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_settings_detail_body, LV_ALIGN_TOP_LEFT, 18, 52);

  lv_obj_t* action_row = lv_obj_create(detail_panel);
  lv_obj_set_size(action_row, kDisplayWidth - 306, 64);
  lv_obj_align(action_row, LV_ALIGN_BOTTOM_MID, 0, -54);
  lv_obj_set_style_bg_color(action_row, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_radius(action_row, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(action_row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(action_row, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* action_primary = lv_btn_create(action_row);
  lv_obj_set_size(action_primary, 150, 42);
  lv_obj_align(action_primary, LV_ALIGN_LEFT_MID, 12, 0);
  lv_obj_set_style_bg_color(action_primary, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_border_width(action_primary, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(action_primary, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(action_primary, settings_action_primary_event_cb, LV_EVENT_CLICKED, nullptr);
  lbl_settings_action_primary = lv_label_create(action_primary);
  lv_obj_set_style_text_color(lbl_settings_action_primary, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_action_primary, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_settings_action_primary);

  lv_obj_t* action_secondary = lv_btn_create(action_row);
  lv_obj_set_size(action_secondary, 150, 42);
  lv_obj_align(action_secondary, LV_ALIGN_LEFT_MID, 174, 0);
  lv_obj_set_style_bg_color(action_secondary, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
  lv_obj_set_style_border_width(action_secondary, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(action_secondary, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(action_secondary, settings_action_secondary_event_cb, LV_EVENT_CLICKED, nullptr);
  lbl_settings_action_secondary = lv_label_create(action_secondary);
  lv_obj_set_style_text_color(lbl_settings_action_secondary, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_action_secondary, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_settings_action_secondary);

  lv_obj_t* action_refresh = lv_btn_create(action_row);
  lv_obj_set_size(action_refresh, 170, 42);
  lv_obj_align(action_refresh, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_set_style_bg_color(action_refresh, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_border_width(action_refresh, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(action_refresh, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_radius(action_refresh, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(action_refresh, settings_action_refresh_event_cb, LV_EVENT_CLICKED, nullptr);
  lbl_settings_action_refresh = lv_label_create(action_refresh);
  lv_obj_set_style_text_color(lbl_settings_action_refresh, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_action_refresh, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_settings_action_refresh);

  lv_obj_t* footer = lv_obj_create(detail_panel);
  lv_obj_set_size(footer, kDisplayWidth - 306, 40);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(footer, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(footer, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(footer, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(footer, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_settings_hint = lv_label_create(footer);
  lv_label_set_text(lbl_settings_hint, settings_state.hint);
  lv_obj_set_style_text_color(lbl_settings_hint, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_settings_hint, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_center(lbl_settings_hint);

  selectSettingsMenu(SettingsMenu::System);
}

// ── Dashboard: Main overview + single-channel detail (#78) ─────────────────
// Geometry follows docs/display-project/mockups/crowpanel-fixed-rail-mockup.html.
// Only ASCII text and LV_SYMBOL_* glyphs are used (Montserrat has no U+00B7/U+00B0).
struct RailSpec {
  const char* name;
  const char* nominal_full;
  uint32_t color;
  uint32_t tint;
  int32_t v_axis_mV;
  int32_t i_axis_mA;
};

constexpr RailSpec kRails[2] = {
  {"+5V Supply", "+5.00 V", UiTheme::kCh1, UiTheme::kCh1Tint, 6000, 4000},
  {"+3.3V Supply", "+3.30 V", UiTheme::kCh2, UiTheme::kCh2Tint, 4000, 3000},
};

constexpr int kBackBtnW = 110;
constexpr int kBackBtnH = 44;
constexpr int kOutBtnW = 240;
constexpr int kOutBtnH = 46;
constexpr int kNavBtnW = 200;
constexpr int kNavBtnH = 52;
constexpr int kCardW = 388;
constexpr int kCardH = 336;
constexpr int kEditBtnW = 208;
constexpr int kEditBtnH = 56;
static_assert(kBackBtnW >= kTouchMinPx && kBackBtnH >= kTouchMinPx, "Back button below 44x44");
static_assert(kOutBtnW >= kTouchMinPx && kOutBtnH >= kTouchMinPx, "OUTPUT button below 44x44");
static_assert(kNavBtnW >= kTouchMinPx && kNavBtnH >= kTouchMinPx, "Nav button below 44x44");
static_assert(kCardW >= kTouchMinPx && kCardH >= kTouchMinPx, "Channel card below 44x44");
static_assert(kEditBtnW >= kTouchMinPx && kEditBtnH >= kTouchMinPx, "Edit button below 44x44");

constexpr uint8_t kStatusEnabledAny = 0xC0u;  // CH1 | CH2 enabled bits
constexpr uint8_t kTripOvp = 1u;
constexpr uint8_t kTripOcp = 2u;
constexpr uint8_t kTripOtp = 4u;

static Chip detail_top_chip;

LinkState linkStateOf(const DisplayTelemetry& t, uint32_t now_ms, uint32_t* age_ms) {
  if (age_ms) *age_ms = 0;
  if (demo_mode) return LinkState::Demo;
  if (t.last_rx_ms == 0) return LinkState::Unknown;
  const uint32_t age = (now_ms >= t.last_rx_ms) ? (now_ms - t.last_rx_ms) : 0;
  if (age_ms) *age_ms = age;
  return age > kLinkStaleMs ? LinkState::Stale : LinkState::Live;
}

void setLabel(lv_obj_t* lbl, const char* text) {
  if (!lbl || !text) return;
  const char* cur = lv_label_get_text(lbl);
  if (cur && strcmp(cur, text) == 0) return;
  lv_label_set_text(lbl, text);
}

void setHidden(lv_obj_t* obj, bool hidden) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void setTextOpa(lv_obj_t* obj, lv_opa_t opa) {
  if (!obj || lv_obj_get_style_text_opa(obj, LV_PART_MAIN) == opa) return;
  lv_obj_set_style_text_opa(obj, opa, LV_PART_MAIN);
}

lv_obj_t* makeBox(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, uint32_t border,
                  int border_w, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, border_w, LV_PART_MAIN);
  lv_obj_set_style_border_color(o, lv_color_hex(border), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* makeLabel(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                    const lv_font_t* font, uint32_t color,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(l, w);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
  lv_obj_set_pos(l, x, y + (h - static_cast<int>(lv_font_get_line_height(font))) / 2);
  return l;
}

void makeChip(lv_obj_t* parent, int x, int y, int w, int h, const lv_font_t* font, Chip& chip) {
  chip.box = makeBox(parent, x, y, w, h, UiTheme::kBg, UiTheme::kBorder, 2, 6);
  chip.lbl = lv_label_create(chip.box);
  lv_label_set_long_mode(chip.lbl, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(chip.lbl, w - 12);
  lv_label_set_text(chip.lbl, "");
  lv_obj_set_style_text_font(chip.lbl, font, LV_PART_MAIN);
  lv_obj_set_style_text_align(chip.lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(chip.lbl, LV_ALIGN_CENTER, 0, 0);
}

void setChip(Chip& chip, const char* text, uint32_t color) {
  if (!chip.box || !chip.lbl) return;
  setLabel(chip.lbl, text);
  if (chip.color == color) return;
  chip.color = color;
  lv_obj_set_style_text_color(chip.lbl, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_border_color(chip.box, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(chip.box,
                            lv_color_mix(lv_color_hex(color), lv_color_hex(UiTheme::kBg), 36),
                            LV_PART_MAIN);
}

void fillChip(Chip& chip, uint32_t color) {
  if (!chip.box || !chip.lbl) return;
  chip.color = color;
  lv_obj_set_style_text_color(chip.lbl, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_color(chip.box, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(chip.box, lv_color_hex(color), LV_PART_MAIN);
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, uint32_t border,
                     int border_w, int radius) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(b, radius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, border_w, LV_PART_MAIN);
  lv_obj_set_style_border_color(b, lv_color_hex(border), LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  return b;
}

lv_obj_t* makeScreen(lv_obj_t* root) {
  lv_obj_t* s = lv_obj_create(root);
  lv_obj_set_pos(s, 0, 0);
  lv_obj_set_size(s, kDisplayWidth, kDisplayHeight);
  lv_obj_set_style_pad_all(s, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(s, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_border_width(s, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

void postNotice(const char* text, uint32_t color, uint32_t now_ms) {
  strncpy(dash_notice.text, text, sizeof(dash_notice.text) - 1);
  dash_notice.text[sizeof(dash_notice.text) - 1] = '\0';
  dash_notice.color = color;
  dash_notice.until_ms = now_ms + kNoticeMs;
  dash_notice.active = true;
}

void dash_open_detail_cb(lv_event_t* e) {
  const uintptr_t ch = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  detail_channel = (ch == 1u) ? 1u : 0u;
  set_active_screen(UiScreen::Detail);
}

void dash_back_cb(lv_event_t* /*e*/) {
  set_active_screen(UiScreen::Main);
}

// Opens the existing Setup editor on this channel's ILIM field; no new write path.
void dash_edit_limit_cb(lv_event_t* /*e*/) {
  setup_binding.selected = (detail_channel == 0u) ? SetupField::Ch1CurrentLimit
                                                  : SetupField::Ch2CurrentLimit;
  enterSetupScreen();
}

// Tapping the detail graph panel opens the dedicated Graph screen.
void dash_open_graph_cb(lv_event_t* /*e*/) {
  set_active_screen(UiScreen::Graph);
}

void sendOutputCommand(bool turn_on) {
  const uint32_t now_ms = millis();
  const auto link = disp_link_slave::commandSnapshot();
  if (!disp_link_slave::sendCommand(turn_on ? "OUTPUT ON" : "OUTPUT OFF")) {
    postNotice("OUTPUT: link not ready - command not sent", UiTheme::kAccentWarn, now_ms);
    refreshDashboard(true);
    return;
  }
  out_cmd.pending = true;
  out_cmd.sent_ms = now_ms;
  out_cmd.ack_at_send = link.ack_count;
  out_cmd.err_at_send = link.err_count;
  refreshDashboard(true);
}

// OFF is one tap; ON must be confirmed in the dialog because it energizes both rails.
void main_output_toggle_event_cb(lv_event_t* /*e*/) {
  if (out_cmd.pending) return;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  // The button is disabled in every other state; guard anyway.
  if (linkStateOf(t, now_ms, nullptr) != LinkState::Live || !t.has_extended) return;

  if ((t.status & kStatusEnabledAny) != 0u) {
    sendOutputCommand(false);
    return;
  }
  out_confirm_until_ms = now_ms + kOutputConfirmMs;
  setHidden(out_confirm_layer, false);
}

void output_confirm_cancel_cb(lv_event_t* /*e*/) {
  setHidden(out_confirm_layer, true);
}

void output_confirm_ok_cb(lv_event_t* /*e*/) {
  setHidden(out_confirm_layer, true);
  if (out_cmd.pending) return;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  if (linkStateOf(t, now_ms, nullptr) != LinkState::Live || !t.has_extended) {
    postNotice("OUTPUT ON cancelled - link not live", UiTheme::kAccentWarn, now_ms);
    return;
  }
  if ((t.status & kStatusEnabledAny) != 0u) return;
  sendOutputCommand(true);
}

void createTopBar(lv_obj_t* parent, bool detail, TopBar& bar) {
  makeBox(parent, 0, 0, kDisplayWidth, 52, UiTheme::kStatusBar, UiTheme::kBorder, 0, 0);
  if (detail) {
    lv_obj_t* back = makeButton(parent, 8, 4, kBackBtnW, kBackBtnH, UiTheme::kPanelSoft,
                                UiTheme::kTextMuted, 2, 8);
    lv_obj_add_event_cb(back, dash_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(bl, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
    lv_obj_center(bl);
    makeChip(parent, 126, 8, 200, 36, &lv_font_montserrat_28, detail_top_chip);
    makeChip(parent, 334, 10, 160, 32, &lv_font_montserrat_20, bar.link);
  } else {
    makeLabel(parent, 12, 0, 210, 52, "WORKSTATION PSU", &lv_font_montserrat_20, UiTheme::kTextPrimary);
    makeChip(parent, 226, 10, 200, 32, &lv_font_montserrat_20, bar.link);
  }

  // The one shared OUTPUT control; it affects both channels together on every screen.
  bar.out_btn = makeButton(parent, 552, 3, kOutBtnW, kOutBtnH, UiTheme::kPanelSoft,
                           UiTheme::kBorder, 2, 8);
  lv_obj_add_event_cb(bar.out_btn, main_output_toggle_event_cb, LV_EVENT_CLICKED, nullptr);
  bar.out_main = makeLabel(bar.out_btn, 0, 1, kOutBtnW - 4, 22, "OUTPUT ?",
                           &lv_font_montserrat_20, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  bar.out_sub = makeLabel(bar.out_btn, 0, 22, kOutBtnW - 4, 18, "BOTH CHANNELS",
                          &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);

  // Info strip: CrowPanel uptime stands in until a time source exists (WiFi time is a later PR).
  makeBox(parent, 0, 52, kDisplayWidth, 28, UiTheme::kInfoStrip, UiTheme::kBorder, 0, 0);
  bar.clock = makeLabel(parent, 12, 52, 250, 28, "UPTIME 0:00:00", &lv_font_montserrat_20, UiTheme::kTextMuted);
  bar.temp = makeLabel(parent, 290, 52, 230, 28, "UNIT TEMP -- C", &lv_font_montserrat_20,
                       UiTheme::kTextPrimary);
}

// Command-result notice sits in the empty right side of the info strip and scrolls long text.
void createNotice(lv_obj_t* parent, TopBar& bar) {
  lv_obj_t* n = lv_label_create(parent);
  lv_label_set_long_mode(n, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(n, 264);
  lv_label_set_text(n, "");
  lv_obj_set_style_text_font(n, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(n, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_bg_color(n, lv_color_hex(UiTheme::kInfoStrip), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(n, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(n, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(n, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_radius(n, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(n, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(n, 2, LV_PART_MAIN);
  lv_obj_set_pos(n, 524, 53);
  lv_obj_add_flag(n, LV_OBJ_FLAG_HIDDEN);
  bar.notice = n;
}

void createReadoutRow(lv_obj_t* parent, RailCard& c, int idx, int x, int y, int w, int label_w,
                      const char* unit, const char* caption, uint32_t color, uint32_t caption_color) {
  lv_obj_t* row = makeBox(parent, x, y, w - label_w, 56, UiTheme::kBg, UiTheme::kBg, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  c.row[idx] = row;

  lv_obj_t* num = lv_label_create(row);
  lv_label_set_text(num, "---");
  lv_obj_set_style_text_font(num, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_style_text_color(num, lv_color_hex(color), LV_PART_MAIN);
  c.num[idx] = num;

  lv_obj_t* u = lv_label_create(row);
  lv_label_set_text(u, unit);
  lv_obj_set_style_text_font(u, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_set_style_text_color(u, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_pad_left(u, 8, LV_PART_MAIN);

  makeLabel(parent, x + w - label_w, y, label_w, 56, caption, &lv_font_montserrat_16,
            caption_color, LV_TEXT_ALIGN_RIGHT);
}

void createMainCard(lv_obj_t* parent, int ch) {
  RailCard& c = main_card[ch];
  const RailSpec& r = kRails[ch];
  const int x = (ch == 0) ? 8 : 404;
  lv_obj_t* card = makeBox(parent, x, 84, kCardW, kCardH, r.tint, r.color, 2, 10);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_clip_corner(card, true, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, lv_color_mix(lv_color_hex(0xFFFFFF), lv_color_hex(r.tint), 32),
                            static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_PRESSED);
  lv_obj_add_event_cb(card, dash_open_detail_cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(ch)));
  c.panel = card;
  c.accent = makeBox(card, 0, 0, kCardW - 4, 6, r.color, r.color, 0, 0);

  makeChip(card, 12, 12, 200, 36, &lv_font_montserrat_28, c.ch_chip);
  setLabel(c.ch_chip.lbl, r.name);
  fillChip(c.ch_chip, r.color);
  makeChip(card, 220, 12, 76, 32, &lv_font_montserrat_16, c.demo_tag);
  setChip(c.demo_tag, "DEMO", UiTheme::kDemo);
  lv_obj_add_flag(c.demo_tag.box, LV_OBJ_FLAG_HIDDEN);
  makeLabel(card, kCardW - 44, 12, 36, 36, LV_SYMBOL_RIGHT, &lv_font_montserrat_28,
            UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);

  createReadoutRow(card, c, 0, 12, 52, kCardW - 24, 104, "V", "VOLTAGE", UiTheme::kAccentV, UiTheme::kAccentV);
  createReadoutRow(card, c, 1, 12, 106, kCardW - 24, 104, "A", "CURRENT", UiTheme::kAccentI, UiTheme::kAccentI);
  createReadoutRow(card, c, 2, 12, 160, kCardW - 24, 104, "W", "POWER", UiTheme::kPower, UiTheme::kTextMuted);

  makeLabel(card, 12, 222, 72, 36, "I LIMIT", &lv_font_montserrat_16, UiTheme::kTextMuted);
  c.limit_val = makeLabel(card, 88, 222, 124, 36, "-.--- A", &lv_font_montserrat_28, UiTheme::kAccentI);
  makeChip(card, 216, 224, kCardW - 24 - 204, 32, &lv_font_montserrat_16, c.limit_chip);
  setChip(c.limit_chip, "NO VALUE", UiTheme::kUnknown);

  makeChip(card, 12, 264, kCardW - 24, 36, &lv_font_montserrat_20, c.status);
  setChip(c.status, "UNKNOWN", UiTheme::kUnknown);
  c.note = makeLabel(card, 12, 304, kCardW - 24, 24, "", &lv_font_montserrat_16, UiTheme::kTextMuted);
}

// Bottom navigation shared by Main and Detail; Detail counts as part of Main.
void createBottomNav(lv_obj_t* parent) {
  makeBox(parent, 0, 428, kDisplayWidth, 52, UiTheme::kStatusBar, UiTheme::kBorder, 0, 0);
  static constexpr const char* kNavNames[4] = {"Main", "Setup", "Graph", "Settings"};
  static constexpr UiScreen kNavTargets[4] = {UiScreen::Main, UiScreen::Setup, UiScreen::Graph, UiScreen::Settings};
  for (int k = 0; k < 4; ++k) {
    const bool on = (k == 0);
    lv_obj_t* b = makeButton(parent, k * kNavBtnW, 428, kNavBtnW, kNavBtnH,
                             on ? UiTheme::kBadge : UiTheme::kPanelSoft,
                             on ? UiTheme::kAccentI : UiTheme::kBorder, on ? 2 : 1, 0);
    lv_obj_add_event_cb(b, nav_btn_event_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(kNavTargets[k])));
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, kNavNames[k]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(on ? UiTheme::kTextPrimary : UiTheme::kTextMuted), LV_PART_MAIN);
    lv_obj_center(l);
  }
}

// Full-screen scrim on the top layer so nothing underneath is touchable while confirming.
void createOutputConfirm() {
  lv_obj_t* scrim = lv_obj_create(lv_layer_top());
  lv_obj_set_size(scrim, kDisplayWidth, kDisplayHeight);
  lv_obj_set_pos(scrim, 0, 0);
  lv_obj_set_style_radius(scrim, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(scrim, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scrim, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scrim, LV_OPA_70, LV_PART_MAIN);
  lv_obj_clear_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
  out_confirm_layer = scrim;

  lv_obj_t* dlg = makeBox(scrim, 140, 124, 520, 232, UiTheme::kPanel, UiTheme::kAccentWarn, 3, 12);
  makeLabel(dlg, 0, 14, 514, 36, "TURN OUTPUT ON?", &lv_font_montserrat_28,
            UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  makeLabel(dlg, 16, 62, 482, 60,
            "Both channels will switch ON together:\n+5V Supply and +3.3V Supply",
            &lv_font_montserrat_20, UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);

  lv_obj_t* cancel = makeButton(dlg, 16, 142, 236, 64, UiTheme::kPanelSoft, UiTheme::kTextMuted, 2, 8);
  lv_obj_add_event_cb(cancel, output_confirm_cancel_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(cancel, 0, 0, 232, 60, "CANCEL", &lv_font_montserrat_28, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);

  lv_obj_t* ok = makeButton(dlg, 268, 142, 236, 64, 0x1E4A33, UiTheme::kAccentOk, 2, 8);
  lv_obj_add_event_cb(ok, output_confirm_ok_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(ok, 0, 0, 232, 60, "TURN ON", &lv_font_montserrat_28, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
}

void create_main_screen(lv_obj_t* root) {
  screen_main = makeScreen(root);
  createTopBar(screen_main, false, main_bar);
  createMainCard(screen_main, 0);
  createMainCard(screen_main, 1);
  createBottomNav(screen_main);
  createNotice(screen_main, main_bar);
}

void create_detail_screen(lv_obj_t* root) {
  screen_detail = makeScreen(root);
  createTopBar(screen_detail, true, detail_bar);

  RailCard& c = detail_card;
  c.panel = makeBox(screen_detail, 8, 84, 320, 316, UiTheme::kPanel, UiTheme::kBorder, 2, 10);
  lv_obj_set_style_clip_corner(c.panel, true, LV_PART_MAIN);
  c.accent = makeBox(c.panel, 0, 0, 316, 6, UiTheme::kBorder, UiTheme::kBorder, 0, 0);
  makeChip(c.panel, 12, 12, 200, 36, &lv_font_montserrat_28, c.ch_chip);
  createReadoutRow(c.panel, c, 0, 12, 44, 292, 84, "V", "VOLTAGE", UiTheme::kAccentV, UiTheme::kAccentV);
  createReadoutRow(c.panel, c, 1, 12, 96, 292, 84, "A", "CURRENT", UiTheme::kAccentI, UiTheme::kAccentI);
  createReadoutRow(c.panel, c, 2, 12, 148, 292, 84, "W", "POWER", UiTheme::kPower, UiTheme::kTextMuted);
  makeChip(c.panel, 12, 208, 292, 34, &lv_font_montserrat_20, c.status);
  setChip(c.status, "UNKNOWN", UiTheme::kUnknown);
  c.note = makeLabel(c.panel, 12, 244, 292, 24, "", &lv_font_montserrat_16, UiTheme::kTextMuted);

  detail_graph_panel = makeBox(screen_detail, 336, 84, 456, 276, UiTheme::kPanel, UiTheme::kBorder, 2, 10);
  lv_obj_set_style_clip_corner(detail_graph_panel, true, LV_PART_MAIN);
  lv_obj_add_flag(detail_graph_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(detail_graph_panel,
                            lv_color_mix(lv_color_hex(0xFFFFFF), lv_color_hex(UiTheme::kPanel), 24),
                            static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_PRESSED);
  lv_obj_add_event_cb(detail_graph_panel, dash_open_graph_cb, LV_EVENT_CLICKED, nullptr);
  detail_graph_accent = makeBox(detail_graph_panel, 0, 0, 452, 6, UiTheme::kBorder, UiTheme::kBorder, 0, 0);
  detail_graph_title = makeLabel(detail_graph_panel, 12, 10, 250, 24, "TREND", &lv_font_montserrat_16, UiTheme::kTextPrimary);
  lv_obj_t* legend = makeLabel(detail_graph_panel, 250, 10, 190, 24,
                               "#F5C316 V#   #2EA5F9 I#   #8FB4CC LIMIT#",
                               &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_RIGHT);
  lv_label_set_recolor(legend, true);

  // Plot area 348x176 at (52,56); V axis (yellow) left, A axis (blue) right.
  detail_chart = lv_chart_create(detail_graph_panel);
  lv_obj_set_pos(detail_chart, 52, 56);
  lv_obj_set_size(detail_chart, 348, 176);
  lv_obj_set_style_pad_all(detail_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(detail_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(detail_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(detail_chart, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(detail_chart, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_line_color(detail_chart, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_line_width(detail_chart, 1, LV_PART_MAIN);
  lv_obj_set_style_line_width(detail_chart, 3, LV_PART_ITEMS);
  lv_obj_set_style_size(detail_chart, 0, LV_PART_INDICATOR);
  lv_obj_clear_flag(detail_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_chart_set_type(detail_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(detail_chart, kDetailChartPoints);
  lv_chart_set_div_line_count(detail_chart, 3, 0);
  detail_ser_v = lv_chart_add_series(detail_chart, lv_color_hex(UiTheme::kAccentV), LV_CHART_AXIS_PRIMARY_Y);
  detail_ser_i = lv_chart_add_series(detail_chart, lv_color_hex(UiTheme::kAccentI), LV_CHART_AXIS_SECONDARY_Y);
  detail_ser_lim = lv_chart_add_series(detail_chart, lv_color_hex(UiTheme::kLimitTrace), LV_CHART_AXIS_SECONDARY_Y);
  lv_chart_set_all_value(detail_chart, detail_ser_v, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(detail_chart, detail_ser_i, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(detail_chart, detail_ser_lim, LV_CHART_POINT_NONE);

  for (int j = 0; j < 5; ++j) {
    const int y = 56 + j * 44 - 8;
    detail_axis_v[j] = makeLabel(detail_graph_panel, 0, y, 46, 16, "", &lv_font_montserrat_12,
                                 UiTheme::kAccentV, LV_TEXT_ALIGN_RIGHT);
    detail_axis_i[j] = makeLabel(detail_graph_panel, 406, y, 44, 16, "", &lv_font_montserrat_12,
                                 UiTheme::kAccentI, LV_TEXT_ALIGN_LEFT);
  }
  makeLabel(detail_graph_panel, 0, 32, 46, 16, "V", &lv_font_montserrat_12, UiTheme::kAccentV, LV_TEXT_ALIGN_RIGHT);
  makeLabel(detail_graph_panel, 406, 32, 44, 16, "A", &lv_font_montserrat_12, UiTheme::kAccentI, LV_TEXT_ALIGN_LEFT);

  detail_graph_note = lv_label_create(detail_graph_panel);
  lv_label_set_text(detail_graph_note, "");
  lv_obj_set_style_text_font(detail_graph_note, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_set_style_bg_color(detail_graph_note, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(detail_graph_note, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(detail_graph_note, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(detail_graph_note, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(detail_graph_note, 6, LV_PART_MAIN);
  lv_obj_align(detail_graph_note, LV_ALIGN_TOP_MID, 0, 62);
  lv_obj_add_flag(detail_graph_note, LV_OBJ_FLAG_HIDDEN);

  detail_window_lbl = makeLabel(detail_graph_panel, 12, 242, 230, 22, "", &lv_font_montserrat_16, UiTheme::kTextMuted);
  makeLabel(detail_graph_panel, 240, 242, 150, 22, "Tap for Graphs " LV_SYMBOL_RIGHT, &lv_font_montserrat_16,
            UiTheme::kTextPrimary, LV_TEXT_ALIGN_RIGHT);
  makeLabel(detail_graph_panel, 392, 242, 52, 22, "now", &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_RIGHT);

  // Footer: read-only nominal voltage, confirmed Iset (read-only here), and the existing Setup editor.
  lv_obj_t* fixed = makeBox(screen_detail, 8, 366, 230, 56, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 8);
  makeLabel(fixed, 10, 2, 210, 20, "FIXED - READ-ONLY", &lv_font_montserrat_16, UiTheme::kTextMuted);
  detail_fixed_val = makeLabel(fixed, 10, 20, 210, 32, "", &lv_font_montserrat_28, UiTheme::kTextPrimary);

  lv_obj_t* iset = makeBox(screen_detail, 246, 366, 330, 56, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 8);
  makeLabel(iset, 10, 2, 150, 20, "Iset LIMIT", &lv_font_montserrat_16, UiTheme::kTextMuted);
  detail_iset_val = makeLabel(iset, 10, 20, 150, 32, "-.--- A", &lv_font_montserrat_28, UiTheme::kAccentI);
  makeChip(iset, 166, 12, 156, 32, &lv_font_montserrat_16, detail_iset_chip);
  setChip(detail_iset_chip, "NO VALUE", UiTheme::kUnknown);

  lv_obj_t* edit = makeButton(screen_detail, 584, 366, kEditBtnW, kEditBtnH, UiTheme::kPanelSoft,
                              UiTheme::kAccentI, 2, 8);
  lv_obj_add_event_cb(edit, dash_edit_limit_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(edit, 0, 2, kEditBtnW - 4, 26, "EDIT LIMIT", &lv_font_montserrat_20, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  makeLabel(edit, 0, 28, kEditBtnW - 4, 22, "in Setup", &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);

  createBottomNav(screen_detail);
  createNotice(screen_detail, detail_bar);
}

// ── Dashboard refresh: runs every loop, independent of telemetry frame changes,
// so stale links and host-confirmed limit changes redraw even when V/I are unchanged.
struct RailData {
  bool have_reading;
  bool state_known;
  bool enabled;
  uint8_t trip_flags;
  bool have_limit;
  int32_t v_mV;
  int32_t i_mA;
  uint16_t limit_mA;
};

struct RailStatus {
  const char* text;
  const char* note;
  uint32_t color;
};

// CH1 = fixed +5 V (legacy "v12" fields alias the 5 V rail); CH2 = fixed +3.3 V (extended frames only).
RailData railData(int ch, const DisplayTelemetry& t, LinkState link) {
  RailData d = {};
  const bool frame = (link != LinkState::Unknown);
  d.state_known = frame && t.has_extended;
  if (ch == 0) {
    d.have_reading = frame;
    d.v_mV = t.last_v12_mV;
    d.i_mA = t.last_i12_mA;
    if (d.state_known) {
      d.enabled = (t.status & 0x80u) != 0u;
      if (t.protection_flags & 0x80u) d.trip_flags |= kTripOvp;
      if (t.protection_flags & 0x40u) d.trip_flags |= kTripOcp;
      if (t.protection_flags & 0x08u) d.trip_flags |= kTripOtp;
    }
    d.have_limit = setup_binding.have_ch1_limit;
    d.limit_mA = setup_binding.ch1_limit_mA;
  } else {
    d.have_reading = frame && t.has_extended;
    d.v_mV = t.last_v3v3_mV;
    d.i_mA = t.last_i3v3_mA;
    if (d.state_known) {
      d.enabled = (t.status & 0x40u) != 0u;
      if (t.protection_flags & 0x20u) d.trip_flags |= kTripOvp;
      if (t.protection_flags & 0x10u) d.trip_flags |= kTripOcp;
      if (t.protection_flags & 0x04u) d.trip_flags |= kTripOtp;
    }
    d.have_limit = setup_binding.have_ch2_limit;
    d.limit_mA = setup_binding.ch2_limit_mA;
  }
  return d;
}

// CC is never asserted: "AT LIMIT" only compares measured current with the host-confirmed limit.
RailStatus railStatus(const RailData& d, LinkState link, char* note_buf, size_t note_len) {
  if (link == LinkState::Unknown) return {"UNKNOWN", "No telemetry yet", UiTheme::kUnknown};
  if (link == LinkState::Demo) return {"DEMO DATA", "Simulated - not hardware", UiTheme::kDemo};
  if (link == LinkState::Stale) return {"NO DATA - STALE", "Last values shown dimmed", UiTheme::kAccentWarn};
  if (!d.have_reading) return {"NO DATA", "Host frame has no data for this rail", UiTheme::kUnknown};
  if (!d.state_known) return {"STATE UNKNOWN", "Legacy frame: no status byte", UiTheme::kUnknown};
  if (d.trip_flags != 0u) {
    snprintf(note_buf, note_len, "STM32 flags:%s%s%s",
             (d.trip_flags & kTripOvp) ? " OVP" : "",
             (d.trip_flags & kTripOcp) ? " OCP" : "",
             (d.trip_flags & kTripOtp) ? " OTP" : "");
    return {"TRIP", note_buf, UiTheme::kError};
  }
  if (!d.enabled) return {"OUTPUT OFF", "Output off or rail below min V", UiTheme::kUnknown};
  if (!d.have_limit) return {"ON - LIMIT UNKNOWN", "No confirmed Iset yet", UiTheme::kUnknown};
  if (d.limit_mA == 0u) return {"LIMIT 0.000 A", "Zero-limit meaning unverified", UiTheme::kAccentWarn};
  if (d.i_mA >= static_cast<int32_t>(d.limit_mA)) {
    return {"AT LIMIT", "I >= limit - CC not verified", UiTheme::kAccentWarn};
  }
  return {"ON - BELOW LIMIT", "CV/CC not verified (#77)", UiTheme::kAccentOk};
}

void refreshRailCard(RailCard& c, const RailData& d, LinkState link, bool main_layout) {
  char note_buf[48];
  const RailStatus st = railStatus(d, link, note_buf, sizeof(note_buf));

  char v[16], i[16], p[16];
  if (d.have_reading) {
    const float volts = d.v_mV / 1000.0f;
    const float amps = d.i_mA / 1000.0f;
    snprintf(v, sizeof(v), "%.2f", volts);
    snprintf(i, sizeof(i), "%.3f", amps);
    snprintf(p, sizeof(p), "%.2f", volts * amps);
  } else {
    strcpy(v, "---");
    strcpy(i, "---");
    strcpy(p, "---");
  }
  setLabel(c.num[0], v);
  setLabel(c.num[1], i);
  setLabel(c.num[2], p);

  lv_opa_t opa = LV_OPA_COVER;
  if (link == LinkState::Stale) opa = LV_OPA_50;
  else if (!d.have_reading || (link == LinkState::Live && d.state_known && !d.enabled)) opa = LV_OPA_60;
  for (int k = 0; k < 3; ++k) setTextOpa(c.row[k], opa);

  setChip(c.status, st.text, st.color);
  setLabel(c.note, st.note);

  if (!main_layout) return;
  setHidden(c.demo_tag.box, link != LinkState::Demo);
  char lim[16];
  if (d.have_limit) snprintf(lim, sizeof(lim), "%.3f A", d.limit_mA / 1000.0f);
  else strcpy(lim, "-.--- A");
  setLabel(c.limit_val, lim);
  setTextOpa(c.limit_val, (link == LinkState::Stale || !d.have_limit) ? LV_OPA_50 : LV_OPA_COVER);
  setHidden(c.limit_chip.box, d.have_limit);
}

void refreshTopBar(TopBar& b, const DisplayTelemetry& t, LinkState link, uint32_t age_ms) {
  char buf[32];
  switch (link) {
    case LinkState::Live:
      setChip(b.link, "LIVE", UiTheme::kAccentOk);
      break;
    case LinkState::Stale:
      snprintf(buf, sizeof(buf), "STALE %lu s", static_cast<unsigned long>(age_ms / 1000UL));
      setChip(b.link, buf, UiTheme::kAccentWarn);
      break;
    case LinkState::Demo:
      setChip(b.link, "DEMO DATA", UiTheme::kDemo);
      break;
    default:
      setChip(b.link, "UNKNOWN", UiTheme::kUnknown);
      break;
  }

  // Output state comes from the extended telemetry status byte; it is never inferred from current.
  const bool have_state = t.has_extended && link != LinkState::Unknown;
  const bool on = have_state && (t.status & kStatusEnabledAny) != 0u;
  const char* main_txt = "OUTPUT ?";
  const char* sub_txt = "BOTH CHANNELS";
  uint8_t style = 0;  // 0 disabled, 1 pending, 2 on, 3 off
  bool clickable = false;
  if (out_cmd.pending) {
    main_txt = on ? "OUTPUT ON" : "OUTPUT OFF";
    sub_txt = "sent - waiting";
    style = 1;
  } else if (link == LinkState::Demo) {
    main_txt = "OUTPUT (DEMO)";
    sub_txt = "demo - no control";
  } else if (link == LinkState::Unknown) {
    sub_txt = "no link - no control";
  } else if (link == LinkState::Stale) {
    main_txt = have_state ? (on ? "OUTPUT ON ?" : "OUTPUT OFF ?") : "OUTPUT ?";
    sub_txt = "stale - no control";
  } else if (!have_state) {
    sub_txt = "state unknown";
  } else {
    main_txt = on ? "OUTPUT ON" : "OUTPUT OFF";
    sub_txt = on ? "BOTH - tap to turn OFF" : "BOTH - tap to turn ON";
    style = on ? 2 : 3;
    clickable = true;
  }
  setLabel(b.out_main, main_txt);
  setLabel(b.out_sub, sub_txt);
  if (b.out_mode != style) {
    b.out_mode = style;
    uint32_t bg = UiTheme::kPanelSoft;
    uint32_t border = UiTheme::kBorder;
    uint32_t text = UiTheme::kTextPrimary;
    if (style == 0) text = UiTheme::kUnknown;
    if (style == 1) border = UiTheme::kAccentWarn;
    if (style == 2) {
      border = UiTheme::kAccentOk;
      bg = 0x1E4A33;
    }
    if (style == 3) border = UiTheme::kTextMuted;
    lv_obj_set_style_bg_color(b.out_btn, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_border_color(b.out_btn, lv_color_hex(border), LV_PART_MAIN);
    lv_obj_set_style_text_color(b.out_main, lv_color_hex(text), LV_PART_MAIN);
  }
  if (clickable == lv_obj_has_state(b.out_btn, LV_STATE_DISABLED)) {
    if (clickable) lv_obj_clear_state(b.out_btn, LV_STATE_DISABLED);
    else lv_obj_add_state(b.out_btn, LV_STATE_DISABLED);
  }

  char tb[32];
  const uint32_t up_s = millis() / 1000UL;
  snprintf(tb, sizeof(tb), "UPTIME %lu:%02lu:%02lu", static_cast<unsigned long>(up_s / 3600UL),
           static_cast<unsigned long>((up_s / 60UL) % 60UL), static_cast<unsigned long>(up_s % 60UL));
  setLabel(b.clock, tb);

  if (have_state) snprintf(tb, sizeof(tb), "UNIT TEMP %u C", static_cast<unsigned>(t.last_temp_C));
  else strcpy(tb, "UNIT TEMP -- C");
  setLabel(b.temp, tb);
  setTextOpa(b.temp, link == LinkState::Stale ? LV_OPA_50 : LV_OPA_COVER);

  if (dash_notice.active) {
    setLabel(b.notice, dash_notice.text);
    lv_obj_set_style_border_color(b.notice, lv_color_hex(dash_notice.color), LV_PART_MAIN);
    lv_obj_set_style_text_color(b.notice, lv_color_hex(dash_notice.color), LV_PART_MAIN);
  }
  setHidden(b.notice, !dash_notice.active);
}

// Shared OUTPUT command bookkeeping: a failure or missing ACK is shown; success is only claimed by telemetry.
void serviceOutputCommand(uint32_t now_ms) {
  if (!out_cmd.pending) return;
  const auto c = disp_link_slave::commandSnapshot();
  if (c.err_count != out_cmd.err_at_send) {
    out_cmd.pending = false;
    char msg[80];
    snprintf(msg, sizeof(msg), "OUTPUT ERR: %.60s", c.last_err[0] ? c.last_err : "(no text)");
    postNotice(msg, UiTheme::kError, now_ms);
  } else if (c.ack_count != out_cmd.ack_at_send && strncmp(c.last_ack, "OUTPUT", 6) == 0) {
    out_cmd.pending = false;
  } else if ((now_ms - out_cmd.sent_ms) > kOutputAckTimeoutMs) {
    out_cmd.pending = false;
    postNotice("OUTPUT: no ACK from host - state unconfirmed", UiTheme::kAccentWarn, now_ms);
  }
}

// Host-confirmed limits come only from GET ILIM / ILIM ACK+EVT; ask for any that are still missing.
void requestMissingLimits(LinkState link, uint32_t now_ms) {
  static uint32_t last_req_ms = 0;
  if (link != LinkState::Live) return;
  if (setup_binding.have_ch1_limit && setup_binding.have_ch2_limit) return;
  if ((now_ms - last_req_ms) < kLimitGetRetryMs) return;
  last_req_ms = now_ms;
  disp_link_slave::sendCommand(!setup_binding.have_ch1_limit ? "GET ILIM CH1" : "GET ILIM CH2");
}

void applyDetailChannelStyle(int ch) {
  const RailSpec& r = kRails[ch];
  lv_obj_set_style_border_color(detail_card.panel, lv_color_hex(r.color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(detail_card.panel, lv_color_hex(r.tint), LV_PART_MAIN);
  lv_obj_set_style_bg_color(detail_card.accent, lv_color_hex(r.color), LV_PART_MAIN);
  lv_obj_set_style_border_color(detail_graph_panel, lv_color_hex(r.color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(detail_graph_accent, lv_color_hex(r.color), LV_PART_MAIN);
  setLabel(detail_card.ch_chip.lbl, r.name);
  fillChip(detail_card.ch_chip, r.color);
  setLabel(detail_top_chip.lbl, r.name);
  fillChip(detail_top_chip, r.color);

  setLabel(detail_fixed_val, r.nominal_full);
  char buf[24];
  for (int j = 0; j < 5; ++j) {
    snprintf(buf, sizeof(buf), "%.1f", (r.v_axis_mV / 1000.0f) * (4 - j) / 4.0f);
    setLabel(detail_axis_v[j], buf);
    snprintf(buf, sizeof(buf), "%.2f", (r.i_axis_mA / 1000.0f) * (4 - j) / 4.0f);
    setLabel(detail_axis_i[j], buf);
  }
  lv_chart_set_range(detail_chart, LV_CHART_AXIS_PRIMARY_Y, 0, r.v_axis_mV);
  lv_chart_set_range(detail_chart, LV_CHART_AXIS_SECONDARY_Y, 0, r.i_axis_mA);
}

// Both traces come from the selected rail's samples; live and demo samples are never mixed.
void rebuildDetailChart(int ch, const RailData& d) {
  lv_chart_set_all_value(detail_chart, detail_ser_v, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(detail_chart, detail_ser_i, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(detail_chart, detail_ser_lim, d.have_limit ? static_cast<lv_coord_t>(d.limit_mA) : LV_CHART_POINT_NONE);

  detail_plotted = 0;
  uint32_t newest_ms = 0;
  uint32_t oldest_ms = 0;
  for (size_t k = 0; k < trend_count && k < kDetailChartPoints; ++k) {
    const TrendSample& s = trendAt(trend_count - 1 - k);
    if (s.demo != demo_mode) break;
    if (ch == 1 && !s.has_ch2) continue;
    const uint16_t idx = static_cast<uint16_t>(kDetailChartPoints - 1 - k);
    lv_chart_set_value_by_id(detail_chart, detail_ser_v, idx,
                             static_cast<lv_coord_t>(ch == 0 ? s.v12_mV : s.v3v3_mV));
    lv_chart_set_value_by_id(detail_chart, detail_ser_i, idx,
                             static_cast<lv_coord_t>(ch == 0 ? s.i12_mA : s.i3v3_mA));
    if (detail_plotted == 0) newest_ms = s.t_ms;
    oldest_ms = s.t_ms;
    ++detail_plotted;
  }
  lv_chart_refresh(detail_chart);

  char wbuf[48];
  if (detail_plotted == 0) {
    strcpy(wbuf, "no samples for this rail yet");
  } else {
    snprintf(wbuf, sizeof(wbuf), "last %lu s, %u samples",
             static_cast<unsigned long>((newest_ms - oldest_ms) / 1000UL),
             static_cast<unsigned>(detail_plotted));
  }
  setLabel(detail_window_lbl, wbuf);
}

void refreshDetail(const DisplayTelemetry& t, LinkState link, uint32_t age_ms,
                   const RailData* d, uint32_t now_ms) {
  const int ch = detail_channel;
  if (detail_styled_channel != static_cast<int8_t>(ch)) {
    applyDetailChannelStyle(ch);
    detail_styled_channel = static_cast<int8_t>(ch);
    detail_chart_dirty = true;
  }
  refreshTopBar(detail_bar, t, link, age_ms);
  refreshRailCard(detail_card, d[ch], link, false);

  char lim[16];
  if (d[ch].have_limit) snprintf(lim, sizeof(lim), "%.3f A", d[ch].limit_mA / 1000.0f);
  else strcpy(lim, "-.--- A");
  setLabel(detail_iset_val, lim);
  setTextOpa(detail_iset_val, (link == LinkState::Stale || !d[ch].have_limit) ? LV_OPA_50 : LV_OPA_COVER);
  if (d[ch].have_limit) setChip(detail_iset_chip, "CONFIRMED", UiTheme::kAccentOk);
  else setChip(detail_iset_chip, "NO VALUE", UiTheme::kUnknown);

  static int32_t last_limit_key = -2;
  static bool last_demo = false;
  const int32_t limit_key = d[ch].have_limit ? static_cast<int32_t>(d[ch].limit_mA) : -1;
  const bool changed = detail_chart_dirty || trend_head != detail_last_trend_head ||
                       trend_count != detail_last_trend_count || limit_key != last_limit_key ||
                       demo_mode != last_demo;
  if (changed && (detail_chart_dirty || (now_ms - detail_last_chart_ms) >= kDetailUiUpdateMinMs)) {
    rebuildDetailChart(ch, d[ch]);
    detail_chart_dirty = false;
    detail_last_chart_ms = now_ms;
    detail_last_trend_head = trend_head;
    detail_last_trend_count = trend_count;
    last_limit_key = limit_key;
    last_demo = demo_mode;
  }

  char title[40];
  snprintf(title, sizeof(title), "%s TREND%s", kRails[ch].name, link == LinkState::Demo ? "  DEMO DATA" : "");
  setLabel(detail_graph_title, title);

  const char* note = nullptr;
  uint32_t note_color = UiTheme::kUnknown;
  if (link == LinkState::Unknown) {
    note = "NO DATA";
  } else if (link == LinkState::Demo) {
    note = "DEMO DATA - SIMULATED";
    note_color = UiTheme::kDemo;
  } else if (!d[ch].have_reading) {
    note = "NO CH2 DATA (legacy frame)";
  } else if (link == LinkState::Stale) {
    note = "STALE - no new samples";
    note_color = UiTheme::kAccentWarn;
  } else if (detail_plotted == 0) {
    note = "NO SAMPLES YET";
  }
  if (note) {
    setLabel(detail_graph_note, note);
    lv_obj_set_style_text_color(detail_graph_note, lv_color_hex(note_color), LV_PART_MAIN);
  }
  setHidden(detail_graph_note, note == nullptr);
}

void refreshDashboard(bool force) {
  if (active_screen != UiScreen::Main && active_screen != UiScreen::Detail) return;
  if (!screen_main || !screen_detail) return;

  static uint32_t last_refresh_ms = 0;
  const uint32_t now_ms = millis();
  if (!force && (now_ms - last_refresh_ms) < kDashboardRefreshMs) return;
  last_refresh_ms = now_ms;

  const DisplayTelemetry t = get_display_telemetry();
  uint32_t age_ms = 0;
  const LinkState link = linkStateOf(t, now_ms, &age_ms);
  serviceOutputCommand(now_ms);
  requestMissingLimits(link, now_ms);
  if (dash_notice.active && now_ms > dash_notice.until_ms) dash_notice.active = false;

  // The ON confirmation must not outlive the conditions it was opened under.
  if (out_confirm_layer && !lv_obj_has_flag(out_confirm_layer, LV_OBJ_FLAG_HIDDEN)) {
    const bool already_on = t.has_extended && (t.status & kStatusEnabledAny) != 0u;
    if (link != LinkState::Live || !t.has_extended) {
      setHidden(out_confirm_layer, true);
      postNotice("OUTPUT ON cancelled - link not live", UiTheme::kAccentWarn, now_ms);
    } else if (already_on || out_cmd.pending) {
      setHidden(out_confirm_layer, true);
    } else if (now_ms > out_confirm_until_ms) {
      setHidden(out_confirm_layer, true);
      postNotice("OUTPUT ON cancelled - confirmation timed out", UiTheme::kAccentWarn, now_ms);
    }
  }
  const RailData d[2] = {railData(0, t, link), railData(1, t, link)};
  if (active_screen == UiScreen::Main) {
    refreshTopBar(main_bar, t, link, age_ms);
    refreshRailCard(main_card[0], d[0], link, true);
    refreshRailCard(main_card[1], d[1], link, true);
  } else {
    refreshDetail(t, link, age_ms, d, now_ms);
  }
}

// Strip-recorder Graph screen (D5a): 640x122 plot areas stacked V over I, two pens each.
constexpr int kGraphChartX = 62;
constexpr int kGraphChartW = 640;
constexpr int kGraphChartH = 122;
constexpr int kGraphVChartY = 128;
constexpr int kGraphIChartY = 270;

lv_obj_t* createGraphChart(int y, int32_t y_min, int32_t y_max) {
  lv_obj_t* c = lv_chart_create(screen_graph);
  lv_obj_set_pos(c, kGraphChartX, y);
  lv_obj_set_size(c, kGraphChartW, kGraphChartH);
  lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(c, lv_color_hex(UiTheme::kScope), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(c, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_line_color(c, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_line_width(c, 1, LV_PART_MAIN);
  lv_obj_set_style_line_width(c, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_chart_set_type(c, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(c, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(c, kChartPoints);
  // lv_chart skips the two border positions, so N divs draw N-2 interior lines: 3 horizontal, 5 vertical.
  lv_chart_set_div_line_count(c, 5, 7);
  lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
  return c;
}

void create_graph_screen(lv_obj_t* root) {
  screen_graph = makeScreen(root);

  // Header bezel: title, group label, nav.
  lv_obj_t* status = makeBox(screen_graph, 8, 6, kDisplayWidth - 16, 44, UiTheme::kStatusBar, UiTheme::kBorder, 1, 8);
  makeLabel(status, 14, 0, 190, 44, "STRIP RECORDER", &lv_font_montserrat_20, UiTheme::kTextPrimary);
  makeLabel(status, 212, 0, 330, 44, "GROUP 1   +5V / +3.3V", &lv_font_montserrat_16, UiTheme::kTextMuted);
  create_nav_btn(status, "Settings", UiScreen::Settings, -120);
  create_nav_btn(status, "Main", UiScreen::Main, -10);

  // Pen legend: swatch + live value per channel.
  static const struct { const char* tag; uint32_t color; } kPens[4] = {
    {"CH1 +5V  VOLT", UiTheme::kCh1},
    {"CH1 +5V  CURR", UiTheme::kCh1},
    {"CH2 +3.3V  VOLT", UiTheme::kCh2},
    {"CH2 +3.3V  CURR", UiTheme::kCh2},
  };
  for (int k = 0; k < 4; ++k) {
    lv_obj_t* cell = makeBox(screen_graph, 18 + k * 193, 56, 185, 52, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 6);
    makeBox(cell, 8, 10, 6, 30, kPens[k].color, kPens[k].color, 0, 2);
    makeLabel(cell, 22, 4, 158, 18, kPens[k].tag, &lv_font_montserrat_12, kPens[k].color);
    lbl_pen_val[k] = makeLabel(cell, 22, 22, 158, 26, "--", &lv_font_montserrat_20, UiTheme::kTextPrimary);
  }

  lbl_graph_stats = makeLabel(screen_graph, kGraphChartX + kGraphChartW + 12, 330, 74, 54, "n=0\nT=--C",
                              &lv_font_montserrat_12, UiTheme::kTextMuted);

  makeLabel(screen_graph, kGraphChartX, 110, 300, 16, "VOLTAGE (V)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  chart_v = createGraphChart(kGraphVChartY, 0, 6000);
  chart_ch1_v_series = lv_chart_add_series(chart_v, lv_color_hex(UiTheme::kCh1), LV_CHART_AXIS_PRIMARY_Y);
  chart_ch2_v_series = lv_chart_add_series(chart_v, lv_color_hex(UiTheme::kCh2), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_v, chart_ch1_v_series, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(chart_v, chart_ch2_v_series, LV_CHART_POINT_NONE);

  makeLabel(screen_graph, kGraphChartX, 252, 300, 16, "CURRENT (A)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  chart_i = createGraphChart(kGraphIChartY, 0, 4000);
  chart_ch1_i_series = lv_chart_add_series(chart_i, lv_color_hex(UiTheme::kCh1), LV_CHART_AXIS_PRIMARY_Y);
  chart_ch2_i_series = lv_chart_add_series(chart_i, lv_color_hex(UiTheme::kCh2), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_i, chart_ch1_i_series, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(chart_i, chart_ch2_i_series, LV_CHART_POINT_NONE);

  // Axis scales are fixed (V 0..6 V, I 0..4 A), so these labels are static.
  for (int j = 0; j < 5; ++j) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1f", 6.0f * (4 - j) / 4.0f);
    makeLabel(screen_graph, 12, kGraphVChartY + j * (kGraphChartH - 1) / 4 - 8, 46, 16, buf,
              &lv_font_montserrat_12, UiTheme::kTextMuted, LV_TEXT_ALIGN_RIGHT);
    snprintf(buf, sizeof(buf), "%.1f", 4.0f * (4 - j) / 4.0f);
    makeLabel(screen_graph, 12, kGraphIChartY + j * (kGraphChartH - 1) / 4 - 8, 46, 16, buf,
              &lv_font_montserrat_12, UiTheme::kTextMuted, LV_TEXT_ALIGN_RIGHT);
  }

  // One label per major vertical grid line (6 columns); times are CrowPanel uptime until a real clock exists (#82).
  for (int k = 0; k < 7; ++k) {
    const int cx = kGraphChartX + k * kGraphChartW / 6;
    const int x = (k == 0) ? kGraphChartX : (k == 6) ? kGraphChartX + kGraphChartW - 70 : cx - 35;
    const lv_text_align_t al = (k == 0) ? LV_TEXT_ALIGN_LEFT : (k == 6) ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
    lbl_graph_time[k] = makeLabel(screen_graph, x, 394, 70, 16, "--:--:--", &lv_font_montserrat_12, UiTheme::kTextMuted, al);
  }
  makeLabel(screen_graph, kGraphChartX + kGraphChartW + 12, 394, 74, 16, "UPTIME", &lv_font_montserrat_12, UiTheme::kTextMuted);

  lbl_window = lv_label_create(screen_graph);
  lv_label_set_text(lbl_window, "win: waiting for samples");
  lv_obj_set_style_text_color(lbl_window, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_window, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_window, LV_ALIGN_BOTTOM_LEFT, 24, -8);

  lbl_graph_window_v = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_v, "CH1 V min/max --.-- / --.--");
  lv_obj_set_style_text_color(lbl_graph_window_v, lv_color_hex(UiTheme::kCh1), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_v, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_v, LV_ALIGN_BOTTOM_MID, -120, -8);

  lbl_graph_window_i = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_i, "CH1 I min/max --.--- / --.---");
  lv_obj_set_style_text_color(lbl_graph_window_i, lv_color_hex(UiTheme::kCh1), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_i, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_i, LV_ALIGN_BOTTOM_RIGHT, -22, -8);

  lv_obj_t* fault_row = lv_obj_create(screen_graph);
  lv_obj_set_size(fault_row, kDisplayWidth - 36, 30);
  lv_obj_align(fault_row, LV_ALIGN_BOTTOM_MID, 0, -26);
  lv_obj_clear_flag(fault_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(fault_row, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(fault_row, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(fault_row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(fault_row, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_fault_graph = lv_label_create(fault_row);
  lv_label_set_text(lbl_fault_graph, "NO TELEMETRY");
  lv_obj_set_style_text_color(lbl_fault_graph, lv_color_hex(UiTheme::kUnknown), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_fault_graph, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_fault_graph);
}

void create_dashboard() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(UiTheme::kBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  create_splash_screen(scr);
  create_setup_screen(scr);
  create_main_screen(scr);
  create_detail_screen(scr);
  create_graph_screen(scr);
  create_settings_screen(scr);
  createOutputConfirm();
  set_active_screen(UiScreen::Splash);
}

// ── Telemetry label updates ────────────────────────────────────────────────
// Lists only active trips; no CC/CV claim (#77). Runs every loop so STALE shows without a new frame.
void refreshGraphFaultRow(const DisplayTelemetry& t, uint32_t now_ms) {
  if (!lbl_fault_graph || active_screen != UiScreen::Graph) return;

  uint32_t age_ms = 0;
  const LinkState link = linkStateOf(t, now_ms, &age_ms);
  char buf[96] = "";
  uint32_t color = UiTheme::kAccentOk;
  if (link == LinkState::Unknown) {
    strcpy(buf, "NO TELEMETRY");
    color = UiTheme::kUnknown;
  } else if (link == LinkState::Demo) {
    strcpy(buf, "DEMO DATA - NO FAULT INFO");
    color = UiTheme::kDemo;
  } else if (link == LinkState::Stale) {
    strcpy(buf, "LINK STALE - FAULT STATE UNKNOWN");
    color = UiTheme::kAccentWarn;
  } else if (!t.has_extended) {
    strcpy(buf, "FAULT FLAGS NOT REPORTED (LEGACY FRAME)");
    color = UiTheme::kUnknown;
  } else {
    static const char* const kTripNames[3] = {"OVP", "OCP", "OTP"};
    static const uint8_t kTripBits[3] = {kTripOvp, kTripOcp, kTripOtp};
    for (int ch = 0; ch < 2; ++ch) {
      const RailData d = railData(ch, t, link);
      char part[40] = "";
      for (int k = 0; k < 3; ++k) {
        if (d.trip_flags & kTripBits[k]) {
          strncat(part, part[0] ? " " : "", sizeof(part) - strlen(part) - 1);
          strncat(part, kTripNames[k], sizeof(part) - strlen(part) - 1);
        }
      }
      if (part[0]) {
        char entry[56];
        snprintf(entry, sizeof(entry), "%sCH%d %s", buf[0] ? "   " : "", ch + 1, part);
        strncat(buf, entry, sizeof(buf) - strlen(buf) - 1);
      }
    }
    if (t.status & 0x08u) strncat(buf, buf[0] ? "   THERM WARN" : "THERM WARN", sizeof(buf) - strlen(buf) - 1);
    if (buf[0]) {
      color = UiTheme::kError;
    } else {
      strcpy(buf, "NO FAULTS");
    }
  }
  static uint32_t last_color = 0xFFFFFFFFu;
  if (strcmp(lv_label_get_text(lbl_fault_graph), buf) != 0) {
    lv_label_set_text(lbl_fault_graph, buf);
    lv_obj_center(lbl_fault_graph);
  }
  if (color != last_color) {
    last_color = color;
    lv_obj_set_style_text_color(lbl_fault_graph, lv_color_hex(color), LV_PART_MAIN);
  }
}

void update_telemetry_labels() {
  if (!kLiveTelemetryUiEnabled) return;

  const uint32_t now_ms = millis();
  if (now_ms - last_ui_update_ms < kUiUpdateMinMs) return;

  const DisplayTelemetry t = get_display_telemetry();
  refreshGraphFaultRow(t, now_ms);
  if (t.rx_count == last_drawn_count &&
      t.last_seq == last_drawn_seq) return;

  last_ui_update_ms = now_ms;
  last_drawn_count = t.rx_count;
  last_drawn_seq   = t.last_seq;

  // Main and channel-detail screens are driven by refreshDashboard(); this updates the Graph screen only.
  {
    uint32_t age_ms = 0;
    const LinkState link = linkStateOf(t, now_ms, &age_ms);
    for (int ch = 0; ch < 2; ++ch) {
      const RailData d = railData(ch, t, link);
      char vbuf[16];
      char ibuf[16];
      if (d.have_reading) {
        snprintf(vbuf, sizeof(vbuf), "%.2f V", d.v_mV / 1000.0f);
        snprintf(ibuf, sizeof(ibuf), "%.3f A", d.i_mA / 1000.0f);
      } else {
        strcpy(vbuf, "--");
        strcpy(ibuf, "--");
      }
      setLabel(lbl_pen_val[ch * 2], vbuf);
      setLabel(lbl_pen_val[ch * 2 + 1], ibuf);
    }
  }

  if (kMinimalUiLabelsOnly) return;

  if (now_ms - last_detail_ui_update_ms < kDetailUiUpdateMinMs) return;
  last_detail_ui_update_ms = now_ms;

  if (lbl_graph_stats) {
    char gbuf[48];
    snprintf(gbuf, sizeof(gbuf), "n=%lu\nT=%uC\nerr=%lu",
             static_cast<unsigned long>(trend_count),
             static_cast<unsigned>(t.last_temp_C),
             static_cast<unsigned long>(t.err_count));
    setLabel(lbl_graph_stats, gbuf);
  }

  if (trend_total != last_drawn_samples) {
    last_drawn_samples = trend_total;
    const TrendWindowStats stats = getTrendWindowStats(static_cast<size_t>(kChartPoints) * kChartDecimation);

    // Plotted window = measured sample spacing x decimation x (points - 1).
    const size_t span = trend_count < 100 ? trend_count : 100;
    uint32_t window_ms = 0;
    if (span >= 2) {
      const uint32_t dt_ms = trendAt(trend_count - 1).t_ms - trendAt(trend_count - span).t_ms;
      window_ms = static_cast<uint32_t>(
          static_cast<uint64_t>(dt_ms) * kChartDecimation * (kChartPoints - 1) / (span - 1));
    }
    const uint32_t window_s = (window_ms + 500) / 1000;

    char wbuf[128];
    if (!stats.has_data) {
      snprintf(wbuf, sizeof(wbuf), "win: waiting for samples");
      if (lbl_graph_window_v) lv_label_set_text(lbl_graph_window_v, "CH1 V min/max --.-- / --.--");
      if (lbl_graph_window_i) lv_label_set_text(lbl_graph_window_i, "CH1 I min/max --.--- / --.---");
    } else {
      // range detail lives in lbl_graph_window_v/_i; keep this one short to avoid overlap
      snprintf(wbuf, sizeof(wbuf), "win %lu s", static_cast<unsigned long>(window_s));

      if (lbl_graph_window_v) {
        char v_win[64];
        snprintf(v_win, sizeof(v_win), "CH1 V min/max %.2f / %.2f",
                 stats.min_v12_mV / 1000.0f,
                 stats.max_v12_mV / 1000.0f);
        lv_label_set_text(lbl_graph_window_v, v_win);
      }
      if (lbl_graph_window_i) {
        char i_win[64];
        snprintf(i_win, sizeof(i_win), "CH1 I min/max %.3f / %.3f",
                 stats.min_i12_mA / 1000.0f,
                 stats.max_i12_mA / 1000.0f);
        lv_label_set_text(lbl_graph_window_i, i_win);
      }
    }
    if (lbl_window) lv_label_set_text(lbl_window, wbuf);

    if (span >= 2) {
      // Grid line k sits k/6 of the way across the plotted window; blank where it predates boot.
      const uint32_t newest_ms = trendAt(trend_count - 1).t_ms;
      for (int k = 0; k < 7; ++k) {
        const uint32_t back_ms = static_cast<uint32_t>(static_cast<uint64_t>(window_ms) * (6 - k) / 6);
        char tbuf[16];
        if (back_ms > newest_ms) {
          strcpy(tbuf, "--:--:--");
        } else {
          const uint32_t s = (newest_ms - back_ms) / 1000UL;
          snprintf(tbuf, sizeof(tbuf), "%lu:%02lu:%02lu", static_cast<unsigned long>(s / 3600UL),
                   static_cast<unsigned long>((s / 60UL) % 60UL), static_cast<unsigned long>(s % 60UL));
        }
        setLabel(lbl_graph_time[k], tbuf);
      }
    }
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
  Serial.printf("rx frames=%lu errs=%lu uart=%lu seq=%u V5=%u mV I5=%d mA V3=%u mV I3=%d mA T=%uC st=0x%02X pf=0x%02X ext=%u age=%lu ms uartB=%lu\n",
                static_cast<unsigned long>(t.rx_count),
                static_cast<unsigned long>(t.err_count),
                static_cast<unsigned long>(t.i2c_rx_count),
                static_cast<unsigned>(t.last_seq),
                static_cast<unsigned>(t.last_v5_mV),
                static_cast<int>(t.last_i5_mA),
                static_cast<unsigned>(t.last_v3v3_mV),
                static_cast<int>(t.last_i3v3_mA),
                static_cast<unsigned>(t.last_temp_C),
                static_cast<unsigned>(t.status),
                static_cast<unsigned>(t.protection_flags),
                static_cast<unsigned>(t.has_extended ? 1 : 0),
                static_cast<unsigned long>(age_ms),
                static_cast<unsigned long>(t.uart_bytes));
}

void printUdiLinkStatus() {
  const auto c = disp_link_slave::commandSnapshot();
  const uint32_t age_ms = (c.last_rx_ms == 0) ? 0 : (millis() - c.last_rx_ms);
  Serial.printf("udi tx=%lu ack=%lu err=%lu evt=%lu age=%lu ms last_ack=%s last_err=%s last_evt=%s\n",
                static_cast<unsigned long>(c.tx_count),
                static_cast<unsigned long>(c.ack_count),
                static_cast<unsigned long>(c.err_count),
                static_cast<unsigned long>(c.evt_count),
                static_cast<unsigned long>(age_ms),
                c.last_ack[0] ? c.last_ack : "--",
                c.last_err[0] ? c.last_err : "--",
                c.last_evt[0] ? c.last_evt : "--");
}

void sendUdiOutputCommand(bool enabled) {
  const bool ok = disp_link_slave::sendCommand(enabled ? "OUTPUT ON" : "OUTPUT OFF");
  if (!ok) {
    Serial.println("ERR UDI_OUTPUT: link not ready");
    return;
  }
  Serial.printf("ACK UDI_OUTPUT %s\n", enabled ? "ON" : "OFF");
}

void sendUdiCurrentLimitCommand(const String& channel, uint16_t limit_mA) {
  const uint16_t max_mA = channel.equalsIgnoreCase("CH1") ? 3000u : 2000u;
  if (limit_mA > max_mA) {
    Serial.printf("ERR UDI_ILIM: %s out of range (0..%u mA)\n",
                  channel.c_str(),
                  static_cast<unsigned>(max_mA));
    return;
  }

  char payload[48];
  snprintf(payload,
           sizeof(payload),
           "ILIM %s %u",
           channel.c_str(),
           static_cast<unsigned>(limit_mA));
  const bool ok = disp_link_slave::sendCommand(payload);
  if (!ok) {
    Serial.println("ERR UDI_ILIM: link not ready");
    return;
  }
  Serial.printf("ACK UDI_ILIM %s %u\n", channel.c_str(), static_cast<unsigned>(limit_mA));
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
    Serial.println("Commands: HELP, PING, STATUS, RX, UDI_STATUS, UDI_OUTPUT <ON|OFF>, UDI_ILIM <CH1|CH2> <mA>, OTA, SCREEN <SPLASH|SETUP|MAIN|DETAIL1|DETAIL2|GRAPH|SETTINGS>, SPLASH <ON|OFF>, DEMO <ON|OFF>, TOUR <ON|OFF>, SETUP_ENC <ROT <n>|PRESS|LONG>, SETTINGS_ENC <ROT <n>|PRESS|LONG>, PROBE <pin> [ms], LOG_START, LOG_STOP, LOG_STATUS, LOG_CLEAR, LOG_DUMP_CSV [N]");
    return;
  }
  if (line.equalsIgnoreCase("PING"))         { Serial.println("PONG"); return; }
  if (line.equalsIgnoreCase("STATUS"))       { printStatus();           return; }
  if (line.equalsIgnoreCase("RX"))           { printRxStatus();         return; }
  if (line.equalsIgnoreCase("UDI_STATUS"))   { printUdiLinkStatus();    return; }
  if (line.equalsIgnoreCase("OTA") )         { printOtaStatus();        return; }
  if (line.equalsIgnoreCase("LOG_START"))    { trend_logging_enabled = true;  Serial.println("ACK LOG_START"); return; }
  if (line.equalsIgnoreCase("LOG_STOP"))     { trend_logging_enabled = false; Serial.println("ACK LOG_STOP");  return; }
  if (line.equalsIgnoreCase("LOG_STATUS"))   { printLogStatus(); return; }
  if (line.equalsIgnoreCase("LOG_CLEAR"))    { trendClear(); Serial.println("ACK LOG_CLEAR"); return; }
  if (line.startsWith("SCREEN") || line.startsWith("screen")) {
    String arg = line.substring(6);
    arg.trim();
    if (arg.equalsIgnoreCase("SPLASH")) {
      set_active_screen(UiScreen::Splash);
      splash_done = false;
      splash_start_ms = millis();
      Serial.println("ACK SCREEN SPLASH");
      return;
    }
    if (arg.equalsIgnoreCase("SETUP")) {
      enterSetupScreen();
      Serial.println("ACK SCREEN SETUP");
      return;
    }
    if (arg.equalsIgnoreCase("MAIN")) {
      setup_done = true;
      set_active_screen(UiScreen::Main);
      Serial.println("ACK SCREEN MAIN");
      return;
    }
    if (arg.equalsIgnoreCase("GRAPH")) {
      set_active_screen(UiScreen::Graph);
      Serial.println("ACK SCREEN GRAPH");
      return;
    }
    if (arg.equalsIgnoreCase("SETTINGS")) {
      set_active_screen(UiScreen::Settings);
      Serial.println("ACK SCREEN SETTINGS");
      return;
    }
    if (arg.equalsIgnoreCase("DETAIL1") || arg.equalsIgnoreCase("DETAIL2")) {
      detail_channel = arg.endsWith("2") ? 1u : 0u;
      set_active_screen(UiScreen::Detail);
      Serial.printf("ACK SCREEN DETAIL%u\n", static_cast<unsigned>(detail_channel + 1u));
      return;
    }
    Serial.println("ERR SCREEN: use SPLASH|SETUP|MAIN|DETAIL1|DETAIL2|GRAPH|SETTINGS");
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
  if (line.startsWith("SETUP_ENC") || line.startsWith("setup_enc")) {
    String args = line.substring(9);
    args.trim();
    if (args.equalsIgnoreCase("PRESS")) {
      handleSetupEncoderPress();
      Serial.println("ACK SETUP_ENC PRESS");
      return;
    }
    if (args.equalsIgnoreCase("LONG")) {
      handleSetupEncoderLongPress();
      Serial.println("ACK SETUP_ENC LONG");
      return;
    }
    if (args.startsWith("ROT") || args.startsWith("rot")) {
      String detents_text = args.substring(3);
      detents_text.trim();
      if (detents_text.isEmpty()) {
        Serial.println("ERR SETUP_ENC: use ROT <n>, PRESS, or LONG");
        return;
      }
      const long detents = detents_text.toInt();
      if (detents == 0) {
        Serial.println("ERR SETUP_ENC: ROT detents must be non-zero");
        return;
      }
      const int8_t step = (detents > 0) ? 1 : -1;
      const long repeats = (detents > 0) ? detents : -detents;
      for (long i = 0; i < repeats; ++i) {
        handleSetupEncoderRotate(step);
      }
      Serial.printf("ACK SETUP_ENC ROT %ld\n", detents);
      return;
    }
    Serial.println("ERR SETUP_ENC: use ROT <n>, PRESS, or LONG");
    return;
  }
  if (line.startsWith("SETTINGS_ENC") || line.startsWith("settings_enc")) {
    String args = line.substring(12);
    args.trim();
    if (args.equalsIgnoreCase("PRESS")) {
      handleSettingsEncoderPress();
      Serial.println("ACK SETTINGS_ENC PRESS");
      return;
    }
    if (args.equalsIgnoreCase("LONG")) {
      handleSettingsEncoderLongPress();
      Serial.println("ACK SETTINGS_ENC LONG");
      return;
    }
    if (args.startsWith("ROT") || args.startsWith("rot")) {
      String detents_text = args.substring(3);
      detents_text.trim();
      if (detents_text.isEmpty()) {
        Serial.println("ERR SETTINGS_ENC: use ROT <n>, PRESS, or LONG");
        return;
      }
      const long detents = detents_text.toInt();
      if (detents == 0) {
        Serial.println("ERR SETTINGS_ENC: ROT detents must be non-zero");
        return;
      }
      const int8_t step = (detents > 0) ? 1 : -1;
      const long repeats = (detents > 0) ? detents : -detents;
      for (long i = 0; i < repeats; ++i) {
        handleSettingsEncoderRotate(step);
      }
      Serial.printf("ACK SETTINGS_ENC ROT %ld\n", detents);
      return;
    }
    Serial.println("ERR SETTINGS_ENC: use ROT <n>, PRESS, or LONG");
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
  if (line.startsWith("UDI_OUTPUT") || line.startsWith("udi_output")) {
    String arg = line.substring(10);
    arg.trim();
    if (arg.equalsIgnoreCase("ON")) {
      sendUdiOutputCommand(true);
      return;
    }
    if (arg.equalsIgnoreCase("OFF")) {
      sendUdiOutputCommand(false);
      return;
    }
    Serial.println("ERR UDI_OUTPUT: use ON|OFF");
    return;
  }
  if (line.startsWith("UDI_ILIM") || line.startsWith("udi_ilim")) {
    String args = line.substring(8);
    args.trim();
    const int split = args.indexOf(' ');
    if (split <= 0) {
      Serial.println("ERR UDI_ILIM: use UDI_ILIM <CH1|CH2> <mA>");
      return;
    }
    String channel = args.substring(0, split);
    channel.trim();
    String limit_text = args.substring(split + 1);
    limit_text.trim();

    if (!channel.equalsIgnoreCase("CH1") && !channel.equalsIgnoreCase("CH2")) {
      Serial.println("ERR UDI_ILIM: channel must be CH1 or CH2");
      return;
    }
    if (limit_text.isEmpty()) {
      Serial.println("ERR UDI_ILIM: missing mA value");
      return;
    }
    const long parsed_mA = limit_text.toInt();
    if (parsed_mA < 0) {
      Serial.println("ERR UDI_ILIM: mA must be >= 0");
      return;
    }
    sendUdiCurrentLimitCommand(channel, static_cast<uint16_t>(parsed_mA));
    return;
  }
  Serial.printf("ERR unknown: %s\n", line.c_str());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("policy: OTA disabled");

  if (disp_link_slave::transportMode() == disp_link_slave::TransportMode::Uart1) {
    // IO19/IO20 are S3 USB-JTAG D-/D+ pads. Release them so Serial1 can own
    // them as UART1 TX/RX. Must happen before Serial1.begin().
    REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    gpio_reset_pin(GPIO_NUM_19);
    gpio_reset_pin(GPIO_NUM_20);
    Serial.println("transport: UART1 (IO19/IO20), K1 expected 0,1");
  } else {
    Serial.println("transport: UART0-IN (IO44/IO43), shared with Serial console");
  }

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

  trendInit();
  init_lvgl();
  create_dashboard();
  {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("lvgl heap: total=%lu free=%lu used=%u%% biggest_free=%lu\n",
                  static_cast<unsigned long>(mon.total_size),
                  static_cast<unsigned long>(mon.free_size),
                  static_cast<unsigned>(mon.used_pct),
                  static_cast<unsigned long>(mon.free_biggest_size));
  }
  splash_start_ms = millis();
  demo_start_ms = splash_start_ms;
  demo_last_tour_switch_ms = splash_start_ms;
  splash_done = false;

  Serial.println("ready");
}

void loop() {
  disp_link_slave::poll();

  // In UART0 transport mode telemetry shares Serial, so command parsing must
  // stay off to avoid consuming telemetry bytes as CLI input.
  if (!disp_link_slave::telemetryOnConsoleSerial()) {
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
  }

  // Echo telemetry status to serial once per second.
  static uint32_t last_log_ms = 0;
  const uint32_t  now             = millis();
  if (kRxSerialLogEnabled && !disp_link_slave::telemetryOnConsoleSerial() && now - last_log_ms >= 1000) {
    last_log_ms = now;
    printRxStatus();
  }

  updateSetupBindingsFromUdi();
  if (active_screen == UiScreen::Setup) {
    refreshSetupScreenLabels();
  } else if (active_screen == UiScreen::Settings) {
    refreshSettingsScreenLabels(false);
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
        sampled.last_v3v3_mV = t.last_v3v3_mV;
        sampled.last_i3v3_mA = t.last_i3v3_mA;
        sampled.has_extended = t.has_extended;
        sampled.last_rx_ms = t.last_rx_ms;
        sampled.uart_bytes = t.uart_bytes;
        last_trend_rx_count = t.rx_count;
        trendPushSample(sampled, now);
      }
    }
  }

  update_telemetry_labels();  // Graph-screen labels; only changes when a new frame arrives
  refreshDashboard(false);    // Main/Detail: link state, limits, and readings every cycle

  // Boot sequence: Splash → Setup (30s timeout) → Main
  if (!splash_done && (now - splash_start_ms) >= kSplashDurationMs) {
    splash_done = true;
    enterSetupScreen();
    if (lbl_splash_hint) {
      lv_label_set_text(lbl_splash_hint, "Entering Setup...");
    }
  }

  // Setup timeout (30s) or skip if demo/tour enabled
  if (!setup_done && splash_done && (now - setup_start_ms >= 30000)) {
    setup_done = true;
    set_active_screen(UiScreen::Main);
  }

  // Demo mode: auto-tour between Setup, Main, Graph, Settings (when enabled)
  if (demo_mode && demo_tour && splash_done && setup_done && (now - demo_last_tour_switch_ms) >= kDemoTourSwitchMs) {
    demo_last_tour_switch_ms = now;
    if (active_screen == UiScreen::Setup) {
      set_active_screen(UiScreen::Main);
    } else if (active_screen == UiScreen::Main) {
      set_active_screen(UiScreen::Graph);
    } else if (active_screen == UiScreen::Graph) {
      set_active_screen(UiScreen::Settings);
    } else if (active_screen == UiScreen::Settings) {
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
