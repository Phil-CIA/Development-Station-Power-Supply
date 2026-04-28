#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

class CrowPanel43Display : public lgfx::LGFX_Device {
 private:
  lgfx::Panel_RGB panel_;
  lgfx::Bus_RGB bus_;
  lgfx::Touch_GT911 touch_;

 public:
  CrowPanel43Display() {
    {
      auto cfg = panel_.config();
      cfg.memory_width = 800;
      cfg.memory_height = 480;
      cfg.panel_width = 800;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      panel_.config(cfg);
    }

    {
      auto cfg = panel_.config_detail();
      cfg.use_psram = 1;
      panel_.config_detail(cfg);
    }

    {
      auto cfg = bus_.config();
      cfg.panel = &panel_;

      cfg.pin_d0 = GPIO_NUM_21;
      cfg.pin_d1 = GPIO_NUM_47;
      cfg.pin_d2 = GPIO_NUM_48;
      cfg.pin_d3 = GPIO_NUM_45;
      cfg.pin_d4 = GPIO_NUM_38;
      cfg.pin_d5 = GPIO_NUM_9;
      cfg.pin_d6 = GPIO_NUM_10;
      cfg.pin_d7 = GPIO_NUM_11;
      cfg.pin_d8 = GPIO_NUM_12;
      cfg.pin_d9 = GPIO_NUM_13;
      cfg.pin_d10 = GPIO_NUM_14;
      cfg.pin_d11 = GPIO_NUM_7;
      cfg.pin_d12 = GPIO_NUM_17;
      cfg.pin_d13 = GPIO_NUM_18;
      cfg.pin_d14 = GPIO_NUM_3;
      cfg.pin_d15 = GPIO_NUM_46;

      cfg.pin_henable = GPIO_NUM_42;
      cfg.pin_vsync = GPIO_NUM_41;
      cfg.pin_hsync = GPIO_NUM_40;
      cfg.pin_pclk = GPIO_NUM_39;

      cfg.freq_write = 21000000;
      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 8;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 8;
      cfg.pclk_idle_high = 1;

      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = touch_.config();
      cfg.x_min = 0;
      cfg.x_max = 800;
      cfg.y_min = 0;
      cfg.y_max = 480;
      cfg.pin_int = -1;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = I2C_NUM_0;
      cfg.pin_sda = GPIO_NUM_15;
      cfg.pin_scl = GPIO_NUM_16;
      cfg.pin_rst = -1;
      cfg.freq = 400000;
      cfg.i2c_addr = 0x5D;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }

    setPanel(&panel_);
  }
};
