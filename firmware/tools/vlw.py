"""Tiny VLW (Processing / TFT_eSPI / LovyanGFX smooth font) writer and reader."""
import struct


def make_vlw(ttf, px, chars):
    """Build a LovyanGFX/TFT_eSPI compatible .vlw smooth font (8-bit alpha) in memory."""
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype(ttf, px)
    ascent, descent = font.getmetrics()
    heads, bitmaps = [], []
    for ch in sorted(set(chars), key=ord):
        adv = round(font.getlength(ch))
        if ch == " ":
            heads.append(struct.pack(">7i", 0x20, 0, 0, adv, 0, 0, 0)); continue
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(1, x1 - x0), max(1, y1 - y0)
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        heads.append(struct.pack(">7i", ord(ch), h, w, adv, -y0, x0, 0))
        bitmaps.append(img.tobytes())
    head = struct.pack(">6i", len(heads), 11, px, 0, ascent, descent)
    return head + b"".join(heads) + b"".join(bitmaps)


def parse_vlw(data):
    n, _ver, px, _z, ascent, descent = struct.unpack(">6i", data[:24])
    glyphs, ptr = {}, 24 + n * 28
    max_asc, max_desc = ascent, descent
    for i in range(n):
        u, h, w, adv, dy, dx, _ = struct.unpack(">7i", data[24 + i * 28: 52 + i * 28])
        glyphs[chr(u)] = (h, w, adv, dy, dx, ptr)
        if u > 0x20:
            max_asc = max(max_asc, dy); max_desc = max(max_desc, h - dy)
        ptr += w * h
    return {"g": glyphs, "asc": max_asc, "desc": max_desc, "data": data, "px": px}


def text_width(f, s):
    return sum(f["g"].get(c, (0, 0, f["px"] * 2 // 7, 0, 0, 0))[2] for c in s)


def draw_text(img, f, s, cx, cy, rgb, scale=1):
    """Draw like LovyanGFX middle_center datum: alpha-blend 8-bit glyph bitmaps."""
    from PIL import Image
    x = cx - text_width(f, s) * scale / 2
    top = cy - (f["asc"] + f["desc"]) * scale / 2
    for c in s:
        if c not in f["g"]:
            x += f["px"] * 2 // 7 * scale; continue
        h, w, adv, dy, dx, ptr = f["g"][c]
        if w and h and c != " ":
            m = Image.frombytes("L", (w, h), f["data"][ptr:ptr + w * h])
            if scale != 1:
                m = m.resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.LANCZOS)
            img.paste(Image.new("RGB", m.size, rgb), (round(x + dx * scale), round(top + (f["asc"] - dy) * scale)), m)
        x += adv * scale
