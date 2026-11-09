"""Single source of truth for the clock icons.

All icons are full-colour Fluent Emoji 3D images (Microsoft, MIT, see
THIRD_PARTY_NOTICES.md). Each entry names the Fluent Emoji asset folder and the
default slot colours: "bg" (screen background) and "tx" (label and clock text).
The icons keep their own colours; only the background, the text colour and the
software dimming apply.

Pixels live in ../assets/icons3d/<id>.qoi (made by tools/fetch_icons.py) and are
compiled into the app. The same data is used by:
  * the firmware (generated src/icons_gen.h + src/icons_img_gen.cpp),
  * the web UI pickers and previews (served from /icons/<id>.qoi),
  * the PNG mockups (tools/render_mockups.py).
"""

# Old (pre-1.1.1) vector icon ids, mapped when an old config is loaded.
LEGACY_IDS = {"moon": "moon3d", "sun": "sun3d", "bed": "bed3d", "star": "star3d",
              "book": "openbook3d", "heart": "heart3d"}

# "sleep": True marks night icons: a slot with one of them is sleep time (the eyes
# screensaver is off or sleepy there, see README).

# Default slot colour palettes.
_NIGHT = {"bg": "#0B1030", "tx": "#FFE08A"}
_DUSK = {"bg": "#1B1640", "tx": "#C9D6FF"}
_SKY = {"bg": "#8FD3FF", "tx": "#1D3A6E"}
_GREEN = {"bg": "#2E4A3A", "tx": "#FFF4D6"}
_AQUA = {"bg": "#CDEFFF", "tx": "#0B4F6C"}
_CREAM = {"bg": "#FFF4D6", "tx": "#5A3A1A"}
_PINK = {"bg": "#FFE3EC", "tx": "#C2185B"}
_LILAC = {"bg": "#EDE4FF", "tx": "#4B2A86"}
_MINT = {"bg": "#D8F5E1", "tx": "#1E5631"}

ICONS = {
    # sleep and wake-up
    "moon3d": {"sleep": True, "fluent": "Last quarter moon face", "defaults": _NIGHT},
    "sleeping3d": {"sleep": True, "fluent": "Sleeping face", "defaults": _NIGHT},
    "star3d": {"sleep": True, "fluent": "Glowing star", "defaults": _NIGHT},
    "bed3d": {"sleep": True, "fluent": "Bed", "defaults": _DUSK},
    "sun3d": {"fluent": "Sun with face", "defaults": _SKY},
    "sunrise3d": {"fluent": "Sunrise", "defaults": _SKY},
    "alarm3d": {"fluent": "Alarm clock", "defaults": _LILAC},
    "nap3d": {"fluent": "Person in bed", "file": "Default/3D/person_in_bed_3d_default.png", "defaults": _DUSK},
    # daily routine
    "toothbrush3d": {"fluent": "Toothbrush", "defaults": _AQUA},
    "bath3d": {"fluent": "Bathtub", "defaults": _AQUA},
    "toilet3d": {"fluent": "Toilet", "defaults": _AQUA},
    "tshirt3d": {"fluent": "T-shirt", "defaults": _MINT},
    "pancakes3d": {"fluent": "Pancakes", "defaults": _CREAM},
    "meal3d": {"fluent": "Fork and knife with plate", "defaults": _CREAM},
    "apple3d": {"fluent": "Red apple", "defaults": _CREAM},
    "sandwich3d": {"fluent": "Sandwich", "defaults": _CREAM},
    "spaghetti3d": {"fluent": "Spaghetti", "defaults": _CREAM},
    "milk3d": {"fluent": "Glass of milk", "defaults": _SKY},
    "backpack3d": {"fluent": "Backpack", "defaults": _GREEN},
    "school3d": {"fluent": "School", "defaults": _GREEN},
    "child3d": {"fluent": "Child", "file": "Default/3D/child_3d_default.png", "defaults": _MINT},
    "house3d": {"fluent": "House", "defaults": _CREAM},
    "car3d": {"fluent": "Automobile", "defaults": _SKY},
    "bike3d": {"fluent": "Bicycle", "defaults": _MINT},
    "tree3d": {"fluent": "Deciduous tree", "defaults": _GREEN},
    "slide3d": {"fluent": "Playground slide", "defaults": _SKY},
    # play and fun
    "openbook3d": {"fluent": "Open book", "defaults": _GREEN},
    "teddy3d": {"fluent": "Teddy bear", "defaults": _CREAM},
    "puzzle3d": {"fluent": "Puzzle piece", "defaults": _CREAM},
    "bricks3d": {"fluent": "Brick", "defaults": _CREAM},
    "doll3d": {"fluent": "Nesting dolls", "defaults": _PINK},
    "ball3d": {"fluent": "Soccer ball", "defaults": _MINT},
    "palette3d": {"fluent": "Artist palette", "defaults": _LILAC},
    "crayon3d": {"fluent": "Crayon", "defaults": _LILAC},
    "music3d": {"fluent": "Musical notes", "defaults": _LILAC},
    "rocket3d": {"fluent": "Rocket", "defaults": _DUSK},
    "gift3d": {"fluent": "Wrapped gift", "defaults": _PINK},
    "cake3d": {"fluent": "Birthday cake", "defaults": _PINK},
    "heart3d": {"fluent": "Red heart", "defaults": _PINK},
    "balloon3d": {"fluent": "Balloon", "defaults": _SKY},
    "crown3d": {"fluent": "Crown", "defaults": _LILAC},
    "icecream3d": {"fluent": "Soft ice cream", "defaults": _PINK},
    "xmastree3d": {"fluent": "Christmas tree", "defaults": _DUSK},
    "pumpkin3d": {"fluent": "Jack-o-lantern", "defaults": _DUSK},
    "snowman3d": {"fluent": "Snowman", "defaults": _SKY},
    "rainbow3d": {"fluent": "Rainbow", "defaults": _SKY},
    "cloud3d": {"fluent": "Cloud", "defaults": {"bg": "#3D6FB8", "tx": "#FFFFFF"}},
    # animals
    "owl3d": {"fluent": "Owl", "defaults": _NIGHT},
    "cat3d": {"fluent": "Cat face", "defaults": _PINK},
    "dog3d": {"fluent": "Dog face", "defaults": _MINT},
    "unicorn3d": {"fluent": "Unicorn", "defaults": _PINK},
    "panda3d": {"fluent": "Panda", "defaults": _MINT},
    "bunny3d": {"fluent": "Rabbit face", "defaults": _MINT},
    "fox3d": {"fluent": "Fox", "defaults": _GREEN},
    "turtle3d": {"fluent": "Turtle", "defaults": _AQUA},
    "fish3d": {"fluent": "Tropical fish", "defaults": _AQUA},
    "penguin3d": {"fluent": "Penguin", "defaults": _SKY},
    "dino3d": {"fluent": "Sauropod", "defaults": _CREAM},
}
BITMAP_DIR = "assets/icons3d"   # relative to firmware/

# ---------------------------------------------------------------------------
# Round 240x240 screen layout (GC9A01). Everything must fit inside the circle
# (center 120,120, radius 120). icon = [x, y, size] in pixels,
# label_y / clock_y = vertical text center. Key = which texts are shown.
SCREEN = {"w": 240, "h": 240, "round": True}
LAYOUTS = {
    "none":  {"icon": [42, 42, 156]},
    "clock": {"icon": [55, 32, 130], "clock_y": 186},
    "label": {"icon": [55, 34, 130], "label_y": 184},
    "both":  {"icon": [65, 26, 110], "label_y": 154, "clock_y": 190},
}

# Colour icons are stored once at the largest layout size and downscaled on the device.
# ICON_FILL: the longer side of every (trimmed, centred) icon fills this share of its box.
ICON_FILL = 0.90
BITMAP_SIZE = max(v["icon"][2] for v in LAYOUTS.values())

# On-device smooth fonts (VLW, 8-bit anti-aliased), generated from DejaVu Sans Bold.
# The label font contains ASCII + every letter used by any language in ../lang/*.json.
from i18n_def import font_chars  # noqa: E402

FONTS = {
    "label": {"px": 20, "chars": font_chars()},
    "clock": {"px": 38, "chars": " 0123456789:-"},
}
