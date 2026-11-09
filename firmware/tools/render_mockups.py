"""Render docs/screens.png, docs/vivid.png (what the round 1.28" GC9A01 240x240 shows) and docs/icons.png
(all icons), using the same icon data (icons_def.py, assets/icons3d/*.qoi), layouts,
colour-icon scaler (bitmap_icons.py) and on-device VLW fonts as the firmware.

Usage: python3 tools/render_mockups.py [screens.png] [icons.png]
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from icons_def import ICONS, LAYOUTS, FONTS, BITMAP_DIR, BITMAP_SIZE
from vlw import make_vlw, parse_vlw, draw_text, text_width
from bitmap_icons import qoi_decode, area_scale, blend_rgb, vivid_px

VIVID = 40  # default 'Vivid colours' setting (Config::vivid)
from i18n_def import resolved

FW = os.path.dirname(HERE)
ALL_DEFAULTS = {k: v["defaults"] for k, v in ICONS.items()}
_qoi_cache = {}


def bitmap_rgba(key):
    if key not in _qoi_cache:
        _qoi_cache[key] = qoi_decode(open(os.path.join(FW, BITMAP_DIR, key + ".qoi"), "rb").read())[2]
    return _qoi_cache[key]


def draw_bitmap(img, key, bg_hex, pct, ox, oy, size, vivid=None):
    """Exactly what screen.cpp does: box-filter, vivid boost, blend on the slot background, dim."""
    vivid = VIVID if vivid is None else vivid
    s = 0 if pct <= 0 else max(8, round(256 * (pct / 100) ** 1.2))
    bg = tuple(int(bg_hex.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4))
    rows = area_scale(BITMAP_SIZE, bitmap_rgba(key), size)
    px = img.load()
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            r, g, b = blend_rgb(vivid_px(p, vivid), bg)
            px[ox + x, oy + y] = (min(255, r) * s >> 8, min(255, g) * s >> 8, min(255, b) * s >> 8)

TTF = os.path.join(HERE, "fonts", "DejaVuSans-Bold.ttf")
F = {k: parse_vlw(make_vlw(TTF, v["px"], v["chars"])) for k, v in FONTS.items()}
W = H = 240
SS = 4  # supersampling for icons


def scale_hex(h, pct):
    """Same software dimming as firmware screen.cpp."""
    s = 0 if pct <= 0 else max(8, round(256 * (pct / 100) ** 1.2))
    v = int(h.lstrip("#"), 16)
    r, g, b = (v >> 16 & 255) * s >> 8, (v >> 8 & 255) * s >> 8, (v & 255) * s >> 8
    return "#%02X%02X%02X" % (r, g, b)


def chord(y, margin=14):
    dy = abs(y - 120)
    return 0 if dy >= 120 else int(2 * math.sqrt(120 * 120 - dy * dy)) - margin


def fit_text(img, f, txt, cy, half_h, rgb):
    avail = chord(cy - half_h if cy < 120 else cy + half_h)
    w = text_width(f, txt)
    draw_text(img, f, txt, 120, cy, rgb, scale=min(1, avail / w) if w else 1)


def render_screen(icon, colors=None, label=None, clock="06:45", pct=100, vivid=None):
    colors = colors or ALL_DEFAULTS[icon]
    c = {k: scale_hex(v, pct) for k, v in colors.items()}
    lay = LAYOUTS["both" if label and clock else "clock" if clock else "label" if label else "none"]
    ix, iy, isz = lay["icon"]
    img = Image.new("RGB", (W, H), c["bg"])
    draw_bitmap(img, icon, colors["bg"], pct, ix, iy, isz, vivid)
    tx = tuple(int(c["tx"][i:i + 2], 16) for i in (1, 3, 5))
    if label:
        fit_text(img, F["label"], label, lay["label_y"], 10, tx)
    if clock:
        fit_text(img, F["clock"], clock, lay["clock_y"], 15, tx)
    return img


def render_message(title, lines, title_rgb=(255, 255, 255)):
    """screenMessage() in screen.cpp: title + up to 3 lines on the dark setup background."""
    img = Image.new("RGB", (W, H), "#1B1640")
    ys = [70, 110, 142, 174] if len(lines) >= 3 else [84, 124, 156]
    fit_text(img, F["label"], title, ys[0], 10, title_rgb)
    for y, t in zip(ys[1:], lines):
        fit_text(img, F["label"], t, y, 10, (0xC9, 0xD6, 0xFF))
    return img


def round_tile(img):
    """Device look: circular glass in a dark bezel."""
    pad = 14
    D = W + 2 * pad
    bm = Image.new("L", (D * SS, D * SS), 0)
    ImageDraw.Draw(bm).ellipse([0, 0, D * SS - 1, D * SS - 1], fill=255)
    t = Image.new("RGBA", (D, D), (0, 0, 0, 0))
    t.paste(Image.new("RGB", (D, D), "#1d1d24"), (0, 0), bm.resize((D, D), Image.LANCZOS))
    m = Image.new("L", (W * SS, H * SS), 0)
    ImageDraw.Draw(m).ellipse([0, 0, W * SS - 1, H * SS - 1], fill=255)
    t.paste(img, (pad, pad), m.resize((W, H), Image.LANCZOS))
    return t


def ui_font(sz, bold=False):
    p = "/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if bold else "")
    return ImageFont.truetype(p if os.path.exists(p) else TTF, sz)


EN = resolved("en", "device")
PL = resolved("pl", "device")

# (render function, caption)
SCREENS = [
    (lambda: render_screen("moon3d", clock="05:58"), "moon3d - too early (default)"),
    (lambda: render_screen("sun3d", clock="06:45"), "sun3d - time to get up"),
    (lambda: render_screen("openbook3d", clock="18:30"), "openbook3d - story time"),
    (lambda: render_screen("bed3d", clock="19:30"), "bed3d - bedtime"),
    (lambda: render_screen("teddy3d", None, None, None), "teddy3d - no clock (%d px)" % LAYOUTS["none"]["icon"][2]),
    (lambda: render_screen("sun3d", None, EN["lbl_wake"], "06:45"), "label + clock (en, %d px)" % LAYOUTS["both"]["icon"][2]),
    (lambda: render_screen("moon3d", None, None, "02:10", 20), "night: brightness 20%"),
    (lambda: render_screen("owl3d", clock="21:40"), "owl3d - night owl"),
    (lambda: render_screen("bed3d", None, PL["lbl_bed"], "19:30"), "bed3d with label (pl)"),
    (lambda: render_message(EN["setup_title"], [EN["setup_connect"], "Kido-Clock-Setup", "192.168.4.1"]),
     "Wi-Fi setup (English)"),
    (lambda: render_message(EN["wifi_failed"], [EN["err_password"], EN["retry_via"], "Kido-Clock-Setup"], (0xFF, 0x8A, 0x80)),
     "Wi-Fi error, portal reopened"),
    (lambda: render_message(EN["reset_hold"], [EN["reset_hold2"], "3"], (0xFF, 0xD2, 0x7F)),
     "button held at power-on: Wi-Fi reset"),
]


def gallery(out):
    keys = list(ICONS)
    cols, t, gap, cap = 7, 120, 14, 22
    rows = math.ceil(len(keys) / cols)
    sheet = Image.new("RGB", (cols * (t + gap) + gap, 50 + rows * (t + cap + gap)), "#F4F1EA")
    d = ImageDraw.Draw(sheet)
    d.text((gap, 14), "Kido Clock icons: %d Fluent Emoji 3D colour icons (MIT, Microsoft)" % len(ICONS), font=ui_font(15, True), fill="#333")
    for i, k in enumerate(keys):
        r, c = divmod(i, cols)
        x, y = gap + c * (t + gap), 50 + r * (t + cap + gap)
        scr = render_screen(k, None, None, None).resize((t, t), Image.LANCZOS)
        m = Image.new("L", (t * 4, t * 4), 0)
        ImageDraw.Draw(m).ellipse([0, 0, t * 4 - 1, t * 4 - 1], fill=255)
        sheet.paste(scr, (x, y), m.resize((t, t), Image.LANCZOS))
        d.text((x + t / 2, y + t + 10), k, font=ui_font(12), fill="#333", anchor="mm")
    sheet.save(out)
    print("wrote", out, sheet.size)


def vivid_sheet(out, icons=("sun3d", "rainbow3d", "unicorn3d"), levels=(0, 50, 100)):
    """docs/vivid.png: the 'Vivid colours' setting (screen.cpp vivid()) at a few levels."""
    gap, cap, tw = 26, 34, W + 28
    sheet = Image.new("RGB", (len(levels) * (tw + gap) + gap, 60 + len(icons) * (tw + cap + gap)), "#F4F1EA")
    d = ImageDraw.Draw(sheet)
    d.text((gap, 18), "Vivid colours: %s (default %d%%)" % (" / ".join("%d%%" % v for v in levels), VIVID),
           font=ui_font(17, True), fill="#333")
    for r, k in enumerate(icons):
        for c, v in enumerate(levels):
            x = gap + c * (tw + gap); y = 60 + r * (tw + cap + gap)
            t = round_tile(render_screen(k, clock="06:45", vivid=v))
            sheet.paste(t, (x, y), t)
            d.text((x + tw / 2, y + tw + 16), "%s - vivid %d%%" % (k, v), font=ui_font(14), fill="#333", anchor="mm")
    sheet.save(out)
    print("wrote", out, sheet.size)


if __name__ == "__main__":
    docs = os.path.join(FW, "..", "docs")
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(docs, "screens.png")
    cols, gap, cap, tw = 4, 26, 34, W + 28
    rows = math.ceil(len(SCREENS) / cols)
    sheet = Image.new("RGB", (cols * (tw + gap) + gap, 60 + rows * (tw + cap + gap)), "#F4F1EA")
    d = ImageDraw.Draw(sheet)
    d.text((gap, 18), "Kido Clock - screen previews (round 1.28\" GC9A01 TFT, 240x240)", font=ui_font(17, True), fill="#333")
    for i, (fn, title) in enumerate(SCREENS):
        r, c = divmod(i, cols)
        x = gap + c * (tw + gap); y = 60 + r * (tw + cap + gap)
        t = round_tile(fn())
        sheet.paste(t, (x, y), t)
        d.text((x + tw / 2, y + tw + 16), title, font=ui_font(14), fill="#333", anchor="mm")
    sheet.save(out)
    print("wrote", out, sheet.size)
    gallery(sys.argv[2] if len(sys.argv) > 2 else os.path.join(docs, "icons.png"))
    vivid_sheet(sys.argv[3] if len(sys.argv) > 3 else os.path.join(docs, "vivid.png"))
