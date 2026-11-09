// Kido Clock - kids sleep clock, ESP32-C3 + round 1.28" GC9A01 (or ST7789 with -D PANEL_ST7789)
// Shows a picture (sleepy moon, sun, bed, books, ...) depending on the time of day.
// Parents configure time slots, icons, colors and brightness at http://kido-clock.local
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <time.h>
#include "board_pins.h"
#include "config.h"
#include "screen.h"
#include "icons_gen.h"
#include "web_page_gen.h"
#include "version.h"
#include "eyes.h"

// POSIX TZ string (Central European Time). Examples: "GMT0BST,M3.5.0/1,M10.5.0" (UK), "EST5EDT,M3.2.0,M11.1.0" (US East)
static const char *TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";
static const char *HOSTNAME = "kido-clock";
static const char *AP_NAME = "Kido-Clock-Setup";

WebServer server(80);
WiFiManager wm;

static int shownSlot = -2;          // -2 = nothing drawn yet
static int shownMinute = -1;
static bool redrawAll = true;
static Slot previewSlot;
static uint32_t previewUntil = 0;   // millis; 0 = no preview
static uint32_t boostUntil = 0;     // button press: temporarily brighter
static float ldrAvg = -1;
static uint32_t lastOnline = 0;
static uint32_t lastReconnectAt = 0;
static uint8_t wifiReconnectN = 0;
static volatile bool g_gotIp = false;
static bool wifiBadgeOn = false;       // drawn on normal views after debounce
static bool saverClockOn = false;      // clock-only screensaver active
static int saverClockMinute = -1;

// eyes screensaver
static constexpr int EYES_CY_WITH_CLOCK = 108, EYES_CLOCK_Y = 204;
static uint32_t lastActivity = 0;      // last button press / change from the web UI
static uint32_t eyesPreviewUntil = 0;  // "Show eyes now" from the web UI
static EyesCfg eyesPreviewCfg;
static EyesCfg eyesCur;                // settings of the eyes on screen
static bool eyesOn = false, eyesNight = false;
static EyesAnimator eyesAnim;
static uint32_t eyesLastFrame = 0, eyesFrames = 0, eyesStatAt = 0;
static int eyesMinute = -1;

static bool timeValid(struct tm &t) {
  time_t now = time(nullptr);
  localtime_r(&now, &t);
  return t.tm_year + 1900 >= 2025;
}

static int wdayMon0(const struct tm &t) { return (t.tm_wday + 6) % 7; }

// ---------------- web ----------------
static void sendJson(int code, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static void sendError(int code, const String &msg) {
  JsonDocument d;
  d["error"] = msg;
  sendJson(code, d);
}

static void handleGetConfig() {
  JsonDocument doc;
  configToJson(cfg, doc);
  sendJson(200, doc);
}

// Panel colour profile + vivid colours from cfg (runtime, no reboot). The caller redraws.
static void applyDisplay() {
  screenSetPanelProfile(cfg.panel);
  screenSetVivid(cfg.vivid);
}

// Try "panel" / "vivid" without saving (the web UI calls this while the user moves the
// controls). Saving the config keeps them; a restart without saving restores the saved ones.
static void handleDisplay() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) return sendError(400, "JSON");
  String err;
  if (!displayFromJson(doc.as<JsonVariantConst>(), cfg, err)) return sendError(400, err);
  applyDisplay();
  redrawAll = true;
  JsonDocument ok;
  ok["ok"] = true;
  sendJson(200, ok);
}

static void handlePostConfig() {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, server.arg("plain"));
  if (e) return sendError(400, String("JSON: ") + e.c_str());
  Config c;
  String err;
  if (!configFromJson(doc.as<JsonVariantConst>(), c, err)) return sendError(400, err);
  cfg = c;
  applyDisplay();
  if (!configSave()) return sendError(500, "could not write flash");
  redrawAll = true;
  previewUntil = 0;
  eyesPreviewUntil = 0;
  lastActivity = millis();
  handleGetConfig();
}

static void handleStatus() {
  struct tm t;
  bool ok = timeValid(t);
  JsonDocument d;
  char buf[48];
  if (ok) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    d["time"] = buf;
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    d["date"] = buf;               // the web UI formats it in the selected language
    d["wday"] = wdayMon0(t);       // 0 = Monday
    d["slot"] = activeSlot(cfg, wdayMon0(t), t.tm_hour * 60 + t.tm_min);
  } else {
    d["time"] = "--:--";
    d["date"] = "";
    d["slot"] = -1;
  }
  d["synced"] = ok;
  d["lang"] = LANG_CODES[cfg.lang];
  bool wifi = WiFi.status() == WL_CONNECTED;
  d["wifi"] = wifi;
  d["ip"] = wifi ? WiFi.localIP().toString() : "";
  d["rssi"] = wifi ? WiFi.RSSI() : 0;
  d["ldr"] = (int)ldrAvg;
  d["heap"] = ESP.getFreeHeap();
  d["heapMaxBlock"] = ESP.getMaxAllocHeap();
  d["version"] = KIDO_VERSION;
  d["eyes"] = eyesOn;
#if defined(PANEL_ST7789)
  d["panelProfiles"] = false;  // gamma profiles are GC9A01 only
#else
  d["panelProfiles"] = true;
#endif
  d["uptime"] = millis() / 1000;
  sendJson(200, d);
}

static void handlePreview() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) return sendError(400, "JSON");
  String err;
  if (!slotFromJson(doc.as<JsonVariantConst>(), previewSlot, err)) return sendError(400, err);
  previewUntil = millis() + 15000;
  if (!previewUntil) previewUntil = 1;
  eyesPreviewUntil = 0;
  lastActivity = millis();
  redrawAll = true;
  JsonDocument ok;
  ok["ok"] = true;
  sendJson(200, ok);
}

// POST /api/eyes-preview, body = eyes settings (optional, unsaved): show the eyes for 20 s
static void handleEyesPreview() {
  EyesCfg e = cfg.eyes;
  String body = server.arg("plain");
  if (body.length()) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) return sendError(400, "JSON");
    String err;
    if (!eyesFromJson(doc.as<JsonVariantConst>(), e, err)) return sendError(400, err);
  }
  eyesPreviewCfg = e;
  eyesPreviewUntil = millis() + 20000;
  if (!eyesPreviewUntil) eyesPreviewUntil = 1;
  previewUntil = 0;
  eyesOn = false;  // restart with these settings
  JsonDocument ok;
  ok["ok"] = true;
  sendJson(200, ok);
}

static void handleWifiReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  wm.resetSettings();
  ESP.restart();
}

static void setupWeb() {
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html; charset=utf-8", (const char *)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
  });
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/preview", HTTP_POST, handlePreview);
  server.on("/api/eyes-preview", HTTP_POST, handleEyesPreview);
  server.on("/api/display", HTTP_POST, handleDisplay);
  server.on("/api/wifi-reset", HTTP_POST, handleWifiReset);
  // colour icons for the web UI previews, straight from flash (decoded in the browser)
  for (int i = 0; i < ICON_COUNT; i++) {
    if (!ICON_DEFS[i].img) continue;
    server.on(String("/icons/") + ICON_DEFS[i].id + ".qoi", HTTP_GET, [i] {
      server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");  // URL carries ?v=<version>
      server.send_P(200, "image/qoi", (const char *)ICON_DEFS[i].img, ICON_DEFS[i].img_len);
    });
  }
  server.onNotFound([] { server.send(404, "text/plain", "404"); });
  server.begin();
}

// ---------------- wifi ----------------
// Many ESP32-C3 SuperMini boards have a weak/badly matched antenna and fail to associate
// at full TX power. KIDO_WIFI_TX_POWER (set per env in platformio.ini) caps it.
// Mode changes (e.g. WiFiManager switching to AP+STA) can reset the power, so it is
// re-applied on every STA/AP start and right before each connect.
static void applyTxPower(const char *why) {
#ifdef KIDO_WIFI_TX_POWER
  if (WiFi.getMode() == WIFI_OFF) return;
  bool ok = WiFi.setTxPower(KIDO_WIFI_TX_POWER);
  Serial.printf("wifi: tx power %.1f dBm (%s)%s\n", (int)WiFi.getTxPower() / 4.0f, why, ok ? "" : " - setTxPower FAILED");
#else
  (void)why;
#endif
}

static const char *wlStatusName(wl_status_t st) {
  switch (st) {
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_STOPPED: return "STOPPED";
    default: return "?";
  }
}

// Why the last connect attempt failed. Counted from disconnect events (Wi-Fi event task).
enum WifiFail : uint8_t { WF_NONE, WF_PASSWORD, WF_NOT_FOUND, WF_TIMEOUT };
static volatile uint8_t g_authFails = 0, g_noApFails = 0;

static void resetFailCounters() {
  g_authFails = 0;
  g_noApFails = 0;
}

static bool isAuthReason(uint8_t r) {
  return r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT ||
         r == WIFI_REASON_802_1X_AUTH_FAILED;
}
static bool isNoApReason(uint8_t r) {
  return r == WIFI_REASON_NO_AP_FOUND || r == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY ||
         r == WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD || r == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD;
}
static StrId failText(WifiFail f) {
  return f == WF_PASSWORD ? S_ERR_PASSWORD : f == WF_NOT_FOUND ? S_ERR_NOT_FOUND : S_ERR_TIMEOUT;
}

static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START: applyTxPower("sta start"); break;
    case ARDUINO_EVENT_WIFI_AP_START: applyTxPower("ap start"); break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: Serial.println("wifi: associated"); break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("wifi: got IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      applyTxPower("got ip");
      g_gotIp = true;  // restart mDNS / nudge NTP from loop (not from the Wi-Fi task)
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      uint8_t r = info.wifi_sta_disconnected.reason;
      // 15/204/202 = handshake/auth failure (wrong password), 201/210-212 = AP not found (5 GHz only / wrong SSID)
      if (isAuthReason(r) && g_authFails < 255) g_authFails = g_authFails + 1;
      if (isNoApReason(r) && g_noApFails < 255) g_noApFails = g_noApFails + 1;
      Serial.printf("wifi: disconnected, reason %u (%s)\n", r, WiFi.disconnectReasonName((wifi_err_reason_t)r));
      break;
    }
    default: break;
  }
}

// ---- setup portal (WiFiManager, non-blocking) ----
// The first portal page gets a language picker (GET /lang?l=<code>, saved to the clock config
// at once). WiFiManager's own pages are English; a script in the page head swaps their texts
// for the chosen language (PORTAL_I18N, from the "portal" section of lang/*.json) and, after a
// failed attempt, shows an error banner. WiFiManager keeps pointers to both strings.
static String g_portalHead, g_portalMenu;
static volatile bool g_langChanged = false;  // picked in the portal: redraw the device screen
static bool g_portalSave = false;   // set by the portal's Save handler
static String g_portalSsid;
static WifiFail g_lastFail = WF_NONE;
static String g_failSsid;

static String htmlEsc(const String &in) {
  String o;
  for (char c : in) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      case '\'': o += "&#39;"; break;
      default: o += c;
    }
  }
  return o;
}

// JSON string literal that is also safe inside a <script> element.
static String jsonStr(const String &in) {
  String o = "\"";
  for (char c : in) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20 || c == '<' || c == '>' || c == '&' || c == '\'') {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", (uint8_t)c);
      o += b;
    } else o += c;
  }
  return o + "\"";
}

// Translates WiFiManager's pages in the browser (ES5 for old captive-portal browsers).
// P = PORTAL_I18N entry, F = last failure or null.
static const char PORTAL_JS[] PROGMEM = R"JS(
function kcTr(P,F){var d=document,t=P.t,w=P.w,k=P.k,a=[],n,i,v,s,ws=[];
for(s in w)ws.push(s);var re=ws.length?new RegExp('\\b('+ws.join('|')+')\\b','g'):null;
d.documentElement.lang=P.lang;if(t[d.title])d.title=t[d.title];
var tw=d.createTreeWalker(d.body,4,null,false);while((n=tw.nextNode()))a.push(n);
for(i=0;i<a.length;i++){n=a[i];v=n.nodeValue;s=v.replace(/^\s+|\s+$/g,'');
var pn=n.parentNode.nodeName;if(!s||pn=='A'||pn=='OPTION'||pn=='SCRIPT'||pn=='STYLE')continue;
if(t.hasOwnProperty(s)){n.nodeValue=v.replace(s,t[s]);continue}
if(v.indexOf(' to ')==0&&k.to)n.nodeValue=' '+k.to+' '+v.substr(4);
else if(re&&pn=='DD')n.nodeValue=v.replace(re,function(m){return w[m]})}
var e=d.querySelectorAll('[data-k]');for(i=0;i<e.length;i++){s=k[e[i].getAttribute('data-k')];if(s)e[i].textContent=s}
if(F&&k[F.r]){var b=d.createElement('div');b.className='msg D';b.id='kcerr';
b.textContent=k[F.r].replace('{ssid}',F.s);var r=d.querySelector('.wrap')||d.body;r.insertBefore(b,r.firstChild)}}
)JS";

static void buildPortalHtml() {
  const int lang = cfg.lang < LANG_COUNT ? cfg.lang : LANG_DEFAULT;
  String m = "<form action='/lang' method='get' style='margin-bottom:6px'>"
             "<label for='kl' style='font-weight:bold'>&#127760; ";
  m += PORTAL_LANG_LABEL;
  m += "</label><select id='kl' name='l' onchange='this.form.submit()' "
       "style='font-size:1.2rem;padding:10px;margin:8px 0'>";
  for (int i = 0; i < LANG_COUNT; i++) {
    m += "<option value='";
    m += LANG_CODES[i];
    m += i == lang ? "' selected>" : "'>";
    m += htmlEsc(LANG_NAMES[i]);
    m += "</option>";
  }
  m += "</select><noscript><button>OK</button></noscript></form>"
       "<p data-k='intro'>Choose the clock's language, then set up Wi-Fi.</p>";
  g_portalMenu = m;

  String h = "<script>";
  h += FPSTR(PORTAL_JS);
  h += "document.addEventListener('DOMContentLoaded',function(){kcTr(";
  h += PORTAL_I18N[lang];
  h += ",";
  if (g_lastFail == WF_NONE) h += "null";
  else {
    h += "{\"r\":\"";
    h += g_lastFail == WF_PASSWORD ? "err_password" : g_lastFail == WF_NOT_FOUND ? "err_not_found" : "err_timeout";
    h += "\",\"s\":";
    h += jsonStr(g_failSsid);
    h += "}";
  }
  h += ")})</script>";
  g_portalHead = h;
  wm.setCustomHeadElement(g_portalHead.c_str());
  wm.setCustomMenuHTML(g_portalMenu.c_str());
}

// GET /lang?l=<code> from the picker on the first portal page.
static void handlePortalLang() {
  int i = langIndex(wm.server->arg("l").c_str());
  if (i >= 0 && i != cfg.lang) {
    configSetLanguage(cfg, i);
    configSave();
    g_langChanged = true;
    Serial.printf("portal: language %s\n", LANG_CODES[i]);
  }
  buildPortalHtml();
  wm.server->sendHeader("Location", "/", true);
  wm.server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  wm.server->send(302, "text/plain", "");
}

static void showSetupScreen() {
  screenSetBrightnessPct(80);
  screenMessage(T(S_SETUP_TITLE), T(S_SETUP_CONNECT), AP_NAME, "192.168.4.1");
}

static void showFailScreen(WifiFail f, const String &ssid, bool portalOpen) {
  screenSetBrightnessPct(80);
  if (portalOpen) screenMessage(T(S_WIFI_FAILED), T(failText(f)), T(S_RETRY_VIA), AP_NAME, 0xFF8A80);
  else screenMessage(T(S_WIFI_FAILED), T(failText(f)), ssid.c_str(), nullptr, 0xFF8A80);
}

static void onPortal(WiFiManager *) {
  applyTxPower("portal");
  if (g_lastFail != WF_NONE) showFailScreen(g_lastFail, g_failSsid, true);
  else showSetupScreen();
}

static void onPortalSave() {  // portal "Save" pressed; WiFiManager calls WiFi.begin() right after
  applyTxPower("portal save");
  g_portalSsid = wm.server->arg("s");
  if (g_portalSsid.isEmpty()) g_portalSsid = wm.getWiFiSSID(true);  // password-only change
  g_portalSave = true;
  screenSetBrightnessPct(80);
  screenMessage(T(S_CONNECTING), g_portalSsid.c_str());
}

// Waits for the connection that WiFi.begin() / the portal started, with a seconds counter on
// screen. Gives up after timeoutMs, or earlier when the failure is clear (wrong password,
// network not found). With the portal open it keeps serving it (and restarts the wait if
// the user saves again).
static bool waitConnect(String ssid, uint32_t timeoutMs, bool portal, WifiFail &fail) {
  resetFailCounters();
  uint32_t t0 = millis(), failSince = 0;
  int lastSec = -1, last = -1;
  for (;;) {
    if (portal) {
      wm.process();
      if (g_portalSave) {  // saved again while waiting: start over with the new network
        g_portalSave = false;
        ssid = g_portalSsid;
        resetFailCounters();
        t0 = millis(); failSince = 0; lastSec = -1;
      }
    }
    wl_status_t st = WiFi.status();
    if (st != last) {
      Serial.printf("wifi: status %d %s (%lu ms)\n", st, wlStatusName(st), (unsigned long)(millis() - t0));
      last = st;
    }
    if (st == WL_CONNECTED) { fail = WF_NONE; return true; }
    bool definite = g_authFails >= 2 || g_noApFails >= 2;
    if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || g_authFails || g_noApFails) {
      if (!failSince) failSince = millis();
      else if (millis() - failSince > 4000) definite = true;  // no recovery for 4 s after a failure
    }
    if (definite || millis() - t0 >= timeoutMs) break;
    int sec = (millis() - t0) / 1000;
    if (g_langChanged) { g_langChanged = false; lastSec = -1; }
    if (sec != lastSec) {
      lastSec = sec;
      char buf[16];
      snprintf(buf, sizeof(buf), "%d s", sec);
      screenMessage(T(S_CONNECTING), ssid.c_str(), buf);
    }
    delay(portal ? 5 : 100);
  }
  fail = g_authFails ? WF_PASSWORD : g_noApFails ? WF_NOT_FOUND : WF_TIMEOUT;
  Serial.printf("wifi: connect to \"%s\" FAILED (%s), status %s\n", ssid.c_str(),
                fail == WF_PASSWORD ? "wrong password" : fail == WF_NOT_FOUND ? "network not found" : "timeout",
                wlStatusName(WiFi.status()));
  return false;
}

// Connect with the stored credentials. Returns true when connected.
static bool connectStored(uint32_t timeoutMs, WifiFail &fail) {
  String ssid = wm.getWiFiSSID(true), pass = wm.getWiFiPass(true);
  fail = WF_NONE;
  if (ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  applyTxPower("connect");
  Serial.printf("wifi: connecting to \"%s\"\n", ssid.c_str());
  resetFailCounters();
  WiFi.begin(ssid.c_str(), pass.c_str());
  applyTxPower("after begin");
  if (waitConnect(ssid, timeoutMs, false, fail)) return true;
  g_failSsid = ssid;
  return false;
}

// Runs the setup portal until a new network connects (true) or nobody used it for 5 min (false).
static bool runPortal() {
  WiFi.mode(WIFI_STA);  // WiFiManager restores this mode when the portal closes
  buildPortalHtml();
  g_portalSave = false;
  wm.startConfigPortal(AP_NAME);  // non-blocking: returns at once, served by wm.process()
  uint32_t screenAt = millis();
  bool showingFail = g_lastFail != WF_NONE;
  for (;;) {
    wm.process();
    if (!wm.getConfigPortalActive()) {
      Serial.println("wifi: portal timeout");
      return false;
    }
    if (g_portalSave) {
      g_portalSave = false;
      WifiFail f;
      if (waitConnect(g_portalSsid, 22000, true, f)) {
        Serial.println("wifi: connected, closing portal");
        g_lastFail = WF_NONE;
        wm.stopConfigPortal();
        return true;
      }
      g_lastFail = f;
      g_failSsid = g_portalSsid;
      WiFi.disconnect(false, false);  // stop retrying in the background so the setup AP stays stable
      buildPortalHtml();
      showFailScreen(f, g_failSsid, true);
      showingFail = true;
      screenAt = millis();
    }
    if (g_langChanged) {  // language picked in the portal: same screen, new language
      g_langChanged = false;
      if (showingFail) showFailScreen(g_lastFail, g_failSsid, true);
      else showSetupScreen();
    }
    // after a failure: alternate between the error and the setup instructions
    if (g_lastFail != WF_NONE && millis() - screenAt > 5000) {
      screenAt = millis();
      showingFail = !showingFail;
      if (showingFail) showFailScreen(g_lastFail, g_failSsid, true);
      else showSetupScreen();
    }
    delay(5);
  }
}

static void onNetworkUp() {
  applyTxPower("network up");
  MDNS.end();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  // Re-apply TZ + NTP servers so SNTP resumes after a long outage.
  configTzTime(TZ_INFO, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  Serial.printf("wifi: mDNS http://%s.local  IP %s\n", HOSTNAME, WiFi.localIP().toString().c_str());
}

static void setupWifi() {
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  applyTxPower("boot");
  WiFi.setHostname(HOSTNAME);
  wm.setAPCallback(onPortal);
  wm.setPreSaveConfigCallback(onPortalSave);
  wm.setWebServerCallback([]() { wm.server->on("/lang", HTTP_GET, handlePortalLang); });
  buildPortalHtml();
  wm.setConfigPortalBlocking(false);
  wm.setBreakAfterConfig(false);   // a failed attempt keeps the portal open for a retry
  wm.setSaveConnectTimeout(1);     // don't block in WiFiManager: waitConnect() watches the attempt
  wm.setConfigPortalTimeout(300);  // 5 min without use, then continue offline and retry later
  wm.setConnectTimeout(20);
  wm.setTitle("Kido Clock");
  wm.setHostname(HOSTNAME);
  // "custom" = the language picker (g_portalMenu) first; no "update": no OTA partition
  std::vector<const char *> menu = {"custom", "wifi", "info", "exit"};
  wm.setMenu(menu);
  screenSetBrightnessPct(60);

  WifiFail fail;
  if (!connectStored(25000, fail)) {
    if (fail != WF_NONE) {  // stored network did not work: show why, then open the portal
      g_lastFail = fail;
      Serial.println("wifi: FAILED - check 2.4 GHz network / password; reopening setup portal");
      showFailScreen(fail, g_failSsid, false);
      delay(6000);
    }
    if (!runPortal()) {  // nobody used the portal
      Serial.println("wifi: running offline");
      if (wm.getWiFiIsSaved()) {
        WiFi.mode(WIFI_STA);
        applyTxPower("offline");
        WiFi.begin();  // keep retrying in the background (auto-reconnect)
      }
    }
  }
  WiFi.setAutoReconnect(true);
  if (WiFi.status() == WL_CONNECTED) {
    applyTxPower("connected");
    lastOnline = millis();
  }
}

// ---------------- brightness ----------------
// Returns the target brightness in %. On the round GC9A01 this is color scaling,
// so it is quantized (LDR: 5 % steps) to avoid constant full redraws.
static float targetBrightness(const Slot &s) {
  float pct = s.bright * cfg.maxBright / 100.0f;
  if (cfg.autoDim && ldrAvg >= 0) {
    float f = 0.2f + 0.8f * min(1.0f, ldrAvg / 2500.0f);  // dark room -> 20 % of the set value
    pct = roundf(pct * f / 5.0f) * 5.0f;
  }
  if (millis() < boostUntil) pct = 100.0f;  // button boost: full brightness, visible in daylight
  return pct;
}

// ---------------- button ----------------
// Returns true once per press (on release). During normal use a press only lights up the
// screen (kids hold buttons for a long time); see loop() for the screensaver.
static bool pollButton() {
  static bool was = false;
  static uint32_t downAt = 0;
  bool down = digitalRead(PIN_BUTTON) == LOW;
  if (down && !was) downAt = millis();
  bool pressed = !down && was && millis() - downAt > 40;
  was = down;
  return pressed;
}

// Wi-Fi reset: hold the button while powering on, then keep holding through a 5 s countdown.
// Releasing it earlier cancels and the clock starts normally.
static void bootResetCheck() {
  if (digitalRead(PIN_BUTTON) != LOW) return;
  delay(30);
  if (digitalRead(PIN_BUTTON) != LOW) return;  // bounce / noise
  Serial.println("button held at power-on: Wi-Fi reset countdown");
  screenSetBrightnessPct(80);
  const uint32_t t0 = millis();
  uint32_t upSince = 0;
  int shown = -1;
  while (millis() - t0 < 5000) {
    bool down = digitalRead(PIN_BUTTON) == LOW;
    if (down) upSince = 0;
    else if (!upSince) upSince = millis();
    else if (millis() - upSince > 80) {  // released (debounced): cancel
      Serial.println("Wi-Fi reset cancelled");
      screenMessage(T(S_RESET_CANCEL), "");
      delay(1200);
      return;
    }
    int left = 5 - (int)((millis() - t0) / 1000);
    if (left != shown) {
      shown = left;
      char buf[12];
      snprintf(buf, sizeof(buf), "%d", left);
      screenMessage(T(S_RESET_HOLD), T(S_RESET_HOLD2), buf, nullptr, 0xFFD27F);
    }
    delay(10);
  }
  Serial.println("Wi-Fi reset: forgetting the network");
  screenMessage(T(S_WIFI_RESET), T(S_FORGETTING));
  wm.resetSettings();  // setupWifi() then finds no network and opens the setup portal
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Kido Clock %s\n", KIDO_VERSION);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  analogReadResolution(12);
  screenInit();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  if (!configLoad()) {
    Serial.println("config: defaults");
    configSave();
  }
  applyDisplay();
  bootResetCheck();
  setupWifi();
  configTzTime(TZ_INFO, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  setupWeb();
  Serial.printf("ready: http://%s.local  http://%s\n", HOSTNAME, WiFi.localIP().toString().c_str());
}

// ---------------- eyes screensaver ----------------
static void eyesStop() {
  if (!eyesOn) return;
  eyesOn = false;
  redrawAll = true;
  shownSlot = -2;
}

static void eyesFrame() {
  uint32_t now = millis();
  float dt = (now - eyesLastFrame) / 1000.0f;
  eyesLastFrame = now;
  EyeParams p;
  eyesAnim.step(dt * eyesCur.speed / 100.0f, p);
  EyeGeom g;
  g.cy = eyesCur.clock ? EYES_CY_WITH_CLOCK : 120;
  g.roundShape = eyesCur.roundShape;
  screenEyesFrame(p, g, eyesCur.color, eyesCur.bg);
  eyesFrames++;
  if (now - eyesStatAt >= 10000) {
    Serial.printf("eyes: %.1f fps\n", eyesFrames * 1000.0f / (now - eyesStatAt));
    eyesFrames = 0;
    eyesStatAt = now;
  }
}

// Called from the 250 ms tick while the screensaver should be on.
static void eyesTick(const EyesCfg &e, float pct, bool night, const char *clock, int minute) {
  bool dimChanged = screenSetBrightnessPct(pct);
  bool restart = !eyesOn || night != eyesNight;
  if (restart) {
    eyesAnim.reset(night, esp_random());
    eyesNight = night;
    eyesLastFrame = eyesStatAt = millis();
    eyesFrames = 0;
    Serial.printf("eyes: on (%s)\n", night ? "night" : "day");
  }
  if (!eyesOn || dimChanged) {
    eyesCur = e;
    screenEyesBegin(e.bg);
    eyesMinute = -1;
  }
  eyesOn = true;
  if (e.clock && minute != eyesMinute) {
    screenEyesClock(clock, EYES_CLOCK_Y, e.color, e.bg);
    eyesMinute = minute;
  }
}

void loop() {
  server.handleClient();
  if (pollButton()) {
    lastActivity = millis();
    boostUntil = millis() + 15000;  // one press: leave screensaver (if any) and restore brightness
    if (eyesOn) {
      eyesPreviewUntil = 0;
      eyesStop();
    }
    if (saverClockOn) {
      saverClockOn = false;
      redrawAll = true;
      shownSlot = -2;
    }
  }
  if (eyesOn && millis() - eyesLastFrame >= 33) eyesFrame();  // ~30 fps

  static uint32_t lastTick = 0;
  if (millis() - lastTick < 250) return;
  lastTick = millis();

  int raw = analogRead(PIN_LDR);
  ldrAvg = ldrAvg < 0 ? raw : ldrAvg * 0.9f + raw * 0.1f;

  // ---- Wi-Fi keep-alive (app-level; Arduino autoReconnect alone can stall after a long outage) ----
  bool wifiOnline = (WiFi.status() == WL_CONNECTED);
  if (wifiOnline) {
    lastOnline = millis();
    wifiReconnectN = 0;
    if (g_gotIp) {
      g_gotIp = false;
      onNetworkUp();
    }
  } else if (wm.getWiFiIsSaved() && millis() - lastOnline >= 15000UL && millis() - lastReconnectAt >= 30000UL) {
    lastReconnectAt = millis();
    wifiReconnectN++;
    applyTxPower("reconnect");
    if (wifiReconnectN % 3 == 0) {
      String ssid = wm.getWiFiSSID(true), pass = wm.getWiFiPass(true);
      Serial.printf("wifi: app begin(\"%s\") (attempt %u)\n", ssid.c_str(), (unsigned)wifiReconnectN);
      WiFi.mode(WIFI_STA);
      WiFi.setHostname(HOSTNAME);
      WiFi.begin(ssid.c_str(), pass.c_str());
      applyTxPower("after reconnect begin");
    } else {
      Serial.printf("wifi: app reconnect (attempt %u)\n", (unsigned)wifiReconnectN);
      WiFi.reconnect();
    }
  }
  // Debounced no-Wi-Fi badge on the normal clock view (not on screensavers).
  bool wantBadge = !wifiOnline && (millis() - lastOnline >= 20000UL);
  if (wantBadge != wifiBadgeOn) {
    wifiBadgeOn = wantBadge;
    if (!eyesOn && !saverClockOn) redrawAll = true;
  }

  struct tm t;
  if (!timeValid(t)) {
    eyesOn = false;
    saverClockOn = false;
    if (shownSlot != -1) {
      screenSetBrightnessPct(50);
      screenMessage(T(S_CLOCK_SETTING), wifiOnline ? T(S_CLOCK_FETCHING) : T(S_NO_WIFI),
                    wifiOnline ? nullptr : T(S_HOLD_RESET));
      shownSlot = -1;
    }
    // never had time and no network for 10 min: reboot to retry / reopen the setup portal
    if (millis() - lastOnline > 600000UL) ESP.restart();
    return;
  }

  char clock[6];
  snprintf(clock, sizeof(clock), "%02d:%02d", t.tm_hour, t.tm_min);

  const Slot *s = nullptr;
  int idx;
  if (previewUntil && millis() < previewUntil) {
    s = &previewSlot;
    idx = 1000;
  } else {
    if (previewUntil) { previewUntil = 0; redrawAll = true; }
    idx = activeSlot(cfg, wdayMon0(t), t.tm_hour * 60 + t.tm_min);
    if (idx >= 0) s = &cfg.slots[idx];
  }

  if (!s) {
    eyesOn = false;
    saverClockOn = false;
    if (shownSlot != -3) {
      screenSetBrightnessPct(30);
      screenMessage("Kido Clock", T(S_NO_SLOTS), "kido-clock.local");
      shownSlot = -3;
    }
    return;
  }

  // Screensaver after idle (or eyes preview from the web UI). saver: off / eyes / clock.
  bool eyesPreview = eyesPreviewUntil && millis() < eyesPreviewUntil;
  if (eyesPreviewUntil && !eyesPreview) { eyesPreviewUntil = 0; lastActivity = millis(); }
  bool night = slotIsNight(*s);
  bool idle = idx != 1000 && millis() - lastActivity >= cfg.eyes.idleMin * 60000UL;
  bool wantEyes = eyesPreview ||
                  (cfg.saver == SAVER_EYES && idle && !(night && cfg.eyes.night == EYES_NIGHT_OFF));
  bool wantSaverClock = !eyesPreview && cfg.saver == SAVER_CLOCK && idle;

  if (wantEyes) {
    if (saverClockOn) { saverClockOn = false; shownSlot = -2; }
    eyesTick(eyesPreview ? eyesPreviewCfg : cfg.eyes, eyesPreview ? (float)cfg.maxBright : targetBrightness(*s),
             night && !eyesPreview, clock, t.tm_min);
    return;
  }
  eyesStop();

  if (wantSaverClock) {
    float pct = targetBrightness(*s);
    bool dimChanged = screenSetBrightnessPct(pct);
    if (!saverClockOn || dimChanged || t.tm_min != saverClockMinute || redrawAll) {
      screenDrawSaverClock(s->bg, s->tx, clock, cfg.clockStyle, t.tm_hour, t.tm_min);
      saverClockOn = true;
      saverClockMinute = t.tm_min;
      redrawAll = false;
      shownSlot = -2;
    }
    return;
  }
  if (saverClockOn) {
    saverClockOn = false;
    redrawAll = true;
    shownSlot = -2;
  }

  if (screenSetBrightnessPct(targetBrightness(*s))) redrawAll = true;  // color scaling changed

  if (idx != shownSlot || redrawAll) {
    screenDrawSlot(*s, cfg.showLabel, cfg.showClock, clock, cfg.clockStyle, wifiBadgeOn);
    shownSlot = idx;
    shownMinute = t.tm_min;
    redrawAll = false;
  } else if (cfg.showClock && t.tm_min != shownMinute) {
    screenDrawClock(*s, cfg.showLabel, clock, cfg.clockStyle, wifiBadgeOn);
    shownMinute = t.tm_min;
  }
}
