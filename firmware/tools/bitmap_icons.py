"""Colour (bitmap) icons: QOI encode/decode and the on-device area downscaler, in pure Python.

The firmware stores every bitmap icon once, as a QOI image (https://qoiformat.org, lossless
RGBA, tiny streaming decoder) at the largest on-screen size (BITMAP_SIZE in icons_def.py).
Smaller layouts are produced on the device with an area-average (box) filter in
premultiplied alpha, then alpha-blended against the slot background and dimmed.
area_scale() below is the exact same algorithm, so mockups match the screen.
"""
import struct

QOI_OP_INDEX, QOI_OP_DIFF, QOI_OP_LUMA, QOI_OP_RUN, QOI_OP_RGB, QOI_OP_RGBA = 0x00, 0x40, 0x80, 0xC0, 0xFE, 0xFF


def _h(r, g, b, a):
    return (r * 3 + g * 5 + b * 7 + a * 11) % 64


def qoi_encode(w, h, pixels):
    """pixels: flat list/bytes of RGBA (len w*h*4). Returns bytes (header + data + end marker)."""
    out = bytearray(b"qoif" + struct.pack(">IIBB", w, h, 4, 0))
    index = [(0, 0, 0, 0)] * 64
    prev = (0, 0, 0, 255)
    run = 0
    n = w * h
    for i in range(n):
        p = tuple(pixels[i * 4:i * 4 + 4])
        if p == prev:
            run += 1
            if run == 62 or i == n - 1:
                out.append(QOI_OP_RUN | (run - 1)); run = 0
            continue
        if run:
            out.append(QOI_OP_RUN | (run - 1)); run = 0
        hi = _h(*p)
        if index[hi] == p:
            out.append(QOI_OP_INDEX | hi)
        else:
            index[hi] = p
            if p[3] == prev[3]:
                dr = (p[0] - prev[0] + 128) % 256 - 128
                dg = (p[1] - prev[1] + 128) % 256 - 128
                db = (p[2] - prev[2] + 128) % 256 - 128
                dr_dg, db_dg = dr - dg, db - dg
                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                    out.append(QOI_OP_DIFF | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2))
                elif -32 <= dg <= 31 and -8 <= dr_dg <= 7 and -8 <= db_dg <= 7:
                    out += bytes([QOI_OP_LUMA | (dg + 32), (dr_dg + 8) << 4 | (db_dg + 8)])
                else:
                    out += bytes([QOI_OP_RGB, p[0], p[1], p[2]])
            else:
                out += bytes([QOI_OP_RGBA, *p])
        prev = p
    out += b"\x00" * 7 + b"\x01"
    return bytes(out)


def qoi_decode(data):
    """Returns (w, h, bytearray RGBA)."""
    assert data[:4] == b"qoif"
    w, h = struct.unpack(">II", data[4:12])
    px = bytearray(w * h * 4)
    index = [(0, 0, 0, 0)] * 64
    r, g, b, a = 0, 0, 0, 255
    p = 14
    run = 0
    for i in range(w * h):
        if run:
            run -= 1
        else:
            b1 = data[p]; p += 1
            if b1 == QOI_OP_RGB:
                r, g, b = data[p], data[p + 1], data[p + 2]; p += 3
            elif b1 == QOI_OP_RGBA:
                r, g, b, a = data[p], data[p + 1], data[p + 2], data[p + 3]; p += 4
            elif b1 & 0xC0 == QOI_OP_INDEX:
                r, g, b, a = index[b1]
            elif b1 & 0xC0 == QOI_OP_DIFF:
                r = (r + ((b1 >> 4) & 3) - 2) & 255
                g = (g + ((b1 >> 2) & 3) - 2) & 255
                b = (b + (b1 & 3) - 2) & 255
            elif b1 & 0xC0 == QOI_OP_LUMA:
                b2 = data[p]; p += 1
                vg = (b1 & 0x3F) - 32
                r = (r + vg - 8 + ((b2 >> 4) & 15)) & 255
                g = (g + vg) & 255
                b = (b + vg - 8 + (b2 & 15)) & 255
            else:
                run = b1 & 0x3F
            index[_h(r, g, b, a)] = (r, g, b, a)
        px[i * 4:i * 4 + 4] = bytes((r, g, b, a))
    return w, h, px


def area_scale(S, src, D):
    """Box-filter an SxS RGBA image to DxD (D <= S). Same integer math as screen.cpp.
    Returns a list of D rows, each a list of (pr, pg, pb, a) with premultiplied 0..255 colour."""
    # horizontal weights: source pixel i covers [i*D, (i+1)*D), output x covers [x*S, (x+1)*S)
    def spans():
        out = []
        for i in range(S):
            lo, hi = i * D, (i + 1) * D
            x0 = lo // S
            x1 = (hi - 1) // S
            if x0 == x1:
                out.append(((x0, D),))
            else:
                cut = x1 * S
                out.append(((x0, cut - lo), (x1, hi - cut)))
        return out
    SP = spans()
    TOT = S * S
    rows = []
    acc = [[0, 0, 0, 0] for _ in range(D)]
    for y in range(S):
        hrow = [[0, 0, 0, 0] for _ in range(D)]
        base = y * S * 4
        for i in range(S):
            r, g, b, a = src[base + i * 4:base + i * 4 + 4]
            pr, pg, pb = r * a // 255, g * a // 255, b * a // 255
            for x, w in SP[i]:
                hx = hrow[x]
                hx[0] += pr * w; hx[1] += pg * w; hx[2] += pb * w; hx[3] += a * w
        for oy, w in SP[y]:
            if oy != len(rows):  # this source row spills into the next output row: flush first
                rows.append([tuple(min(255, (c + TOT // 2) // TOT) for c in px) for px in acc])
                acc = [[0, 0, 0, 0] for _ in range(D)]
            for x in range(D):
                ax, hx = acc[x], hrow[x]
                ax[0] += hx[0] * w; ax[1] += hx[1] * w; ax[2] += hx[2] * w; ax[3] += hx[3] * w
    rows.append([tuple(min(255, (c + TOT // 2) // TOT) for c in px) for px in acc])
    assert len(rows) == D
    return rows


def blend_rgb(px, bg):
    """Premultiplied (pr,pg,pb,a) over an RGB background tuple -> RGB tuple."""
    pr, pg, pb, a = px
    ia = 255 - a
    return (pr + (bg[0] * ia + 127) // 255, pg + (bg[1] * ia + 127) // 255, pb + (bg[2] * ia + 127) // 255)


def vivid_factors(pct):
    """8.8 fixed-point saturation and contrast factors for the 'Vivid colours' setting (0..100 %)."""
    pct = max(0, min(100, pct))
    return 256 + pct * 154 // 100, 256 + pct * 77 // 100


def vivid_px(px, pct):
    """Same integer math as vivid() in screen.cpp: premultiplied (pr,pg,pb,a) in and out."""
    sat, con = vivid_factors(pct)
    pr, pg, pb, a = px
    if sat == 256 or not a:
        return px
    y = (77 * pr + 150 * pg + 29 * pb) >> 8
    piv = a >> 1
    out = []
    for v in (pr, pg, pb):
        v = y + (((v - y) * sat) >> 8)
        v = piv + (((v - piv) * con) >> 8)
        out.append(0 if v < 0 else a if v > a else v)
    return (out[0], out[1], out[2], a)


def vivid_rgb(rgb, pct):
    """vividRgb() in screen.cpp: an opaque RGB tuple."""
    return vivid_px(tuple(rgb) + (255,), pct)[:3]
