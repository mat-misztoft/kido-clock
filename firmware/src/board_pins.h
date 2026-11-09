#pragma once
// Pin map. Change here if you wire differently.
//
// Default panel: generic 1.28" round GC9A01 240x240, 7 pins (RST CS DC SDA SCL GND VCC),
// NO backlight pin -> brightness is done in software (colors are scaled).
// Alternative: -D PANEL_ST7789 for a 2.0" 240x320 ST7789 with a BL pin (hardware PWM dimming).
#if defined(BOARD_S3_DEVKIT)
  #define PIN_TFT_SCLK 12
  #define PIN_TFT_MOSI 11
  #define PIN_TFT_CS   10   // optional on the GC9A01 module (R8 pull-down), connected for reliability
  #define PIN_TFT_DC    9
  #define PIN_TFT_RST   8   // optional on the GC9A01 module, connected for reliability
  #define PIN_TFT_BL    7   // only used with PANEL_ST7789
  #define PIN_LDR       1   // ADC1_CH0, optional
  #define PIN_BUTTON    4   // optional tact switch to GND (not GPIO0/BOOT: it is read at power-on)
  #define TFT_SPI_HOST SPI2_HOST
#else  // ESP32-C3 SuperMini (default)
  #define PIN_TFT_SCLK  4   // hardware SPI SCK  -> TFT "SCL"
  #define PIN_TFT_MOSI  6   // hardware SPI MOSI -> TFT "SDA"
  #define PIN_TFT_CS    7   // optional on the GC9A01 module, connected for reliability
  #define PIN_TFT_DC    5
  #define PIN_TFT_RST  10   // optional on the GC9A01 module, connected for reliability
  #define PIN_TFT_BL    3   // only used with PANEL_ST7789
  #define PIN_LDR       0   // ADC1_CH0, optional photoresistor divider
  #define PIN_BUTTON    1   // optional tact switch to GND (internal pull-up)
  #define TFT_SPI_HOST SPI2_HOST
#endif

#if defined(PANEL_ST7789)
  #define TFT_W 240
  #define TFT_H 320
  #define HAS_BACKLIGHT_PWM 1
#else
  #define TFT_W 240
  #define TFT_H 240
  #define HAS_BACKLIGHT_PWM 0
#endif
// The round 240x240 layout is drawn centered vertically on taller panels.
#define LAYOUT_OY ((TFT_H - 240) / 2)
