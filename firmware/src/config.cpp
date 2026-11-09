#include "config.h"
#include <LittleFS.h>
#include "icons_gen.h"

Config cfg;
static const char *CFG_PATH = "/config.json";

static int iconIndex(const char *id) {
  for (const IconAlias &a : ICON_ALIASES)  // configs from before 1.1.1 (vector icons)
    if (strcmp(a.from, id) == 0) { id = a.to; break; }
  for (int i = 0; i < ICON_COUNT; i++)
    if (strcmp(ICON_DEFS[i].id, id) == 0) return i;
  return -1;
}

static void hex(char *buf, uint32_t c) { snprintf(buf, 8, "#%06X", (unsigned)(c & 0xFFFFFF)); }

static bool parseHex(const char *s, uint32_t &out) {
  if (!s || s[0] != '#' || strlen(s) != 7) return false;
  char *end;
  unsigned long v = strtoul(s + 1, &end, 16);
  if (*end) return false;
  out = v;
  return true;
}

static bool parseHHMM(const char *s, uint16_t &out) {
  int h, m;
  if (!s || sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return false;
  out = h * 60 + m;
  return true;
}

static void addSlot(Config &c, const char *t, uint8_t days, const char *icon, uint8_t bright, const char *label) {
  Slot &s = c.slots[c.nslots++];
  parseHHMM(t, s.start);
  s.days = days;
  s.icon = iconIndex(icon);
  const IconDef &d = ICON_DEFS[s.icon];
  s.bg = d.bg; s.tx = d.tx;
  s.bright = bright;
  strlcpy(s.label, label, sizeof(s.label));
}

void configDefaults(Config &c, int lang) {
  c = Config();
  c.lang = lang;
  const char *const *L = STRINGS[lang];
  // brightness = color scaling on the round GC9A01 (no backlight pin)
  // keep in sync with demo() in web/index.html
  addSlot(c, "06:45", 0x1F, "sun3d", 100, L[S_LBL_WAKE]);   // Mon-Fri
  addSlot(c, "07:30", 0x60, "sun3d", 100, L[S_LBL_WAKE]);   // Sat-Sun
  addSlot(c, "18:30", 0x7F, "openbook3d", 70, L[S_LBL_STORY]);
  addSlot(c, "19:30", 0x7F, "bed3d", 45, L[S_LBL_BED]);
  addSlot(c, "20:15", 0x7F, "moon3d", 20, L[S_LBL_SLEEP]);  // dim at night by default
}

int langIndex(const char *code) {
  if (!code) return -1;
  for (int i = 0; i < LANG_COUNT; i++)
    if (strcmp(code, LANG_CODES[i]) == 0) return i;
  return -1;
}

void configSetLanguage(Config &c, int lang) {
  if (lang < 0 || lang >= LANG_COUNT || lang == c.lang) return;
  // Slot labels that are still a default label of the old language are translated too;
  // labels the parents typed themselves are kept.
  static const StrId LBL[] = {S_LBL_WAKE, S_LBL_STORY, S_LBL_BED, S_LBL_SLEEP};
  const int old = c.lang < LANG_COUNT ? c.lang : LANG_DEFAULT;
  for (int i = 0; i < c.nslots; i++)
    for (StrId id : LBL)
      if (strcmp(c.slots[i].label, STRINGS[old][id]) == 0) {
        strlcpy(c.slots[i].label, STRINGS[lang][id], sizeof(c.slots[i].label));
        break;
      }
  c.lang = lang;
}

const char *const PANEL_NAMES[3] = {"standard", "vivid", "extra"};
const char *const CLOCK_STYLE_NAMES[2] = {"digital", "analog"};
const char *const SAVER_NAMES[3] = {"off", "eyes", "clock"};

static int nameIndex(const char *const *names, int n, const char *s) {
  if (!s) return -1;
  for (int i = 0; i < n; i++) if (!strcmp(names[i], s)) return i;
  return -1;
}

static void syncEyesOn(Config &c) { c.eyes.on = (c.saver == SAVER_EYES); }

void configToJson(const Config &c, JsonDocument &doc) {
  doc.clear();
  doc["v"] = 1;
  doc["maxBright"] = c.maxBright;
  doc["autoDim"] = c.autoDim;
  doc["showClock"] = c.showClock;
  doc["showLabel"] = c.showLabel;
  uint8_t cs = c.clockStyle < 2 ? c.clockStyle : 0;
  uint8_t sv = c.saver < 3 ? c.saver : 1;
  doc["clockStyle"] = CLOCK_STYLE_NAMES[cs];
  doc["saver"] = SAVER_NAMES[sv];
  doc["lang"] = LANG_CODES[c.lang];
  doc["panel"] = PANEL_NAMES[c.panel];
  doc["vivid"] = c.vivid;
  eyesToJson(c.eyes, doc["eyes"].to<JsonObject>());
  JsonArray arr = doc["slots"].to<JsonArray>();
  char b[8];
  for (int i = 0; i < c.nslots; i++) {
    const Slot &s = c.slots[i];
    JsonObject o = arr.add<JsonObject>();
    char t[12];
    snprintf(t, sizeof(t), "%02u:%02u", s.start / 60, s.start % 60);
    o["start"] = t;
    o["days"] = s.days;
    o["icon"] = ICON_DEFS[s.icon].id;
    hex(b, s.bg); o["bg"] = b;
    hex(b, s.tx); o["tx"] = b;
    o["bright"] = s.bright;
    o["label"] = s.label;
  }
}

bool slotFromJson(JsonVariantConst o, Slot &s, String &err) {
  memset(&s, 0, sizeof(s));
  if (!parseHHMM(o["start"] | "", s.start)) { err = "bad start time (HH:MM)"; return false; }
  int days = o["days"] | 0x7F;
  if (days <= 0 || days > 0x7F) { err = "bad days mask"; return false; }
  s.days = days;
  int ic = iconIndex(o["icon"] | "");
  if (ic < 0) { err = "unknown icon"; return false; }
  s.icon = ic;
  const IconDef &d = ICON_DEFS[ic];
  s.bg = d.bg; s.tx = d.tx;
  if (o["bg"].is<const char *>() && !parseHex(o["bg"], s.bg)) { err = "bad bg color"; return false; }
  if (o["tx"].is<const char *>() && !parseHex(o["tx"], s.tx)) { err = "bad tx color"; return false; }
  s.bright = constrain((int)(o["bright"] | 50), 0, 100);
  strlcpy(s.label, o["label"] | "", sizeof(s.label));
  return true;
}

void eyesToJson(const EyesCfg &e, JsonObject o) {
  char b[8];
  o["on"] = e.on;
  hex(b, e.color); o["color"] = b;
  hex(b, e.bg); o["bg"] = b;
  o["shape"] = e.roundShape ? "round" : "rect";
  o["speed"] = e.speed;
  o["idle"] = e.idleMin;
  o["night"] = e.night == EYES_NIGHT_SLEEPY ? "sleepy" : "off";
  o["clock"] = e.clock;
}

bool eyesFromJson(JsonVariantConst o, EyesCfg &e, String &err) {
  EyesCfg n = e;
  if (o["on"].is<bool>()) n.on = o["on"];
  if (o["color"].is<const char *>() && !parseHex(o["color"], n.color)) { err = "bad color"; return false; }
  if (o["bg"].is<const char *>() && !parseHex(o["bg"], n.bg)) { err = "bad bg color"; return false; }
  if (o["shape"].is<const char *>()) {
    const char *sh = o["shape"];
    if (!strcmp(sh, "round")) n.roundShape = true;
    else if (!strcmp(sh, "rect")) n.roundShape = false;
    else { err = "shape must be round or rect"; return false; }
  }
  if (!o["speed"].isNull()) n.speed = constrain((int)(o["speed"] | 100), 50, 200);
  if (!o["idle"].isNull()) n.idleMin = constrain((int)(o["idle"] | 2), 1, 60);
  if (o["night"].is<const char *>()) {
    const char *ni = o["night"];
    if (!strcmp(ni, "off")) n.night = EYES_NIGHT_OFF;
    else if (!strcmp(ni, "sleepy")) n.night = EYES_NIGHT_SLEEPY;
    else { err = "night must be off or sleepy"; return false; }
  }
  if (o["clock"].is<bool>()) n.clock = o["clock"];
  e = n;
  return true;
}

bool slotIsNight(const Slot &s) { return s.icon < ICON_COUNT && ICON_DEFS[s.icon].sleep; }

bool displayFromJson(JsonVariantConst root, Config &c, String &err) {
  if (!root["panel"].isNull()) {
    const char *p = root["panel"] | "";
    int i = 0;
    while (i < 3 && strcmp(p, PANEL_NAMES[i])) i++;
    if (i == 3) { err = "panel must be standard, vivid or extra"; return false; }
    c.panel = i;
  }
  if (!root["vivid"].isNull()) c.vivid = constrain((int)(root["vivid"] | 40), 0, 100);
  return true;
}

bool configFromJson(JsonVariantConst root, Config &out, String &err) {
  Config c;
  c.maxBright = constrain((int)(root["maxBright"] | 100), 5, 100);
  c.autoDim = root["autoDim"] | false;
  c.showClock = root["showClock"] | true;
  c.showLabel = root["showLabel"] | false;
  int lang = langIndex(root["lang"] | LANG_CODES[LANG_DEFAULT]);
  if (lang < 0) { err = "unknown language"; return false; }
  c.lang = lang;
  if (!displayFromJson(root, c, err)) return false;
  if (!root["eyes"].isNull() && !eyesFromJson(root["eyes"], c.eyes, err)) { err = "eyes: " + err; return false; }
  // clockStyle / saver: new in 1.3.0; older configs keep digital + derive saver from eyes.on
  if (!root["clockStyle"].isNull()) {
    int i = nameIndex(CLOCK_STYLE_NAMES, 2, root["clockStyle"] | "");
    if (i < 0) { err = "clockStyle must be digital or analog"; return false; }
    c.clockStyle = i;
  } else {
    c.clockStyle = CLOCK_DIGITAL;
  }
  if (!root["saver"].isNull()) {
    int i = nameIndex(SAVER_NAMES, 3, root["saver"] | "");
    if (i < 0) { err = "saver must be off, eyes or clock"; return false; }
    c.saver = i;
  } else {
    c.saver = c.eyes.on ? SAVER_EYES : SAVER_OFF;
  }
  syncEyesOn(c);
  JsonArrayConst arr = root["slots"];
  if (arr.isNull()) { err = "missing slots array"; return false; }
  if (arr.size() > MAX_SLOTS) { err = "too many slots (max 16)"; return false; }
  for (JsonVariantConst o : arr) {
    if (!slotFromJson(o, c.slots[c.nslots], err)) {
      err = "slot " + String(c.nslots + 1) + ": " + err;
      return false;
    }
    c.nslots++;
  }
  out = c;
  return true;
}

bool configLoad() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { configDefaults(cfg); return false; }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  String err;
  if (e || !configFromJson(doc.as<JsonVariantConst>(), cfg, err)) {
    Serial.printf("config: invalid file (%s %s), using defaults\n", e.c_str(), err.c_str());
    configDefaults(cfg);
    return false;
  }
  return true;
}

bool configSave() {
  JsonDocument doc;
  configToJson(cfg, doc);
  File f = LittleFS.open("/config.tmp", "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (!ok) return false;
  LittleFS.remove(CFG_PATH);
  return LittleFS.rename("/config.tmp", CFG_PATH);
}

int activeSlot(const Config &c, int wday, int minute) {
  // Look back up to 7 days: latest slot that started today before now,
  // otherwise the latest slot of the most recent earlier day it applies to.
  for (int back = 0; back <= 7; back++) {
    int d = ((wday - back) % 7 + 7) % 7;
    int best = -1;
    for (int i = 0; i < c.nslots; i++) {
      const Slot &s = c.slots[i];
      if (!(s.days & (1 << d))) continue;
      if (back == 0 && s.start > minute) continue;
      if (best < 0 || s.start > c.slots[best].start) best = i;
    }
    if (best >= 0) return best;
  }
  return -1;
}
