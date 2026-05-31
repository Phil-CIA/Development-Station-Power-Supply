#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
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

// ── telemetry change-detection ─────────────────────────────────────────────
static uint32_t last_drawn_seq   = 0xFFFFFFFFu;
static uint32_t last_drawn_count = 0;
static uint32_t last_drawn_uartB = 0xFFFFFFFFu;

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
  lv_label_set_text(lbl_sub, "I2C + ESP-NOW from HAT");
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
}

// ── Telemetry label updates ────────────────────────────────────────────────
void update_telemetry_labels() {
  const auto t = disp_link_slave::snapshot();
  if (t.rx_count == last_drawn_count &&
      t.last_seq == last_drawn_seq &&
      t.uart_bytes == last_drawn_uartB) return;
  last_drawn_count = t.rx_count;
  last_drawn_seq   = t.last_seq;
  last_drawn_uartB = t.uart_bytes;

  char buf[32];
  snprintf(buf, sizeof(buf), "%5.2f V", t.last_v12_mV / 1000.0f);
  lv_label_set_text(lbl_voltage, buf);

  snprintf(buf, sizeof(buf), "%5.3f A", t.last_i12_mA / 1000.0f);
  lv_label_set_text(lbl_current, buf);

  char sbuf[64];
  snprintf(sbuf, sizeof(sbuf), "rx=%lu err=%lu seq=%u uartB=%lu i2c=%lu",
           static_cast<unsigned long>(t.rx_count),
           static_cast<unsigned long>(t.err_count),
           static_cast<unsigned>(t.last_seq),
           static_cast<unsigned long>(t.uart_bytes),
           static_cast<unsigned long>(t.i2c_rx_count));
  lv_label_set_text(lbl_stats, sbuf);
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
  Serial.printf("rx frames=%lu errs=%lu i2c=%lu seq=%u V12=%u mV I12=%d mA age=%lu ms uartB=%lu\n",
                static_cast<unsigned long>(t.rx_count),
                static_cast<unsigned long>(t.err_count),
                static_cast<unsigned long>(t.i2c_rx_count),
                static_cast<unsigned>(t.last_seq),
                static_cast<unsigned>(t.last_v12_mV),
                static_cast<int>(t.last_i12_mA),
                static_cast<unsigned long>(age_ms),
                static_cast<unsigned long>(t.uart_bytes));
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
    Serial.println("Commands: HELP, PING, STATUS, RX, PROBE <pin> [ms]");
    return;
  }
  if (line.equalsIgnoreCase("PING"))         { Serial.println("PONG"); return; }
  if (line.equalsIgnoreCase("STATUS"))       { printStatus();           return; }
  if (line.equalsIgnoreCase("RX"))           { printRxStatus();         return; }
  if (line.startsWith("PROBE") || line.startsWith("probe")) {
    handleProbe(line.substring(5));
    return;
  }
  Serial.printf("ERR unknown: %s\n", line.c_str());
}

}  // namespace

void setup() {
  Serial.begin(115200);

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
  if (now - last_log_ms >= 1000) {
    last_log_ms = now;
    const auto t = disp_link_slave::snapshot();
    if (t.rx_count != last_logged_cnt) {
      last_logged_cnt = t.rx_count;
      printRxStatus();
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
  delay(5);
}
