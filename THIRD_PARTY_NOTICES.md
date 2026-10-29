# Third-party notices for Fosi S3 Controller

`orbit_display.cpp` follows the display/touch registration in LilyGO T-RGB's `LV_Helper.cpp`, originally by Lewis He, Copyright (c) 2024 Shenzhen Xin Yuan Electronic Technology Co., Ltd. The startup also follows its `lv_helloworld` example. `tools/prepare.py` pins the upstream base and applies `patches/t-rgb-safety.patch`: fix an N+1-byte SPI write and report unavailable panel initialization correctly. The patch preserves upstream attribution, pin assignments and display timings.

## LilyGO T-RGB

MIT License

Copyright (c) 2024 Xinyuan-LilyGO

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
SOFTWARE.

## Linked dependencies

LVGL 8.4.0 (MIT), Arduino-ESP32 2.0.17 (LGPL-2.1 and bundled component licenses),
ArduinoJson 6.21.6 (MIT), SensorLib 0.2.3 (MIT), TFT_eSPI 2.5.43 (FreeBSD), and
Arduino_GFX 1.5.0 (BSD) are downloaded as build dependencies. Their own license
and copyright notices remain in their distributions. TJpgDec (ChaN), LodePNG
(Lode Vandevenne) and Project Nayuki's QR generator are used from LVGL's bundled
sources under their respective notices. Do not remove dependency licenses when
redistributing their source or compiled products.

## Fonts and original graphics

The Noto-derived CJK fonts use SIL OFL 1.1; copyright and full license are in
[assets/fonts/OFL-NotoSansCJK.txt](assets/fonts/OFL-NotoSansCJK.txt).
The Montserrat-derived Latin fonts use SIL OFL 1.1; see
[assets/fonts/OFL-Montserrat.txt](assets/fonts/OFL-Montserrat.txt).
Unicode-derived composition data use the
[Unicode data license](assets/fonts/UNICODE-DATA-LICENSE.txt).
Font provenance is summarized in [assets/fonts/README.md](assets/fonts/README.md).

Spotify, Tidal and Roon marks derive from Simple Icons under
[CC0 1.0](assets/source-icons/CC0-1.0.txt). Other active interface, weather and
pixel-clock graphics are original project drawings. Trademarks belong to their
owners. No Apple, Elektron, stock-reference image, album artwork or downloaded
lyric content is included in the repository.

## Timezones and external data

Bundled timezone intervals derive from public-domain IANA TZif data;
[the manifest](assets/timezones/manifest.json) records version, range and hashes.
These tables must be updated when applicable civil-time rules change.

Weather data are provided by [Open-Meteo](https://open-meteo.com/), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). This client formats
forecast/model data for display. Hosted API access has separate
[terms](https://open-meteo.com/en/terms), including free-tier restrictions.
Approximate location comes from [ipwhois](https://ipwhois.io/terms).
[LRCLIB](https://lrclib.net/docs),
[LrcAPI](https://github.com/HisAtri/LrcApi) and
[Bark](https://github.com/Finb/bark-server) are external services, not vendored
server implementations. Music, lyrics and artwork remain subject to their
owners' rights. Fosi S3 Controller's MIT license does not relicense hosted content or APIs.

The independent S3 adapter references
[faiz-m/ha_fosi_s3](https://github.com/faiz-m/ha_fosi_s3) and
[zzznz27/Fosi-S3](https://github.com/zzznz27/Fosi-S3); no integration code is copied.

## Browser simulator resources

The local browser simulator uses the same pinned Montserrat font under OFL and
LVGL-derived native symbol masks. LVGL's full MIT notice is preserved in
[assets/preview/LICENSE-LVGL.txt](assets/preview/LICENSE-LVGL.txt). Symbols derived
from Font Awesome 5 Free are attributed to Font Awesome and covered by its
[font license](assets/preview/LICENSE-Font-Awesome.txt). The extracted browser
masks are modified raster representations of those font glyphs. No product
screenshot, music artwork or stock-reference bitmap is shipped as a simulator asset.
