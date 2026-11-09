#include "eyes.h"
#include <math.h>
#include <string.h>

// Shape constants (layout pixels). Keep in sync with tools/eyes.py.
static constexpr float SPACING = 50;  // eye centre to pair centre
static constexpr float EYE_W = 66, EYE_H = 80;
static constexpr float LOOK_X = 22, LOOK_Y = 16;
static constexpr float PERSP = 0.08f;  // the eye on the look side gets a bit bigger
static constexpr float RECT_R = 14;    // corner radius of the rounded-rectangle shape
static constexpr float MIN_H = 4;      // a closed eye is a thin line

struct Eye {
  float ex, ey, w, h, hFull, rr, top, bot, lidBase, lidSlope, cutY, cutRx, cutRy;
};

static Eye eyeOf(const EyeParams &p, const EyeGeom &g, int side /* -1 left, +1 right */) {
  Eye e;
  float persp = 1.0f + PERSP * p.lookX * side;
  e.ex = g.cx + side * SPACING + p.lookX * LOOK_X;
  e.ey = g.cy + p.lookY * LOOK_Y;
  e.w = EYE_W * p.scale * persp;
  e.hFull = EYE_H * p.scale * persp;
  float open = side < 0 ? p.openL : p.openR;
  e.h = e.hFull * open;
  if (e.h < MIN_H) e.h = MIN_H;
  float rr = g.roundShape ? e.w * 0.5f : RECT_R * p.scale;
  if (rr > e.w * 0.5f) rr = e.w * 0.5f;
  if (rr > e.h * 0.5f) rr = e.h * 0.5f;
  e.rr = rr;
  e.top = e.ey - e.h * 0.5f;
  e.bot = e.ey + e.h * 0.5f;
  e.lidBase = e.ey - e.hFull * 0.5f + p.lid * e.hFull * 0.55f;
  e.lidSlope = side * 0.3f * p.lid;  // outer corner lower: droopy
  // happy squint: an ellipse below the eye rises and cuts it into an arch (^)
  e.cutY = e.bot + 0.3f * e.h;
  e.cutRx = 0.7f * e.w;
  e.cutRy = 0.3f * e.h + p.happy * 0.55f * e.h;
  return e;
}

void eyesRows(const EyeParams &p, const EyeGeom &g, int &y0, int &y1) {
  Eye l = eyeOf(p, g, -1), r = eyeOf(p, g, 1);
  float t = l.top < r.top ? l.top : r.top, b = l.bot > r.bot ? l.bot : r.bot;
  y0 = (int)floorf(t) - 1;
  y1 = (int)ceilf(b) + 1;
}

// Adds horizontal coverage of [a, b) (layout x) to acc, in 1/64 px.
static void addSpan(uint16_t *acc, float a, float b) {
  int A = (int)floorf((a - EYES_X0) * 64.0f + 0.5f), B = (int)floorf((b - EYES_X0) * 64.0f + 0.5f);
  if (A < 0) A = 0;
  if (B > EYES_W * 64) B = EYES_W * 64;
  if (B <= A) return;
  int pa = A >> 6, pb = (B - 1) >> 6;
  if (pa == pb) { acc[pa] += B - A; return; }
  acc[pa] += (pa + 1) * 64 - A;
  for (int x = pa + 1; x < pb; x++) acc[x] += 64;
  acc[pb] += B - pb * 64;
}

static void eyeSubRow(const Eye &e, float y, uint16_t *acc) {
  if (y < e.top || y >= e.bot) return;
  // rounded rectangle (a pill when rr = w/2)
  float dy = fabsf(y - e.ey), inner = e.h * 0.5f - e.rr, hw = e.w * 0.5f;
  if (dy > inner) {
    float t = dy - inner, q = e.rr * e.rr - t * t;
    hw = e.w * 0.5f - e.rr + (q > 0 ? sqrtf(q) : 0);
  }
  float a = e.ex - hw, b = e.ex + hw;
  // sleepy eyelid: visible below the line lidBase + lidSlope * (x - ex)
  if (e.lidSlope != 0) {
    float xb = e.ex + (y - e.lidBase) / e.lidSlope;
    if (e.lidSlope > 0) { if (xb < b) b = xb; }
    else if (xb > a) a = xb;
  } else if (y < e.lidBase) return;
  if (b <= a) return;
  // happy squint: remove the part inside the rising ellipse
  float dc = (y - e.cutY) / e.cutRy;
  if (dc * dc < 1) {
    float c = e.cutRx * sqrtf(1 - dc * dc);
    float c0 = e.ex - c, c1 = e.ex + c;
    if (c0 > a) addSpan(acc, a, c0 < b ? c0 : b);
    if (c1 < b) addSpan(acc, c1 > a ? c1 : a, b);
    return;
  }
  addSpan(acc, a, b);
}

void eyesRow(const EyeParams &p, const EyeGeom &g, int y, uint8_t *level) {
  uint16_t acc[EYES_W];
  memset(acc, 0, sizeof(acc));
  Eye l = eyeOf(p, g, -1), r = eyeOf(p, g, 1);
  for (int s = 0; s < 4; s++) {
    float sy = y + (s + 0.5f) * 0.25f;
    eyeSubRow(l, sy, acc);
    eyeSubRow(r, sy, acc);
  }
  for (int x = 0; x < EYES_W; x++) {
    uint32_t v = acc[x] > 256 ? 256 : acc[x];
    level[x] = (uint8_t)((v * 15 + 128) >> 8);
  }
}

// ---------------- animator ----------------
float EyesAnimator::rnd() {  // xorshift32, same sequence as tools/eyes.py
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return (rng_ >> 8) * (1.0f / 16777216.0f);
}

void EyesAnimator::reset(bool night, uint32_t seed) {
  night_ = night;
  rng_ = seed ? seed : 1;
  cur_ = EyeParams();
  tgt_ = EyeParams();
  if (night) cur_.lid = tgt_.lid = 0.55f;
  tLook_ = range(0.5f, 1.5f);
  tBlink_ = range(0.8f, 2.5f);
  tExpr_ = range(4.0f, 8.0f);
  exprLeft_ = 0;
  blinkT_ = -1;
  expr_ = NEUTRAL;
}

void EyesAnimator::startBlink(int eye, float duration) {
  blinkT_ = 0;
  blinkEye_ = eye;
  blinkDur_ = duration;
}

static float smooth(float x) {
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  return x * x * (3 - 2 * x);
}

void EyesAnimator::step(float dt, EyeParams &out) {
  if (dt > 0.1f) dt = 0.1f;
  // where to look
  tLook_ -= dt;
  if (tLook_ <= 0) {
    float r = rnd();
    if (r < 0.35f) { tgt_.lookX = 0; tgt_.lookY = 0; }
    else {
      static const float DIRS[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-0.7f, -0.7f}, {0.7f, -0.7f}, {-0.7f, 0.7f}, {0.7f, 0.7f}};
      int d = (int)(rnd() * 8) & 7;
      float m = night_ ? range(0.2f, 0.5f) : range(0.5f, 1.0f);
      tgt_.lookX = DIRS[d][0] * m;
      tgt_.lookY = DIRS[d][1] * m * 0.8f;
    }
    tLook_ = night_ ? range(3.0f, 6.0f) : range(1.2f, 3.5f);
  }
  // blinking
  tBlink_ -= dt;
  if (tBlink_ <= 0 && blinkT_ < 0) {
    startBlink(0, night_ ? 0.4f : 0.18f);
    tBlink_ = rnd() < 0.15f ? 0.35f : (night_ ? range(1.5f, 4.0f) : range(2.0f, 5.5f));
  }
  // expressions
  if (expr_ != NEUTRAL) {
    exprLeft_ -= dt;
    if (exprLeft_ <= 0) expr_ = NEUTRAL;
  } else if (!night_) {
    tExpr_ -= dt;
    if (tExpr_ <= 0) {
      float r = rnd();
      if (r < 0.35f) { expr_ = HAPPY; exprLeft_ = range(1.6f, 2.6f); }
      else if (r < 0.55f) { expr_ = SURPRISED; exprLeft_ = range(0.9f, 1.4f); tgt_.lookX = 0; tgt_.lookY = -0.3f; }
      else if (r < 0.75f) { if (blinkT_ < 0) startBlink(rnd() < 0.5f ? 1 : 2, 0.45f); }
      else { expr_ = SLEEPY; exprLeft_ = range(2.5f, 4.0f); }
      tExpr_ = range(5.0f, 11.0f);
    }
  }
  tgt_.scale = expr_ == SURPRISED ? 1.22f : 1.0f;
  tgt_.happy = expr_ == HAPPY ? 1.0f : 0.0f;
  tgt_.lid = night_ ? 0.55f : expr_ == SLEEPY ? 0.45f : 0.0f;
  // smooth easing towards the targets
  float kl = 1.0f - expf(-dt * (night_ ? 2.5f : 7.0f));
  float ke = 1.0f - expf(-dt * (expr_ == SURPRISED ? 14.0f : 6.0f));
  float kd = 1.0f - expf(-dt * 1.5f);  // eyelids droop slowly
  cur_.lookX += (tgt_.lookX - cur_.lookX) * kl;
  cur_.lookY += (tgt_.lookY - cur_.lookY) * kl;
  cur_.scale += (tgt_.scale - cur_.scale) * ke;
  cur_.happy += (tgt_.happy - cur_.happy) * ke;
  cur_.lid += (tgt_.lid - cur_.lid) * kd;
  out = cur_;
  out.openL = out.openR = 1;
  if (blinkT_ >= 0) {
    float x = blinkT_ / blinkDur_;
    float c = x < 0.4f ? smooth(x / 0.4f) : x < 0.55f ? 1.0f : 1.0f - smooth((x - 0.55f) / 0.45f);
    if (blinkEye_ != 2) out.openL = 1 - c;
    if (blinkEye_ != 1) out.openR = 1 - c;
    blinkT_ += dt;
    if (blinkT_ > blinkDur_) blinkT_ = -1;
  }
}
