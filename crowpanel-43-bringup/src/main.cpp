#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <cstring>
#include <cmath>
#include <driver/gpio.h>
#include <soc/usb_serial_jtag_reg.h>

#include "CrowPanel43Display.h"
#include "disp_link_slave.h"
#include "ilim_core.h"
#include "ilim_core_checks.h"

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
constexpr uint32_t kSplashDurationMs = 1800;
constexpr uint32_t kDemoFrameMs = 50;
constexpr uint32_t kDemoTourSwitchMs = 8000;
// 15 min at the 5 Hz cap (kTrendSampleMs). sizeof(TrendSample)=28 B -> 4500 * 28 = 126,000 B in PSRAM.
constexpr size_t   kTrendCapacity = 4500;
constexpr uint16_t kChartPoints   = 120;   // max plotted points per pen; a feed rate may use fewer

// Strip-recorder feed rates (D5b): window = time per division x 6 columns.
struct FeedRate {
  uint32_t div_ms;
  const char* label;
};
constexpr FeedRate kFeedRates[] = {
  {10000UL, "10 s"}, {30000UL, "30 s"}, {60000UL, "1 min"},
  {300000UL, "5 min"}, {900000UL, "15 min"}, {3600000UL, "1 hr"},
};
constexpr uint8_t kFeedRateCount = sizeof(kFeedRates) / sizeof(kFeedRates[0]);
constexpr uint8_t kGraphDivisions = 6;
constexpr uint32_t kGraphMinBucketMs = 1000;  // keeps >= 1 sample per point at the 500 ms STM32 cadence
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
  Micro = 6,
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

// Host-confirmed ILIM per channel (index 0 = CH1, 1 = CH2). Only ACK/EVT/GET replies write it, so it
// never holds a draft; Setup keeps its own edit copy in setup_binding.
struct LimitConfirmed {
  bool have;
  bool fresh;      // false after a link loss until the host answers again
  uint16_t mA;
  uint32_t seq;    // bumps on every host report
};
static LimitConfirmed limit_confirmed[2] = {};
// The single outstanding ILIM write shared by the Iset editor, Setup and the console aid.
static ilim::Tx limit_tx = {};
static char limit_refusal[56] = "";
static uint32_t limit_last_ctrl_dropped = 0;

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

// Graph recorder state (D5b). Pen order matches lbl_pen_val: CH1 V, CH1 I, CH2 V, CH2 I.
static uint8_t graph_rate_idx = 1;
static bool graph_paused = false;
static uint32_t graph_pause_ms = 0;
static uint32_t graph_pause_oldest_ms = 0;  // oldest ring sample when paused; detects rollover while frozen
static bool graph_pause_has_oldest = false;
static bool graph_pen_visible[4] = {true, true, true, true};
static bool graph_dirty = true;
static uint32_t graph_last_end_bucket = 0;
static uint32_t graph_last_total = 0;
static lv_coord_t graph_pts[4][kChartPoints];
static lv_obj_t* graph_pen_cell[4] = {};
static lv_obj_t* graph_pen_tag[4] = {};
static lv_obj_t* graph_feed_lbl = nullptr;
static lv_obj_t* graph_pause_btn = nullptr;
static lv_obj_t* graph_pause_lbl = nullptr;

// Per-channel micro-view (D5c): one screen reused for CH1/CH2; feed rate and pause are shared with Graph.
static lv_obj_t* screen_micro = nullptr;
static uint8_t micro_channel = 0;
static bool micro_dirty = true;
static uint32_t micro_last_end_bucket = 0;
static uint32_t micro_last_total = 0;
static lv_coord_t micro_pts[2][kChartPoints];  // [0] voltage mV, [1] current mA
static lv_obj_t* micro_chart = nullptr;
static lv_chart_series_t* micro_ser_v = nullptr;
static lv_chart_series_t* micro_ser_i = nullptr;
static lv_obj_t* micro_title = nullptr;
static lv_obj_t* micro_ch_btn[2] = {};
static lv_obj_t* micro_lbl_v_now = nullptr;
static lv_obj_t* micro_lbl_i_now = nullptr;
static lv_obj_t* micro_lbl_v_mm = nullptr;
static lv_obj_t* micro_lbl_i_mm = nullptr;
static lv_obj_t* micro_axis_v[5] = {};
static lv_obj_t* micro_axis_i[5] = {};
static lv_obj_t* micro_time[7] = {};
static lv_obj_t* micro_window_lbl = nullptr;
static lv_obj_t* micro_feed_lbl = nullptr;
static lv_obj_t* micro_pause_btn = nullptr;
static lv_obj_t* micro_pause_lbl = nullptr;
static lv_obj_t* micro_fault_lbl = nullptr;
static Chip micro_link_chip;
static lv_obj_t* micro_note = nullptr;
static lv_obj_t* micro_back_lbl = nullptr;
static UiScreen micro_return_screen = UiScreen::Graph;
static bool micro_have_points = false;
static bool micro_window_lost = false;  // paused window lost samples to ring rollover
static bool micro_dim = false;
static int32_t micro_scale_key = -1;

// Held display-only axis span (snapped data bounds); expands at once, contracts only when much larger than needed.
struct MicroSpan {
  int32_t lo;
  int32_t hi;
  bool valid;
};
static MicroSpan micro_hold_v = {0, 0, false};
static MicroSpan micro_hold_i = {0, 0, false};

static UiScreen active_screen = UiScreen::Splash;
static bool splash_done = false;
static uint32_t splash_start_ms = 0;

// ── telemetry change-detection ─────────────────────────────────────────────
static uint32_t last_drawn_seq   = 0xFFFFFFFFu;
static uint32_t last_drawn_count = 0;
static uint32_t last_ui_update_ms = 0;
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
bool limitWrite(uint8_t ch, uint16_t mA, char* why, size_t why_len);
void limitEditorClose();

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
  graph_dirty = true;
  micro_dirty = true;
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
  if (screen != UiScreen::Detail) limitEditorClose();  // closes the view only; a sent write stays tracked
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
  if (screen_micro) {
    if (screen == UiScreen::Micro) lv_obj_clear_flag(screen_micro, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(screen_micro, LV_OBJ_FLAG_HIDDEN);
  }
  if (screen == UiScreen::Detail) detail_chart_dirty = true;
  if (screen == UiScreen::Graph) graph_dirty = true;
  if (screen == UiScreen::Micro) micro_dirty = true;
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
  long limit = 0;
  if (sscanf(payload, "ILIM %7s %ld", channel, &limit) != 2 || limit < 0 || limit > 65535L) {
    return false;
  }
  strncpy(channel_out, channel, channel_len - 1);
  channel_out[channel_len - 1] = '\0';
  *limit_mA_out = static_cast<uint16_t>(limit);
  return true;
}

// One host-reported limit (ACK, EVT or GET reply). Updates the confirmed value, the Setup copy
// (unless Setup is mid-edit on that channel) and resolves a matching pending write.
void limitReportReceived(uint8_t ch, uint16_t mA) {
  LimitConfirmed& c = limit_confirmed[ch];
  c.have = true;
  c.fresh = true;
  c.mA = mA;
  c.seq++;
  const SetupField field = (ch == 0) ? SetupField::Ch1CurrentLimit : SetupField::Ch2CurrentLimit;
  if (!(setup_binding.editing && setup_binding.selected == field)) {
    if (ch == 0) setup_binding.ch1_limit_mA = mA;
    else setup_binding.ch2_limit_mA = mA;
  }
  if (ch == 0) setup_binding.have_ch1_limit = true;
  else setup_binding.have_ch2_limit = true;
  ilim::onReport(limit_tx, ch, mA);
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
      limitReportReceived(0, limit_mA);
      return;
    }
    if (strcmp(channel, "CH2") == 0) {
      limitReportReceived(1, limit_mA);
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

// One-line status of the shared ILIM write for Setup; empty when nothing has been written yet.
void limitTxSetupText(char* buf, size_t n) {
  buf[0] = '\0';
  char req[16];
  char rep[16];
  ilim::formatAmps(limit_tx.requested_mA, req, sizeof(req));
  ilim::formatAmps(limit_tx.reported_mA, rep, sizeof(rep));
  const unsigned ch = static_cast<unsigned>(limit_tx.ch) + 1u;
  if (limit_tx.state == ilim::TxState::Pending) {
    snprintf(buf, n, "ILIM CH%u %s A PENDING - waiting for host ACK", ch, req);
    return;
  }
  if (limit_tx.state == ilim::TxState::Unconfirmed) {
    snprintf(buf, n, "ILIM CH%u %s A UNCONFIRMED - reading back from host", ch, req);
    return;
  }
  switch (limit_tx.result) {
    case ilim::TxResult::Confirmed:
    case ilim::TxResult::ConfirmedLate:
      snprintf(buf, n, "ILIM CH%u CONFIRMED by host: %s A", ch, req);
      break;
    case ilim::TxResult::Rejected:
      snprintf(buf, n, "ILIM CH%u rejected by host: %.40s", ch, limit_tx.err);
      break;
    case ilim::TxResult::NotApplied:
      snprintf(buf, n, "ILIM CH%u NOT APPLIED - host reports %s A", ch, rep);
      break;
    case ilim::TxResult::NotSent:
    case ilim::TxResult::SendFailed:
      snprintf(buf, n, "ILIM CH%u NOT SENT: %.40s", ch, limit_refusal);
      break;
    default:
      break;
  }
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

  const bool tx_open = limit_tx.state != ilim::TxState::Idle;
  const char* ch1_note = setup_binding.have_ch1_limit ? "" : "(pending)";
  const char* ch2_note = setup_binding.have_ch2_limit ? "" : "(pending)";
  if (tx_open && limit_tx.ch == 0) ch1_note = "(WRITE PENDING)";
  if (tx_open && limit_tx.ch == 1) ch2_note = "(WRITE PENDING)";
  char list_buf[360];
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
           ch1_note,
           setup_binding.selected == SetupField::Ch2CurrentLimit ? '>' : ' ',
           setup_binding.ch2_limit_mA / 1000.0f,
           ch2_note);
  lv_label_set_text(lbl_setup_list, list_buf);

  char tx_buf[96];
  limitTxSetupText(tx_buf, sizeof(tx_buf));
  if (setup_binding.last_error[0] != '\0') {
    char hint_buf[160];
    snprintf(hint_buf,
             sizeof(hint_buf),
             "Host error: %s",
             setup_binding.last_error);
    lv_label_set_text(lbl_setup_hint, hint_buf);
  } else if (tx_buf[0] != '\0' && !setup_binding.editing) {
    lv_label_set_text(lbl_setup_hint, tx_buf);
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

// Drains the ordered ACK/ERR/EVT FIFO, so back-to-back replies are all applied (and a write is
// matched only by its own reply), instead of reading the last-line-wins snapshot.
void updateSetupBindingsFromUdi() {
  // On UART0 the console is the host link; any non-CMD: line we print draws an ERR reply, which we would echo again.
  const bool echo = !disp_link_slave::telemetryOnConsoleSerial();
  disp_link_slave::ControlLine line;
  uint8_t budget = 16;
  while (budget-- > 0 && disp_link_slave::popControlLine(&line)) {
    switch (line.kind) {
      case disp_link_slave::ControlKind::Ack:
        if (echo) Serial.printf("udi ack: %s\n", line.text[0] ? line.text : "(empty)");
        applySetupAck(line.text);
        break;
      case disp_link_slave::ControlKind::Err:
        if (echo) Serial.printf("udi err: %s\n", line.text[0] ? line.text : "(empty)");
        applySetupError(line.text);
        ilim::onErr(limit_tx, line.text);  // claims only ILIM errors; others stay unrelated
        break;
      case disp_link_slave::ControlKind::Evt:
        if (echo) Serial.printf("udi evt: %s\n", line.text[0] ? line.text : "(empty)");
        applySetupEvent(line.text);
        break;
    }
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
    if (limit_tx.state != ilim::TxState::Idle) {
      strncpy(setup_binding.last_error, "ILIM write unresolved - wait before changing OUTPUT",
              sizeof(setup_binding.last_error) - 1);
      setup_binding.last_error[sizeof(setup_binding.last_error) - 1] = '\0';
      setup_binding.editing = false;
      refreshSetupScreenLabels();
      return;
    }
    sent = disp_link_slave::sendCommand(setup_binding.output_enabled ? "OUTPUT ON" : "OUTPUT OFF");
  } else {
    // Same transaction as the Iset editor: one tracked write, confirmed only by a matching reply.
    const bool ch1 = setup_binding.selected == SetupField::Ch1CurrentLimit;
    char why[sizeof(setup_binding.last_error)];
    sent = limitWrite(ch1 ? 0u : 1u,
                      ch1 ? setup_binding.ch1_limit_mA : setup_binding.ch2_limit_mA,
                      why, sizeof(why));
    if (!sent) {
      strncpy(setup_binding.last_error, why, sizeof(setup_binding.last_error) - 1);
      setup_binding.last_error[sizeof(setup_binding.last_error) - 1] = '\0';
      setup_binding.editing = false;
      refreshSetupScreenLabels();
      return;
    }
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

lv_obj_t* create_nav_btn(lv_obj_t* parent, const char* text, UiScreen target, int x_ofs, int h = 32) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 102, h);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, x_ofs, h > 32 ? -1 : 6);
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
constexpr int kMicroBtnW = 132;
constexpr int kMicroBtnH = 56;
constexpr int kResultBoxW = 136;
constexpr int kResultBoxH = 56;
constexpr int kHdrBtnH = 44;
constexpr int kMicroChBtnW = 56;
constexpr int kMicroBackW = 110;
static_assert(kBackBtnW >= kTouchMinPx && kBackBtnH >= kTouchMinPx, "Back button below 44x44");
static_assert(kOutBtnW >= kTouchMinPx && kOutBtnH >= kTouchMinPx, "OUTPUT button below 44x44");
static_assert(kNavBtnW >= kTouchMinPx && kNavBtnH >= kTouchMinPx, "Nav button below 44x44");
static_assert(kCardW >= kTouchMinPx && kCardH >= kTouchMinPx, "Channel card below 44x44");
static_assert(kMicroBtnW >= kTouchMinPx && kMicroBtnH >= kTouchMinPx, "Micro button below 44x44");
static_assert(kMicroChBtnW >= kTouchMinPx && kHdrBtnH >= kTouchMinPx, "Micro CH button below 44x44");
static_assert(kMicroBackW >= kTouchMinPx && kHdrBtnH >= kTouchMinPx, "Micro Back button below 44x44");

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

void openMicroView(uint8_t channel);

void dash_open_detail_cb(lv_event_t* e) {
  const uintptr_t ch = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  detail_channel = (ch == 1u) ? 1u : 0u;
  set_active_screen(UiScreen::Detail);
}

void dash_back_cb(lv_event_t* /*e*/) {
  set_active_screen(UiScreen::Main);
}

// Tapping the detail graph panel opens the dedicated Graph screen.
void dash_open_graph_cb(lv_event_t* /*e*/) {
  set_active_screen(UiScreen::Graph);
}

void dash_open_micro_cb(lv_event_t* /*e*/) {
  openMicroView(detail_channel);
}

void sendOutputCommand(bool turn_on) {
  const uint32_t now_ms = millis();
  const auto link = disp_link_slave::commandSnapshot();
  if (limit_tx.state != ilim::TxState::Idle) {
    postNotice("OUTPUT: ILIM write unresolved - wait", UiTheme::kAccentWarn, now_ms);
    refreshDashboard(true);
    return;
  }
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
  if (out_cmd.pending || limit_tx.state != ilim::TxState::Idle) return;
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

// ── Iset editor (#74/#75): modal slider + numeric keypad over one draft ─────
// Scope: display-side only, existing `CMD:ILIM CHn <mA>` / `GET ILIM CHn`. Draft, keypad text and the
// host-confirmed value are three separate things; only Apply sends, and only a matching host reply
// (or a readback) confirms. All widgets are created once and shown/hidden, so opening and closing
// allocates nothing and registers no timers or extra callbacks.
constexpr int kEdX = 60;
constexpr int kEdY = 36;
constexpr int kEdW = 680;
constexpr int kEdH = 408;
constexpr int kEdInnerW = kEdW - 6;
constexpr int kEdInnerH = kEdH - 6;
constexpr int kEdBtnW = 200;
constexpr int kEdBtnH = 56;
constexpr int kEdApplyW = 214;
constexpr int kEdBackW = 130;
constexpr int kEdBackH = 44;
constexpr int kEdFieldW = 300;
constexpr int kEdFieldH = 76;
constexpr int kEdKeyW = 76;
constexpr int kEdKeyH = 60;
constexpr int kEdKeyGap = 8;
constexpr int kEdSliderH = 20;
constexpr int kEdSliderClickPad = 14;  // slider track 20 px + 2 x 14 = 48 px touch height
static_assert(kEdBtnW >= kTouchMinPx && kEdBtnH >= kTouchMinPx, "Editor button below 44x44");
static_assert(kEdApplyW >= kTouchMinPx, "Apply below 44 px wide");
static_assert(kEdBackW >= kTouchMinPx && kEdBackH >= kTouchMinPx, "Keypad Back below 44x44");
static_assert(kEdFieldW >= kTouchMinPx && kEdFieldH >= kTouchMinPx, "Value field below 44x44");
static_assert(kEdKeyW >= kTouchMinPx && kEdKeyH >= kTouchMinPx, "Keypad key below 44x44");
static_assert(kEdSliderH + 2 * kEdSliderClickPad >= kTouchMinPx, "Slider touch height below 44");
static_assert(kEdY + kEdH <= kDisplayHeight && kEdX + kEdW <= kDisplayWidth, "Editor popup off screen");

enum class EditView : uint8_t { Closed, Slider, Keypad };

struct LimitEditor {
  EditView view;
  uint8_t ch;
  bool drafted;        // draft was initialised from a host report received after opening
  bool sent_session;   // Apply was pressed since opening: dismissal is then "Close", not "Cancel"
  uint16_t draft_mA;
  uint32_t seq_at_open;
  uint32_t seen_seq;
  uint16_t seen_mA;
  bool get_sent;
  uint32_t get_sent_ms;
  char entry[ilim::kEntryCap];
  char entry_msg[104];
  uint32_t entry_msg_color;
  char note[104];
  uint32_t note_color;
};
static LimitEditor ed = {};

struct LimitEditorUi {
  lv_obj_t* scrim;
  lv_obj_t* popup;
  lv_obj_t* slider_view;
  lv_obj_t* keypad_view;
  Chip ch_chip;
  lv_obj_t* conf_lbl;
  Chip state_chip;
  lv_obj_t* field_btn;
  lv_obj_t* field_val;
  lv_obj_t* slider;
  lv_obj_t* min_lbl;
  lv_obj_t* max_lbl;
  lv_obj_t* msg_lbl;
  lv_obj_t* cancel_btn;
  lv_obj_t* cancel_lbl;
  lv_obj_t* refresh_btn;
  lv_obj_t* apply_btn;
  lv_obj_t* apply_lbl;
  Chip k_ch_chip;
  lv_obj_t* k_entry;
  lv_obj_t* k_draft_lbl;
  lv_obj_t* k_range_lbl;
  lv_obj_t* k_msg;
};
static LimitEditorUi edui = {};
static lv_obj_t* detail_res_l1 = nullptr;
static lv_obj_t* detail_res_l2 = nullptr;

void copyText(char* dst, size_t n, const char* src) {
  if (n == 0) return;
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

// Console evidence for bench logs. Skipped when the console is the host link (UART0 mode).
void logLimitTx(const char* what) {
  if (disp_link_slave::telemetryOnConsoleSerial()) return;
  Serial.printf("ilim tx: %s | CH%u req=%u rep=%u state=%u result=%u why=%u err=%s\n", what,
                static_cast<unsigned>(limit_tx.ch) + 1u, static_cast<unsigned>(limit_tx.requested_mA),
                static_cast<unsigned>(limit_tx.reported_mA), static_cast<unsigned>(limit_tx.state),
                static_cast<unsigned>(limit_tx.result), static_cast<unsigned>(limit_tx.why),
                limit_tx.err[0] ? limit_tx.err : "-");
}

// Every ILIM write (editor, Setup, console aid) goes through here: one send, only while the link is
// live and nothing else is unresolved. true means "sent"; only the host's reply confirms it.
bool limitWrite(uint8_t ch, uint16_t mA, char* why, size_t why_len) {
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  const LinkState link = linkStateOf(t, now_ms, nullptr);
  const char* refuse = nullptr;
  if (ch > 1u) refuse = "bad channel";
  else if (link == LinkState::Demo) refuse = "demo data - no host control";
  else if (link != LinkState::Live) refuse = "link not live";
  else if (limit_tx.state != ilim::TxState::Idle) refuse = "another limit write is unresolved";
  else if (out_cmd.pending) refuse = "OUTPUT command pending";
  else if (mA < ilim::kMinWriteMa) refuse = "zero limit is blocked";
  else if (mA > ilim::kMaxMa[ch]) refuse = "above channel maximum";
  if (refuse != nullptr) {
    copyText(limit_refusal, sizeof(limit_refusal), refuse);
    if (why != nullptr) copyText(why, why_len, refuse);
    ilim::noteNotSent(limit_tx, ch, mA);
    return false;
  }
  char payload[24];
  snprintf(payload, sizeof(payload), "ILIM CH%u %u", static_cast<unsigned>(ch) + 1u, static_cast<unsigned>(mA));
  if (!disp_link_slave::sendCommand(payload)) {
    copyText(limit_refusal, sizeof(limit_refusal), "UART not ready");
    if (why != nullptr) copyText(why, why_len, "UART not ready");
    ilim::noteSendFailed(limit_tx, ch, mA);
    return false;
  }
  ilim::begin(limit_tx, ch, mA, now_ms);
  if (why != nullptr && why_len > 0) why[0] = '\0';
  return true;
}

// Per-loop upkeep of the shared write: staleness after link loss, timeouts, readback after an
// uncertain outcome, and the Setup copy after a write ends. Runs on every screen.
void serviceLimitLink() {
  static bool was_live = false;
  static ilim::TxState prev_state = ilim::TxState::Idle;
  static ilim::TxResult prev_result = ilim::TxResult::None;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  const LinkState link = linkStateOf(t, now_ms, nullptr);
  const bool live = (link == LinkState::Live);

  if (was_live && !live) {
    limit_confirmed[0].fresh = false;  // values must be re-read once the host is back
    limit_confirmed[1].fresh = false;
  }
  was_live = live;

  const uint32_t dropped = disp_link_slave::commandSnapshot().ctrl_dropped;
  if (dropped != limit_last_ctrl_dropped) {
    limit_last_ctrl_dropped = dropped;
    ilim::markUnconfirmed(limit_tx, ilim::Why::Dropped);
  }
  ilim::tick(limit_tx, now_ms, live);
  if (ilim::reconcileDue(limit_tx, now_ms, live, false)) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "GET ILIM CH%u", static_cast<unsigned>(limit_tx.ch) + 1u);
    if (disp_link_slave::sendCommand(cmd)) ilim::markReconcileSent(limit_tx, now_ms);
  }

  if (limit_tx.state != prev_state || limit_tx.result != prev_result) {
    logLimitTx(limit_tx.state == ilim::TxState::Pending ? "write sent" : "state change");
    prev_state = limit_tx.state;
    prev_result = limit_tx.result;
  }

  // Setup shows its own edit copy; once nothing is in flight and Setup is not editing, resync it.
  if (!setup_binding.editing && limit_tx.state == ilim::TxState::Idle) {
    if (limit_confirmed[0].have) setup_binding.ch1_limit_mA = limit_confirmed[0].mA;
    if (limit_confirmed[1].have) setup_binding.ch2_limit_mA = limit_confirmed[1].mA;
  }
}

// Detail footer "last ILIM result" box: two short lines, never claims more than the host confirmed.
void refreshLimitResultBox() {
  if (!detail_res_l1 || !detail_res_l2) return;
  char l1[24] = "LAST ILIM";
  char l2[32] = "--";
  uint32_t color = UiTheme::kTextMuted;
  char amt[16];
  const unsigned ch = static_cast<unsigned>(limit_tx.ch) + 1u;
  if (limit_tx.state == ilim::TxState::Pending) {
    ilim::formatAmps(limit_tx.requested_mA, amt, sizeof(amt));
    snprintf(l1, sizeof(l1), "CH%u PENDING", ch);
    snprintf(l2, sizeof(l2), "%s A", amt);
    color = UiTheme::kAccentWarn;
  } else if (limit_tx.state == ilim::TxState::Unconfirmed) {
    snprintf(l1, sizeof(l1), "CH%u UNCONF.", ch);
    copyText(l2, sizeof(l2), limit_tx.why == ilim::Why::Timeout ? "timeout"
                             : limit_tx.why == ilim::Why::LinkLost ? "link lost" : "reply lost");
    color = UiTheme::kAccentWarn;
  } else {
    switch (limit_tx.result) {
      case ilim::TxResult::Confirmed:
      case ilim::TxResult::ConfirmedLate:
        ilim::formatAmps(limit_tx.requested_mA, amt, sizeof(amt));
        snprintf(l1, sizeof(l1), "CH%u OK", ch);
        snprintf(l2, sizeof(l2), "%s A", amt);
        color = UiTheme::kAccentOk;
        break;
      case ilim::TxResult::Rejected:
        snprintf(l1, sizeof(l1), "CH%u ERR", ch);
        copyText(l2, sizeof(l2), "host rejected");
        color = UiTheme::kError;
        break;
      case ilim::TxResult::NotApplied:
        ilim::formatAmps(limit_tx.reported_mA, amt, sizeof(amt));
        snprintf(l1, sizeof(l1), "CH%u NOT SET", ch);
        snprintf(l2, sizeof(l2), "host %s A", amt);
        color = UiTheme::kAccentWarn;
        break;
      case ilim::TxResult::NotSent:
      case ilim::TxResult::SendFailed:
        snprintf(l1, sizeof(l1), "CH%u NOT SENT", ch);
        copyText(l2, sizeof(l2), limit_refusal);
        color = UiTheme::kError;
        break;
      default:
        break;
    }
  }
  setLabel(detail_res_l1, l1);
  setLabel(detail_res_l2, l2);
  static uint32_t last_color = 0xFFFFFFFFu;
  if (color != last_color) {
    last_color = color;
    lv_obj_set_style_text_color(detail_res_l1, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_color(detail_res_l2, lv_color_hex(color), LV_PART_MAIN);
  }
}

enum class Block : uint8_t { None, NoValue, Demo, LinkDown, Refreshing, Unresolved, OutputBusy, Invalid, NoChange };

Block edApplyBlock(bool live, bool demo) {
  const LimitConfirmed& cf = limit_confirmed[ed.ch];
  if (!ed.drafted) return Block::NoValue;
  if (demo) return Block::Demo;
  if (!live) return Block::LinkDown;
  if (!cf.fresh) return Block::Refreshing;
  if (limit_tx.state != ilim::TxState::Idle) return Block::Unresolved;
  if (out_cmd.pending) return Block::OutputBusy;
  if (ed.draft_mA < ilim::kMinWriteMa || ed.draft_mA > ilim::kMaxMa[ed.ch]) return Block::Invalid;
  if (ed.draft_mA == cf.mA) return Block::NoChange;
  return Block::None;
}

void edSetEnabled(lv_obj_t* obj, bool enabled) {
  if (!obj) return;
  if (enabled == !lv_obj_has_state(obj, LV_STATE_DISABLED)) return;
  if (enabled) {
    lv_obj_clear_state(obj, LV_STATE_DISABLED);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  } else {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
    lv_obj_set_style_opa(obj, LV_OPA_40, LV_PART_MAIN);
  }
}

void edSetMsg(lv_obj_t* lbl, const char* text, uint32_t color) {
  setLabel(lbl, text);
  const lv_color_t c = lv_color_hex(color);
  if (lv_obj_get_style_text_color(lbl, LV_PART_MAIN).full != c.full) {
    lv_obj_set_style_text_color(lbl, c, LV_PART_MAIN);
  }
}

void edParseText(ilim::Parse p, char* out, size_t n) {
  char max[16];
  ilim::formatAmps(ilim::kMaxMa[ed.ch], max, sizeof(max));
  switch (p) {
    case ilim::Parse::Empty:
      copyText(out, n, "Nothing typed. Type a value, or tap Back to keep the draft.");
      break;
    case ilim::Parse::Malformed:
      copyText(out, n, "Not a valid number.");
      break;
    case ilim::Parse::Negative:
      copyText(out, n, "Negative values are not allowed.");
      break;
    case ilim::Parse::TooManyDecimals:
      copyText(out, n, "At most 3 decimals (1 mA). Not rounded.");
      break;
    case ilim::Parse::Zero:
      copyText(out, n, "Zero is blocked. Minimum is 0.001 A.");
      break;
    case ilim::Parse::OutOfRange:
      snprintf(out, n, "Above the CH%u maximum of %s A. Not clamped.", static_cast<unsigned>(ed.ch) + 1u, max);
      break;
    default:
      out[0] = '\0';
      break;
  }
}

// Live validation of the keypad text; also run after every key press.
void edEntryHint() {
  char max[16];
  ilim::formatAmps(ilim::kMaxMa[ed.ch], max, sizeof(max));
  if (ed.entry[0] == '\0') {
    snprintf(ed.entry_msg, sizeof(ed.entry_msg), "Type the new limit: 0.001 to %s A, up to 3 decimals.", max);
    ed.entry_msg_color = UiTheme::kTextMuted;
    return;
  }
  const ilim::ParseResult r = ilim::parseAmps(ed.entry, ed.ch);
  if (r.status == ilim::Parse::Ok) {
    char amt[16];
    ilim::formatAmps(r.mA, amt, sizeof(amt));
    snprintf(ed.entry_msg, sizeof(ed.entry_msg), "Valid: %s A. Tap OK to use it as the draft.", amt);
    ed.entry_msg_color = UiTheme::kAccentOk;
  } else {
    edParseText(r.status, ed.entry_msg, sizeof(ed.entry_msg));
    ed.entry_msg_color = UiTheme::kAccentWarn;
  }
}

void limitEditorRefresh() {
  if (ed.view == EditView::Closed || !edui.scrim) return;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  const LinkState link = linkStateOf(t, now_ms, nullptr);
  const bool live = (link == LinkState::Live);
  const bool demo = (link == LinkState::Demo);
  const LimitConfirmed& cf = limit_confirmed[ed.ch];
  const bool tx_open = limit_tx.state != ilim::TxState::Idle;
  const bool tx_mine = tx_open && limit_tx.ch == ed.ch;
  const unsigned chn = static_cast<unsigned>(ed.ch) + 1u;
  char amt[16];

  // The draft starts only from a host report that arrived after opening; never from a cached or default value.
  if (!ed.drafted && cf.have && cf.fresh && cf.seq != ed.seq_at_open) {
    ed.drafted = true;
    ed.draft_mA = cf.mA;
    ed.seen_seq = cf.seq;
    ed.seen_mA = cf.mA;
  }
  if (ed.drafted && cf.seq != ed.seen_seq) {
    if (cf.mA != ed.seen_mA && !ed.sent_session) {
      ilim::formatAmps(cf.mA, amt, sizeof(amt));
      snprintf(ed.note, sizeof(ed.note), "Host now reports %s A (changed outside this editor). Draft kept.", amt);
      ed.note_color = UiTheme::kAccentWarn;
    }
    ed.seen_seq = cf.seq;
    ed.seen_mA = cf.mA;
  }
  if (!ed.drafted && live && !tx_mine && !out_cmd.pending &&
      (!ed.get_sent || (now_ms - ed.get_sent_ms) >= kLimitGetRetryMs)) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "GET ILIM CH%u", chn);
    if (disp_link_slave::sendCommand(cmd)) {
      ed.get_sent = true;
      ed.get_sent_ms = now_ms;
    }
  }
  const bool no_reply = ed.get_sent && (now_ms - ed.get_sent_ms) >= kLimitGetRetryMs;

  setHidden(edui.slider_view, ed.view != EditView::Slider);
  setHidden(edui.keypad_view, ed.view != EditView::Keypad);

  const Block blk = edApplyBlock(live, demo);
  const bool locked = !ed.drafted || tx_mine;

  if (ed.view == EditView::Keypad) {
    ilim::formatAmps(ed.draft_mA, amt, sizeof(amt));
    char buf[40];
    snprintf(buf, sizeof(buf), "Draft now: %s A", amt);
    setLabel(edui.k_draft_lbl, buf);
    setLabel(edui.k_entry, ed.entry[0] ? ed.entry : "-.---");
    setTextOpa(edui.k_entry, ed.entry[0] ? LV_OPA_COVER : LV_OPA_40);
    edSetMsg(edui.k_msg, ed.entry_msg, ed.entry_msg_color);
    return;
  }

  // Confirmed (host) value row.
  char buf[104];
  if (cf.have) {
    ilim::formatAmps(cf.mA, amt, sizeof(amt));
    snprintf(buf, sizeof(buf), "Confirmed limit: %s A%s", amt, cf.fresh ? "" : " (stale)");
  } else {
    copyText(buf, sizeof(buf), "Confirmed limit: -.--- A");
  }
  setLabel(edui.conf_lbl, buf);

  // State chip.
  const char* chip = "";
  uint32_t chip_c = UiTheme::kUnknown;
  if (tx_mine && limit_tx.state == ilim::TxState::Pending) {
    chip = "PENDING";
    chip_c = UiTheme::kAccentWarn;
  } else if (tx_mine) {
    chip = "UNCONFIRMED";
    chip_c = UiTheme::kAccentWarn;
  } else if (!ed.drafted) {
    if (demo) { chip = "DEMO - NO CONTROL"; chip_c = UiTheme::kDemo; }
    else if (!live || no_reply) { chip = "UNAVAILABLE"; chip_c = UiTheme::kAccentWarn; }
    else chip = "LOADING...";
  } else if (!live) {
    chip = "LINK NOT LIVE";
    chip_c = UiTheme::kAccentWarn;
  } else if (!cf.fresh) {
    chip = "REFRESHING";
  } else if (ed.draft_mA == cf.mA) {
    chip = "NO CHANGE";
  } else {
    chip = "DRAFT - NOT APPLIED";
    chip_c = UiTheme::kAccentWarn;
  }
  setChip(edui.state_chip, chip, chip_c);

  // Draft field and slider.
  if (ed.drafted) {
    ilim::formatAmps(ed.draft_mA, amt, sizeof(amt));
    setLabel(edui.field_val, amt);
    const int32_t pos = ilim::sliderFromMa(ed.draft_mA, ed.ch);
    if (lv_slider_get_value(edui.slider) != pos && !lv_slider_is_dragged(edui.slider)) {
      lv_slider_set_value(edui.slider, pos, LV_ANIM_OFF);  // does not fire VALUE_CHANGED, so no draft change
    }
  } else {
    setLabel(edui.field_val, "-.---");
  }
  setTextOpa(edui.field_val, ed.drafted ? LV_OPA_COVER : LV_OPA_40);
  edSetEnabled(edui.slider, !locked);
  edSetEnabled(edui.field_btn, !locked);

  // Message line, most important first.
  char req[16];
  char rep[16];
  char cur[16];
  ilim::formatAmps(limit_tx.requested_mA, req, sizeof(req));
  ilim::formatAmps(limit_tx.reported_mA, rep, sizeof(rep));
  ilim::formatAmps(cf.mA, cur, sizeof(cur));
  uint32_t msg_c = UiTheme::kTextMuted;
  if (tx_mine && limit_tx.state == ilim::TxState::Pending) {
    snprintf(buf, sizeof(buf), "PENDING: ILIM CH%u %s A sent. Waiting for the host ACK. Closing does not undo it.", chn, req);
    msg_c = UiTheme::kAccentWarn;
  } else if (tx_mine) {
    const char* why = limit_tx.why == ilim::Why::Timeout ? "no reply in time"
                      : limit_tx.why == ilim::Why::LinkLost ? "link lost" : "reply lost";
    snprintf(buf, sizeof(buf), "UNCONFIRMED (%s): it may have applied. Reading back; Apply stays locked.", why);
    msg_c = UiTheme::kAccentWarn;
  } else if (tx_open) {
    snprintf(buf, sizeof(buf), "CH%u limit write is unresolved. Wait for its result before writing CH%u.",
             static_cast<unsigned>(limit_tx.ch) + 1u, chn);
    msg_c = UiTheme::kAccentWarn;
  } else if (ed.sent_session && limit_tx.ch == ed.ch && limit_tx.result != ilim::TxResult::None) {
    switch (limit_tx.result) {
      case ilim::TxResult::Confirmed:
        snprintf(buf, sizeof(buf), "CONFIRMED by host: limit is now %s A.", req);
        msg_c = UiTheme::kAccentOk;
        break;
      case ilim::TxResult::ConfirmedLate:
        snprintf(buf, sizeof(buf), "CONFIRMED by readback: limit is now %s A.", req);
        msg_c = UiTheme::kAccentOk;
        break;
      case ilim::TxResult::Rejected:
        snprintf(buf, sizeof(buf), "ERR from host (%.40s). Confirmed limit unchanged: %s A.", limit_tx.err, cur);
        msg_c = UiTheme::kError;
        break;
      case ilim::TxResult::NotApplied:
        snprintf(buf, sizeof(buf), "NOT APPLIED: host still reports %s A. You may retry.", rep);
        msg_c = UiTheme::kAccentWarn;
        break;
      default:
        snprintf(buf, sizeof(buf), "NOT SENT: %.50s. Nothing was written.", limit_refusal);
        msg_c = UiTheme::kError;
        break;
    }
  } else if (!ed.drafted) {
    if (demo) copyText(buf, sizeof(buf), "DEMO DATA: no host control, so the limit cannot be edited.");
    else if (!live) copyText(buf, sizeof(buf), "Link not live: cannot read the host limit. Tap REFRESH when it is back.");
    else if (no_reply) copyText(buf, sizeof(buf), "No reply from the host yet. Retrying; tap REFRESH to retry now.");
    else copyText(buf, sizeof(buf), "Reading the current limit from the host...");
    if (demo || !live || no_reply) msg_c = UiTheme::kAccentWarn;
  } else if (blk == Block::Invalid) {
    ilim::formatAmps(ed.draft_mA, amt, sizeof(amt));
    if (ed.draft_mA < ilim::kMinWriteMa) {
      snprintf(buf, sizeof(buf), "Draft %s A: zero writes are blocked. Choose 0.001 A or more.", amt);
    } else {
      snprintf(buf, sizeof(buf), "Draft %s A is above the CH%u editor range.", amt, chn);
    }
    msg_c = UiTheme::kError;
  } else if (blk == Block::Demo || blk == Block::LinkDown) {
    copyText(buf, sizeof(buf), blk == Block::Demo ? "DEMO DATA: Apply is disabled."
                                                    : "Link not live: Apply is disabled.");
    msg_c = UiTheme::kAccentWarn;
  } else if (blk == Block::Refreshing) {
    copyText(buf, sizeof(buf), "Refreshing the limit from the host. Apply is disabled until it answers.");
    msg_c = UiTheme::kAccentWarn;
  } else if (blk == Block::OutputBusy) {
    copyText(buf, sizeof(buf), "An OUTPUT command is pending. Apply is disabled until it finishes.");
    msg_c = UiTheme::kAccentWarn;
  } else if (ed.note[0] != '\0') {
    copyText(buf, sizeof(buf), ed.note);
    msg_c = ed.note_color;
  } else if (blk == Block::NoChange) {
    copyText(buf, sizeof(buf), "No change from the confirmed limit. Drag the slider or tap the value.");
  } else {
    copyText(buf, sizeof(buf), "Draft only: nothing is sent until you tap Apply.");
  }
  edSetMsg(edui.msg_lbl, buf, msg_c);

  // Buttons.
  const char* apply_txt = "Apply";
  if (tx_mine && limit_tx.state == ilim::TxState::Pending) apply_txt = "PENDING...";
  else if (tx_mine) apply_txt = "LOCKED";
  setLabel(edui.apply_lbl, apply_txt);
  edSetEnabled(edui.apply_btn, blk == Block::None);
  edSetEnabled(edui.refresh_btn, live && !demo && !(tx_mine && limit_tx.state == ilim::TxState::Pending));
  setLabel(edui.cancel_lbl, (tx_mine || ed.sent_session) ? "CLOSE" : "CANCEL");
}

void limitEditorClose() {
  if (ed.view == EditView::Closed) return;
  ed.view = EditView::Closed;
  ed.entry[0] = '\0';
  setHidden(edui.scrim, true);
}

void limitEditorOpen() {
  if (!edui.scrim) return;
  const uint8_t ch = (detail_channel == 1u) ? 1u : 0u;
  ed = {};
  ed.view = EditView::Slider;
  ed.ch = ch;
  ed.seq_at_open = limit_confirmed[ch].seq;
  ed.seen_seq = ed.seq_at_open;
  ed.entry_msg_color = UiTheme::kTextMuted;
  ed.note_color = UiTheme::kTextMuted;

  const RailSpec& r = kRails[ch];
  static const char* const kChipText[2] = {"CH1  +5V Supply", "CH2  +3.3V Supply"};
  lv_obj_set_style_border_color(edui.popup, lv_color_hex(r.color), LV_PART_MAIN);
  setLabel(edui.ch_chip.lbl, kChipText[ch]);
  fillChip(edui.ch_chip, r.color);
  setLabel(edui.k_ch_chip.lbl, kChipText[ch]);
  fillChip(edui.k_ch_chip, r.color);
  lv_obj_set_style_bg_color(edui.slider, lv_color_hex(r.color), LV_PART_INDICATOR);
  lv_obj_set_style_border_color(lv_obj_get_parent(edui.k_entry), lv_color_hex(r.color), LV_PART_MAIN);

  char amt[16];
  char buf[24];
  ilim::formatAmps(ilim::maFromSlider(ilim::sliderMin()), amt, sizeof(amt));
  snprintf(buf, sizeof(buf), "%s A", amt);
  setLabel(edui.min_lbl, buf);
  ilim::formatAmps(ilim::kMaxMa[ch], amt, sizeof(amt));
  snprintf(buf, sizeof(buf), "%s A", amt);
  setLabel(edui.max_lbl, buf);
  lv_slider_set_range(edui.slider, ilim::sliderMin(), ilim::sliderMax(ch));
  lv_slider_set_value(edui.slider, ilim::sliderMin(), LV_ANIM_OFF);
  char range[48];
  snprintf(range, sizeof(range), "Range 0.001 - %s A, 3 decimals", amt);
  setLabel(edui.k_range_lbl, range);

  setHidden(edui.scrim, false);
  limitEditorRefresh();
}

void ed_open_cb(lv_event_t* /*e*/) { limitEditorOpen(); }

void ed_cancel_cb(lv_event_t* /*e*/) {
  limitEditorClose();  // never sends; a write already sent keeps being tracked
}

void ed_slider_cb(lv_event_t* /*e*/) {
  if (ed.view != EditView::Slider || !ed.drafted) return;
  if (limit_tx.state != ilim::TxState::Idle && limit_tx.ch == ed.ch) return;
  ed.draft_mA = ilim::maFromSlider(lv_slider_get_value(edui.slider));
  ed.note[0] = '\0';
  limitEditorRefresh();
}

void ed_field_cb(lv_event_t* /*e*/) {
  if (ed.view != EditView::Slider || !ed.drafted) return;
  if (limit_tx.state != ilim::TxState::Idle && limit_tx.ch == ed.ch) return;
  ed.view = EditView::Keypad;
  ed.entry[0] = '\0';
  edEntryHint();
  limitEditorRefresh();
}

void ed_back_cb(lv_event_t* /*e*/) {
  if (ed.view != EditView::Keypad) return;
  ed.view = EditView::Slider;  // the draft was never touched by keypad typing
  ed.entry[0] = '\0';
  limitEditorRefresh();
}

void ed_key_cb(lv_event_t* e) {
  if (ed.view != EditView::Keypad) return;
  const char code = static_cast<char>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  if (code == 'B') {
    ilim::entryBackspace(ed.entry);
    edEntryHint();
  } else if (code == 'K') {
    if (ed.entry[0] == '\0') {
      edParseText(ilim::Parse::Empty, ed.entry_msg, sizeof(ed.entry_msg));
      ed.entry_msg_color = UiTheme::kAccentWarn;
    } else {
      const ilim::ParseResult r = ilim::parseAmps(ed.entry, ed.ch);
      if (r.status != ilim::Parse::Ok) {
        edParseText(r.status, ed.entry_msg, sizeof(ed.entry_msg));
        ed.entry_msg_color = UiTheme::kError;
      } else {
        ed.draft_mA = r.mA;  // exact: not rounded to the slider step
        char amt[16];
        ilim::formatAmps(r.mA, amt, sizeof(amt));
        snprintf(ed.note, sizeof(ed.note), "Draft set to %s A from the keypad. Not applied yet.", amt);
        ed.note_color = UiTheme::kTextMuted;
        ed.view = EditView::Slider;
        ed.entry[0] = '\0';
      }
    }
  } else {
    const ilim::Entry r = ilim::entryAppend(ed.entry, sizeof(ed.entry), code);
    if (r == ilim::Entry::Ok) {
      edEntryHint();
    } else {
      switch (r) {
        case ilim::Entry::SecondPoint:
          copyText(ed.entry_msg, sizeof(ed.entry_msg), "Only one decimal point.");
          break;
        case ilim::Entry::TooManyDecimals:
          copyText(ed.entry_msg, sizeof(ed.entry_msg), "At most 3 decimals (1 mA). Extra digit ignored.");
          break;
        case ilim::Entry::TooManyIntDigits:
          copyText(ed.entry_msg, sizeof(ed.entry_msg), "At most 2 digits before the point.");
          break;
        default:
          copyText(ed.entry_msg, sizeof(ed.entry_msg), "Entry is full.");
          break;
      }
      ed.entry_msg_color = UiTheme::kAccentWarn;
    }
  }
  limitEditorRefresh();
}

void ed_refresh_cb(lv_event_t* /*e*/) {
  if (ed.view != EditView::Slider) return;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  if (linkStateOf(t, now_ms, nullptr) != LinkState::Live) return;
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "GET ILIM CH%u", static_cast<unsigned>(ed.ch) + 1u);
  if (limit_tx.state == ilim::TxState::Unconfirmed && limit_tx.ch == ed.ch) {
    if (ilim::reconcileDue(limit_tx, now_ms, true, true) && disp_link_slave::sendCommand(cmd)) {
      ilim::markReconcileSent(limit_tx, now_ms);
    }
  } else if (limit_tx.state == ilim::TxState::Idle && !out_cmd.pending) {
    if (disp_link_slave::sendCommand(cmd)) {
      ed.get_sent = true;
      ed.get_sent_ms = now_ms;
    }
  }
  limitEditorRefresh();
}

void ed_apply_cb(lv_event_t* /*e*/) {
  if (ed.view != EditView::Slider) return;
  const uint32_t now_ms = millis();
  const DisplayTelemetry t = get_display_telemetry();
  const LinkState link = linkStateOf(t, now_ms, nullptr);
  if (edApplyBlock(link == LinkState::Live, link == LinkState::Demo) != Block::None) {
    limitEditorRefresh();
    return;
  }
  char why[56];
  limitWrite(ed.ch, ed.draft_mA, why, sizeof(why));  // success or refusal is shown from limit_tx
  ed.sent_session = true;
  ed.note[0] = '\0';
  limitEditorRefresh();
}

lv_obj_t* edWrapLabel(lv_obj_t* parent, int x, int y, int w, int h, const lv_font_t* font, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_size(l, w, h);
  lv_label_set_text(l, "");
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  return l;
}

lv_obj_t* edKey(lv_obj_t* parent, int x, int y, int w, int h, const char* text, char code,
                uint32_t bg, uint32_t border, const lv_font_t* font) {
  lv_obj_t* b = makeButton(parent, x, y, w, h, bg, border, 2, 8);
  lv_obj_add_event_cb(b, ed_key_cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<unsigned char>(code))));
  makeLabel(b, 0, 0, w - 4, h - 4, text, font, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  return b;
}

// Built once at boot on the top layer; the scrim swallows every touch while the editor is shown.
void createLimitEditor() {
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
  edui.scrim = scrim;

  lv_obj_t* pop = makeBox(scrim, kEdX, kEdY, kEdW, kEdH, UiTheme::kPanel, UiTheme::kCh1, 3, 12);
  edui.popup = pop;

  // Slider view.
  lv_obj_t* sv = makeBox(pop, 0, 0, kEdInnerW, kEdInnerH, UiTheme::kPanel, UiTheme::kPanel, 0, 0);
  lv_obj_set_style_bg_opa(sv, LV_OPA_TRANSP, LV_PART_MAIN);
  edui.slider_view = sv;
  makeChip(sv, 16, 10, 290, 36, &lv_font_montserrat_28, edui.ch_chip);
  makeLabel(sv, 318, 6, 340, 26, "CURRENT LIMIT (Iset)", &lv_font_montserrat_20, UiTheme::kTextPrimary);
  makeLabel(sv, 318, 30, 340, 20, "Set limit - not measured current", &lv_font_montserrat_16, UiTheme::kTextMuted);
  edui.conf_lbl = makeLabel(sv, 16, 58, 400, 30, "Confirmed limit: -.--- A", &lv_font_montserrat_20, UiTheme::kTextPrimary);
  makeChip(sv, 420, 56, 238, 34, &lv_font_montserrat_16, edui.state_chip);
  setChip(edui.state_chip, "LOADING...", UiTheme::kUnknown);

  edui.field_btn = makeButton(sv, 187, 98, kEdFieldW, kEdFieldH, UiTheme::kScope, UiTheme::kAccentI, 2, 10);
  lv_obj_add_event_cb(edui.field_btn, ed_field_cb, LV_EVENT_CLICKED, nullptr);
  edui.field_val = makeLabel(edui.field_btn, 6, 0, 236, kEdFieldH - 4, "-.---", &lv_font_montserrat_48,
                             UiTheme::kAccentI, LV_TEXT_ALIGN_RIGHT);
  makeLabel(edui.field_btn, 250, 0, 44, kEdFieldH - 4, "A", &lv_font_montserrat_28, UiTheme::kAccentI);
  lv_obj_t* hint = edWrapLabel(sv, 16, 104, 164, 60, &lv_font_montserrat_16, UiTheme::kTextMuted);
  lv_label_set_text(hint, "Tap the value to type an exact amount");

  edui.slider = lv_slider_create(sv);
  lv_obj_set_pos(edui.slider, 44, 200);
  lv_obj_set_size(edui.slider, 586, kEdSliderH);
  lv_obj_set_ext_click_area(edui.slider, kEdSliderClickPad);
  lv_obj_set_style_bg_color(edui.slider, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(edui.slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(edui.slider, lv_color_hex(UiTheme::kCh1), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(edui.slider, lv_color_hex(UiTheme::kTextPrimary), LV_PART_KNOB);
  lv_obj_set_style_pad_all(edui.slider, 14, LV_PART_KNOB);
  lv_slider_set_range(edui.slider, ilim::sliderMin(), ilim::sliderMax(0));
  lv_obj_add_event_cb(edui.slider, ed_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  edui.min_lbl = makeLabel(sv, 16, 246, 200, 22, "0.010 A", &lv_font_montserrat_16, UiTheme::kTextMuted);
  makeLabel(sv, 220, 246, 234, 22, "step 0.010 A", &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);
  edui.max_lbl = makeLabel(sv, 458, 246, 200, 22, "3.000 A", &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_RIGHT);
  edui.msg_lbl = edWrapLabel(sv, 16, 274, 642, 54, &lv_font_montserrat_20, UiTheme::kTextMuted);

  edui.cancel_btn = makeButton(sv, 16, 336, kEdBtnW, kEdBtnH, UiTheme::kPanelSoft, UiTheme::kTextMuted, 2, 8);
  lv_obj_add_event_cb(edui.cancel_btn, ed_cancel_cb, LV_EVENT_CLICKED, nullptr);
  edui.cancel_lbl = makeLabel(edui.cancel_btn, 0, 0, kEdBtnW - 4, kEdBtnH - 4, "CANCEL", &lv_font_montserrat_28,
                              UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  edui.refresh_btn = makeButton(sv, 232, 336, kEdBtnW, kEdBtnH, UiTheme::kPanelSoft, UiTheme::kAccentI, 2, 8);
  lv_obj_add_event_cb(edui.refresh_btn, ed_refresh_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(edui.refresh_btn, 0, 0, kEdBtnW - 4, kEdBtnH - 4, "REFRESH", &lv_font_montserrat_28,
            UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  edui.apply_btn = makeButton(sv, 448, 336, kEdApplyW, kEdBtnH, 0x1E4A33, UiTheme::kAccentOk, 2, 8);
  lv_obj_add_event_cb(edui.apply_btn, ed_apply_cb, LV_EVENT_CLICKED, nullptr);
  edui.apply_lbl = makeLabel(edui.apply_btn, 0, 0, kEdApplyW - 4, kEdBtnH - 4, "Apply", &lv_font_montserrat_28,
                             UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);

  // Keypad view.
  lv_obj_t* kv = makeBox(pop, 0, 0, kEdInnerW, kEdInnerH, UiTheme::kPanel, UiTheme::kPanel, 0, 0);
  lv_obj_set_style_bg_opa(kv, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_add_flag(kv, LV_OBJ_FLAG_HIDDEN);
  edui.keypad_view = kv;
  lv_obj_t* back = makeButton(kv, 16, 10, kEdBackW, kEdBackH, UiTheme::kPanelSoft, UiTheme::kTextMuted, 2, 8);
  lv_obj_add_event_cb(back, ed_back_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(back, 0, 0, kEdBackW - 4, kEdBackH - 4, LV_SYMBOL_LEFT " Back", &lv_font_montserrat_20,
            UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  makeChip(kv, 160, 12, 290, 36, &lv_font_montserrat_28, edui.k_ch_chip);
  makeLabel(kv, 464, 14, 196, 32, "ENTER Iset", &lv_font_montserrat_20, UiTheme::kTextPrimary);

  lv_obj_t* entry_box = makeBox(kv, 16, 68, kEdFieldW, 84, UiTheme::kScope, UiTheme::kCh1, 2, 10);
  edui.k_entry = makeLabel(entry_box, 6, 0, 236, 80, "-.---", &lv_font_montserrat_48, UiTheme::kAccentI,
                           LV_TEXT_ALIGN_RIGHT);
  makeLabel(entry_box, 250, 0, 44, 80, "A", &lv_font_montserrat_28, UiTheme::kAccentI);
  edui.k_draft_lbl = makeLabel(kv, 16, 158, 304, 22, "Draft now: -.--- A", &lv_font_montserrat_16, UiTheme::kTextPrimary);
  edui.k_range_lbl = makeLabel(kv, 16, 180, 304, 22, "Range 0.001 - 3.000 A, 3 decimals", &lv_font_montserrat_16,
                               UiTheme::kTextMuted);
  edui.k_msg = edWrapLabel(kv, 16, 212, 306, 116, &lv_font_montserrat_20, UiTheme::kTextMuted);
  makeLabel(kv, 16, 346, 642, 24, "Keypad edits the draft only. Nothing is sent until Apply.", &lv_font_montserrat_16,
            UiTheme::kTextMuted);

  constexpr int kGx = 336;
  constexpr int kGy = 68;
  constexpr int kPitchX = kEdKeyW + kEdKeyGap;
  constexpr int kPitchY = kEdKeyH + kEdKeyGap;
  const uint32_t kb = UiTheme::kPanelSoft;
  const uint32_t kd = UiTheme::kBorder;
  const lv_font_t* kf = &lv_font_montserrat_28;
  edKey(kv, kGx + 0 * kPitchX, kGy + 0 * kPitchY, kEdKeyW, kEdKeyH, "7", '7', kb, kd, kf);
  edKey(kv, kGx + 1 * kPitchX, kGy + 0 * kPitchY, kEdKeyW, kEdKeyH, "8", '8', kb, kd, kf);
  edKey(kv, kGx + 2 * kPitchX, kGy + 0 * kPitchY, kEdKeyW, kEdKeyH, "9", '9', kb, kd, kf);
  edKey(kv, kGx + 3 * kPitchX, kGy + 0 * kPitchY, kEdKeyW, kEdKeyH, "DEL", 'B', kb, UiTheme::kAccentWarn,
        &lv_font_montserrat_20);
  edKey(kv, kGx + 0 * kPitchX, kGy + 1 * kPitchY, kEdKeyW, kEdKeyH, "4", '4', kb, kd, kf);
  edKey(kv, kGx + 1 * kPitchX, kGy + 1 * kPitchY, kEdKeyW, kEdKeyH, "5", '5', kb, kd, kf);
  edKey(kv, kGx + 2 * kPitchX, kGy + 1 * kPitchY, kEdKeyW, kEdKeyH, "6", '6', kb, kd, kf);
  edKey(kv, kGx + 3 * kPitchX, kGy + 1 * kPitchY, kEdKeyW, 3 * kEdKeyH + 2 * kEdKeyGap, "OK", 'K', 0x1E4A33,
        UiTheme::kAccentOk, kf);
  edKey(kv, kGx + 0 * kPitchX, kGy + 2 * kPitchY, kEdKeyW, kEdKeyH, "1", '1', kb, kd, kf);
  edKey(kv, kGx + 1 * kPitchX, kGy + 2 * kPitchY, kEdKeyW, kEdKeyH, "2", '2', kb, kd, kf);
  edKey(kv, kGx + 2 * kPitchX, kGy + 2 * kPitchY, kEdKeyW, kEdKeyH, "3", '3', kb, kd, kf);
  edKey(kv, kGx + 0 * kPitchX, kGy + 3 * kPitchY, 2 * kEdKeyW + kEdKeyGap, kEdKeyH, "0", '0', kb, kd, kf);
  edKey(kv, kGx + 2 * kPitchX, kGy + 3 * kPitchY, kEdKeyW, kEdKeyH, ".", '.', kb, kd, kf);
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
  // Reduce panel height slightly to avoid overlapping the FIXED/Iset footer boxes.
  c.panel = makeBox(screen_detail, 8, 84, 320, 280, UiTheme::kPanel, UiTheme::kBorder, 2, 10);
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

  // Footer: read-only nominal voltage, confirmed Iset (tap opens the editor), and the last ILIM result.
  lv_obj_t* fixed = makeBox(screen_detail, 8, 366, 170, 56, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 8);
  makeLabel(fixed, 10, 2, 150, 20, "FIXED - READ-ONLY", &lv_font_montserrat_16, UiTheme::kTextMuted);
  detail_fixed_val = makeLabel(fixed, 10, 20, 150, 32, "", &lv_font_montserrat_28, UiTheme::kTextPrimary);

  lv_obj_t* iset = makeBox(screen_detail, 186, 366, 322, 56, UiTheme::kPanelSoft, UiTheme::kAccentI, 2, 8);
  lv_obj_add_flag(iset, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(iset, lv_color_mix(lv_color_hex(0xFFFFFF), lv_color_hex(UiTheme::kPanelSoft), 32),
                            static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_PRESSED);
  lv_obj_add_event_cb(iset, ed_open_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(iset, 10, 2, 150, 20, "Iset LIMIT " LV_SYMBOL_EDIT, &lv_font_montserrat_16, UiTheme::kTextMuted);
  detail_iset_val = makeLabel(iset, 10, 20, 150, 32, "-.--- A", &lv_font_montserrat_28, UiTheme::kAccentI);
  makeChip(iset, 162, 12, 152, 32, &lv_font_montserrat_16, detail_iset_chip);
  setChip(detail_iset_chip, "NO VALUE", UiTheme::kUnknown);

  lv_obj_t* micro = makeButton(screen_detail, 516, 366, kMicroBtnW, kMicroBtnH, UiTheme::kPanelSoft,
                               UiTheme::kAccentV, 2, 8);
  lv_obj_add_event_cb(micro, dash_open_micro_cb, LV_EVENT_CLICKED, nullptr);
  makeLabel(micro, 0, 2, kMicroBtnW - 4, 26, "MICRO", &lv_font_montserrat_20, UiTheme::kTextPrimary, LV_TEXT_ALIGN_CENTER);
  makeLabel(micro, 0, 28, kMicroBtnW - 4, 22, "V / A zoom", &lv_font_montserrat_16, UiTheme::kTextMuted, LV_TEXT_ALIGN_CENTER);

  lv_obj_t* res = makeBox(screen_detail, 656, 366, kResultBoxW, kResultBoxH, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 8);
  detail_res_l1 = makeLabel(res, 4, 4, kResultBoxW - 8, 22, "LAST ILIM", &lv_font_montserrat_16, UiTheme::kTextMuted);
  detail_res_l2 = makeLabel(res, 4, 28, kResultBoxW - 8, 22, "--", &lv_font_montserrat_16, UiTheme::kTextMuted);

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
  uint8_t limit_flag;  // kLimitConfirmed / kLimitRefreshing / kLimitPending / kLimitUnconfirmed
  int32_t v_mV;
  int32_t i_mA;
  uint16_t limit_mA;
};

struct RailStatus {
  const char* text;
  const char* note;
  uint32_t color;
};

constexpr uint8_t kLimitConfirmed = 0;
constexpr uint8_t kLimitRefreshing = 1;
constexpr uint8_t kLimitPending = 2;
constexpr uint8_t kLimitUnconfirmed = 3;

// The confirmed limit plus whether a write for this channel is in flight; never a draft value.
void fillLimit(RailData& d, int ch) {
  const LimitConfirmed& c = limit_confirmed[ch];
  d.have_limit = c.have;
  d.limit_mA = c.mA;
  d.limit_flag = c.fresh ? kLimitConfirmed : kLimitRefreshing;
  if (limit_tx.ch == ch) {
    if (limit_tx.state == ilim::TxState::Pending) d.limit_flag = kLimitPending;
    else if (limit_tx.state == ilim::TxState::Unconfirmed) d.limit_flag = kLimitUnconfirmed;
  }
}

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
  }
  fillLimit(d, ch);
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

// Chip text/color for the confirmed-limit state; PENDING and UNCONFIRMED stay distinct from CONFIRMED.
void limitChipFor(const RailData& d, const char** text, uint32_t* color) {
  if (d.limit_flag == kLimitPending) {
    *text = "PENDING";
    *color = UiTheme::kAccentWarn;
  } else if (d.limit_flag == kLimitUnconfirmed) {
    *text = "UNCONFIRMED";
    *color = UiTheme::kAccentWarn;
  } else if (!d.have_limit) {
    *text = "NO VALUE";
    *color = UiTheme::kUnknown;
  } else if (d.limit_flag == kLimitRefreshing) {
    *text = "STALE";
    *color = UiTheme::kUnknown;
  } else {
    *text = "CONFIRMED";
    *color = UiTheme::kAccentOk;
  }
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
  const char* chip_text = "";
  uint32_t chip_color = UiTheme::kUnknown;
  limitChipFor(d, &chip_text, &chip_color);
  setChip(c.limit_chip, chip_text, chip_color);
  setHidden(c.limit_chip.box, d.have_limit && d.limit_flag == kLimitConfirmed);
}

void setLinkChip(Chip& chip, LinkState link, uint32_t age_ms) {
  char buf[32];
  switch (link) {
    case LinkState::Live:
      setChip(chip, "LIVE", UiTheme::kAccentOk);
      break;
    case LinkState::Stale:
      snprintf(buf, sizeof(buf), "STALE %lu s", static_cast<unsigned long>(age_ms / 1000UL));
      setChip(chip, buf, UiTheme::kAccentWarn);
      break;
    case LinkState::Demo:
      setChip(chip, "DEMO DATA", UiTheme::kDemo);
      break;
    default:
      setChip(chip, "UNKNOWN", UiTheme::kUnknown);
      break;
  }
}

void refreshTopBar(TopBar& b, const DisplayTelemetry& t, LinkState link, uint32_t age_ms) {
  setLinkChip(b.link, link, age_ms);

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
  } else if (limit_tx.state != ilim::TxState::Idle && link == LinkState::Live) {
    main_txt = have_state ? (on ? "OUTPUT ON" : "OUTPUT OFF") : "OUTPUT ?";
    sub_txt = "ILIM write pending";
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

// Host-confirmed limits come only from GET ILIM / ILIM ACK+EVT. Ask for any that are missing or
// went stale after a link loss, one at a time, and never while a write or OUTPUT command is in flight.
void requestMissingLimits(LinkState link, uint32_t now_ms) {
  static uint32_t last_req_ms = 0;
  if (link != LinkState::Live) return;
  if (limit_tx.state != ilim::TxState::Idle || out_cmd.pending) return;
  const bool ok0 = limit_confirmed[0].have && limit_confirmed[0].fresh;
  const bool ok1 = limit_confirmed[1].have && limit_confirmed[1].fresh;
  if (ok0 && ok1) return;
  if ((now_ms - last_req_ms) < kLimitGetRetryMs) return;
  last_req_ms = now_ms;
  disp_link_slave::sendCommand(!ok0 ? "GET ILIM CH1" : "GET ILIM CH2");
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
  {
    const char* chip_text = "";
    uint32_t chip_color = UiTheme::kUnknown;
    limitChipFor(d[ch], &chip_text, &chip_color);
    setChip(detail_iset_chip, chip_text, chip_color);
  }
  refreshLimitResultBox();

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

struct GraphPen {
  const char* tag;
  uint32_t color;
};
constexpr GraphPen kGraphPens[4] = {
  {"CH1 +5V  VOLT", UiTheme::kCh1},
  {"CH1 +5V  CURR", UiTheme::kCh1},
  {"CH2 +3.3V  VOLT", UiTheme::kCh2},
  {"CH2 +3.3V  CURR", UiTheme::kCh2},
};

uint32_t graphWindowMs() { return kFeedRates[graph_rate_idx].div_ms * kGraphDivisions; }

uint32_t graphBucketMs() {
  const uint32_t b = graphWindowMs() / kChartPoints;
  return b < kGraphMinBucketMs ? kGraphMinBucketMs : b;
}

uint16_t graphPointCount() { return static_cast<uint16_t>(graphWindowMs() / graphBucketMs()); }

void formatSpan(uint32_t ms, char* out, size_t n) {
  if (ms < 120000UL) {
    snprintf(out, n, "%lu s", static_cast<unsigned long>((ms + 500UL) / 1000UL));
  } else if (ms < 10800000UL) {
    snprintf(out, n, "%lu min", static_cast<unsigned long>((ms + 30000UL) / 60000UL));
  } else {
    snprintf(out, n, "%.1f hr", ms / 3600000.0f);
  }
}

void applyGraphRate() {
  const uint16_t n = graphPointCount();
  if (chart_v) lv_chart_set_point_count(chart_v, n);
  if (chart_i) lv_chart_set_point_count(chart_i, n);
  if (micro_chart) lv_chart_set_point_count(micro_chart, n);
  char buf[24];
  snprintf(buf, sizeof(buf), "%s\n/div", kFeedRates[graph_rate_idx].label);
  if (graph_feed_lbl) lv_label_set_text(graph_feed_lbl, buf);
  if (micro_feed_lbl) lv_label_set_text(micro_feed_lbl, buf);
  graph_dirty = true;
  micro_dirty = true;
}

void stylePauseButton(lv_obj_t* btn, lv_obj_t* lbl) {
  if (!btn || !lbl) return;
  lv_label_set_text(lbl, graph_paused ? "RESUME" : "PAUSE");
  const uint32_t accent = graph_paused ? UiTheme::kAccentWarn : UiTheme::kTextPrimary;
  lv_obj_set_style_bg_color(btn, lv_color_hex(graph_paused ? 0x4A2E1A : UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(graph_paused ? UiTheme::kAccentWarn : UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl, lv_color_hex(accent), LV_PART_MAIN);
}

void applyGraphPause() {
  stylePauseButton(graph_pause_btn, graph_pause_lbl);
  stylePauseButton(micro_pause_btn, micro_pause_lbl);
  graph_dirty = true;
  micro_dirty = true;
}

void applyGraphPen(int k) {
  lv_obj_t* const chart = (k % 2 == 0) ? chart_v : chart_i;
  lv_chart_series_t* const series[4] = {chart_ch1_v_series, chart_ch1_i_series,
                                        chart_ch2_v_series, chart_ch2_i_series};
  if (chart && series[k]) lv_chart_hide_series(chart, series[k], !graph_pen_visible[k]);
  if (graph_pen_cell[k]) lv_obj_set_style_opa(graph_pen_cell[k], graph_pen_visible[k] ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
  if (graph_pen_tag[k]) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%s%s", kGraphPens[k].tag, graph_pen_visible[k] ? "" : "  [OFF]");
    lv_label_set_text(graph_pen_tag[k], buf);
  }
}

void graph_feed_cb(lv_event_t* /*e*/) {
  graph_rate_idx = static_cast<uint8_t>((graph_rate_idx + 1) % kFeedRateCount);
  applyGraphRate();
}

void graph_pause_cb(lv_event_t* /*e*/) {
  graph_paused = !graph_paused;
  if (graph_paused) {
    graph_pause_ms = millis();
    graph_pause_has_oldest = trend_buf && trend_count > 0;
    graph_pause_oldest_ms = graph_pause_has_oldest ? trendAt(0).t_ms : 0;
  }
  applyGraphPause();
}

void graph_pen_cb(lv_event_t* e) {
  const int k = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  if (k < 0 || k >= 4) return;
  graph_pen_visible[k] = !graph_pen_visible[k];
  applyGraphPen(k);
}

// Grid line k sits k/6 of the way across the window; blank where it predates boot.
void setTimeAxis(lv_obj_t* const* lbls, uint32_t window_ms, uint32_t end_ms) {
  for (int k = 0; k < 7; ++k) {
    const uint32_t back_ms = static_cast<uint32_t>(static_cast<uint64_t>(window_ms) * (6 - k) / 6);
    char tbuf[16];
    if (back_ms > end_ms) {
      strcpy(tbuf, "--:--:--");
    } else {
      const uint32_t s = (end_ms - back_ms) / 1000UL;
      snprintf(tbuf, sizeof(tbuf), "%lu:%02lu:%02lu", static_cast<unsigned long>(s / 3600UL),
               static_cast<unsigned long>((s / 60UL) % 60UL), static_cast<unsigned long>(s % 60UL));
    }
    setLabel(lbls[k], tbuf);
  }
}

void windowText(char* buf, size_t n, uint32_t window_ms) {
  const char* const prefix = graph_paused ? "PAUSED  " : "";
  if (trend_count == 0) {
    snprintf(buf, n, "%swaiting for samples", prefix);
    return;
  }
  char win[16];
  char hist[16];
  formatSpan(window_ms, win, sizeof(win));
  formatSpan(trendAt(trend_count - 1).t_ms - trendAt(0).t_ms, hist, sizeof(hist));
  snprintf(buf, n, "%swin %s  hist %s", prefix, win, hist);
}

// Resamples the PSRAM trend ring into the charts by time; empty buckets stay blank (no fabricated history).
void refreshGraphChart(uint32_t now_ms) {
  if (!kLiveChartsEnabled || active_screen != UiScreen::Graph || !trend_buf || !chart_v || !chart_i) return;

  const uint32_t window_ms = graphWindowMs();
  const uint32_t bucket_ms = graphBucketMs();
  const uint16_t n = graphPointCount();
  const uint32_t end_ms = graph_paused ? graph_pause_ms : now_ms;
  const uint32_t end_bucket = end_ms / bucket_ms;
  if (!graph_dirty &&
      (graph_paused || (end_bucket == graph_last_end_bucket && trend_total == graph_last_total))) return;
  graph_dirty = false;
  graph_last_end_bucket = end_bucket;
  graph_last_total = trend_total;

  static int32_t sum[4][kChartPoints];
  static uint16_t cnt[4][kChartPoints];
  memset(sum, 0, sizeof(sum));
  memset(cnt, 0, sizeof(cnt));

  bool have_ch1 = false;
  uint16_t v_min = 0, v_max = 0;
  int16_t i_min = 0, i_max = 0;
  for (size_t idx = trend_count; idx-- > 0;) {
    const TrendSample& s = trendAt(idx);
    if (s.t_ms > end_ms) continue;  // recorded after pause
    const uint32_t back = end_bucket - s.t_ms / bucket_ms;
    if (back >= n) break;
    if (s.demo != demo_mode) continue;  // never mix demo and live samples
    const int p = n - 1 - static_cast<int>(back);
    sum[0][p] += s.v12_mV;
    cnt[0][p]++;
    sum[1][p] += s.i12_mA;
    cnt[1][p]++;
    if (s.has_ch2) {
      sum[2][p] += s.v3v3_mV;
      cnt[2][p]++;
      sum[3][p] += s.i3v3_mA;
      cnt[3][p]++;
    }
    if (!have_ch1) {
      have_ch1 = true;
      v_min = v_max = s.v12_mV;
      i_min = i_max = s.i12_mA;
    } else {
      if (s.v12_mV < v_min) v_min = s.v12_mV;
      if (s.v12_mV > v_max) v_max = s.v12_mV;
      if (s.i12_mA < i_min) i_min = s.i12_mA;
      if (s.i12_mA > i_max) i_max = s.i12_mA;
    }
  }
  for (int k = 0; k < 4; ++k) {
    for (int p = 0; p < n; ++p) {
      graph_pts[k][p] = cnt[k][p] ? static_cast<lv_coord_t>(sum[k][p] / cnt[k][p]) : LV_CHART_POINT_NONE;
    }
  }
  lv_chart_refresh(chart_v);
  lv_chart_refresh(chart_i);

  setTimeAxis(lbl_graph_time, window_ms, end_ms);
  char wbuf[64];
  windowText(wbuf, sizeof(wbuf), window_ms);
  setLabel(lbl_window, wbuf);

  char mm[64];
  if (have_ch1) {
    snprintf(mm, sizeof(mm), "CH1 V min/max %.2f / %.2f", v_min / 1000.0f, v_max / 1000.0f);
    setLabel(lbl_graph_window_v, mm);
    snprintf(mm, sizeof(mm), "CH1 I min/max %.3f / %.3f", i_min / 1000.0f, i_max / 1000.0f);
    setLabel(lbl_graph_window_i, mm);
  } else {
    setLabel(lbl_graph_window_v, "CH1 V min/max --.-- / --.--");
    setLabel(lbl_graph_window_i, "CH1 I min/max --.--- / --.---");
  }
}

// Micro-view geometry: one tall plot, V axis on the left, I axis on the right.
constexpr int kMicroChartW = 600;
constexpr int kMicroChartH = 262;
constexpr int kMicroChartY = 128;
constexpr int kMicroRightX = kGraphChartX + kMicroChartW + 6;

// Trace separation: V is fitted into the upper band and I into the lower band of the plot, so the
// two traces cannot overlap whatever their values. The 45-55 % gap holds the status note.
constexpr float kMicroVBandLo = 0.55f;
constexpr float kMicroVBandHi = 0.92f;
constexpr float kMicroIBandLo = 0.08f;
constexpr float kMicroIBandHi = 0.45f;
constexpr int32_t kMicroVStep_mV = 100;  // axis bounds snap outward to these steps
constexpr int32_t kMicroIStep_mA = 10;  // resolves 5-15 mA signature steps
constexpr int32_t kMicroMinSteps = 2;  // minimum span in steps, so flat data is centred and readable

int32_t floorToStep(int32_t v, int32_t step) {
  int32_t q = v / step;
  if (v % step != 0 && v < 0) --q;
  return q * step;
}

int32_t ceilToStep(int32_t v, int32_t step) { return -floorToStep(-v, step); }

// Never clips: the span always contains [data_lo, data_hi]. Grows at once; shrinks only when the held
// span is more than twice the needed span, so the scale does not jitter at step boundaries.
void stabilizeSpan(MicroSpan& hold, int32_t data_lo, int32_t data_hi, int32_t step) {
  int32_t lo = floorToStep(data_lo, step);
  int32_t hi = ceilToStep(data_hi, step);
  bool grow_low = true;
  while (hi - lo < kMicroMinSteps * step) {
    if (grow_low) lo -= step;
    else hi += step;
    grow_low = !grow_low;
  }
  if (hold.valid && lo >= hold.lo && hi <= hold.hi && hold.hi - hold.lo <= 2 * (hi - lo)) return;
  hold.lo = lo;
  hold.hi = hi;
  hold.valid = true;
}

// Chooses the full axis range so that span [lo, hi] occupies the given fraction of the plot height.
void fitMicroAxis(const MicroSpan& s, float band_lo, float band_hi, int32_t* axis_min, int32_t* axis_max) {
  const float span = static_cast<float>(s.hi - s.lo) / (band_hi - band_lo);
  const float amin = static_cast<float>(s.lo) - band_lo * span;
  *axis_min = static_cast<int32_t>(lroundf(amin));
  *axis_max = static_cast<int32_t>(lroundf(amin + span));
}

void setMicroAxis(int32_t v_min, int32_t v_max, int32_t i_min, int32_t i_max) {
  lv_chart_set_range(micro_chart, LV_CHART_AXIS_PRIMARY_Y, v_min, v_max);
  lv_chart_set_range(micro_chart, LV_CHART_AXIS_SECONDARY_Y, i_min, i_max);
  char buf[12];
  for (int j = 0; j < 5; ++j) {
    const float f = static_cast<float>(4 - j) / 4.0f;
    snprintf(buf, sizeof(buf), "%.2fV", (static_cast<float>(v_min) + static_cast<float>(v_max - v_min) * f) / 1000.0f);
    setLabel(micro_axis_v[j], buf);
    snprintf(buf, sizeof(buf), "%.3fA", (static_cast<float>(i_min) + static_cast<float>(i_max - i_min) * f) / 1000.0f);
    setLabel(micro_axis_i[j], buf);
  }
}

// Same time-bucketed resample as the Graph screen, for the selected channel only, auto-scaled to the window.
void refreshMicroChart(uint32_t now_ms) {
  if (!kLiveChartsEnabled || active_screen != UiScreen::Micro || !trend_buf || !micro_chart) return;

  const uint32_t window_ms = graphWindowMs();
  const uint32_t bucket_ms = graphBucketMs();
  const uint16_t n = graphPointCount();
  const uint32_t end_ms = graph_paused ? graph_pause_ms : now_ms;
  const uint32_t end_bucket = end_ms / bucket_ms;
  if (!micro_dirty &&
      (graph_paused || (end_bucket == micro_last_end_bucket && trend_total == micro_last_total))) return;
  micro_dirty = false;
  micro_last_end_bucket = end_bucket;
  micro_last_total = trend_total;

  // Held scales apply to one channel/feed-rate/source only.
  const int32_t scale_key = micro_channel | (graph_rate_idx << 1) | (demo_mode ? 0x100 : 0);
  if (scale_key != micro_scale_key) {
    micro_scale_key = scale_key;
    micro_hold_v.valid = false;
    micro_hold_i.valid = false;
  }

  static int32_t sum[2][kChartPoints];
  static uint16_t cnt[kChartPoints];  // V and I share one count per bucket
  memset(sum, 0, sizeof(sum));
  memset(cnt, 0, sizeof(cnt));

  const bool ch2 = (micro_channel == 1);
  bool have = false;
  int32_t v_min = 0, v_max = 0, i_min = 0, i_max = 0;
  for (size_t idx = trend_count; idx-- > 0;) {
    const TrendSample& s = trendAt(idx);
    if (s.t_ms > end_ms) continue;  // recorded after pause
    const uint32_t back = end_bucket - s.t_ms / bucket_ms;
    if (back >= n) break;
    if (s.demo != demo_mode) continue;  // never mix demo and live samples
    if (ch2 && !s.has_ch2) continue;
    const int32_t v = ch2 ? s.v3v3_mV : s.v12_mV;
    const int32_t i = ch2 ? s.i3v3_mA : s.i12_mA;
    const int p = n - 1 - static_cast<int>(back);
    sum[0][p] += v;
    sum[1][p] += i;
    cnt[p]++;
    if (!have) {
      have = true;
      v_min = v_max = v;
      i_min = i_max = i;
    } else {
      if (v < v_min) v_min = v;
      if (v > v_max) v_max = v;
      if (i < i_min) i_min = i;
      if (i > i_max) i_max = i;
    }
  }
  for (int p = 0; p < n; ++p) {
    micro_pts[0][p] = cnt[p] ? static_cast<lv_coord_t>(sum[0][p] / cnt[p]) : LV_CHART_POINT_NONE;
    micro_pts[1][p] = cnt[p] ? static_cast<lv_coord_t>(sum[1][p] / cnt[p]) : LV_CHART_POINT_NONE;
  }

  int32_t v_ax_min, v_ax_max, i_ax_min, i_ax_max;
  const int32_t v_nom = ch2 ? 3300 : 5000;
  stabilizeSpan(micro_hold_v, have ? v_min : v_nom, have ? v_max : v_nom, kMicroVStep_mV);
  stabilizeSpan(micro_hold_i, have ? i_min : 0, have ? i_max : 0, kMicroIStep_mA);
  fitMicroAxis(micro_hold_v, kMicroVBandLo, kMicroVBandHi, &v_ax_min, &v_ax_max);
  fitMicroAxis(micro_hold_i, kMicroIBandLo, kMicroIBandHi, &i_ax_min, &i_ax_max);
  setMicroAxis(v_ax_min, v_ax_max, i_ax_min, i_ax_max);
  lv_chart_refresh(micro_chart);

  micro_have_points = have;
  const uint32_t start_ms = end_ms > window_ms ? end_ms - window_ms : 0;
  micro_window_lost = graph_paused && graph_pause_has_oldest && trend_count > 0 &&
                      trendAt(0).t_ms > graph_pause_oldest_ms && trendAt(0).t_ms > start_ms;

  setTimeAxis(micro_time, window_ms, end_ms);
  char wbuf[64];
  windowText(wbuf, sizeof(wbuf), window_ms);
  setLabel(micro_window_lbl, wbuf);

  char mm[40];
  if (have) {
    snprintf(mm, sizeof(mm), "%.2f / %.2f", v_min / 1000.0f, v_max / 1000.0f);
    setLabel(micro_lbl_v_mm, mm);
    snprintf(mm, sizeof(mm), "%.3f / %.3f", i_min / 1000.0f, i_max / 1000.0f);
    setLabel(micro_lbl_i_mm, mm);
  } else {
    setLabel(micro_lbl_v_mm, "--.-- / --.--");
    setLabel(micro_lbl_i_mm, "--.--- / --.---");
  }
}

void applyMicroChannel() {
  const bool ch2 = (micro_channel == 1);
  const uint32_t color = ch2 ? UiTheme::kCh2 : UiTheme::kCh1;
  setLabel(micro_title, ch2 ? "CH2 +3.3V  MICRO VIEW" : "CH1 +5V  MICRO VIEW");
  if (micro_title) lv_obj_set_style_text_color(micro_title, lv_color_hex(color), LV_PART_MAIN);
  for (int k = 0; k < 2; ++k) {
    if (!micro_ch_btn[k]) continue;
    const bool active = (k == micro_channel);
    const uint32_t accent = (k == 1) ? UiTheme::kCh2 : UiTheme::kCh1;
    const uint32_t tint = (k == 1) ? UiTheme::kCh2Tint : UiTheme::kCh1Tint;
    lv_obj_set_style_bg_color(micro_ch_btn[k], lv_color_hex(active ? tint : UiTheme::kPanelSoft), LV_PART_MAIN);
    lv_obj_set_style_border_color(micro_ch_btn[k], lv_color_hex(active ? accent : UiTheme::kBorder), LV_PART_MAIN);
    lv_obj_set_style_border_width(micro_ch_btn[k], active ? 2 : 1, LV_PART_MAIN);
  }
  micro_dirty = true;
}

void applyMicroBack() {
  setLabel(micro_back_lbl, micro_return_screen == UiScreen::Detail ? LV_SYMBOL_LEFT " Detail" : LV_SYMBOL_LEFT " Graph");
}

// Entry from Graph or Detail records where Back returns; switching CH inside Micro keeps the origin.
void openMicroView(uint8_t channel) {
  if (active_screen != UiScreen::Micro) {
    micro_return_screen = (active_screen == UiScreen::Detail) ? UiScreen::Detail : UiScreen::Graph;
  }
  micro_channel = (channel == 1u) ? 1u : 0u;
  applyMicroChannel();
  applyMicroBack();
  set_active_screen(UiScreen::Micro);
}

void micro_back_cb(lv_event_t* /*e*/) {
  if (micro_return_screen == UiScreen::Detail) detail_channel = micro_channel;
  set_active_screen(micro_return_screen);
}

void micro_open_cb(lv_event_t* e) {
  openMicroView(static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
}

lv_obj_t* createMicroBtn(lv_obj_t* parent, const char* text, int channel, int x_ofs) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, kMicroChBtnW, kHdrBtnH);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, x_ofs, -1);
  lv_obj_set_style_bg_color(btn, lv_color_hex(UiTheme::kPanelSoft), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(btn, micro_open_cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(channel)));
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(channel == 1 ? UiTheme::kCh2 : UiTheme::kCh1), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(label);
  return btn;
}

lv_obj_t* createFaultRow(lv_obj_t* parent) {
  lv_obj_t* fault_row = lv_obj_create(parent);
  lv_obj_set_size(fault_row, kDisplayWidth - 36, 30);
  lv_obj_align(fault_row, LV_ALIGN_BOTTOM_MID, 0, -26);
  lv_obj_clear_flag(fault_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(fault_row, lv_color_hex(UiTheme::kStatusBar), LV_PART_MAIN);
  lv_obj_set_style_radius(fault_row, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(fault_row, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(fault_row, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* lbl = lv_label_create(fault_row);
  lv_label_set_text(lbl, "NO TELEMETRY");
  lv_obj_set_style_text_color(lbl, lv_color_hex(UiTheme::kUnknown), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl);
  return lbl;
}

lv_obj_t* createGraphChart(lv_obj_t* parent, int y, int w, int h, int32_t y_min, int32_t y_max) {
  lv_obj_t* c = lv_chart_create(parent);
  lv_obj_set_pos(c, kGraphChartX, y);
  lv_obj_set_size(c, w, h);
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
  createMicroBtn(status, "CH2", 1, -230);
  createMicroBtn(status, "CH1", 0, -294);

  // Pen legend: swatch + live value per channel; tap a cell to show/hide that pen.
  for (int k = 0; k < 4; ++k) {
    lv_obj_t* cell = makeBox(screen_graph, 18 + k * 193, 56, 185, 52, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 6);
    graph_pen_cell[k] = cell;
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cell, graph_pen_cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(k)));
    makeBox(cell, 8, 10, 6, 30, kGraphPens[k].color, kGraphPens[k].color, 0, 2);
    graph_pen_tag[k] = makeLabel(cell, 22, 4, 158, 18, kGraphPens[k].tag, &lv_font_montserrat_12, kGraphPens[k].color);
    lbl_pen_val[k] = makeLabel(cell, 22, 22, 158, 26, "--", &lv_font_montserrat_20, UiTheme::kTextPrimary);
  }

  lbl_graph_stats = makeLabel(screen_graph, kGraphChartX + kGraphChartW + 12, 330, 74, 54, "n=0\nT=--C",
                              &lv_font_montserrat_12, UiTheme::kTextMuted);

  makeLabel(screen_graph, kGraphChartX, 110, 300, 16, "VOLTAGE (V)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  chart_v = createGraphChart(screen_graph, kGraphVChartY, kGraphChartW, kGraphChartH, 0, 6000);
  chart_ch1_v_series = lv_chart_add_series(chart_v, lv_color_hex(UiTheme::kCh1), LV_CHART_AXIS_PRIMARY_Y);
  chart_ch2_v_series = lv_chart_add_series(chart_v, lv_color_hex(UiTheme::kCh2), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_v, chart_ch1_v_series, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(chart_v, chart_ch2_v_series, LV_CHART_POINT_NONE);

  makeLabel(screen_graph, kGraphChartX, 252, 300, 16, "CURRENT (A)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  chart_i = createGraphChart(screen_graph, kGraphIChartY, kGraphChartW, kGraphChartH, 0, 4000);
  chart_ch1_i_series = lv_chart_add_series(chart_i, lv_color_hex(UiTheme::kCh1), LV_CHART_AXIS_PRIMARY_Y);
  chart_ch2_i_series = lv_chart_add_series(chart_i, lv_color_hex(UiTheme::kCh2), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_i, chart_ch1_i_series, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(chart_i, chart_ch2_i_series, LV_CHART_POINT_NONE);

  // Pens plot from static arrays refilled by refreshGraphChart() (time-based resample).
  for (auto& row : graph_pts) {
    for (auto& v : row) v = LV_CHART_POINT_NONE;
  }
  lv_chart_set_ext_y_array(chart_v, chart_ch1_v_series, graph_pts[0]);
  lv_chart_set_ext_y_array(chart_i, chart_ch1_i_series, graph_pts[1]);
  lv_chart_set_ext_y_array(chart_v, chart_ch2_v_series, graph_pts[2]);
  lv_chart_set_ext_y_array(chart_i, chart_ch2_i_series, graph_pts[3]);

  // Right column: feed rate (tap = next, wraps) and pause/resume.
  constexpr int kCtlX = kGraphChartX + kGraphChartW + 10;
  makeLabel(screen_graph, kCtlX, 128, 78, 14, "FEED (TAP)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  lv_obj_t* feed_btn = makeButton(screen_graph, kCtlX, 146, 78, 56, UiTheme::kPanelSoft, UiTheme::kBorder, 2, 8);
  lv_obj_add_event_cb(feed_btn, graph_feed_cb, LV_EVENT_CLICKED, nullptr);
  graph_feed_lbl = lv_label_create(feed_btn);
  lv_obj_set_width(graph_feed_lbl, 74);
  lv_obj_set_style_text_font(graph_feed_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(graph_feed_lbl, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_align(graph_feed_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(graph_feed_lbl);

  graph_pause_btn = makeButton(screen_graph, kCtlX, 214, 78, 50, UiTheme::kPanelSoft, UiTheme::kBorder, 2, 8);
  lv_obj_add_event_cb(graph_pause_btn, graph_pause_cb, LV_EVENT_CLICKED, nullptr);
  graph_pause_lbl = lv_label_create(graph_pause_btn);
  lv_obj_set_width(graph_pause_lbl, 74);
  lv_obj_set_style_text_font(graph_pause_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_align(graph_pause_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(graph_pause_lbl);
  applyGraphPause();
  applyGraphRate();

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

  lbl_fault_graph = createFaultRow(screen_graph);
}

void create_micro_screen(lv_obj_t* root) {
  screen_micro = makeScreen(root);

  lv_obj_t* status = makeBox(screen_micro, 8, 6, kDisplayWidth - 16, 44, UiTheme::kStatusBar, UiTheme::kBorder, 1, 8);
  micro_title = makeLabel(status, 14, 0, 272, 44, "CH1 +5V  MICRO VIEW", &lv_font_montserrat_20, UiTheme::kCh1);
  makeChip(status, 292, 6, 120, 32, &lv_font_montserrat_16, micro_link_chip);
  setChip(micro_link_chip, "UNKNOWN", UiTheme::kUnknown);
  create_nav_btn(status, "Main", UiScreen::Main, -10, kHdrBtnH);
  lv_obj_t* back = lv_btn_create(status);
  lv_obj_set_size(back, kMicroBackW, kHdrBtnH);
  lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -120, -1);
  lv_obj_set_style_bg_color(back, lv_color_hex(UiTheme::kBadge), LV_PART_MAIN);
  lv_obj_set_style_border_color(back, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_border_width(back, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(back, micro_back_cb, LV_EVENT_CLICKED, nullptr);
  micro_back_lbl = lv_label_create(back);
  lv_obj_set_style_text_color(micro_back_lbl, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(micro_back_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(micro_back_lbl);
  applyMicroBack();
  micro_ch_btn[1] = createMicroBtn(status, "CH2", 1, -236);
  micro_ch_btn[0] = createMicroBtn(status, "CH1", 0, -298);

  // Readouts: live value and window min/max for each trace.
  struct Cell {
    const char* tag;
    uint32_t color;
    const lv_font_t* font;
    lv_obj_t** val;
  };
  const Cell cells[4] = {
    {"VOLT NOW", UiTheme::kAccentV, &lv_font_montserrat_20, &micro_lbl_v_now},
    {"CURR NOW", UiTheme::kAccentI, &lv_font_montserrat_20, &micro_lbl_i_now},
    {"VOLT MIN / MAX (V)", UiTheme::kAccentV, &lv_font_montserrat_16, &micro_lbl_v_mm},
    {"CURR MIN / MAX (A)", UiTheme::kAccentI, &lv_font_montserrat_16, &micro_lbl_i_mm},
  };
  for (int k = 0; k < 4; ++k) {
    lv_obj_t* cell = makeBox(screen_micro, 18 + k * 193, 56, 185, 52, UiTheme::kPanelSoft, UiTheme::kBorder, 1, 6);
    makeBox(cell, 8, 10, 6, 30, cells[k].color, cells[k].color, 0, 2);
    makeLabel(cell, 22, 4, 158, 18, cells[k].tag, &lv_font_montserrat_12, cells[k].color);
    *cells[k].val = makeLabel(cell, 22, 22, 158, 26, "--", cells[k].font, UiTheme::kTextPrimary);
  }

  makeLabel(screen_micro, kGraphChartX, 110, 300, 16, "V = VOLTS  (left axis, upper trace)", &lv_font_montserrat_12, UiTheme::kAccentV);
  makeLabel(screen_micro, kGraphChartX + kMicroChartW - 300, 110, 300, 16, "A = AMPS  (right axis, lower trace)", &lv_font_montserrat_12,
            UiTheme::kAccentI, LV_TEXT_ALIGN_RIGHT);

  micro_chart = createGraphChart(screen_micro, kMicroChartY, kMicroChartW, kMicroChartH, 0, 6000);
  micro_ser_v = lv_chart_add_series(micro_chart, lv_color_hex(UiTheme::kAccentV), LV_CHART_AXIS_PRIMARY_Y);
  micro_ser_i = lv_chart_add_series(micro_chart, lv_color_hex(UiTheme::kAccentI), LV_CHART_AXIS_SECONDARY_Y);
  for (auto& row : micro_pts) {
    for (auto& v : row) v = LV_CHART_POINT_NONE;
  }
  lv_chart_set_ext_y_array(micro_chart, micro_ser_v, micro_pts[0]);
  lv_chart_set_ext_y_array(micro_chart, micro_ser_i, micro_pts[1]);

  // Status note sits in the empty 45-55 % gap between the V and I bands, so it never covers a trace.
  micro_note = lv_label_create(micro_chart);
  lv_label_set_text(micro_note, "");
  lv_obj_set_style_text_font(micro_note, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_bg_color(micro_note, lv_color_hex(UiTheme::kScope), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(micro_note, LV_OPA_80, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(micro_note, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(micro_note, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(micro_note, 6, LV_PART_MAIN);
  lv_obj_align(micro_note, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(micro_note, LV_OBJ_FLAG_HIDDEN);

  // Right column: feed rate (tap = next, wraps) and pause/resume, shared with the Graph screen.
  constexpr int kCtlX = kMicroRightX + 54;
  makeLabel(screen_micro, kCtlX, 128, 70, 14, "FEED (TAP)", &lv_font_montserrat_12, UiTheme::kTextMuted);
  lv_obj_t* feed_btn = makeButton(screen_micro, kCtlX, 146, 70, 56, UiTheme::kPanelSoft, UiTheme::kBorder, 2, 8);
  lv_obj_add_event_cb(feed_btn, graph_feed_cb, LV_EVENT_CLICKED, nullptr);
  micro_feed_lbl = lv_label_create(feed_btn);
  lv_obj_set_width(micro_feed_lbl, 66);
  lv_obj_set_style_text_font(micro_feed_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(micro_feed_lbl, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_align(micro_feed_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(micro_feed_lbl);

  micro_pause_btn = makeButton(screen_micro, kCtlX, 214, 70, 50, UiTheme::kPanelSoft, UiTheme::kBorder, 2, 8);
  lv_obj_add_event_cb(micro_pause_btn, graph_pause_cb, LV_EVENT_CLICKED, nullptr);
  micro_pause_lbl = lv_label_create(micro_pause_btn);
  lv_obj_set_width(micro_pause_lbl, 66);
  lv_obj_set_style_text_font(micro_pause_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_align(micro_pause_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(micro_pause_lbl);
  applyGraphPause();
  applyGraphRate();

  for (int j = 0; j < 5; ++j) {
    const int y = kMicroChartY + j * (kMicroChartH - 1) / 4 - 8;
    micro_axis_v[j] = makeLabel(screen_micro, 12, y, 46, 16, "", &lv_font_montserrat_12, UiTheme::kAccentV, LV_TEXT_ALIGN_RIGHT);
    micro_axis_i[j] = makeLabel(screen_micro, kMicroRightX, y, 54, 16, "", &lv_font_montserrat_12, UiTheme::kAccentI);
  }
  setMicroAxis(4900, 5200, 0, 400);

  for (int k = 0; k < 7; ++k) {
    const int cx = kGraphChartX + k * kMicroChartW / 6;
    const int x = (k == 0) ? kGraphChartX : (k == 6) ? kGraphChartX + kMicroChartW - 70 : cx - 35;
    const lv_text_align_t al = (k == 0) ? LV_TEXT_ALIGN_LEFT : (k == 6) ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER;
    micro_time[k] = makeLabel(screen_micro, x, 394, 70, 16, "--:--:--", &lv_font_montserrat_12, UiTheme::kTextMuted, al);
  }
  makeLabel(screen_micro, kMicroRightX, 394, 60, 16, "UPTIME", &lv_font_montserrat_12, UiTheme::kTextMuted);

  micro_window_lbl = lv_label_create(screen_micro);
  lv_label_set_text(micro_window_lbl, "win: waiting for samples");
  lv_obj_set_style_text_color(micro_window_lbl, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(micro_window_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(micro_window_lbl, LV_ALIGN_BOTTOM_LEFT, 24, -8);
  makeLabel(screen_micro, kDisplayWidth - 24 - 380, 456, 380, 16,
            "V and A are scaled independently - not comparable", &lv_font_montserrat_12, UiTheme::kTextMuted,
            LV_TEXT_ALIGN_RIGHT);

  micro_fault_lbl = createFaultRow(screen_micro);
  applyMicroChannel();
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
  create_micro_screen(scr);
  create_settings_screen(scr);
  createOutputConfirm();
  createLimitEditor();
  set_active_screen(UiScreen::Splash);
}

// ── Telemetry label updates ────────────────────────────────────────────────
// Lists only active trips; no CC/CV claim (#77). Runs every loop so STALE shows without a new frame.
void faultRowText(const DisplayTelemetry& t, uint32_t now_ms, char (&buf)[96], uint32_t* out_color) {
  uint32_t age_ms = 0;
  const LinkState link = linkStateOf(t, now_ms, &age_ms);
  buf[0] = '\0';
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
  *out_color = color;
}

void applyFaultRow(lv_obj_t* lbl, const char* buf, uint32_t color) {
  if (strcmp(lv_label_get_text(lbl), buf) != 0) {
    lv_label_set_text(lbl, buf);
    lv_obj_center(lbl);
  }
  const lv_color_t want = lv_color_hex(color);
  if (lv_color_to32(lv_obj_get_style_text_color(lbl, LV_PART_MAIN)) != lv_color_to32(want)) {
    lv_obj_set_style_text_color(lbl, want, LV_PART_MAIN);
  }
}

void refreshGraphFaultRow(const DisplayTelemetry& t, uint32_t now_ms) {
  if (!lbl_fault_graph || active_screen != UiScreen::Graph) return;
  char buf[96];
  uint32_t color = 0;
  faultRowText(t, now_ms, buf, &color);
  applyFaultRow(lbl_fault_graph, buf, color);
}

// Micro-view live readouts and fault row; runs every loop like the Graph fault row.
void refreshMicroLive(const DisplayTelemetry& t, uint32_t now_ms) {
  if (!micro_fault_lbl || active_screen != UiScreen::Micro) return;
  char buf[96];
  uint32_t color = 0;
  faultRowText(t, now_ms, buf, &color);
  applyFaultRow(micro_fault_lbl, buf, color);

  uint32_t age_ms = 0;
  const LinkState link = linkStateOf(t, now_ms, &age_ms);
  const RailData d = railData(micro_channel, t, link);
  char vbuf[16];
  char ibuf[16];
  if (d.have_reading) {
    snprintf(vbuf, sizeof(vbuf), "%.2f V", d.v_mV / 1000.0f);
    snprintf(ibuf, sizeof(ibuf), "%.3f A", d.i_mA / 1000.0f);
  } else {
    strcpy(vbuf, "--");
    strcpy(ibuf, "--");
  }
  setLabel(micro_lbl_v_now, vbuf);
  setLabel(micro_lbl_i_now, ibuf);

  setLinkChip(micro_link_chip, link, age_ms);
  const bool stale = (link == LinkState::Stale);
  setTextOpa(micro_lbl_v_now, stale ? LV_OPA_50 : LV_OPA_COVER);
  setTextOpa(micro_lbl_i_now, stale ? LV_OPA_50 : LV_OPA_COVER);
  if (micro_chart && micro_dim != stale) {
    micro_dim = stale;
    lv_obj_set_style_opa(micro_chart, stale ? LV_OPA_50 : LV_OPA_COVER, LV_PART_ITEMS);
  }

  const char* note = nullptr;
  uint32_t note_color = UiTheme::kUnknown;
  if (micro_window_lost) {
    note = "PAUSED - older samples overwritten";
    note_color = UiTheme::kAccentWarn;
  } else if (link == LinkState::Unknown) {
    note = "NO DATA - no telemetry yet";
  } else if (link == LinkState::Demo) {
    note = "DEMO DATA - SIMULATED";
    note_color = UiTheme::kDemo;
  } else if (!d.have_reading) {
    note = "NO CH2 DATA (legacy frame)";
  } else if (stale) {
    note = "STALE - no new samples";
    note_color = UiTheme::kAccentWarn;
  } else if (!micro_have_points) {
    note = "NO SAMPLES IN WINDOW";
  }
  if (note && strcmp(lv_label_get_text(micro_note), note) != 0) {
    lv_label_set_text(micro_note, note);
    lv_obj_set_style_text_color(micro_note, lv_color_hex(note_color), LV_PART_MAIN);
    lv_obj_align(micro_note, LV_ALIGN_CENTER, 0, 0);
  }
  setHidden(micro_note, note == nullptr);
}

void update_telemetry_labels() {
  if (!kLiveTelemetryUiEnabled) return;

  const uint32_t now_ms = millis();
  if (now_ms - last_ui_update_ms < kUiUpdateMinMs) return;

  const DisplayTelemetry t = get_display_telemetry();
  refreshGraphFaultRow(t, now_ms);
  refreshMicroLive(t, now_ms);
  refreshGraphChart(now_ms);
  refreshMicroChart(now_ms);
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
  const uint8_t ch = channel.equalsIgnoreCase("CH1") ? 0u : 1u;
  char why[56];
  // Same tracked, serialized path as the editor; the host reply is reported by the "ilim tx:" log lines.
  if (!limitWrite(ch, limit_mA, why, sizeof(why))) {
    Serial.printf("ERR UDI_ILIM: not sent: %s\n", why);
    return;
  }
  Serial.printf("ACK UDI_ILIM %s %u sent (pending host reply)\n", channel.c_str(), static_cast<unsigned>(limit_mA));
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
    Serial.println("Commands: HELP, PING, STATUS, RX, UDI_STATUS, UDI_OUTPUT <ON|OFF>, UDI_ILIM <CH1|CH2> <mA>, OTA, SCREEN <SPLASH|SETUP|MAIN|DETAIL1|DETAIL2|GRAPH|MICRO1|MICRO2|SETTINGS>, SPLASH <ON|OFF>, DEMO <ON|OFF>, TOUR <ON|OFF>, SETUP_ENC <ROT <n>|PRESS|LONG>, SETTINGS_ENC <ROT <n>|PRESS|LONG>, PROBE <pin> [ms], LOG_START, LOG_STOP, LOG_STATUS, LOG_CLEAR, LOG_DUMP_CSV [N]");
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
    if (arg.equalsIgnoreCase("MICRO1") || arg.equalsIgnoreCase("MICRO2")) {
      micro_channel = arg.endsWith("2") ? 1u : 0u;
      openMicroView(micro_channel);
      Serial.printf("ACK SCREEN MICRO%u\n", static_cast<unsigned>(micro_channel + 1u));
      return;
    }
    Serial.println("ERR SCREEN: use SPLASH|SETUP|MAIN|DETAIL1|DETAIL2|GRAPH|MICRO1|MICRO2|SETTINGS");
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
    if (parsed_mA < 0 || parsed_mA > 65535L) {
      Serial.println("ERR UDI_ILIM: mA must be 0..65535");
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
  serviceLimitLink();
  limitEditorRefresh();
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
