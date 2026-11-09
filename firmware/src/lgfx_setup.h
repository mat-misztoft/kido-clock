#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board_pins.h"

class LGFX : public lgfx::LGFX_Device {
#if defined(PANEL_ST7789)
  lgfx::Panel_ST7789 _panel;
  lgfx::Light_PWM _light;
#else
  lgfx::Panel_GC9A01 _panel;
#endif
  lgfx::Bus_SPI _bus;

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = TFT_SPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_TFT_SCLK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = TFT_W;
      cfg.panel_height = TFT_H;
      cfg.memory_width = TFT_W;
      cfg.memory_height = TFT_H;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = true;      // GC9A01 / IPS ST7789 modules need inversion; set false if colors look negative
      cfg.rgb_order = false;  // set true if red and blue are swapped
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
#if defined(PANEL_ST7789)
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_TFT_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 1;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
#endif
    setPanel(&_panel);
  }
};
