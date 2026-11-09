#pragma once
#include "lgfx_setup.h"
#include "config.h"
#include "eyes.h"

extern LGFX lcd;

void screenInit();
// Brightness 0..100 %. Round GC9A01 (no BL pin): colors are scaled in software, the
// caller must redraw after a change (returns true when a redraw is needed).
// ST7789 variant: hardware PWM backlight, never needs a redraw.
bool screenSetBrightnessPct(float pct);
// Saturation / contrast boost of the colour icons and the eye colour, 0..100 %.
// The caller redraws afterwards.
void screenSetVivid(uint8_t pct);
// Panel colour profile (PanelProfile). GC9A01: re-sends the gamma and grey-scale voltage
// registers, takes effect at once. Other panels: no-op.
void screenSetPanelProfile(uint8_t profile);
// Message screen: title + up to 3 lines (UTF-8, shrunk to fit the circle).
void screenMessage(const char *title, const char *line1, const char *line2 = nullptr, const char *line3 = nullptr,
                   uint32_t titleRgb = 0xFFFFFF);
// Normal clock view. clockStyle analog draws a face with the slot icon in the centre.
// showWifiBadge draws a tiny crossed Wi-Fi glyph (bottom of the circle).
void screenDrawSlot(const Slot &s, bool showLabel, bool showClock, const char *clock, uint8_t clockStyle = CLOCK_DIGITAL,
                    bool showWifiBadge = false);
// Digital: only the time band. Analog: full face (icon + hands) — call on minute change.
void screenDrawClock(const Slot &s, bool showLabel, const char *clock, uint8_t clockStyle = CLOCK_DIGITAL,
                     bool showWifiBadge = false);
// Clock-only screensaver: time in the selected style, no day-slot icon.
void screenDrawSaverClock(uint32_t bg, uint32_t tx, const char *clock, uint8_t clockStyle, int hour, int minute);
// Overlay / clear the small no-Wi-Fi badge without a full redraw (normal views).
void screenDrawWifiBadge(bool show, uint32_t bg, uint32_t fg);

// Eyes screensaver. Begin clears the screen; each frame redraws only the rows the eyes
// covered in this or the previous frame (no sprite, a few rows at a time, no flicker).
void screenEyesBegin(uint32_t bg);
void screenEyesFrame(const EyeParams &p, const EyeGeom &g, uint32_t eye, uint32_t bg);
// Time under the eyes (layout y = cy), in the eye colour.
void screenEyesClock(const char *clock, int cy, uint32_t eye, uint32_t bg);
