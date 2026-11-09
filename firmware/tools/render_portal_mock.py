"""Render docs/portal.png (first portal page, English) and docs/portal_pl.png (Wi-Fi form,
Polish) from WiFiManager's real HTML strings plus the clock's own portal script and
language picker, exactly as src/main.cpp builds them.

Needs a finished build (WiFiManager in .pio/libdeps/esp32c3), Pillow and Google Chrome:
    pio run -e esp32c3 && python3 tools/render_portal_mock.py
"""
import html, json, os, re, subprocess, sys, tempfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
FW = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from i18n_def import LANGS  # noqa: E402

WM = os.path.join(FW, ".pio", "libdeps", "esp32c3", "WiFiManager", "wm_strings_en.h")
SRC = "\n".join(l for l in open(WM, encoding="utf-8").read().splitlines() if not l.lstrip().startswith("//"))
MAIN = open(os.path.join(FW, "src", "main.cpp"), encoding="utf-8").read()
I18N_H = open(os.path.join(FW, "src", "i18n_gen.h"), encoding="utf-8").read()


def S(name):
    m = re.search(r'const char %s\[\]\s+PROGMEM = ((?:\s*"(?:[^"\\]|\\.)*"\s*)+);' % name, SRC)
    return "".join(p.encode().decode("unicode_escape") for p in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))


def MENU(i):
    m = re.search(r"HTTP_PORTAL_MENU\[\] PROGMEM = \{(.*?)\};", SRC, re.S)
    return re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))[i].replace("\\n", "\n")


PORTAL_JS = re.search(r'PORTAL_JS\[\] PROGMEM = R"JS\((.*?)\)JS";', MAIN, re.S).group(1)
PORTAL_I18N = re.findall(r'R"json\((.*?)\)json"', I18N_H)
LABEL = json.loads(re.search(r"PORTAL_LANG_LABEL\[\] = (\".*?\");", I18N_H).group(1))
CODES = list(LANGS)


def head(lang, fail):
    f = "null" if not fail else json.dumps({"r": fail[0], "s": fail[1]})
    return ("<script>" + PORTAL_JS + "document.addEventListener('DOMContentLoaded',function(){kcTr(" +
            PORTAL_I18N[CODES.index(lang)] + "," + f + ")})</script>")


def menu(lang):
    m = ("<form action='/lang' method='get' style='margin-bottom:6px'><label for='kl' style='font-weight:bold'>&#127760; "
         + LABEL + "</label><select id='kl' name='l' onchange='this.form.submit()' "
         "style='font-size:1.2rem;padding:10px;margin:8px 0'>")
    for c in CODES:
        m += "<option value='%s'%s>%s</option>" % (c, " selected" if c == lang else "", html.escape(LANGS[c]["name"]))
    return m + "</select><noscript><button>OK</button></noscript></form><p data-k='intro'>Choose the clock's language, then set up Wi-Fi.</p>"


def page(title, lang, fail, body):
    return (S("HTTP_HEAD_START").replace("{v}", title) + S("HTTP_SCRIPT") + S("HTTP_STYLE") + head(lang, fail)
            + S("HTTP_HEAD_END").replace("{c}", "") + body + S("HTTP_END"))


STATUS = (S("HTTP_STATUS_OFF").replace("{c}", "D").replace("{v}", "HomeNetwork")
          .replace("{r}", S("HTTP_STATUS_OFFFAIL")))


def root(lang, fail):
    body = (S("HTTP_ROOT_MAIN").replace("{t}", "Kido Clock").replace("{v}", "Kido-Clock-Setup")
            + menu(lang) + MENU(0) + MENU(2) + MENU(6) + STATUS)
    return page("Kido Clock", lang, fail, body)


def wifi(lang, fail):
    items = ""
    for n, q in (("HomeNetwork", 82), ("HomeNetwork-Guest", 64), ("Neighbour", 30)):
        qi = S("HTTP_ITEM_QI").replace("{r}", str(q)).replace("{q}", str(round(q * 4 / 100))).replace("{i}", "l").replace("{h}", "")
        items += S("HTTP_ITEM").replace("{V}", n).replace("{v}", n).replace("{qi}", qi).replace("{qp}", "")
    body = (items + "<br/>" + S("HTTP_FORM_START").replace("{v}", "wifisave")
            + S("HTTP_FORM_WIFI").replace("{v}", "HomeNetwork").replace("{p}", "********")
            + S("HTTP_FORM_END") + S("HTTP_SCAN_LINK") + S("HTTP_BACKBTN"))
    return page("Config ESP", lang, fail, body)


def shot(doc, out, h):
    chrome = next((c for c in ("google-chrome", "chromium", "chromium-browser") if subprocess.run(
        ["which", c], capture_output=True).returncode == 0), None)
    if not chrome:
        raise SystemExit("Chrome/Chromium not found")
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "p.html")
        open(p, "w", encoding="utf-8").write(doc)
        png = os.path.join(d, "p.png")
        subprocess.run([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars",
                        "--window-size=400,%d" % (h + 200), "--virtual-time-budget=2000",
                        "--screenshot=" + png, "file://" + p], capture_output=True, timeout=90)
        Image.open(png).convert("RGB").crop((0, 0, 400, h)).save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    docs = os.path.join(FW, "..", "docs")
    fail = ("err_password", "HomeNetwork")
    shot(root("en", fail), os.path.join(docs, "portal.png"), 640)
    shot(wifi("pl", fail), os.path.join(docs, "portal_pl.png"), 600)
