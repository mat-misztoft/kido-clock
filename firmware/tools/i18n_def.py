"""Loads all languages from firmware/lang/<code>.json (one file per language).

Each file: {"name": ..., "alphabet": extra letters for the on-screen font,
            "device": {on-screen strings}, "web": {web UI strings},
            "portal": {Wi-Fi setup portal strings}}.
Missing keys fall back to English (en.json). The generator (gen_assets.py) picks up
every file automatically and adds its letters to the on-screen font.
"""
import glob, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
LANG_DIR = os.path.join(os.path.dirname(HERE), "lang")
DEFAULT_LANG = "en"
FALLBACK_LANG = "en"


def _load():
    langs = {}
    for p in sorted(glob.glob(os.path.join(LANG_DIR, "*.json"))):
        code = os.path.splitext(os.path.basename(p))[0]
        with open(p, encoding="utf-8") as f:
            langs[code] = json.load(f)
    order = [DEFAULT_LANG] + sorted(c for c in langs if c != DEFAULT_LANG)
    return {c: langs[c] for c in order}


LANGS = _load()

# The Wi-Fi setup portal is WiFiManager's own (English) HTML. A small script in the page
# head (see buildPortalHtml() in src/main.cpp) swaps these exact English texts for the
# "portal" strings of the chosen language. Keys not listed here are Kido Clock's own
# portal strings (language picker, error banner, ...).
PORTAL_WM = {
    "menu_wifi": "Configure WiFi", "menu_info": "Info", "menu_exit": "Exit", "back": "Back",
    "ssid": "SSID", "password": "Password", "show_password": "Show Password", "save": "Save",
    "refresh": "Refresh", "no_networks": "No networks found. Refresh to scan again.",
    "saving": "Saving Credentials", "trying": "Trying to connect ESP to network.",
    "if_fails": "If it fails reconnect to AP to try again", "exiting": "Exiting",
    "not_connected": "Not connected", "connected": "Connected", "no_ap_set": "No AP set",
    "auth_failure": "Authentication failure", "ap_not_found": "AP not found",
    "could_not_connect": "Could not connect", "title_wifi": "Config ESP", "title_saved": "Credentials saved",
    "info_uptime": "Uptime", "info_chipid": "Chip ID", "info_chiprev": "Chip rev", "info_flash": "Flash size",
    "info_realflash": "Real flash size", "info_cpu": "CPU frequency", "info_heap": "Memory - Free heap",
    "info_sketch": "Memory - Sketch size", "info_used_total": "Used / Total bytes",
    "info_reset": "Last reset reason", "info_temp": "Temperature", "info_sta_ssid": "Station SSID",
    "info_sta_ip": "Station IP", "info_sta_gw": "Station gateway", "info_sta_sub": "Station subnet",
    "info_dns": "DNS Server", "info_host": "Hostname", "info_sta_mac": "Station MAC",
    "info_ap_ssid": "Access point SSID", "info_ap_ip": "Access point IP", "info_ap_mac": "Access point MAC",
    "info_ap_host": "Access point hostname",
}
# Single words inside longer WiFiManager texts (e.g. "12 mins 5 secs"), replaced word by word.
PORTAL_WORDS = {"w_bytes": "bytes", "w_mins": "mins", "w_secs": "secs", "w_available": "available",
                "w_yes": "Yes", "w_no": "No"}


def resolved(lang, section):
    """Strings of one language with English fallback for missing keys."""
    out = dict(LANGS[FALLBACK_LANG][section])
    out.update(LANGS[lang][section])
    return out


def font_chars():
    """Union of every character any shipped language can put on the screen."""
    chars = set(chr(c) for c in range(0x20, 0x7F))
    for L in LANGS.values():
        chars |= set(L.get("alphabet", ""))
        for v in L["device"].values():
            chars |= set(v)
    return "".join(sorted(chars))


def check():
    """Warn about missing keys (they fall back to English)."""
    ref = LANGS[FALLBACK_LANG]
    for code, L in LANGS.items():
        for sec in ("device", "web", "portal"):
            miss = set(ref[sec]) - set(L.get(sec, {}))
            if miss:
                print("i18n: %s.json missing %s keys (English used): %s" % (code, sec, ", ".join(sorted(miss))))
