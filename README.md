# Alphabounce – SDL3 port

A C++/SDL3 port of **Alphabounce** (Motion Twin, 2007), the space brick-breaker that ran in Flash on alphabounce.com.
It is rebuilt from the source code Motion Twin released in
[WebGamesArchives](https://github.com/motion-twin/WebGamesArchives) and runs natively, offline, with no Flash.

> **Status:** early. The core brick-breaker is playable: level generation, bricks and their special
> behaviours, the 26 capsule options, balls, molecules, missiles, drones, pause/specials and the end-of-level
> flow, with progress saved locally. The galaxy map, shops, missions UI, lander mini-game and editor are not
> ported yet; a simple sector-select screen stands in for the map.

## How it works

The original is ActionScript 2 written in Haxe, drawn with Flash vector art and driven by Flash's timeline.
Rather than redraw anything, the port keeps the original art and behaviour:

- **`tools/swfconv/`** converts Motion Twin's art library (`gfx.swf`, `planet.swf`) into PNG atlases plus a
  JSON description of every symbol's animation timeline. Shapes are rasterised once (via headless Chromium
  rendering SVG) at 2× resolution.
- **`src/flash/`** is a small re-implementation of the parts of Flash 8 the game uses, on SDL3's renderer:
  MovieClips with timelines and frame scripts, nested clips, colour transforms, blend modes, glow/blur/
  drop-shadow/colour-matrix filters, masks, text fields with the game's embedded fonts, BitmapData,
  the line-drawing API, mouse/keyboard events and SharedObject saves.
- **`src/mt/`** ports Motion Twin's helper libraries (DepthManager, OldRandom, bumdum sprites/particles).
- **`src/game/`** is the game itself, ported class by class from `client/src/*.hx`, keeping the original
  structure so the two can be read side by side.
- **`tools/gen_data.py`** extracts missions, items, planets, shop items and all four languages' text
  from the original sources into `data/*.json`.

### Faithfulness

Levels are procedurally generated from the sector coordinates, so getting them identical to the original
needed some care:

- `mt.OldRandom` is reproduced bit for bit, including Flash 8's float-precision quirks.
- Haxe's Flash 8 compiler evaluates function arguments **last to first**; the port does the same wherever
  that changes which random number goes where.
- ActionScript distinguishes `undefined` from `null` in the level grid (`null < 5` is true,
  `undefined < 5` is false); the port models both.
- Level shapes come from rasterising brush shapes into a 14×23 bitmap with Flash's 4×4 anti-aliasing;
  that is done on the CPU from the original vector outlines.
- Original quirks are kept on purpose (a missile hits the centre brick twice, `mt.Timer.tmod` stays
  slightly below 1 after the first slow-motion, etc.) and are commented in the code.

## Building

Requirements: CMake 3.16+, a C++17 compiler. SDL3 is used from your system if found, otherwise CMake
downloads and builds it.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/alphabounce            # run from the repository root (it looks for assets/ and data/)
```

On Linux, SDL3 needs the usual development packages (X11/Wayland, etc.) when it is built from source —
see SDL's [Linux README](https://wiki.libsdl.org/SDL3/README-linux).

The game needs a GPU-backed SDL renderer (OpenGL, Direct3D, Metal or Vulkan). SDL's software renderer
lacks the custom blend modes used for Flash colour effects, so colours will be wrong with it.

### Controls

Same as the original: the mouse moves the pad, click launches/glues/fires depending on the pad,
**Space** or **X** fires missiles, **P**/**Esc** pauses (click a capsule to use it), **Enter** uses the
first capsule. On the sector screen: arrow keys choose a sector, **Enter** plays.

Options: `--lang fr|de|es`, `--scale N`, `--play X,Y`, `--autoplay`.

### Regenerating assets and data

`assets/` and `data/` are generated from the original game files and committed for convenience. To rebuild them:

```sh
git clone --depth 1 --filter=blob:none --sparse https://github.com/motion-twin/WebGamesArchives.git
cd WebGamesArchives && git sparse-checkout set Alphabounce && cd ..
pip install pillow playwright && playwright install chromium
python3 tools/swfconv/convert.py WebGamesArchives/Alphabounce/client/swf/gfx.swf assets --vector mcShape,mcBrush,mcLuz
python3 tools/swfconv/convert.py WebGamesArchives/Alphabounce/web/swf/planet.swf assets --name planet
python3 tools/gen_data.py WebGamesArchives/Alphabounce data
```

## Roadmap

1. ~~Core brick-breaker~~ ✔
2. Galaxy map (navigation, fog, planets, capsules/fuel, travellers, black holes)
3. Shop, inventory and missions screens
4. Lander mini-game (planet surfaces)
5. Level editor, menus/preferences, audio hooks

## Licence and credits

Alphabounce was created by **Motion Twin** (2007). Its source code and assets were released by Motion Twin
under the [Creative Commons Attribution-NonCommercial-ShareAlike 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
licence. This port is a derivative work and is distributed under the same licence: see [LICENSE](LICENSE)
and [NOTICE.md](NOTICE.md). It is a non-commercial fan project and is not affiliated with Motion Twin.

Third-party code: [SDL3](https://libsdl.org) (zlib), [nlohmann/json](https://github.com/nlohmann/json) (MIT),
[stb_image / stb_image_write](https://github.com/nothings/stb) (public domain / MIT).
