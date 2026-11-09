"""Download the Fluent Emoji 3D colour icons listed in icons_def.ICONS and convert
them to the on-device format: firmware/assets/icons3d/<id>.qoi

Source: https://github.com/microsoft/fluentui-emoji (MIT, see THIRD_PARTY_NOTICES.md).
Conversion: 256 px PNG -> trimmed to the visible pixels (alpha > ALPHA_MIN) -> scaled so the
longer side is ICON_FILL x BITMAP_SIZE (Lanczos, premultiplied alpha, aspect kept) -> centred
on its bounding box (so every icon sits in the middle, whatever the artwork's own padding)
-> colours rounded to RGB565 (what the panel shows anyway; makes QOI ~25 % smaller) -> QOI.
Downloaded PNGs are cached in ~/.cache/kido-clock/fluent/.

Usage: python3 tools/fetch_icons.py [--force]     (needs Pillow and internet)
"""
import io, os, sys, urllib.parse, urllib.request
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from icons_def import ICONS, BITMAP_DIR, BITMAP_SIZE, ICON_FILL  # noqa: E402
from bitmap_icons import qoi_encode, qoi_decode  # noqa: E402

BASE = "https://raw.githubusercontent.com/microsoft/fluentui-emoji/main/assets/"
CACHE = os.path.join(os.path.expanduser("~"), ".cache", "kido-clock", "fluent")
ALPHA_MIN = 8  # alpha below this counts as empty when trimming


def to565(px):
    o = bytearray(px)
    for i in range(0, len(o), 4):
        if o[i + 3] == 0:
            o[i:i + 4] = b"\0\0\0\0"
            continue
        r, g, b = min(31, (o[i] + 4) >> 3), min(63, (o[i + 1] + 2) >> 2), min(31, (o[i + 2] + 4) >> 3)
        o[i], o[i + 1], o[i + 2] = (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)
    return bytes(o)


def convert(png_bytes, size=BITMAP_SIZE):
    im = Image.open(io.BytesIO(png_bytes)).convert("RGBA")
    box = im.getchannel("A").point(lambda v: 255 if v >= ALPHA_MIN else 0).getbbox()
    if box:
        im = im.crop(box)
    w, h = im.size
    k = ICON_FILL * size / max(w, h)
    nw, nh = max(1, round(w * k)), max(1, round(h * k))
    im = im.convert("RGBa").resize((nw, nh), Image.LANCZOS).convert("RGBA")
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2))
    px = to565(canvas.tobytes())
    q = qoi_encode(size, size, px)
    assert qoi_decode(q)[2] == px, "QOI round trip failed"
    return q


def source_png(d):
    name = d["fluent"]
    rel = "%s/%s" % (name, d.get("file") or "3D/%s_3d.png" % name.lower().replace(" ", "_"))
    cached = os.path.join(CACHE, rel.replace("/", "__"))
    if os.path.exists(cached):
        return open(cached, "rb").read()
    data = urllib.request.urlopen(BASE + urllib.parse.quote(rel), timeout=30).read()
    os.makedirs(CACHE, exist_ok=True)
    with open(cached, "wb") as f:
        f.write(data)
    return data


def main():
    force = "--force" in sys.argv
    out_dir = os.path.join(os.path.dirname(HERE), BITMAP_DIR)
    os.makedirs(out_dir, exist_ok=True)
    total = 0
    for key, d in ICONS.items():
        dst = os.path.join(out_dir, key + ".qoi")
        if os.path.exists(dst) and not force:
            total += os.path.getsize(dst)
            continue
        name = d["fluent"]
        q = convert(source_png(d))
        with open(dst, "wb") as f:
            f.write(q)
        total += len(q)
        print("%-14s %6d bytes  <- %s" % (key, len(q), name))
    known = {k + ".qoi" for k in ICONS}
    for f in os.listdir(out_dir):
        if f.endswith(".qoi") and f not in known:
            print("note: %s is not in ICONS (not compiled in)" % f)
    print("%d icons, %d bytes total" % (len(ICONS), total))


if __name__ == "__main__":
    main()
