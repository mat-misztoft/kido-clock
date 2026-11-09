"""Render docs/eyes.png (expressions) and docs/eyes.gif (animation) of the eyes
screensaver with tools/eyes.py, the Python port of src/eyes.cpp (same pixels).

Usage: python3 tools/render_eyes.py      (needs Pillow)
"""
import math, os, sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from eyes import EyeParams, EyesAnimator, eyes_rows, eyes_row, EYES_X0  # noqa: E402
from render_mockups import round_tile, ui_font, fit_text, F, scale_hex, VIVID  # noqa: E402
from bitmap_icons import vivid_rgb  # noqa: E402

DOCS = os.path.join(os.path.dirname(HERE), "..", "docs")
EYE, BG = "#4FD1FF", "#000000"   # defaults, see src/config.h
CLOCK_CY, CLOCK_Y = 108, 204      # with the clock shown under the eyes


def rgb(h):
    return tuple(int(h.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4))


def screen(p, eye=EYE, bg=BG, pct=100, round_shape=True, clock="07:12"):
    """Same as screenEyesFrame() in screen.cpp: 16-level ramp bg -> eye colour, then dimming."""
    eye = "#%02X%02X%02X" % vivid_rgb(rgb(eye), VIVID)
    e, b = rgb(scale_hex(eye, pct)), rgb(scale_hex(bg, pct))
    lut = [tuple(b[c] + int((e[c] - b[c]) * i / 15) for c in range(3)) for i in range(16)]
    img = Image.new("RGB", (240, 240), b)
    cy = CLOCK_CY if clock else 120
    y0, y1 = eyes_rows(p, 120, cy, round_shape)
    px = img.load()
    for y in range(max(0, y0), min(240, y1)):
        for x, v in enumerate(eyes_row(p, y, 120, cy, round_shape)):
            if v:
                px[EYES_X0 + x, y] = lut[v]
    if clock:
        fit_text(img, F["clock"], clock, CLOCK_Y, 15, e)
    return img


EXPRESSIONS = [
    (EyeParams(), {}, "neutral"),
    (EyeParams(lookX=-0.9, lookY=0.1), {}, "look left"),
    (EyeParams(lookX=0.7, lookY=-0.6), {}, "look up-right"),
    (EyeParams(openL=0.25, openR=0.25), {}, "blink"),
    (EyeParams(happy=1.0), {}, "happy"),
    (EyeParams(scale=1.22, lookY=-0.3), {}, "surprised"),
    (EyeParams(openL=0.05), {}, "wink"),
    (EyeParams(lid=0.45), {}, "sleepy"),
    (EyeParams(), {"round_shape": False}, "rounded-rectangle shape"),
    (EyeParams(happy=1.0), {"round_shape": False, "eye": "#FFB3D9", "bg": "#2A0E3A"}, "custom colours"),
    (EyeParams(lid=0.55, lookY=0.2), {"pct": 20, "clock": "21:40"}, "night: sleepy, 20%"),
    (EyeParams(lookX=0.4), {"clock": None}, "without the clock"),
]


def sheet(out):
    cols, gap, cap, tw = 4, 26, 34, 240 + 28
    rows = math.ceil(len(EXPRESSIONS) / cols)
    im = Image.new("RGB", (cols * (tw + gap) + gap, 60 + rows * (tw + cap + gap)), "#F4F1EA")
    d = ImageDraw.Draw(im)
    d.text((gap, 18), "Kido Clock - eyes screensaver (drawn in code, round 240x240)", font=ui_font(17, True), fill="#333")
    for i, (p, kw, title) in enumerate(EXPRESSIONS):
        r, c = divmod(i, cols)
        x, y = gap + c * (tw + gap), 60 + r * (tw + cap + gap)
        t = round_tile(screen(p, **kw))
        im.paste(t, (x, y), t)
        d.text((x + tw / 2, y + tw + 16), title, font=ui_font(14), fill="#333", anchor="mm")
    im.save(out, optimize=True)
    print("wrote", out, im.size)


def gif(out, seconds=14, fps=20, seed=7):
    a = EyesAnimator()
    a.reset(False, seed)
    frames = []
    for _ in range(seconds * fps):
        t = round_tile(screen(a.step(1.0 / fps)))
        bgc = Image.new("RGB", t.size, "#F4F1EA")
        bgc.paste(t, (0, 0), t)
        frames.append(bgc.convert("P", palette=Image.ADAPTIVE, colors=32))
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=1000 // fps, loop=0, optimize=True)
    print("wrote", out, len(frames), "frames")


if __name__ == "__main__":
    sheet(os.path.join(DOCS, "eyes.png"))
    gif(os.path.join(DOCS, "eyes.gif"))
