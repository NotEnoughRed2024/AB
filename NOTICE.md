# Notice

**Alphabounce** © Motion Twin, 2007. Original game design, code, art and text by Motion Twin.

Source: https://github.com/motion-twin/WebGamesArchives (folder `Alphabounce`), released by Motion Twin
under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International licence
(https://creativecommons.org/licenses/by-nc-sa/4.0/).

## What this project changed

This repository is an adaptation of that work:

- The game logic was translated from Haxe/ActionScript 2 to C++ (`src/game/`, `src/mt/`).
- The Flash runtime features it relies on were re-implemented on SDL3 (`src/flash/`).
- The art in `assets/` was converted from the original `gfx.swf` and `planet.swf` into PNG atlases
  and JSON timelines by `tools/swfconv/`; the art itself is Motion Twin's, unchanged apart from rasterisation.
- `data/` was extracted from the original Haxe sources (missions, items, planets, shop, game text in
  English, French, German and Spanish) by `tools/gen_data.py`.
- The server-side behaviour is replaced by the offline emulation Motion Twin left in `Api.hx`.

The adaptation is shared under the same licence (CC BY-NC-SA 4.0). It is a non-commercial fan project,
not affiliated with or endorsed by Motion Twin. "Alphabounce" and "Motion Twin" are used here only to
credit the original work.

## Fonts

`gfx.swf` embeds several fonts (e.g. Verdana, Orator, Kiloton, "Digital LCD"). The atlases contain
rasterised glyph images of those embedded fonts, as they appeared in the original game; no font
software is distributed.
