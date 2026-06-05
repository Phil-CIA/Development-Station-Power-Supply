#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <cstring>
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
constexpr size_t   kTrendCapacity = 1800;  // 7.5 minutes at 4 Hz
constexpr uint16_t kChartPoints   = 120;   // 2 minutes visible at 1 Hz (decimated)

// ── hardware ───────────────────────────────────────────────────────────────
CrowPanel43Display display;
String serialLine;

// ── LVGL driver state (full-frame double-buffer from PSRAM) ───────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* lvgl_fb1 = nullptr;
static lv_color_t* lvgl_fb2 = nullptr;

// ── LVGL UI handles ────────────────────────────────────────────────────────
static lv_obj_t* lbl_voltage = nullptr;
static lv_obj_t* lbl_current = nullptr;
static lv_obj_t* lbl_stats   = nullptr;
static lv_obj_t* lbl_window  = nullptr;
static lv_obj_t* chart_v     = nullptr;
static lv_obj_t* chart_i     = nullptr;
static lv_chart_series_t* chart_v_series = nullptr;
static lv_chart_series_t* chart_i_series = nullptr;

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
void create_dashboard() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_t* lbl_title = lv_label_create(scr);
  lv_label_set_text(lbl_title, "Development Station");
  lv_obj_set_style_text_color(lbl_title, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 24, 16);

  lv_obj_t* lbl_sub = lv_label_create(scr);
  lv_label_set_text(lbl_sub, "UART from HAT, OTA disabled");
  lv_obj_set_style_text_color(lbl_sub, lv_color_hex(0x808080), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_sub, LV_ALIGN_TOP_LEFT, 24, 60);

  lv_obj_t* lbl_v_hdr = lv_label_create(scr);
  lv_label_set_text(lbl_v_hdr, "12V INPUT VOLTAGE");
  lv_obj_set_style_text_color(lbl_v_hdr, lv_color_hex(0x606060), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_v_hdr, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_v_hdr, LV_ALIGN_TOP_LEFT, 40, 110);

  lbl_voltage = lv_label_create(scr);
  lv_label_set_text(lbl_voltage, "--.-- V");
  lv_obj_set_style_text_color(lbl_voltage, lv_color_hex(0x00E000), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_voltage, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(lbl_voltage, LV_ALIGN_TOP_LEFT, 40, 136);

  lv_obj_t* lbl_i_hdr = lv_label_create(scr);
  lv_label_set_text(lbl_i_hdr, "12V INPUT CURRENT");
  lv_obj_set_style_text_color(lbl_i_hdr, lv_color_hex(0x606060), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_i_hdr, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_i_hdr, LV_ALIGN_TOP_LEFT, 40, 258);

  lbl_current = lv_label_create(scr);
  lv_label_set_text(lbl_current, "--.--- A");
  lv_obj_set_style_text_color(lbl_current, lv_color_hex(0x00E0E0), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_current, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_obj_align(lbl_current, LV_ALIGN_TOP_LEFT, 40, 284);

  lbl_stats = lv_label_create(scr);
  lv_label_set_text(lbl_stats, "rx=0  err=0  seq=0");
  lv_obj_set_style_text_color(lbl_stats, lv_color_hex(0x505050), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_stats, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_stats, LV_ALIGN_BOTTOM_LEFT, 24, -16);

  lbl_window = lv_label_create(scr);
  lv_label_set_text(lbl_window, "win: waiting for samples");
  lv_obj_set_style_text_color(lbl_window, lv_color_hex(0x707070), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_window, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(lbl_window, LV_ALIGN_BOTTOM_RIGHT, -20, -16);

  lv_obj_t* lbl_v_trend = lv_label_create(scr);
  lv_label_set_text(lbl_v_trend, "Voltage trend (mV)");
  lv_obj_set_style_text_color(lbl_v_trend, lv_color_hex(0x606060), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_v_trend, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_v_trend, LV_ALIGN_TOP_RIGHT, -216, 92);

  chart_v = lv_chart_create(scr);
  lv_obj_set_size(chart_v, 390, 120);
  lv_obj_align(chart_v, LV_ALIGN_TOP_RIGHT, -20, 120);
  lv_chart_set_type(chart_v, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart_v, LV_CHART_UPDATE_MODE_CIRCULAR);
  lv_chart_set_point_count(chart_v, kChartPoints);
  lv_chart_set_range(chart_v, LV_CHART_AXIS_PRIMARY_Y, 9000, 15000);
  lv_obj_set_style_bg_color(chart_v, lv_color_hex(0x101010), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart_v, lv_color_hex(0x303030), LV_PART_MAIN);
  chart_v_series = lv_chart_add_series(chart_v, lv_color_hex(0x00E000), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_v, chart_v_series, LV_CHART_POINT_NONE);

  lv_obj_t* lbl_i_trend = lv_label_create(scr);
  lv_label_set_text(lbl_i_trend, "Current trend (mA)");
  lv_obj_set_style_text_color(lbl_i_trend, lv_color_hex(0x606060), LV_PART_MAIN);
  lv_obj_set_style_text_font(lbl_i_trend, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_align(lbl_i_trend, LV_ALIGN_TOP_RIGHT, -216, 252);

  chart_i = lv_chart_create(scr);
  lv_obj_set_size(chart_i, 390, 120);
  lv_obj_align(chart_i, LV_ALIGN_TOP_RIGHT, -20, 280);
  lv_chart_set_type(chart_i, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart_i, LV_CHART_UPDATE_MODE_CIRCULAR);
  lv_chart_set_point_count(chart_i, kChartPoints);
  lv_chart_set_range(chart_i, LV_CHART_AXIS_PRIMARY_Y, -500, 3000);
  lv_obj_set_style_bg_color(chart_i, lv_color_hex(0x101010), LV_PART_MAIN);
  lv_obj_set_style_border_color(chart_i, lv_color_hex(0x303030), LV_PART_MAIN);
  chart_i_series = lv_chart_add_series(chart_i, lv_color_hex(0x00E0E0), LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(chart_i, chart_i_series, LV_CHART_POINT_NONE);
}

// ── Telemetry label updates ────────────────────────────────────────────────
void update_telemetry_labels() {
  if (!kLiveTelemetryUiEnabled) return;

  const uint32_t now_ms = millis();
  if (now_ms - last_ui_update_ms < kUiUpdateMinMs) return;

  const auto t = disp_link_slave::snapshot();
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
    snprintf(vbuf, sizeof(vbuf), "%5.2f V", t.last_v12_mV / 1000.0f);
    if (strcmp(vbuf, last_voltage_text) != 0) {
      lv_label_set_text(lbl_voltage, vbuf);
      strncpy(last_voltage_text, vbuf, sizeof(last_voltage_text) - 1);
      last_voltage_text[sizeof(last_voltage_text) - 1] = '\0';
    }

    char ibuf[32];
    snprintf(ibuf, sizeof(ibuf), "%5.3f A", t.last_i12_mA / 1000.0f);
    if (strcmp(ibuf, last_current_text) != 0) {
      lv_label_set_text(lbl_current, ibuf);
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

  char sbuf[96];
  snprintf(sbuf, sizeof(sbuf), "rx=%lu err=%lu seq=%u uartB=%lu uart=%lu",
           static_cast<unsigned long>(t.rx_count),
           static_cast<unsigned long>(t.err_count),
           static_cast<unsigned>(t.last_seq),
           static_cast<unsigned long>(t.uart_bytes),
           static_cast<unsigned long>(t.i2c_rx_count));
  lv_label_set_text(lbl_stats, sbuf);

  if (trend_count != last_drawn_samples) {
    last_drawn_samples = trend_count;
    const TrendWindowStats stats = getTrendWindowStats(kChartPoints);
    char wbuf[128];
    if (!stats.has_data) {
      snprintf(wbuf, sizeof(wbuf), "win: waiting for samples");
    } else {
      snprintf(wbuf, sizeof(wbuf), "win %u s  V %.2f..%.2f  I %.3f..%.3f",
               static_cast<unsigned>(stats.samples),
               stats.min_v12_mV / 1000.0f,
               stats.max_v12_mV / 1000.0f,
               stats.min_i12_mA / 1000.0f,
               stats.max_i12_mA / 1000.0f);
    }
    lv_label_set_text(lbl_window, wbuf);
  }
}

// ── I2C helpers ────────────────────────────────────────────────────────────
bool i2cAddressResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// ── Serial command support ─────────────────────────────────────────────────
void printStatus() {
  Serial.printf("status board=0x30:%s touch=0x5D:%s\n",
                i2cAddressResponds(kBoardCtrlAddr) ? "ok" : "missing",
                i2cAddressResponds(kTouchAddr)     ? "ok" : "missing");
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
    Serial.println("Commands: HELP, PING, STATUS, RX, OTA, PROBE <pin> [ms], LOG_START, LOG_STOP, LOG_STATUS, LOG_CLEAR, LOG_DUMP_CSV [N]");
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
      const auto t = disp_link_slave::snapshot();
      if (t.rx_count != last_trend_rx_count) {
        last_trend_rx_count = t.rx_count;
        trendPushSample(t, now);
      }
    }
  }

  update_telemetry_labels();  // updates LVGL labels if telemetry changed

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
