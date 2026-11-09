#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "i18n_gen.h"

static const int MAX_SLOTS = 16;

struct Slot {
  uint16_t start;   // minutes after midnight
  uint8_t days;     // bit0 = Monday ... bit6 = Sunday
  uint8_t icon;     // index into ICON_DEFS
  uint32_t bg, tx;  // RGB888: background, label and clock text
  uint8_t bright;   // 0..100 %
  char label[48];   // UTF-8
};

// Eyes screensaver (see eyes.h)
enum EyesNight : uint8_t { EYES_NIGHT_OFF = 0, EYES_NIGHT_SLEEPY = 1 };
struct EyesCfg {
  bool on = true;            // kept in sync with saver == SAVER_EYES for older web clients
  uint32_t color = 0x4FD1FF, bg = 0x000000;  // RGB888
  bool roundShape = true;    // false: rounded rectangle
  uint8_t speed = 100;       // animation speed, 50..200 %
  uint8_t idleMin = 2;       // minutes without a button press, 1..60
  uint8_t night = EYES_NIGHT_OFF;  // in sleep-time slots (moon, bed, ...)
  bool clock = true;         // show the time under the eyes
};

// Panel colour profile (GC9A01 gamma / voltage registers, see screen.cpp)
enum PanelProfile : uint8_t { PANEL_STANDARD = 0, PANEL_VIVID = 1, PANEL_EXTRA = 2 };
extern const char *const PANEL_NAMES[3];

// How the time is drawn on the normal clock view (and clock-only screensaver).
enum ClockStyle : uint8_t { CLOCK_DIGITAL = 0, CLOCK_ANALOG = 1 };
extern const char *const CLOCK_STYLE_NAMES[2];

// Idle screensaver after eyes.idleMin with no button press.
enum SaverMode : uint8_t { SAVER_OFF = 0, SAVER_EYES = 1, SAVER_CLOCK = 2 };
extern const char *const SAVER_NAMES[3];

struct Config {
  uint8_t maxBright = 100;  // 5..100 %
  uint8_t panel = PANEL_VIVID;
  uint8_t vivid = 40;       // colour icon saturation / contrast boost, 0..100 %
  bool autoDim = false;     // use photoresistor
  bool showClock = true;
  uint8_t clockStyle = CLOCK_DIGITAL;  // digital (default) or analog with icon in the centre
  uint8_t saver = SAVER_EYES;          // off / eyes (default) / clock only
  uint8_t lang = LANG_DEFAULT;  // index into LANG_CODES
  bool showLabel = false;  // round 1.28" screen: bigger icon without the label
  EyesCfg eyes;
  uint8_t nslots = 0;
  Slot slots[MAX_SLOTS];
};

extern Config cfg;

// Translated on-screen string in the configured language (lang/*.json).
inline const char *T(StrId id) { return STRINGS[cfg.lang < LANG_COUNT ? cfg.lang : LANG_DEFAULT][id]; }

void configDefaults(Config &c, int lang = LANG_DEFAULT);
// Index into LANG_CODES, or -1.
int langIndex(const char *code);
// Switch the language; default slot labels follow, custom labels are kept.
void configSetLanguage(Config &c, int lang);
bool configLoad();
bool configSave();
void configToJson(const Config &c, JsonDocument &doc);
// Parses and validates. On error returns false and fills err; `out` untouched.
bool configFromJson(JsonVariantConst root, Config &out, String &err);
// "panel" and "vivid" keys; missing keys keep their current value in `c`.
bool displayFromJson(JsonVariantConst root, Config &c, String &err);
bool slotFromJson(JsonVariantConst o, Slot &s, String &err);
void eyesToJson(const EyesCfg &e, JsonObject o);
// Missing keys keep their current value in `e`.
bool eyesFromJson(JsonVariantConst o, EyesCfg &e, String &err);
// Is this slot sleep time (night icon)?
bool slotIsNight(const Slot &s);
// Index of the slot active at given weekday (0 = Monday) and minute, or -1.
int activeSlot(const Config &c, int wdayMon0, int minute);
