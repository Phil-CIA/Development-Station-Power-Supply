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
constexpr uint16_t kVoltageUpdateDeadband_mV = 0;
constexpr int16_t  kCurrentUpdateDeadband_mA = 0;
constexpr int16_t  kOutputOnThreshold_mA = 50;
constexpr uint32_t kValueKeepaliveMs = 0;
constexpr uint32_t kSplashDurationMs = 1800;
constexpr uint32_t kDemoFrameMs = 50;
constexpr uint32_t kDemoTourSwitchMs = 8000;
constexpr size_t   kTrendCapacity = 1800;  // 7.5 minutes at 4 Hz
constexpr uint16_t kChartPoints   = 120;   // 2 minutes visible at 1 Hz (decimated)
constexpr uint16_t kSetupCh1LimitMax_mA = 3000;
constexpr uint16_t kSetupCh2LimitMax_mA = 2000;
constexpr uint16_t kSetupStep_mA = 50;
constexpr float    kCh1SetVoltage_V = 5.00f;
constexpr float    kCh2SetVoltage_V = 3.30f;
constexpr float    kCh1SetCurrent_A = 3.00f;
constexpr float    kCh2SetCurrent_A = 2.00f;

enum class UiScreen : uint8_t {
  Splash = 0,
  Setup = 1,
  Main = 2,
  Graph = 3,
  Settings = 4,
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

struct SettingsState {
  SettingsMenu selected = SettingsMenu::System;
  char hint[128] = "Select a submenu to view and apply settings.";
};

static SettingsState settings_state = {};

// Main screen (dual-channel layout)
static lv_obj_t* lbl_main_ch1_voltage = nullptr;
static lv_obj_t* lbl_main_ch1_current = nullptr;
static lv_obj_t* lbl_main_ch1_power = nullptr;
static lv_obj_t* lbl_main_ch2_voltage = nullptr;
static lv_obj_t* lbl_main_ch2_current = nullptr;
static lv_obj_t* lbl_main_ch2_power = nullptr;
static lv_obj_t* lbl_main_stats = nullptr;
static lv_obj_t* bar_main_ch1_voltage = nullptr;
static lv_obj_t* bar_main_ch1_current = nullptr;
static lv_obj_t* bar_main_ch2_voltage = nullptr;
static lv_obj_t* bar_main_ch2_current = nullptr;

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
static lv_obj_t* lbl_status_output = nullptr;
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

static lv_obj_t* chart_v     = nullptr;
static lv_obj_t* chart_i     = nullptr;
static lv_obj_t* chart_ch1_v = nullptr;
static lv_obj_t* chart_ch1_i = nullptr;
static lv_obj_t* chart_ch2_v = nullptr;
static lv_obj_t* chart_ch2_i = nullptr;
static lv_chart_series_t* chart_v_series = nullptr;
static lv_chart_series_t* chart_i_series = nullptr;
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
static uint16_t last_v12_drawn_mV = 0;
static int16_t  last_i12_drawn_mA = 0;
static bool     have_drawn_values = false;
static uint32_t last_value_draw_ms = 0;
static char     last_voltage_text[32] = "";
static char     last_current_text[32] = "";
static char     last_ch2_voltage_text[32] = "";
static char     last_ch2_current_text[32] = "";
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

void setupRequestRefresh();
void updateSetupBindingsFromUdi();
void refreshSetupScreenLabels();
void refreshSettingsScreenLabels(bool force = false);
void enterSetupScreen();
void handleSetupEncoderRotate(int8_t detents);
void handleSetupEncoderPress();
void handleSetupEncoderLongPress();
void settings_system_btn_event_cb(lv_event_t* e);
void settings_dataset_btn_event_cb(lv_event_t* e);
void settings_about_btn_event_cb(lv_event_t* e);
void settings_action_primary_event_cb(lv_event_t* e);
void settings_action_secondary_event_cb(lv_event_t* e);
void settings_action_refresh_event_cb(lv_event_t* e);
bool i2cAddressResponds(uint8_t address);
void printLogStatus();

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
  const float v = 12100.0f + 850.0f * sinf(t * 1.12f) + 180.0f * sinf(t * 0.31f);
  const float i = 830.0f + 620.0f * sinf(t * 0.87f + 1.35f) + 140.0f * sinf(t * 2.2f);

  out.rx_count = out.rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.i2c_rx_count = out.i2c_rx_count + static_cast<uint32_t>((now_ms - demo_start_ms) / 200);
  out.last_seq = static_cast<uint8_t>((now_ms - demo_start_ms) / 200);
  out.last_v12_mV = static_cast<uint16_t>(clamp_i32(static_cast<int32_t>(v), 9000, 15000));
  out.last_i12_mA = static_cast<int16_t>(clamp_i32(static_cast<int32_t>(i), -500, 3000));
  out.last_v3v3_mV = static_cast<uint16_t>(clamp_i32(static_cast<int32_t>(3300.0f + 90.0f * sinf(t * 1.7f)), 3000, 3600));
  out.last_i3v3_mA = static_cast<int16_t>(clamp_i32(static_cast<int32_t>(420.0f + 130.0f * sinf(t * 1.9f + 0.6f)), -100, 2000));
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
    snprintf(value_buf, sizeof(value_buf), "► %s ◄", setup_binding.output_enabled ? "ON" : "OFF");
  } else if (setup_binding.selected == SetupField::Ch1CurrentLimit) {
    snprintf(value_buf, sizeof(value_buf), "► %.3f A ◄", setup_binding.ch1_limit_mA / 1000.0f);
  } else {
    snprintf(value_buf, sizeof(value_buf), "► %.3f A ◄", setup_binding.ch2_limit_mA / 1000.0f);
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
  if (udi_link.ack_count != last_udi_ack_count) {
    last_udi_ack_count = udi_link.ack_count;
    Serial.printf("udi ack: %s\n", udi_link.last_ack[0] ? udi_link.last_ack : "(empty)");
    applySetupAck(udi_link.last_ack);
  }
  if (udi_link.err_count != last_udi_err_count) {
    last_udi_err_count = udi_link.err_count;
    Serial.printf("udi err: %s\n", udi_link.last_err[0] ? udi_link.last_err : "(empty)");
    applySetupError(udi_link.last_err);
  }
  if (udi_link.evt_count != last_udi_evt_count) {
    last_udi_evt_count = udi_link.evt_count;
    Serial.printf("udi evt: %s\n", udi_link.last_evt[0] ? udi_link.last_evt : "(empty)");
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
    demo_mode = !demo_mode;
    if (!demo_mode) {
      demo_tour = false;
    } else {
      demo_start_ms = millis();
      demo_last_tour_switch_ms = demo_start_ms;
    }
    setSettingsHint(demo_mode ? "Demo mode enabled from Settings/System." : "Demo mode disabled from Settings/System.");
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    trend_logging_enabled = !trend_logging_enabled;
    setSettingsHint(trend_logging_enabled ? "Trend logging started." : "Trend logging paused.");
    return;
  }
  setSettingsHint("About is read-only.");
}

void applySettingsSecondaryAction() {
  if (settings_state.selected == SettingsMenu::System) {
    demo_tour = !demo_tour;
    if (demo_tour) {
      demo_last_tour_switch_ms = millis();
    }
    setSettingsHint(demo_tour ? "Auto-tour enabled." : "Auto-tour disabled.");
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    trendClear();
    setSettingsHint("Trend dataset cleared.");
    return;
  }
  setSettingsHint("About is read-only.");
}

void applySettingsRefreshAction() {
  if (settings_state.selected == SettingsMenu::System) {
    setupRequestRefresh();
    setSettingsHint(setup_binding.last_error[0] == '\0'
                      ? "Requested host refresh (GET OUTPUT/ILIM)."
                      : "Host refresh request failed: link not ready.");
    return;
  }
  if (settings_state.selected == SettingsMenu::DataSet) {
    printLogStatus();
    setSettingsHint("Dataset status printed on USB serial console.");
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
    snprintf(body,
             sizeof(body),
             "Display Mode: %s\n"
             "Auto Tour:    %s\n"
             "Telemetry:    %s (SEQ %u)\n"
             "I2C Board:    %s  Touch: %s\n"
             "Host Config:  Output %s  CH1 %.3fA  CH2 %.3fA",
             demo_mode ? "DEMO" : "LIVE",
             demo_tour ? "ON" : "OFF",
             link_live ? "LIVE" : "STALE",
             static_cast<unsigned>(t.last_seq),
             i2cAddressResponds(kBoardCtrlAddr) ? "OK" : "MISS",
             i2cAddressResponds(kTouchAddr) ? "OK" : "MISS",
             setup_binding.output_enabled ? "ON" : "OFF",
             setup_binding.ch1_limit_mA / 1000.0f,
             setup_binding.ch2_limit_mA / 1000.0f);
    lv_label_set_text(lbl_settings_action_primary, demo_mode ? "Stop Demo" : "Start Demo");
    lv_label_set_text(lbl_settings_action_secondary, demo_tour ? "Tour Off" : "Tour On");
    lv_label_set_text(lbl_settings_action_refresh, "Refresh Host");
  } else if (settings_state.selected == SettingsMenu::DataSet) {
    const TrendWindowStats stats = getTrendWindowStats(kChartPoints);
    snprintf(body,
             sizeof(body),
             "Trend Logging: %s\n"
             "Samples:       %lu / %lu\n"
             "Latest RX:     %lu  ERR: %lu\n"
             "Window V:      %.2f .. %.2f V\n"
             "Window I:      %.3f .. %.3f A",
             trend_logging_enabled ? "ON" : "PAUSED",
             static_cast<unsigned long>(trend_count),
             static_cast<unsigned long>(kTrendCapacity),
             static_cast<unsigned long>(t.rx_count),
             static_cast<unsigned long>(t.err_count),
             stats.has_data ? (stats.min_v12_mV / 1000.0f) : 0.0f,
             stats.has_data ? (stats.max_v12_mV / 1000.0f) : 0.0f,
             stats.has_data ? (stats.min_i12_mA / 1000.0f) : 0.0f,
             stats.has_data ? (stats.max_i12_mA / 1000.0f) : 0.0f);
    lv_label_set_text(lbl_settings_action_primary, trend_logging_enabled ? "Pause Log" : "Start Log");
    lv_label_set_text(lbl_settings_action_secondary, "Clear Data");
    lv_label_set_text(lbl_settings_action_refresh, "Print Status");
  } else {
    const unsigned long uptime_s = static_cast<unsigned long>(now_ms / 1000UL);
    snprintf(body,
             sizeof(body),
             "Development Station Power Supply\n"
             "CrowPanel Firmware\n"
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
  setSettingsHint("Select an action button below to apply this submenu.");
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
  lv_label_set_text(lbl_setup_value, "► -- ◄");
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
  lv_obj_set_size(btn_edit, 140, 40);
  lv_obj_align(btn_edit, LV_ALIGN_BOTTOM_RIGHT, -160, -16);
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
  lv_label_set_text(mode, "WORKSTATION PSU");
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
  lv_obj_set_size(badge, 118, 32);
  lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -118, 0);
  lv_obj_set_style_bg_color(badge, lv_color_hex(UiTheme::kBadge), LV_PART_MAIN);
  lv_obj_set_style_radius(badge, 9, LV_PART_MAIN);
  lv_obj_set_style_border_width(badge, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(badge, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_status_output = lv_label_create(badge);
  lv_label_set_text(lbl_status_output, "OUTPUT --");
  lv_obj_set_style_text_color(lbl_status_output, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_status_output, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_center(lbl_status_output);

  create_nav_btn(status, "Graphs", UiScreen::Graph, -10);

  lv_obj_t* panel_v = lv_obj_create(screen_main);
  lv_obj_set_size(panel_v, 368, 182);
  lv_obj_align(panel_v, LV_ALIGN_TOP_LEFT, 18, 68);
  lv_obj_set_style_bg_color(panel_v, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(panel_v, 16, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel_v, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel_v, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lv_obj_t* hdr_v = lv_label_create(panel_v);
  lv_label_set_text(hdr_v, "CH1  +5V MAIN");
  lv_obj_set_style_text_color(hdr_v, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_v, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(hdr_v, LV_ALIGN_TOP_LEFT, 18, 16);

  lv_obj_t* hdr_v_set = lv_label_create(panel_v);
  lv_label_set_text(hdr_v_set, "SET 5.00V / 3.00A");
  lv_obj_set_style_text_color(hdr_v_set, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_v_set, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_v_set, LV_ALIGN_TOP_RIGHT, -16, 18);

  lv_obj_t* hdr_v_meas = lv_label_create(panel_v);
  lv_label_set_text(hdr_v_meas, "V OUT");
  lv_obj_set_style_text_color(hdr_v_meas, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_v_meas, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_v_meas, LV_ALIGN_TOP_LEFT, 22, 44);

  lbl_main_ch1_voltage = lv_label_create(panel_v);
  lv_label_set_text(lbl_main_ch1_voltage, "-.--- V");
  lv_obj_set_style_text_color(lbl_main_ch1_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch1_voltage, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_ch1_voltage, 330);
  lv_obj_set_style_text_align(lbl_main_ch1_voltage, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch1_voltage, LV_ALIGN_TOP_LEFT, 22, 56);

  lbl_main_ch1_current = lv_label_create(panel_v);
  lv_label_set_text(lbl_main_ch1_current, "A -.---");
  lv_obj_set_style_text_color(lbl_main_ch1_current, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch1_current, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch1_current, LV_ALIGN_TOP_LEFT, 22, 112);

  lbl_main_ch1_power = lv_label_create(panel_v);
  lv_label_set_text(lbl_main_ch1_power, "P --.--W   T --C   --");
  lv_obj_set_style_text_color(lbl_main_ch1_power, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch1_power, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch1_power, LV_ALIGN_TOP_LEFT, 22, 142);

  bar_main_ch1_voltage = lv_bar_create(panel_v);
  lv_obj_set_size(bar_main_ch1_voltage, 330, 20);
  lv_obj_align(bar_main_ch1_voltage, LV_ALIGN_BOTTOM_LEFT, 18, -20);
  lv_bar_set_range(bar_main_ch1_voltage, 0, 1000);
  lv_bar_set_value(bar_main_ch1_voltage, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_main_ch1_voltage, lv_color_hex(0x1A2735), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_main_ch1_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(bar_main_ch1_voltage, lv_color_hex(0xFFD95A), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(bar_main_ch1_voltage, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_main_ch1_voltage, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_main_ch1_voltage, 4, LV_PART_INDICATOR);

  lv_obj_t* bar_v_lo = lv_label_create(panel_v);
  lv_label_set_text(bar_v_lo, "4.5V");
  lv_obj_set_style_text_color(bar_v_lo, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_v_lo, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_v_lo, LV_ALIGN_BOTTOM_LEFT, 18, -2);

  lv_obj_t* bar_v_mid = lv_label_create(panel_v);
  lv_label_set_text(bar_v_mid, "5.0V");
  lv_obj_set_style_text_color(bar_v_mid, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_v_mid, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_v_mid, LV_ALIGN_BOTTOM_MID, 0, -2);

  lv_obj_t* bar_v_hi = lv_label_create(panel_v);
  lv_label_set_text(bar_v_hi, "5.5V");
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
  lv_label_set_text(hdr_i, "CH2  +3.3V AUX");
  lv_obj_set_style_text_color(hdr_i, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_i, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(hdr_i, LV_ALIGN_TOP_LEFT, 18, 16);

  lv_obj_t* hdr_i_set = lv_label_create(panel_i);
  lv_label_set_text(hdr_i_set, "SET 3.30V / 2.00A");
  lv_obj_set_style_text_color(hdr_i_set, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_i_set, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_i_set, LV_ALIGN_TOP_RIGHT, -16, 18);

  lv_obj_t* hdr_i_meas = lv_label_create(panel_i);
  lv_label_set_text(hdr_i_meas, "V OUT");
  lv_obj_set_style_text_color(hdr_i_meas, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(hdr_i_meas, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hdr_i_meas, LV_ALIGN_TOP_LEFT, 22, 44);

  lbl_main_ch2_voltage = lv_label_create(panel_i);
  lv_label_set_text(lbl_main_ch2_voltage, "-.--- V");
  lv_obj_set_style_text_color(lbl_main_ch2_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch2_voltage, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_ch2_voltage, 330);
  lv_obj_set_style_text_align(lbl_main_ch2_voltage, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch2_voltage, LV_ALIGN_TOP_LEFT, 22, 56);

  lbl_main_ch2_current = lv_label_create(panel_i);
  lv_label_set_text(lbl_main_ch2_current, "A -.---");
  lv_obj_set_style_text_color(lbl_main_ch2_current, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch2_current, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch2_current, LV_ALIGN_TOP_LEFT, 22, 112);

  lbl_main_ch2_power = lv_label_create(panel_i);
  lv_label_set_text(lbl_main_ch2_power, "P --.--W   LINK --");
  lv_obj_set_style_text_color(lbl_main_ch2_power, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_ch2_power, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(lbl_main_ch2_power, LV_ALIGN_TOP_LEFT, 22, 142);

  bar_main_ch2_voltage = lv_bar_create(panel_i);
  lv_obj_set_size(bar_main_ch2_voltage, 330, 20);
  lv_obj_align(bar_main_ch2_voltage, LV_ALIGN_BOTTOM_LEFT, 18, -20);
  lv_bar_set_range(bar_main_ch2_voltage, 0, 1000);
  lv_bar_set_value(bar_main_ch2_voltage, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_main_ch2_voltage, lv_color_hex(0x1A2735), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar_main_ch2_voltage, lv_color_hex(UiTheme::kAccentI), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(bar_main_ch2_voltage, lv_color_hex(0x73C8FF), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(bar_main_ch2_voltage, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar_main_ch2_voltage, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar_main_ch2_voltage, 4, LV_PART_INDICATOR);

  lv_obj_t* bar_i_lo = lv_label_create(panel_i);
  lv_label_set_text(bar_i_lo, "3.0V");
  lv_obj_set_style_text_color(bar_i_lo, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_i_lo, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_i_lo, LV_ALIGN_BOTTOM_LEFT, 18, -2);

  lv_obj_t* bar_i_mid = lv_label_create(panel_i);
  lv_label_set_text(bar_i_mid, "3.3V");
  lv_obj_set_style_text_color(bar_i_mid, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(bar_i_mid, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(bar_i_mid, LV_ALIGN_BOTTOM_MID, 0, -2);

  lv_obj_t* bar_i_hi = lv_label_create(panel_i);
  lv_label_set_text(bar_i_hi, "3.6V");
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
  lv_label_set_text(panel_meta_hdr, "SET / STATUS");
  lv_obj_set_style_text_color(panel_meta_hdr, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(panel_meta_hdr, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(panel_meta_hdr, LV_ALIGN_TOP_LEFT, 16, 16);

  lbl_main_stats = lv_label_create(panel_meta);
  lv_label_set_text(lbl_main_stats,
                    "CH1 SET   5.00V / 3.00A\n"
                    "CH2 SET   3.30V / 2.00A\n"
                    "TEMP      --C   UBYTES 0\n"
                    "LINK      WAIT   SEQ 0\n"
                    "FRAMES    0 RX / 0 SRC\n"
                    "ERRORS    0\n"
                    "UPTIME    00:00:00");
  lv_obj_set_style_text_color(lbl_main_stats, lv_color_hex(UiTheme::kTextPrimary), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_main_stats, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_width(lbl_main_stats, 250);
  lv_obj_align(lbl_main_stats, LV_ALIGN_TOP_LEFT, 16, 56);

  lv_obj_t* panel_meta_sub = lv_label_create(panel_meta);
  lv_label_set_text(panel_meta_sub, "COMMERCIAL-LAYOUT TRANSITION");
  lv_obj_set_style_text_color(panel_meta_sub, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
  lv_obj_set_style_text_font(panel_meta_sub, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(panel_meta_sub, LV_ALIGN_TOP_LEFT, 16, 34);

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
  lv_label_set_text(footer, "Fixed-rail live summary");
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
  lv_obj_set_size(card, kDisplayWidth - 36, 72);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 62);
  lv_obj_set_style_bg_color(card, lv_color_hex(UiTheme::kPanel), LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(UiTheme::kBorder), LV_PART_MAIN);

  lbl_graph_voltage = lv_label_create(card);
  lv_label_set_text(lbl_graph_voltage, "CH1 --.--V / --.---A");
  lv_obj_set_style_text_color(lbl_graph_voltage, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_voltage, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_width(lbl_graph_voltage, 280);
  lv_obj_set_style_text_align(lbl_graph_voltage, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_graph_voltage, LV_ALIGN_LEFT_MID, 18, -14);

  lbl_graph_current = lv_label_create(card);
  lv_label_set_text(lbl_graph_current, "CH2 --.--V / --.---A");
  lv_obj_set_style_text_color(lbl_graph_current, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_current, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_width(lbl_graph_current, 280);
  lv_obj_set_style_text_align(lbl_graph_current, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(lbl_graph_current, LV_ALIGN_LEFT_MID, 18, 14);

  lbl_graph_stats = lv_label_create(card);
  lv_label_set_text(lbl_graph_stats, "samples=0 T=--C");
  lv_obj_set_style_text_color(lbl_graph_stats, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_stats, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_stats, LV_ALIGN_RIGHT_MID, -18, 0);

  lv_obj_t* lbl_v_trend = lv_label_create(screen_graph);
  lv_label_set_text(lbl_v_trend, "Voltage trend (mV)");
  lv_obj_set_style_text_color(lbl_v_trend, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_v_trend, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_v_trend, LV_ALIGN_TOP_LEFT, 22, 144);

  chart_v = lv_chart_create(screen_graph);
  lv_obj_set_size(chart_v, 650, 96);
  lv_obj_align(chart_v, LV_ALIGN_TOP_LEFT, 22, 170);
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
  lv_obj_align(lbl_i_trend, LV_ALIGN_TOP_LEFT, 22, 278);

  chart_i = lv_chart_create(screen_graph);
  lv_obj_set_size(chart_i, 650, 96);
  lv_obj_align(chart_i, LV_ALIGN_TOP_LEFT, 22, 304);
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
  lv_obj_set_style_text_font(lbl_window, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_window, LV_ALIGN_BOTTOM_LEFT, 24, -8);

  lbl_graph_window_v = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_v, "V min/max --.-- / --.--");
  lv_obj_set_style_text_color(lbl_graph_window_v, lv_color_hex(UiTheme::kAccentV), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_v, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_v, LV_ALIGN_BOTTOM_MID, -120, -8);

  lbl_graph_window_i = lv_label_create(screen_graph);
  lv_label_set_text(lbl_graph_window_i, "I min/max --.--- / --.---");
  lv_obj_set_style_text_color(lbl_graph_window_i, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_graph_window_i, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_graph_window_i, LV_ALIGN_BOTTOM_RIGHT, -22, -8);

  lv_obj_t* chip_col = lv_obj_create(screen_graph);
  lv_obj_set_size(chip_col, 92, 246);
  lv_obj_align(chip_col, LV_ALIGN_TOP_RIGHT, -24, 172);
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
  lv_obj_align(fault_row, LV_ALIGN_BOTTOM_MID, 0, -26);
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
  create_setup_screen(scr);
  create_main_screen(scr);
  create_graph_screen(scr);
  create_settings_screen(scr);
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
    if (should_redraw_values) {
      char vbuf[32];
      snprintf(vbuf, sizeof(vbuf), "%0.3f V", t.last_v12_mV / 1000.0f);
      if (strcmp(vbuf, last_voltage_text) != 0) {
        if (lbl_main_ch1_voltage) lv_label_set_text(lbl_main_ch1_voltage, vbuf);
        if (lbl_graph_voltage) {
          char v_graph[32];
          snprintf(v_graph, sizeof(v_graph), "CH1 %05.2fV / %05.3fA",
                   t.last_v12_mV / 1000.0f,
                   t.last_i12_mA / 1000.0f);
          lv_label_set_text(lbl_graph_voltage, v_graph);
        }
        strncpy(last_voltage_text, vbuf, sizeof(last_voltage_text) - 1);
        last_voltage_text[sizeof(last_voltage_text) - 1] = '\0';
      }

      char ibuf[32];
      snprintf(ibuf, sizeof(ibuf), "%0.3f", t.last_i12_mA / 1000.0f);
      if (strcmp(ibuf, last_current_text) != 0) {
        if (lbl_main_ch1_current) {
          char i_main[40];
          snprintf(i_main, sizeof(i_main), "A %s  SET %.3f", ibuf, kCh1SetCurrent_A);
          lv_label_set_text(lbl_main_ch1_current, i_main);
        }
        if (lbl_graph_current) {
          char i_graph[40];
          snprintf(i_graph, sizeof(i_graph), "CH2 %05.2fV / %05.3fA",
                   t.last_v3v3_mV / 1000.0f,
                   t.last_i3v3_mA / 1000.0f);
          lv_label_set_text(lbl_graph_current, i_graph);
        }
        strncpy(last_current_text, ibuf, sizeof(last_current_text) - 1);
        last_current_text[sizeof(last_current_text) - 1] = '\0';
      }

      char v3buf[32];
      snprintf(v3buf, sizeof(v3buf), "%0.3f V", t.last_v3v3_mV / 1000.0f);
      if (strcmp(v3buf, last_ch2_voltage_text) != 0) {
        if (lbl_main_ch2_voltage) lv_label_set_text(lbl_main_ch2_voltage, v3buf);
        strncpy(last_ch2_voltage_text, v3buf, sizeof(last_ch2_voltage_text) - 1);
        last_ch2_voltage_text[sizeof(last_ch2_voltage_text) - 1] = '\0';
      }

      char i3buf[32];
      snprintf(i3buf, sizeof(i3buf), "%0.3f", t.last_i3v3_mA / 1000.0f);
      if (strcmp(i3buf, last_ch2_current_text) != 0) {
        if (lbl_main_ch2_current) {
          char i3_main[40];
          snprintf(i3_main, sizeof(i3_main), "A %s  SET %.3f", i3buf, kCh2SetCurrent_A);
          lv_label_set_text(lbl_main_ch2_current, i3_main);
        }
        strncpy(last_ch2_current_text, i3buf, sizeof(last_ch2_current_text) - 1);
        last_ch2_current_text[sizeof(last_ch2_current_text) - 1] = '\0';
      }

      const bool local_link_stale = (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500));
      const bool ch1_cv = t.has_extended ? ((t.status & 0x20u) != 0u) : (t.last_i12_mA < 1500);

      if (lbl_main_ch1_power) {
        char p1buf[64];
        snprintf(p1buf, sizeof(p1buf), "P %0.2f W   TEMP %uC   MODE %s",
                 (t.last_v12_mV / 1000.0f) * (t.last_i12_mA / 1000.0f),
                 static_cast<unsigned>(t.last_temp_C),
                 ch1_cv ? "CV" : "CC");
        lv_label_set_text(lbl_main_ch1_power, p1buf);
      }
      if (lbl_main_ch2_power) {
        char p2buf[64];
        snprintf(p2buf, sizeof(p2buf), "P %0.2f W   LINK %s",
                 (t.last_v3v3_mV / 1000.0f) * (t.last_i3v3_mA / 1000.0f),
                 demo_mode ? "DEMO" : (local_link_stale ? "STALE" : "LIVE"));
        lv_label_set_text(lbl_main_ch2_power, p2buf);
      }
      last_v12_drawn_mV = t.last_v12_mV;
      last_i12_drawn_mA = t.last_i12_mA;
      last_value_draw_ms = now_ms;
      have_drawn_values = true;
    }

  if (kMinimalUiLabelsOnly) return;

  if (now_ms - last_detail_ui_update_ms < kDetailUiUpdateMinMs) return;
  last_detail_ui_update_ms = now_ms;

  const unsigned long uptime_s = static_cast<unsigned long>(millis() / 1000UL);
  const unsigned long uptime_h = uptime_s / 3600UL;
  const unsigned long uptime_m = (uptime_s % 3600UL) / 60UL;
  const unsigned long uptime_rem_s = uptime_s % 60UL;
  char sbuf[192];
  snprintf(sbuf, sizeof(sbuf),
           "CH1 SET   %.2fV / %.2fA\n"
           "CH2 SET   %.2fV / %.2fA\n"
           "TEMP      %uC   UBYTES %lu\n"
           "LINK      %s   SEQ %u\n"
           "FRAMES    %lu RX / %lu SRC\n"
           "ERRORS    %lu\n"
           "UPTIME    %02lu:%02lu:%02lu",
           kCh1SetVoltage_V,
           kCh1SetCurrent_A,
           kCh2SetVoltage_V,
           kCh2SetCurrent_A,
           static_cast<unsigned>(t.last_temp_C),
           static_cast<unsigned long>(t.uart_bytes),
           demo_mode ? "DEMO" : ((t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500) ? "STALE" : "LIVE"),
           static_cast<unsigned>(t.last_seq),
           static_cast<unsigned long>(t.rx_count),
           static_cast<unsigned long>(t.i2c_rx_count),
           static_cast<unsigned long>(t.err_count),
           uptime_h,
           uptime_m,
           uptime_rem_s);
  if (lbl_main_stats) lv_label_set_text(lbl_main_stats, sbuf);
  
  if (lbl_graph_stats) {
    char gbuf[112];
    snprintf(gbuf, sizeof(gbuf), "samples=%lu rx=%lu src=%lu err=%lu uartB=%lu T=%uC",
             static_cast<unsigned long>(trend_count),
             static_cast<unsigned long>(t.rx_count),
             static_cast<unsigned long>(t.i2c_rx_count),
             static_cast<unsigned long>(t.err_count),
             static_cast<unsigned long>(t.uart_bytes),
             static_cast<unsigned>(t.last_temp_C));
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
    const bool ch1_cv = t.has_extended ? ((t.status & 0x20u) != 0u) : (t.last_i12_mA < 1500);
    if (!ch1_cv) {
      lv_label_set_text(lbl_status_mode, "CC");
      lv_obj_set_style_text_color(lbl_status_mode, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
    } else {
      lv_label_set_text(lbl_status_mode, "CV");
      lv_obj_set_style_text_color(lbl_status_mode, lv_color_hex(UiTheme::kAccentI), LV_PART_MAIN);
    }
  }

  if (lbl_status_output) {
    const bool link_stale = (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500));
    const bool output_on = t.has_extended
        ? ((t.status & 0x80u) != 0u)
        : (t.last_i12_mA >= kOutputOnThreshold_mA);
    if (link_stale) {
      lv_label_set_text(lbl_status_output, "OUTPUT ??");
      lv_obj_set_style_text_color(lbl_status_output, lv_color_hex(UiTheme::kAccentWarn), LV_PART_MAIN);
    } else if (output_on) {
      lv_label_set_text(lbl_status_output, "OUTPUT ON");
      lv_obj_set_style_text_color(lbl_status_output, lv_color_hex(UiTheme::kAccentOk), LV_PART_MAIN);
    } else {
      lv_label_set_text(lbl_status_output, "OUTPUT OFF");
      lv_obj_set_style_text_color(lbl_status_output, lv_color_hex(UiTheme::kTextMuted), LV_PART_MAIN);
    }
  }

  if (lbl_fault_main || lbl_fault_graph) {
    const bool link_stale = (!demo_mode && (t.last_rx_ms == 0 || (millis() - t.last_rx_ms) > 1500));
    const bool cc_mode = t.has_extended ? ((t.status & 0x20u) == 0u) : (t.last_i12_mA >= 1500);
    const bool ch1_ovp = t.has_extended ? ((t.protection_flags & 0x80u) != 0u) : false;
    const bool ch1_ocp = t.has_extended ? ((t.protection_flags & 0x40u) != 0u) : false;
    const bool ch1_otp = t.has_extended ? ((t.protection_flags & 0x08u) != 0u) : false;
    const bool ch2_ovp = t.has_extended ? ((t.protection_flags & 0x20u) != 0u) : false;
    const bool ch2_ocp = t.has_extended ? ((t.protection_flags & 0x10u) != 0u) : false;
    const bool ch2_otp = t.has_extended ? ((t.protection_flags & 0x04u) != 0u) : false;
    const bool thermal_warn = t.has_extended ? ((t.status & 0x08u) != 0u) : false;
    char fbuf[128];
    snprintf(fbuf, sizeof(fbuf), "CH1 %s/%s/%s  CH2 %s/%s/%s  LIM %s%s%s",
             ch1_ovp ? "OVP!" : "OVP",
             ch1_ocp ? "OCP!" : "OCP",
             ch1_otp ? "OTP!" : "OTP",
             ch2_ovp ? "OVP!" : "OVP",
             ch2_ocp ? "OCP!" : "OCP",
             ch2_otp ? "OTP!" : "OTP",
             cc_mode ? "CC" : "CV",
             thermal_warn ? "   THERM WARN" : "",
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
  const bool cc_mode = t.has_extended ? ((t.status & 0x20u) == 0u) : (t.last_i12_mA >= 1500);

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

  if (chip_main_mode) {
    lv_label_set_text(chip_main_mode, demo_mode ? "DEMO" : (t.has_extended ? "EXT" : "LEG"));
    lv_obj_set_style_text_color(chip_main_mode,
                                demo_mode ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentI),
                                LV_PART_MAIN);
  }
  if (chip_graph_mode) {
    lv_label_set_text(chip_graph_mode, demo_mode ? "DEMO" : (t.has_extended ? "EXT" : "LEG"));
    lv_obj_set_style_text_color(chip_graph_mode,
                                demo_mode ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentI),
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
    const bool ch1_enabled = t.has_extended ? ((t.status & 0x80u) != 0u) : (t.last_i12_mA >= kOutputOnThreshold_mA);
    lv_label_set_text(chip_main_run, (link_stale || !ch1_enabled) ? "WAIT" : "RUN");
    lv_obj_set_style_text_color(chip_main_run,
                                (link_stale || !ch1_enabled) ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }
  if (chip_graph_run) {
    const bool ch1_enabled = t.has_extended ? ((t.status & 0x80u) != 0u) : (t.last_i12_mA >= kOutputOnThreshold_mA);
    lv_label_set_text(chip_graph_run, (link_stale || !ch1_enabled) ? "WAIT" : "RUN");
    lv_obj_set_style_text_color(chip_graph_run,
                                (link_stale || !ch1_enabled) ? lv_color_hex(UiTheme::kAccentWarn) : lv_color_hex(UiTheme::kAccentOk),
                                LV_PART_MAIN);
  }

  if (lbl_status_uptime) {
    char ubuf[32];
    snprintf(ubuf, sizeof(ubuf), "UP %lus", static_cast<unsigned long>(millis() / 1000UL));
    lv_label_set_text(lbl_status_uptime, ubuf);
  }

  if (bar_main_ch1_voltage) {
    const int32_t v_scaled = map_i32(static_cast<int32_t>(t.last_v12_mV), 4500, 5500, 0, 1000);
    lv_bar_set_value(bar_main_ch1_voltage, static_cast<lv_coord_t>(v_scaled), LV_ANIM_OFF);
  }
  if (bar_main_ch2_voltage) {
    const int32_t v3_scaled = map_i32(static_cast<int32_t>(t.last_v3v3_mV), 3000, 3600, 0, 1000);
    lv_bar_set_value(bar_main_ch2_voltage, static_cast<lv_coord_t>(v3_scaled), LV_ANIM_OFF);
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
    Serial.println("Commands: HELP, PING, STATUS, RX, UDI_STATUS, UDI_OUTPUT <ON|OFF>, UDI_ILIM <CH1|CH2> <mA>, OTA, SCREEN <SPLASH|SETUP|MAIN|GRAPH|SETTINGS>, SPLASH <ON|OFF>, DEMO <ON|OFF>, TOUR <ON|OFF>, SETUP_ENC <ROT <n>|PRESS|LONG>, PROBE <pin> [ms], LOG_START, LOG_STOP, LOG_STATUS, LOG_CLEAR, LOG_DUMP_CSV [N]");
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
    Serial.println("ERR SCREEN: use SPLASH|SETUP|MAIN|GRAPH|SETTINGS");
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
        sampled.last_rx_ms = t.last_rx_ms;
        sampled.uart_bytes = t.uart_bytes;
        last_trend_rx_count = t.rx_count;
        trendPushSample(sampled, now);
      }
    }
  }

  update_telemetry_labels();  // updates LVGL labels if telemetry changed

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
