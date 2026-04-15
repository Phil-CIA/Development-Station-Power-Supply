#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

static TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t draw_buf;
static lv_color_t draw_buffer[480 * 20];

static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *valueLabel = nullptr;
static lv_obj_t *bar = nullptr;
static uint32_t lastTickMs = 0;
static int demoValue = 0;
static int demoStep = 4;

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors(reinterpret_cast<uint16_t *>(&color_p->full), width * height, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

static void update_demo(lv_timer_t *timer) {
  LV_UNUSED(timer);

  demoValue += demoStep;
  if (demoValue >= 100) {
    demoValue = 100;
    demoStep = -4;
  } else if (demoValue <= 0) {
    demoValue = 0;
    demoStep = 4;
  }

  lv_bar_set_value(bar, demoValue, LV_ANIM_ON);

  char text[32];
  snprintf(text, sizeof(text), "Output %d%%", demoValue);
  lv_label_set_text(valueLabel, text);
}

static void build_ui() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x101820), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "LVGL Demo Ready");
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xF2AA4C), 0);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

  valueLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(valueLabel, "Output 0%");
  lv_obj_set_style_text_color(valueLabel, lv_color_white(), 0);
  lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, -20);

  bar = lv_bar_create(lv_scr_act());
  lv_obj_set_size(bar, 360, 24);
  lv_obj_align(bar, LV_ALIGN_CENTER, 0, 30);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);

  lv_timer_create(update_demo, 120, nullptr);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting LVGL demo...");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, draw_buffer, nullptr, sizeof(draw_buffer) / sizeof(draw_buffer[0]));

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 480;
  disp_drv.ver_res = 320;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  build_ui();
  lastTickMs = millis();
}

void loop() {
  uint32_t now = millis();
  lv_tick_inc(now - lastTickMs);
  lastTickMs = now;

  lv_timer_handler();
  delay(5);
}
