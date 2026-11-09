"""Draw docs/wiring.png: ESP32-C3 SuperMini + round 1.28" GC9A01 TFT + optional LDR and button."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle, Circle
import matplotlib.patheffects as pe

# Colour rule: red = 3V3/VCC only, black = GND only. Signal wires never use red/orange tones.
SIG_BUTTON = "#e6e6e6"   # light grey/white with a dark outline
OUTLINE = [pe.Stroke(linewidth=4.2, foreground="#333"), pe.Normal()]

fig, ax = plt.subplots(figsize=(15, 10), dpi=130)
ax.set_xlim(0, 145); ax.set_ylim(0, 100); ax.axis("off")
ax.text(72, 98, "Kido Clock - wiring (ESP32-C3 SuperMini + round 1.28\" GC9A01 TFT)",
        ha="center", fontsize=14, weight="bold")

# --- ESP32-C3 SuperMini (top view, USB-C at the bottom) ---
bx, by, bw, bh = 52, 22, 26, 62
ax.add_patch(FancyBboxPatch((bx, by), bw, bh, boxstyle="round,pad=0.6", fc="#2f6b3a", ec="black", lw=1.5))
ax.add_patch(Rectangle((bx + 9, by - 3.5), 8, 4, fc="#bbb", ec="black"))
ax.text(bx + 13, by - 6.5, "USB-C 5V", ha="center", fontsize=9)
ax.text(bx + bw / 2, by + 12, "ESP32-C3\nSuperMini", ha="center", va="center", color="white", fontsize=12, weight="bold")
ax.text(bx + bw / 2, by + 5.5, "(top view, USB at the bottom,\npin '0' top-left)", ha="center", va="center", color="#dfe", fontsize=8)
left = ["GPIO0", "GPIO1", "GPIO2", "GPIO3", "GPIO4", "3V3", "GND", "5V"]
right = ["GPIO21", "GPIO20", "GPIO10", "GPIO9", "GPIO8", "GPIO7", "GPIO6", "GPIO5"]
pin = {}
for i, n in enumerate(left):
    y = by + bh - 5 - i * 6.0
    ax.add_patch(Circle((bx + 1.5, y), 0.9, fc="gold", ec="black"))
    ax.text(bx + 3.5, y, n, va="center", fontsize=8.5, color="white")
    pin[n] = (bx + 1.5, y)
for i, n in enumerate(right):
    y = by + bh - 5 - i * 6.0
    ax.add_patch(Circle((bx + bw - 1.5, y), 0.9, fc="gold", ec="black"))
    ax.text(bx + bw - 3.5, y, n, va="center", ha="right", fontsize=8.5, color="white")
    pin[n] = (bx + bw - 1.5, y)

# --- round GC9A01 module (right). Drawn rotated 90 deg: the 7-pin header is on the left ---
tx, ty = 100, 50          # header strip top-left
tft_pins = ["RST", "CS", "DC", "SDA", "SCL", "GND", "VCC"]
tft_alt = ["(opt.*)", "(opt.*)", "", "(MOSI/DIN)", "(SCK/CLK)", "", "3.3 V"]
ax.add_patch(FancyBboxPatch((tx, ty - 1), 9, 40, boxstyle="round,pad=0.5", fc="#1a4f9c", ec="black", lw=1.5))
ax.add_patch(Circle((tx + 27, ty + 19), 18, fc="#1a4f9c", ec="black", lw=1.5))
ax.add_patch(Circle((tx + 27, ty + 19), 15, fc="#0b1030", ec="#888"))
ax.text(tx + 27, ty + 19, "GC9A01\n1.28\" 240x240\nround", color="#FFE08A", ha="center", va="center", fontsize=9, weight="bold")
ax.text(tx + 24, ty - 6.5, "(module drawn rotated:\nthe real header is at the bottom)", ha="center", fontsize=7.5, style="italic", color="#444")
tp = {}
for i, n in enumerate(tft_pins):
    y = ty + 36 - i * 5.6
    ax.add_patch(Circle((tx + 1.5, y), 0.9, fc="gold", ec="black"))
    ax.text(tx + 3.2, y, n, va="center", fontsize=8, color="white")
    tp[n] = (tx + 1.5, y)
    if tft_alt[i]:
        ax.text(tx - 1, y + 1.6, tft_alt[i], ha="right", fontsize=6.5, color="#555")

def wire(p, q, color, via_x=None, lw=2.2):
    (x0, y0), (x1, y1) = p, q
    xm = via_x if via_x is not None else (x0 + x1) / 2
    ax.plot([x0, xm, xm, x1], [y0, y0, y1, y1], color=color, lw=lw, solid_capstyle="round", zorder=1)

# rails
ax.plot([20, 20], [8, 90], color="red", lw=3); ax.text(20, 91.5, "3V3", color="red", ha="center", fontsize=9, weight="bold")
ax.plot([14, 14], [8, 90], color="black", lw=3); ax.text(14, 91.5, "GND", ha="center", fontsize=9, weight="bold")
wire(pin["3V3"], (20, pin["3V3"][1]), "red", via_x=20)
wire(pin["GND"], (14, pin["GND"][1]), "black", via_x=14)

# TFT power from rails (run along the top / bottom)
ax.plot([20, 20, 96, 96, tp["VCC"][0]], [90, 93, 93, tp["VCC"][1], tp["VCC"][1]], color="red", lw=2.2)
ax.plot([14, 14, 98, 98, tp["GND"][0]], [90, 94.5, 94.5, tp["GND"][1], tp["GND"][1]], color="black", lw=2.2)

# signals: right-side pins go straight across
wire(pin["GPIO5"], tp["DC"], "#8e44ad", via_x=86)
wire(pin["GPIO6"], tp["SDA"], "#27ae60", via_x=88)
wire(pin["GPIO7"], tp["CS"], "#8d6e63", via_x=84)
wire(pin["GPIO10"], tp["RES" if "RES" in tp else "RST"], "#7f8c8d", via_x=82)
# GPIO4 (SCK) loops under the board
x4, y4 = pin["GPIO4"]
ax.plot([x4, 46, 46, 94, 94, tp["SCL"][0]], [y4, y4, 14, 14, tp["SCL"][1], tp["SCL"][1]], color="#2980b9", lw=2.2)

# --- optional LDR divider on GPIO0 (top-left pin) ---
x0p, y0p = pin["GPIO0"]
ax.plot([x0p, 30], [y0p, y0p], color="#16a085", lw=2.2)
ax.add_patch(Circle((30, y0p), 0.7, fc="#16a085", zorder=3))
ax.add_patch(Rectangle((28.5, y0p + 2), 3, 6, fc="#f5deb3", ec="black"))        # LDR to 3V3
ax.plot([30, 30], [y0p, y0p + 2], color="#16a085", lw=2.2)
ax.plot([30, 30, 20], [y0p + 8, y0p + 10, y0p + 10], color="red", lw=2.2)
ax.text(32.5, y0p + 5, "LDR GL5528", fontsize=8, va="center")
ax.add_patch(Rectangle((28.5, y0p - 8), 3, 6, fc="#c8a26b", ec="black"))        # 10k to GND
ax.plot([30, 30], [y0p, y0p - 2], color="#16a085", lw=2.2)
ax.plot([30, 30, 14], [y0p - 8, y0p - 10, y0p - 10], color="black", lw=2.2)
ax.text(27.5, y0p - 5, "10 kΩ", fontsize=8, va="center", ha="right")

# --- optional button on GPIO1 ---
x1p, y1p = pin["GPIO1"]
ax.plot([x1p, 38, 38], [y1p, y1p, y1p - 1.5], color=SIG_BUTTON, lw=2.4, path_effects=OUTLINE, solid_capstyle="round")
ax.text(44, y1p + 1.3, "GPIO1", fontsize=7.5, weight="bold", ha="center", color="#333")
ax.add_patch(Rectangle((36, y1p - 5.5), 4, 4, fc="#ddd", ec="black")); ax.add_patch(Circle((38, y1p - 3.5), 1.1, fc="#555"))
ax.plot([38, 38, 14], [y1p - 5.5, y1p - 9, y1p - 9], color="black", lw=2.2)
ax.text(41, y1p - 3.5, "button\n(to GND)", fontsize=8, va="center")
ax.text(23, y1p - 16, "optional:\nlight sensor\nand button", fontsize=7.5, style="italic")
ax.text(22.5, 17, "Button: GPIO1 <-> GND only.\nNever connect it to 3V3 -\npressing it would short\nthe supply.", fontsize=7.3, weight="bold", color="#8B0000",
        bbox=dict(boxstyle="round,pad=0.4", fc="#fff3f3", ec="#8B0000", lw=1))

# legend table (below the TFT)
rows = [("TFT", "ESP32-C3", "wire"), ("VCC", "3V3", "red"), ("GND", "GND", "black"), ("SCL", "GPIO4", "blue"),
        ("SDA", "GPIO6", "green"), ("DC", "GPIO5", "purple"), ("CS *", "GPIO7", "brown"),
        ("RST *", "GPIO10", "grey"), ("", "", ""),
        ("LDR+10k", "GPIO0 (ADC)", "optional"), ("Button", "GPIO1", "grey/white"), ("* works without CS/RST too", "", "")]
ax.add_patch(FancyBboxPatch((103, 7), 38, 32, boxstyle="round,pad=0.5", fc="#f6f6f6", ec="#999"))
for i, r in enumerate(rows):
    y = 37.3 - i * 2.6
    for j, c in enumerate(r):
        ax.text(104.5 + j * 12.5, y, c, fontsize=8.3, weight="bold" if i == 0 else None, va="center")
# junction dots on rails
for (xx, yy) in [(20, pin["3V3"][1]), (14, pin["GND"][1])]:
    ax.add_patch(Circle((xx, yy), 0.8, fc="black", zorder=3))
ax.text(52, 1.2, "Power: USB-C cable to any 5 V charger. Connect the display VCC to 3V3 (not 5V). The display has no BL pin - brightness is done by dimming colours. "
        "Check the silkscreen\non your board - SuperMini batches may differ. GPIO3 is free.", ha="center", fontsize=8.5, style="italic")
out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "wiring.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
plt.savefig(out, bbox_inches="tight", facecolor="white")
print("ok")
