"""Python port of src/eyes.cpp (animated eyes screensaver): same geometry, rasterizer and
animator, used for docs/eyes.png and docs/eyes.gif. Keep both files in sync.
"""
import math

EYES_X0, EYES_W = 4, 232
SPACING, EYE_W, EYE_H = 50.0, 66.0, 80.0
LOOK_X, LOOK_Y, PERSP, RECT_R, MIN_H = 22.0, 16.0, 0.08, 14.0, 4.0


class EyeParams:
    def __init__(self, **kw):
        self.lookX = self.lookY = 0.0
        self.openL = self.openR = 1.0
        self.scale = 1.0
        self.happy = 0.0
        self.lid = 0.0
        self.__dict__.update(kw)

    def copy(self):
        return EyeParams(**self.__dict__)


def eye_of(p, cx, cy, round_shape, side):
    e = {}
    persp = 1.0 + PERSP * p.lookX * side
    e["ex"] = cx + side * SPACING + p.lookX * LOOK_X
    e["ey"] = cy + p.lookY * LOOK_Y
    e["w"] = EYE_W * p.scale * persp
    e["hFull"] = EYE_H * p.scale * persp
    h = e["hFull"] * (p.openL if side < 0 else p.openR)
    e["h"] = max(h, MIN_H)
    rr = e["w"] * 0.5 if round_shape else RECT_R * p.scale
    e["rr"] = min(rr, e["w"] * 0.5, e["h"] * 0.5)
    e["top"] = e["ey"] - e["h"] * 0.5
    e["bot"] = e["ey"] + e["h"] * 0.5
    e["lidBase"] = e["ey"] - e["hFull"] * 0.5 + p.lid * e["hFull"] * 0.55
    e["lidSlope"] = side * 0.3 * p.lid
    e["cutY"] = e["bot"] + 0.3 * e["h"]
    e["cutRx"] = 0.7 * e["w"]
    e["cutRy"] = 0.3 * e["h"] + p.happy * 0.55 * e["h"]
    return e


def add_span(acc, a, b):
    A = int(math.floor((a - EYES_X0) * 64.0 + 0.5))
    B = int(math.floor((b - EYES_X0) * 64.0 + 0.5))
    A = max(A, 0)
    B = min(B, EYES_W * 64)
    if B <= A:
        return
    pa, pb = A >> 6, (B - 1) >> 6
    if pa == pb:
        acc[pa] += B - A
        return
    acc[pa] += (pa + 1) * 64 - A
    for x in range(pa + 1, pb):
        acc[x] += 64
    acc[pb] += B - pb * 64


def eye_sub_row(e, y, acc):
    if y < e["top"] or y >= e["bot"]:
        return
    dy, inner, hw = abs(y - e["ey"]), e["h"] * 0.5 - e["rr"], e["w"] * 0.5
    if dy > inner:
        t = dy - inner
        q = e["rr"] * e["rr"] - t * t
        hw = e["w"] * 0.5 - e["rr"] + (math.sqrt(q) if q > 0 else 0)
    a, b = e["ex"] - hw, e["ex"] + hw
    if e["lidSlope"] != 0:
        xb = e["ex"] + (y - e["lidBase"]) / e["lidSlope"]
        if e["lidSlope"] > 0:
            b = min(b, xb)
        else:
            a = max(a, xb)
    elif y < e["lidBase"]:
        return
    if b <= a:
        return
    dc = (y - e["cutY"]) / e["cutRy"]
    if dc * dc < 1:
        c = e["cutRx"] * math.sqrt(1 - dc * dc)
        c0, c1 = e["ex"] - c, e["ex"] + c
        if c0 > a:
            add_span(acc, a, min(c0, b))
        if c1 < b:
            add_span(acc, max(c1, a), b)
        return
    add_span(acc, a, b)


def eyes_rows(p, cx=120, cy=120, round_shape=True):
    l, r = eye_of(p, cx, cy, round_shape, -1), eye_of(p, cx, cy, round_shape, 1)
    return int(math.floor(min(l["top"], r["top"]))) - 1, int(math.ceil(max(l["bot"], r["bot"]))) + 1


def eyes_row(p, y, cx=120, cy=120, round_shape=True):
    acc = [0] * EYES_W
    l, r = eye_of(p, cx, cy, round_shape, -1), eye_of(p, cx, cy, round_shape, 1)
    for s in range(4):
        sy = y + (s + 0.5) * 0.25
        eye_sub_row(l, sy, acc)
        eye_sub_row(r, sy, acc)
    return [(min(v, 256) * 15 + 128) >> 8 for v in acc]


def smooth(x):
    if x <= 0:
        return 0.0
    if x >= 1:
        return 1.0
    return x * x * (3 - 2 * x)


NEUTRAL, HAPPY, SURPRISED, SLEEPY = range(4)
DIRS = [(-1, 0), (1, 0), (0, -1), (0, 1), (-0.7, -0.7), (0.7, -0.7), (-0.7, 0.7), (0.7, 0.7)]


class EyesAnimator:
    def reset(self, night, seed):
        self.night = night
        self.rng = seed or 1
        self.cur, self.tgt = EyeParams(), EyeParams()
        if night:
            self.cur.lid = self.tgt.lid = 0.55
        self.tLook = self.range(0.5, 1.5)
        self.tBlink = self.range(0.8, 2.5)
        self.tExpr = self.range(4.0, 8.0)
        self.exprLeft = 0.0
        self.blinkT, self.blinkDur, self.blinkEye = -1.0, 0.18, 0
        self.expr = NEUTRAL

    def rnd(self):
        x = self.rng
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.rng = x
        return (x >> 8) * (1.0 / 16777216.0)

    def range(self, a, b):
        return a + (b - a) * self.rnd()

    def start_blink(self, eye, dur):
        self.blinkT, self.blinkEye, self.blinkDur = 0.0, eye, dur

    def step(self, dt):
        dt = min(dt, 0.1)
        n, t, c = self.night, self.tgt, self.cur
        self.tLook -= dt
        if self.tLook <= 0:
            if self.rnd() < 0.35:
                t.lookX = t.lookY = 0.0
            else:
                d = int(self.rnd() * 8) & 7
                m = self.range(0.2, 0.5) if n else self.range(0.5, 1.0)
                t.lookX, t.lookY = DIRS[d][0] * m, DIRS[d][1] * m * 0.8
            self.tLook = self.range(3.0, 6.0) if n else self.range(1.2, 3.5)
        self.tBlink -= dt
        if self.tBlink <= 0 and self.blinkT < 0:
            self.start_blink(0, 0.4 if n else 0.18)
            self.tBlink = 0.35 if self.rnd() < 0.15 else (self.range(1.5, 4.0) if n else self.range(2.0, 5.5))
        if self.expr != NEUTRAL:
            self.exprLeft -= dt
            if self.exprLeft <= 0:
                self.expr = NEUTRAL
        elif not n:
            self.tExpr -= dt
            if self.tExpr <= 0:
                r = self.rnd()
                if r < 0.35:
                    self.expr, self.exprLeft = HAPPY, self.range(1.6, 2.6)
                elif r < 0.55:
                    self.expr, self.exprLeft = SURPRISED, self.range(0.9, 1.4)
                    t.lookX, t.lookY = 0.0, -0.3
                elif r < 0.75:
                    if self.blinkT < 0:
                        self.start_blink(1 if self.rnd() < 0.5 else 2, 0.45)
                else:
                    self.expr, self.exprLeft = SLEEPY, self.range(2.5, 4.0)
                self.tExpr = self.range(5.0, 11.0)
        t.scale = 1.22 if self.expr == SURPRISED else 1.0
        t.happy = 1.0 if self.expr == HAPPY else 0.0
        t.lid = 0.55 if n else (0.45 if self.expr == SLEEPY else 0.0)
        kl = 1.0 - math.exp(-dt * (2.5 if n else 7.0))
        ke = 1.0 - math.exp(-dt * (14.0 if self.expr == SURPRISED else 6.0))
        kd = 1.0 - math.exp(-dt * 1.5)
        c.lookX += (t.lookX - c.lookX) * kl
        c.lookY += (t.lookY - c.lookY) * kl
        c.scale += (t.scale - c.scale) * ke
        c.happy += (t.happy - c.happy) * ke
        c.lid += (t.lid - c.lid) * kd
        out = c.copy()
        out.openL = out.openR = 1.0
        if self.blinkT >= 0:
            x = self.blinkT / self.blinkDur
            cl = smooth(x / 0.4) if x < 0.4 else (1.0 if x < 0.55 else 1.0 - smooth((x - 0.55) / 0.45))
            if self.blinkEye != 2:
                out.openL = 1 - cl
            if self.blinkEye != 1:
                out.openR = 1 - cl
            self.blinkT += dt
            if self.blinkT > self.blinkDur:
                self.blinkT = -1.0
        return out
