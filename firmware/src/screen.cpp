#include "screen.h"
#include <math.h>
#include "icons_gen.h"
#include "fonts_gen.h"

LGFX lcd;

// Software dimming (round GC9A01 has no backlight pin): every color is multiplied
// by g_scale/256 before it is sent to the panel. 256 = full brightness.
static uint16_t g_scale = 256;
static int g_level = -1;  // last applied brightness level (0..100)

static uint16_t c565(uint32_t rgb) {
  uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  r = r * g_scale >> 8; g = g * g_scale >> 8; b = b * g_scale >> 8;
  return lcd.color565(r, g, b);
}

// ---------------- panel colour profile (GC9A01 only) ----------------
// The grey-scale voltages are a ladder from VREG1A (white, V0) down to VREG1B (black, V63, at
// the panel's common voltage). F0/F1 (negative polarity) and F2/F3 (positive polarity) place
// the taps on that ladder: a larger value moves a tap further from white. "Standard" is the
// vendor table that LovyanGFX sends at init. "Vivid" moves the mid and dark taps (V13..V62)
// towards black, so mid-tones and colours are less washed out, and raises the amplitude
// (C9 vrh) a little for brighter whites. Light taps (V0..V6), V63 and the black level
// (C3/C4, which must match the panel's common voltage) stay unchanged. The registers are
// volatile: a reset or power cycle always restores the vendor values.
struct GammaSet { uint8_t c9, f0[6], f1[6]; };
static const GammaSet GAMMA[3] = {
    // vrh  dig2j0|vr1 dig2j1|vr2 vr4 vr6 vr0|vr13 vr20   vr43 vr27|vr57 vr36|vr59 vr61 vr62 vr50|vr63
    {0x22, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}},  // standard
    {0x25, {0x45, 0x09, 0x08, 0x08, 0x27, 0x2F}, {0x4A, 0x73, 0x75, 0x38, 0x39, 0x8F}},  // vivid
    {0x28, {0x45, 0x09, 0x08, 0x08, 0x28, 0x34}, {0x51, 0x76, 0x78, 0x3A, 0x3B, 0xAF}},  // extra vivid
};

void screenSetPanelProfile(uint8_t profile) {
#if defined(PANEL_ST7789)
  (void)profile;
#else
  const GammaSet &g = GAMMA[profile < 3 ? profile : 0];
  auto cmd = [](uint8_t c, const uint8_t *d, int n) {
    lcd.writeCommand(c);
    for (int i = 0; i < n; i++) lcd.writeData(d[i]);
  };
  lcd.startWrite();
  cmd(0xFE, nullptr, 0);  // inter register enable 1 + 2 (needed for C9 and F0..F3)
  cmd(0xEF, nullptr, 0);
  cmd(0xC9, &g.c9, 1);
  cmd(0xF0, g.f0, 6);
  cmd(0xF1, g.f1, 6);
  cmd(0xF2, g.f0, 6);  // positive polarity: same as negative, like the vendor table
  cmd(0xF3, g.f1, 6);
  lcd.endWrite();
#endif
}

// ---------------- vivid colours ----------------
// Saturation and contrast boost for the artwork (colour icons, eye colour), 0..100 %.
// Works on premultiplied colour (pivot = half the alpha), so edges blend cleanly.
// Integer only; tools/bitmap_icons.py vivid_px() is the same math for the mockups.
static int g_vivSat = 256, g_vivCon = 256;  // 8.8 fixed point factors

void screenSetVivid(uint8_t pct) {
  if (pct > 100) pct = 100;
  g_vivSat = 256 + pct * 154 / 100;  // up to 1.6x saturation
  g_vivCon = 256 + pct * 77 / 100;   // up to 1.3x contrast
}

static inline void vivid(int &r, int &g, int &b, int a) {
  int y = (77 * r + 150 * g + 29 * b) >> 8, piv = a >> 1;
  int c[3] = {r, g, b};
  for (int &v : c) {
    v = y + (((v - y) * g_vivSat) >> 8);
    v = piv + (((v - piv) * g_vivCon) >> 8);
    v = v < 0 ? 0 : v > a ? a : v;
  }
  r = c[0]; g = c[1]; b = c[2];
}

static uint32_t vividRgb(uint32_t rgb) {
  if (g_vivSat == 256) return rgb;
  int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  vivid(r, g, b, 255);
  return (uint32_t)r << 16 | g << 8 | b;
}

// ---------------- colour icons ----------------
// Pixels are QOI images (https://qoiformat.org) of BITMAP_SIZE x BITMAP_SIZE RGBA, stored once
// in flash. They are decoded as a stream, box-filtered down to the layout size in
// premultiplied alpha, blended against the slot background, dimmed (c565 scaling) and sent
// to the panel row by row. No frame buffer / sprite: ~6 KB of static RAM in total.
// tools/bitmap_icons.py (area_scale, blend_rgb) is the same algorithm for the mockups.
struct QoiStream {
  const uint8_t *p;
  uint8_t idx[64][4];
  uint8_t r = 0, g = 0, b = 0, a = 255;
  int run = 0;
  explicit QoiStream(const uint8_t *data) : p(data + 14) { memset(idx, 0, sizeof(idx)); }
  inline void next() {
    if (run > 0) { run--; return; }
    uint8_t b1 = *p++;
    if (b1 == 0xFE) { r = p[0]; g = p[1]; b = p[2]; p += 3; }
    else if (b1 == 0xFF) { r = p[0]; g = p[1]; b = p[2]; a = p[3]; p += 4; }
    else switch (b1 & 0xC0) {
      case 0x00: { const uint8_t *e = idx[b1 & 63]; r = e[0]; g = e[1]; b = e[2]; a = e[3]; break; }
      case 0x40: r += ((b1 >> 4) & 3) - 2; g += ((b1 >> 2) & 3) - 2; b += (b1 & 3) - 2; break;
      case 0x80: { uint8_t b2 = *p++; int vg = (b1 & 63) - 32;
                   r += vg - 8 + ((b2 >> 4) & 15); g += vg; b += vg - 8 + (b2 & 15); break; }
      default: run = b1 & 63; break;
    }
    uint8_t *e = idx[(r * 3 + g * 5 + b * 7 + a * 11) % 64];
    e[0] = r; e[1] = g; e[2] = b; e[3] = a;
  }
};

struct Span { uint8_t x0, x1, w0, w1; };  // source pixel -> up to 2 output pixels, weights sum to D
static uint32_t s_hrow[BITMAP_SIZE][4];   // current source row, horizontally filtered
static uint32_t s_acc[BITMAP_SIZE][4];    // current output row, accumulating
static uint16_t s_line[BITMAP_SIZE];      // RGB565 row for the panel

static void drawBitmapIcon(const IconDef &d, uint32_t bg, int ox, int oy, int D) {
  const int S = BITMAP_SIZE;
  if (D > S) D = S;
  if (D < 1) return;
  Span sp[BITMAP_SIZE];
  for (int i = 0; i < S; i++) {
    int lo = i * D, hi = lo + D, x0 = lo / S, x1 = (hi - 1) / S;
    if (x0 == x1) sp[i] = {(uint8_t)x0, (uint8_t)x0, (uint8_t)D, 0};
    else { int cut = x1 * S; sp[i] = {(uint8_t)x0, (uint8_t)x1, (uint8_t)(cut - lo), (uint8_t)(hi - cut)}; }
  }
  const uint32_t TOT = (uint32_t)S * S, HALF = TOT / 2;
  const uint32_t br = (bg >> 16) & 255, bgg = (bg >> 8) & 255, bb = bg & 255;
  const bool viv = g_vivSat != 256;
  auto emit = [&]() {
    for (int x = 0; x < D; x++) {
      uint32_t a = min<uint32_t>((s_acc[x][3] + HALF) / TOT, 255), ia = 255 - a;
      int pr = min<uint32_t>((s_acc[x][0] + HALF) / TOT, 255), pg = min<uint32_t>((s_acc[x][1] + HALF) / TOT, 255),
          pb = min<uint32_t>((s_acc[x][2] + HALF) / TOT, 255);
      if (viv && a) vivid(pr, pg, pb, a);
      uint32_t r = pr + (br * ia + 127) / 255;
      uint32_t g = pg + (bgg * ia + 127) / 255;
      uint32_t b = pb + (bb * ia + 127) / 255;
      r = min<uint32_t>(r, 255); g = min<uint32_t>(g, 255); b = min<uint32_t>(b, 255);
      s_line[x] = lcd.color565(r * g_scale >> 8, g * g_scale >> 8, b * g_scale >> 8);
    }
    lcd.writePixels(s_line, D, true);
    memset(s_acc, 0, sizeof(s_acc[0]) * D);
  };
  QoiStream q(d.img);
  memset(s_acc, 0, sizeof(s_acc[0]) * D);
  lcd.setAddrWindow(ox, oy, D, D);
  int outRow = 0;
  for (int y = 0; y < S; y++) {
    memset(s_hrow, 0, sizeof(s_hrow[0]) * D);
    for (int i = 0; i < S; i++) {
      q.next();
      if (!q.a) continue;
      uint32_t a = q.a, pr = q.r * a / 255, pg = q.g * a / 255, pb = q.b * a / 255;
      const Span &v = sp[i];
      uint32_t *h = s_hrow[v.x0];
      h[0] += pr * v.w0; h[1] += pg * v.w0; h[2] += pb * v.w0; h[3] += a * v.w0;
      if (v.w1) { h = s_hrow[v.x1]; h[0] += pr * v.w1; h[1] += pg * v.w1; h[2] += pb * v.w1; h[3] += a * v.w1; }
    }
    const Span &v = sp[y];
    for (int part = 0; part < 2; part++) {
      int oy2 = part ? v.x1 : v.x0;
      uint32_t w = part ? v.w1 : v.w0;
      if (!w) continue;
      if (oy2 != outRow) { emit(); outRow++; }
      for (int x = 0; x < D; x++)
        for (int c = 0; c < 4; c++) s_acc[x][c] += s_hrow[x][c] * w;
    }
  }
  emit();
}

static void drawIcon(const Slot &s, int ox, int oy, int size) {
  drawBitmapIcon(ICON_DEFS[s.icon], s.bg, ox, oy, size);
}

static const ScreenLayout &layoutFor(bool label, bool clock) {
  if (label && clock) return LAYOUT_BOTH;
  if (clock) return LAYOUT_CLOCK;
  if (label) return LAYOUT_LABEL;
  return LAYOUT_NONE;
}

// Width of the circle (radius 120) at height y, minus a margin.
static int chordAt(int y, int margin = 14) {
  int dy = abs(y - 120);
  if (dy >= 120) return 0;
  return (int)(2.0f * sqrtf(120.0f * 120.0f - dy * dy)) - margin;
}

// Smooth (anti-aliased) UTF-8 text with Polish letters, shrunk to fit inside the circle.
static void drawFitText(const uint8_t *font, const char *txt, int cy, int halfH, uint16_t fg, uint16_t bg) {
  lcd.loadFont(font);
  lcd.setTextDatum(textdatum_t::middle_center);
  lcd.setTextColor(fg, bg);
  float avail = chordAt(cy < 120 ? cy - halfH : cy + halfH);
  int w = lcd.textWidth(txt);
  lcd.setTextSize(w > avail && w > 0 ? avail / w : 1.0f);
  lcd.drawString(txt, TFT_W / 2, cy + LAYOUT_OY);
  lcd.setTextSize(1.0f);
  lcd.unloadFont();
}

void screenInit() {
  lcd.init();
  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
#if HAS_BACKLIGHT_PWM
  lcd.setBrightness(0);
#endif
}

bool screenSetBrightnessPct(float pct) {
  int level = (int)lroundf(constrain(pct, 0.0f, 100.0f));
#if HAS_BACKLIGHT_PWM
  if (level == g_level) return false;
  g_level = level;
  int v = level <= 0 ? 0 : (int)lroundf(255.0f * powf(level / 100.0f, 1.3f));
  if (level > 0 && v < 2) v = 2;
  lcd.setBrightness(v);
  return false;
#else
  if (level == g_level) return false;
  g_level = level;
  // Perceptual-ish curve; 0 % = black (the always-on backlight still glows a little).
  g_scale = level <= 0 ? 0 : (uint16_t)lroundf(256.0f * powf(level / 100.0f, 1.2f));
  if (level > 0 && g_scale < 8) g_scale = 8;
  return true;
#endif
}

void screenMessage(const char *title, const char *line1, const char *line2, const char *line3, uint32_t titleRgb) {
  uint16_t bg = c565(0x1B1640), fg = c565(titleRgb), mut = c565(0xC9D6FF);
  lcd.fillScreen(bg);
  // 2 or 3 lines: same positions as before; 4 lines: a little tighter
  static const int Y3[] = {84, 124, 156}, Y4[] = {70, 110, 142, 174};
  const int *Y = line3 ? Y4 : Y3;
  drawFitText(FONT_LABEL, title, Y[0], 10, fg, bg);
  if (line1) drawFitText(FONT_LABEL, line1, Y[1], 10, mut, bg);
  if (line2) drawFitText(FONT_LABEL, line2, Y[2], 10, mut, bg);
  if (line3) drawFitText(FONT_LABEL, line3, Y[3], 10, mut, bg);
}


// Tiny crossed Wi-Fi glyph near the bottom of the round panel (safe inside the circle).
static void drawWifiGlyph(int cx, int cy, uint16_t fg) {
  // Dot + three rising bars (child-readable "Wi-Fi") with a slash for "no signal".
  lcd.fillCircle(cx, cy + 7, 2, fg);
  lcd.fillRect(cx - 3, cy + 2, 6, 2, fg);
  lcd.fillRect(cx - 5, cy - 2, 10, 2, fg);
  lcd.fillRect(cx - 7, cy - 6, 14, 2, fg);
  lcd.drawLine(cx - 8, cy + 10, cx + 8, cy - 8, fg);
  lcd.drawLine(cx - 7, cy + 10, cx + 9, cy - 8, fg);
}

static void drawWifiBadgeAt(uint32_t bg, uint32_t fg) {
  const int cx = 120, cy = 214;
  uint16_t b = c565(bg), f = c565(fg);
  lcd.fillCircle(cx, cy, 12, b);
  drawWifiGlyph(cx, cy - 2, f);
}

void screenDrawWifiBadge(bool show, uint32_t bg, uint32_t fg) {
  if (!show) {
    lcd.fillCircle(120, 214, 12, c565(bg));
    return;
  }
  drawWifiBadgeAt(bg, fg);
}

// Analog face: ticks + hands. Icon (optional) is drawn in the centre by the caller first.
static void drawAnalogHands(int cx, int cy, int radius, int hour, int minute, uint16_t fg, uint16_t dim) {
  // 12 hour ticks
  for (int i = 0; i < 12; i++) {
    float a = (i / 12.0f) * 2.0f * 3.14159265f - 3.14159265f / 2.0f;
    int x0 = cx + (int)lroundf(cosf(a) * (radius - 4));
    int y0 = cy + (int)lroundf(sinf(a) * (radius - 4));
    int x1 = cx + (int)lroundf(cosf(a) * (radius - 14));
    int y1 = cy + (int)lroundf(sinf(a) * (radius - 14));
    lcd.drawLine(x0, y0 + LAYOUT_OY, x1, y1 + LAYOUT_OY, i % 3 == 0 ? fg : dim);
    if (i % 3 == 0) lcd.drawLine(x0 + 1, y0 + LAYOUT_OY, x1 + 1, y1 + LAYOUT_OY, fg);
  }
  float minh = minute / 60.0f;
  float ha = ((hour % 12) + minh) / 12.0f * 2.0f * 3.14159265f - 3.14159265f / 2.0f;
  float ma = minh * 2.0f * 3.14159265f - 3.14159265f / 2.0f;
  int hl = (int)(radius * 0.48f), ml = (int)(radius * 0.72f);
  int hx = cx + (int)lroundf(cosf(ha) * hl);
  int hy = cy + (int)lroundf(sinf(ha) * hl);
  int mx = cx + (int)lroundf(cosf(ma) * ml);
  int my = cy + (int)lroundf(sinf(ma) * ml);
  // thick hour hand
  lcd.drawLine(cx, cy + LAYOUT_OY, hx, hy + LAYOUT_OY, fg);
  lcd.drawLine(cx + 1, cy + LAYOUT_OY, hx + 1, hy + LAYOUT_OY, fg);
  lcd.drawLine(cx - 1, cy + LAYOUT_OY, hx - 1, hy + LAYOUT_OY, fg);
  lcd.drawLine(cx, cy + LAYOUT_OY, mx, my + LAYOUT_OY, fg);
  lcd.fillCircle(cx, cy + LAYOUT_OY, 3, fg);
}

static void parseClockHM(const char *clock, int &hour, int &minute) {
  hour = 0; minute = 0;
  if (!clock) return;
  int h = 0, m = 0;
  if (sscanf(clock, "%d:%d", &h, &m) == 2) { hour = h; minute = m; }
}

static void drawAnalogSlot(const Slot &s, bool showLabel, const char *clock, bool showWifiBadge) {
  bool label = showLabel && s.label[0];
  // Leave room for an optional label under the face.
  int cy = label ? 108 : 120;
  int radius = label ? 88 : 100;
  int iconSize = label ? 64 : 72;
  uint16_t bg = c565(s.bg), fg = c565(s.tx);
  uint16_t dim = c565(((s.tx & 0xFEFEFE) + (s.bg & 0xFEFEFE)) >> 1);
  lcd.startWrite();
  lcd.fillScreen(bg);
  lcd.drawCircle(120, cy + LAYOUT_OY, radius, fg);
  lcd.drawCircle(120, cy + LAYOUT_OY, radius - 1, fg);
  drawIcon(s, 120 - iconSize / 2, cy - iconSize / 2 + LAYOUT_OY, iconSize);
  lcd.endWrite();
  int hour, minute;
  parseClockHM(clock, hour, minute);
  drawAnalogHands(120, cy, radius, hour, minute, fg, dim);
  if (label) drawFitText(FONT_LABEL, s.label, 198, 10, fg, bg);
  if (showWifiBadge) drawWifiBadgeAt(s.bg, s.tx);
}

void screenDrawClock(const Slot &s, bool showLabel, const char *clock, uint8_t clockStyle, bool showWifiBadge) {
  if (clockStyle == CLOCK_ANALOG) {
    drawAnalogSlot(s, showLabel, clock, showWifiBadge);
    return;
  }
  const ScreenLayout &L = layoutFor(showLabel && s.label[0], true);
  uint16_t bg = c565(s.bg);
  lcd.fillRect(40, L.clock_y - 20 + LAYOUT_OY, TFT_W - 80, 40, bg);
  drawFitText(FONT_CLOCK, clock, L.clock_y, 15, c565(s.tx), bg);
  if (showWifiBadge) drawWifiBadgeAt(s.bg, s.tx);
  else lcd.fillCircle(120, 214, 12, bg);  // clear badge area when hiding on partial update
}

void screenDrawSlot(const Slot &s, bool showLabel, bool showClock, const char *clock, uint8_t clockStyle,
                    bool showWifiBadge) {
  if (showClock && clockStyle == CLOCK_ANALOG) {
    drawAnalogSlot(s, showLabel, clock, showWifiBadge);
    return;
  }
  bool label = showLabel && s.label[0];
  const ScreenLayout &L = layoutFor(label, showClock);
  lcd.startWrite();
  lcd.fillScreen(c565(s.bg));
  drawIcon(s, L.ix, L.iy + LAYOUT_OY, L.isz);
  lcd.endWrite();
  if (label) drawFitText(FONT_LABEL, s.label, L.label_y, 10, c565(s.tx), c565(s.bg));
  if (showClock) {
    uint16_t bg = c565(s.bg);
    lcd.fillRect(40, L.clock_y - 20 + LAYOUT_OY, TFT_W - 80, 40, bg);
    drawFitText(FONT_CLOCK, clock, L.clock_y, 15, c565(s.tx), bg);
  }
  if (showWifiBadge) drawWifiBadgeAt(s.bg, s.tx);
}

void screenDrawSaverClock(uint32_t bgRgb, uint32_t txRgb, const char *clock, uint8_t clockStyle, int hour, int minute) {
  uint16_t bg = c565(bgRgb), fg = c565(txRgb);
  uint16_t dim = c565(((txRgb & 0xFEFEFE) + (bgRgb & 0xFEFEFE)) >> 1);
  lcd.fillScreen(bg);
  if (clockStyle == CLOCK_ANALOG) {
    const int cy = 120, radius = 100;
    lcd.drawCircle(120, cy + LAYOUT_OY, radius, fg);
    lcd.drawCircle(120, cy + LAYOUT_OY, radius - 1, fg);
    drawAnalogHands(120, cy, radius, hour, minute, fg, dim);
  } else {
    drawFitText(FONT_CLOCK, clock, 120, 18, fg, bg);
  }
}

// ---------------- eyes screensaver ----------------
static constexpr int EYES_CHUNK = 8;                  // rows per SPI transfer
static uint16_t s_eyeBuf[EYES_CHUNK * EYES_W];        // 3.7 KB, the only frame memory
static uint8_t s_eyeLevel[EYES_W];
static int s_eyePrev0 = 0, s_eyePrev1 = 0;            // rows drawn last frame

void screenEyesBegin(uint32_t bg) {
  lcd.fillScreen(c565(bg));
  s_eyePrev0 = s_eyePrev1 = 0;
}

void screenEyesFrame(const EyeParams &p, const EyeGeom &g, uint32_t eye, uint32_t bg) {
  // 16-step ramp from the background to the (vivid) eye colour (anti-aliased edges), dimmed
  eye = vividRgb(eye);
  uint16_t lut[EYES_LEVELS];
  const int er = (eye >> 16) & 255, eg = (eye >> 8) & 255, eb = eye & 255;
  const int br = (bg >> 16) & 255, bgg = (bg >> 8) & 255, bb = bg & 255;
  for (int i = 0; i < EYES_LEVELS; i++) {
    uint32_t r = br + (er - br) * i / 15, gg = bgg + (eg - bgg) * i / 15, b = bb + (eb - bb) * i / 15;
    lut[i] = c565((r << 16) | (gg << 8) | b);
  }
  int y0, y1;
  eyesRows(p, g, y0, y1);
  int a = min(y0, s_eyePrev0), b = max(y1, s_eyePrev1);
  if (s_eyePrev1 <= s_eyePrev0) { a = y0; b = y1; }
  s_eyePrev0 = y0;
  s_eyePrev1 = y1;
  if (a < 0) a = 0;
  if (b > 240) b = 240;
  lcd.startWrite();
  for (int y = a; y < b; y += EYES_CHUNK) {
    int n = min(EYES_CHUNK, b - y);
    for (int r = 0; r < n; r++) {
      eyesRow(p, g, y + r, s_eyeLevel);
      uint16_t *o = s_eyeBuf + r * EYES_W;
      for (int x = 0; x < EYES_W; x++) o[x] = lut[s_eyeLevel[x]];
    }
    lcd.setAddrWindow(EYES_X0, y + LAYOUT_OY, EYES_W, n);
    lcd.writePixels(s_eyeBuf, EYES_W * n, true);
  }
  lcd.endWrite();
}

void screenEyesClock(const char *clock, int cy, uint32_t eye, uint32_t bg) {
  uint16_t b = c565(bg);
  lcd.fillRect(40, cy - 20 + LAYOUT_OY, TFT_W - 80, 40, b);
  drawFitText(FONT_CLOCK, clock, cy, 15, c565(vividRgb(eye)), b);
}
