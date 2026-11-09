# Contributing

Thanks for helping! Bug reports, translations, new icons and hardware notes are all welcome.

## Getting started

```bash
pip install platformio pillow
cd firmware
pio run -e esp32c3                     # build (runs tools/gen_assets.py first)
python3 tools/render_mockups.py        # refresh docs/screens.png, docs/icons.png and docs/vivid.png
python3 tools/render_eyes.py           # refresh docs/eyes.png and docs/eyes.gif
python3 tools/render_portal_mock.py    # refresh docs/portal*.png (needs a build and Chrome)
```

Open `firmware/tools/out/index_preview.html` in a browser to try the web UI without a device. Add `?lang=de` to pick a language and `#edit` to open the slot editor.

Before opening a pull request, check that all three envs build:
`pio run -e esp32c3 -e esp32s3 -e esp32c3_st7789`.

## Adding a language

1. Copy `firmware/lang/en.json` to `firmware/lang/<code>.json` (ISO 639-1 code).
2. Set `name` (in the language itself) and `alphabet` (all non-ASCII letters, both cases).
3. Translate every value in `device`, `web` and `portal` (the Wi-Fi setup portal). Keep the keys. `days_short` must list 7 days starting on Monday.
4. Keep screen strings short. Labels must fit a 240 px circle, and longer text is shrunk automatically.
5. Build. The generator picks up the file, adds its glyphs to the on-screen font and adds it to the web UI's language menu.
6. Commit the JSON together with the regenerated `firmware/src/*_gen.h` files. (`src/icons_img_gen.cpp` is generated too, but it is not committed.)

The translations should feel natural to a parent and friendly to a small child, so avoid literal machine translations.

## Adding an icon

1. Pick an emoji from [Fluent Emoji](https://github.com/microsoft/fluentui-emoji) (MIT). Only use images whose license allows redistribution, and list any new source in `THIRD_PARTY_NOTICES.md`.
2. Add an entry to `ICONS` in `firmware/tools/icons_def.py`: the id (ending in `3d`), `fluent` (the folder name under `assets/` in the Fluent Emoji repo) and `defaults` with `bg` and `tx` colours (pick a background with good contrast, e.g. one of the `_NIGHT`, `_SKY` ... palettes).
3. Run `python3 firmware/tools/fetch_icons.py`. It downloads the 3D PNG, trims it to the visible pixels, scales and centres it, and writes `firmware/assets/icons3d/<id>.qoi` (156 px, RGB565 colours, alpha). For emoji with skin tones, add `file` with the path inside the emoji folder (for example `Default/3D/child_3d_default.png`). Commit that file.
4. Add `icon_<id>` names to the `web` section of every `firmware/lang/*.json`.
5. Build. `gen_assets.py` turns the `.qoi` files into `firmware/src/icons_img_gen.cpp` (generated on every build, not committed) and adds the icon to `icons_gen.h`, the web UI and the mockups. Each icon costs about 10-27 KB of flash, so check the size line of the build output.

## Code style

- Keep the code and comments in English.
- Never commit Wi-Fi credentials, keys or personal data.
- Don't commit build outputs (`.pio/`, `release/*.bin`). Release binaries are attached to GitHub Releases.

## Eyes screensaver

`firmware/tools/eyes.py` mirrors `firmware/src/eyes.cpp` line by line (shapes, easing, the xorshift32 random sequence). If you change one, change the other the same way and re-run `render_eyes.py`, so the docs keep showing what the clock draws.

## Vivid colours

`vivid_px()` in `firmware/tools/bitmap_icons.py` and `vivid()` in `firmware/src/screen.cpp` (and `vivPx()` in `web/index.html`) use the same integer formula. Keep them in sync so the mockups and the web previews match the clock.
