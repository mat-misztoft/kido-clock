# Third-party notices

Kido Clock itself is MIT-licensed (see [LICENSE](LICENSE)). It bundles or embeds the
following third-party material.

## Fluent Emoji 3D (Microsoft)

The colour icons in `firmware/assets/icons3d/*.qoi` (and the copies compiled into the
firmware and shown in `docs/`) are resized and colour-reduced versions of
[Fluent Emoji](https://github.com/microsoft/fluentui-emoji) 3D images by Microsoft.
`firmware/tools/fetch_icons.py` lists exactly how they are produced.

```
MIT License

Copyright (c) Microsoft Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
```

## DejaVu Sans Bold

The on-screen font is generated from DejaVu Sans Bold. Its license is in
[`firmware/tools/fonts/DejaVu-LICENSE.txt`](firmware/tools/fonts/DejaVu-LICENSE.txt).

## QOI image format

Colour icons are stored in the [QOI format](https://qoiformat.org). The encoder and
decoders in this repository (`firmware/tools/bitmap_icons.py`, `firmware/src/screen.cpp`,
`firmware/web/index.html`) are independent implementations of the public specification.

## Libraries (downloaded at build time, not bundled)

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) - MIT and BSD-2-Clause
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - MIT
- [WiFiManager](https://github.com/tzapu/WiFiManager) - MIT
- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) - LGPL-2.1
