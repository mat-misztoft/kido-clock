// Animated eyes screensaver: geometry, rasterizer and animator.
// Plain C++ (no Arduino / display code), so it also builds on a PC for tests.
// tools/eyes.py is a line-by-line Python port used for docs/eyes.png and docs/eyes.gif.
#pragma once
#include <stdint.h>

// Columns covered by the eyes band (the eyes never leave it, whatever they do).
static constexpr int EYES_X0 = 4, EYES_W = 232;
static constexpr int EYES_LEVELS = 16;  // coverage levels: 0 = background, 15 = eye colour

struct EyeParams {
  float lookX = 0, lookY = 0;  // -1..1: where the eyes look
  float openL = 1, openR = 1;  // 0..1: blink / wink
  float scale = 1;             // >1: surprised (wide)
  float happy = 0;             // 0..1: happy squint (lower crescent cut)
  float lid = 0;               // 0..1: sleepy upper eyelids
};

struct EyeGeom {
  int cx = 120, cy = 120;  // centre of the pair on the 240x240 layout
  bool roundShape = true;  // true: round (pill), false: rounded rectangle
};

// Rows [y0, y1) the eyes cover in this pose (layout coordinates).
void eyesRows(const EyeParams &p, const EyeGeom &g, int &y0, int &y1);
// Coverage level (0..15) of every pixel of row y, columns EYES_X0 .. EYES_X0 + EYES_W - 1.
// Anti-aliased: 4 sub-rows per row, 1/64 px horizontal precision.
void eyesRow(const EyeParams &p, const EyeGeom &g, int y, uint8_t *level);

// Random but natural behaviour: look around, blink, happy, surprised, wink, sleepy.
// Night mode: half-closed eyes, slow blinks, slow looks, no other expressions.
class EyesAnimator {
 public:
  void reset(bool night, uint32_t seed);
  // dt in seconds, already multiplied by the speed setting.
  void step(float dt, EyeParams &out);
  bool night() const { return night_; }

 private:
  enum Expr : uint8_t { NEUTRAL, HAPPY, SURPRISED, SLEEPY };
  float rnd();
  float range(float a, float b) { return a + (b - a) * rnd(); }
  void startBlink(int eye, float duration);
  EyeParams cur_, tgt_;
  float tLook_ = 0, tBlink_ = 0, tExpr_ = 0, exprLeft_ = 0;
  float blinkT_ = -1, blinkDur_ = 0.18f;
  int blinkEye_ = 0;  // 0 both, 1 left (wink), 2 right (wink)
  Expr expr_ = NEUTRAL;
  bool night_ = false;
  uint32_t rng_ = 1;
};
