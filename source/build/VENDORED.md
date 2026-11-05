# Build engine provenance and local changes

The Build engine was created by Ken Silverman. His source-code release and
its terms are available from [Ken Silverman's Build source page](http://www.advsys.net/ken/buildsrc/).
The release's license is reproduced in [BUILDLIC.TXT](BUILDLIC.TXT):

```c
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
```

## Provenance

This directory is a reconstruction of the earlier Build engine linked into
the December 30, 1994 `D3D.EXE` beta. The executable is the reference for
behavior and the comments in the implementation identify recovered routines
by address. The project's rebuild design uses the separate
[raynorpat/duke3d](https://github.com/raynorpat/duke3d) tree as a guide to
modern port organization, not as a source for copying 1996 game or renderer
code. See the [rebuild design](../../docs/superpowers/specs/2026-11-03-lameduke-rebuild-design.md).

The repository history does not identify a particular Build source-release
archive or commit as an import baseline for this directory. Consequently this
file records the local implementation and its differences in scope; it does
not claim to be a complete file-by-file diff against a specific release.
For comparison, the related duke3d project records its Build import as
[icculus/buildengine at `f3dce0a`](https://github.com/icculus/buildengine/tree/f3dce0a10bff90e1ed5cf8ec186b3731237a86da).
Its `build.h` describes the later 40-byte sector, 32-byte wall and 44-byte
sprite layouts; this beta's version-5 layouts differ as listed below. This is
a comparison point, not an asserted ancestor of the code here.

## Current source layout and changes

| Files | Local implementation |
|---|---|
| `board.c`, `board.h` | Version-5 map loading, packed sector/wall/sprite records, sprite lists, collision and spatial queries. The records are 37, 35 and 38 bytes, respectively, as read by this beta. |
| `draw.c`, `draw.h` | The classic software room and mask drawers and `rotatesprite`, reconstructed from the executable. They replace the earlier portal approximation. |
| `display.c`, `display.h` | Palette and lookup loading, display mode and view setup. |
| `tiles.c`, `tiles.h` | Loose `TILES000.ART`–`TILES007.ART` tile loading and access. |
| `math.c`, `math.h` | Sine-table loading and engine math helpers. |
| `cache.c`, `cache1d.h` | Cache interface used by this rebuild. |
| `engine.c`, `engine.h` | Engine initialization and reading the original `SETUP.DAT` bytes. |
| `network.c`, `network.h` | Single-player connection state; multiplayer is a stub and opens no connection. |
| `sdl_main.c`, `sdl_input.c`, `sdl_window.c`, `sdl_sys.c`, `sdl_opengl.c` and headers | SDL2 startup, input, timing and framebuffer presentation. `sdl_opengl.c` is duke3d's and is not built yet. |
| `polymost.c`, `polymost_soft.c`, `polymost_opengl*`, `polymost.h` | duke3d's Polymost; see below. |
| `platform.h`, `plat_unix.c`, `plat_win32.c` | Platform file access, including case-insensitive filename fallback on Unix. |
| `CMakeLists.txt` | Builds the engine as a static library for the game, the editor and the map tools. |

The game reads the original loose files rather than a GRP archive.

## The map editor

`source/editor/build.c` and `source/editor/bstub.h` come from
[raynorpat/duke3d](https://github.com/raynorpat/duke3d) at
`2f141cac5a0d2106780dff15fc2e2b8757618bfc` (`source/build/`): Ken Silverman's
editor under [BUILDLIC.TXT](BUILDLIC.TXT), with icculus's and duke3d's changes.
They are the only files taken from that tree. Commit `8035cc7` holds them
unchanged, so `git diff 8035cc7 -- source/editor/build.c source/editor/bstub.h`
shows every local change. In summary:

- The 1996 engine headers are replaced by `source/editor/editor.h`, which sits
  over this engine, and the platform calls by `source/editor/edglue.c`.
  Polymost, renderer cycling, `build.cfg` and `editormode` are gone.
- Records are version 5. Wall and sprite `pal`, sprite `clipdist` and offsets,
  and slopes are removed. The slope keys became the floor heightmap tool. T
  and B no longer touch sprite cstat bits above 7.
- Saving goes through `writeboard`, which writes sprites in index order. The
  start sector is no longer recomputed on save, and `fixspritesectors` only
  moves sprites changed since load.
- The file menu lists through `plat_listdir`. Tile names come from
  `DEFS.CON` through a new `ExtLoadNames` hook in `bstub.h`. Drawn walls get
  `(-1,-1)` second links. The window title names the map.

The engine side of the editor was lifted from `D3D.EXE`, whose engine was
shared with the 1994 BUILD:

- the search pick in `drawrooms` and the sprite drawers (`searchit`, 0x44DA6
  and the sprite sites);
- the full 2D view `draw2dscreen` with editor highlights (0x5445E);
- `draw2dgrid` (0x5417F) and `clear2dscreen` (0x54130);
- the 640x480 split-screen 2D mode `qsetmode640480` (0x53FF8).

## Polymost

`polymost.c`, `polymost_soft.c`, `polymost_opengl.c`,
`polymost_opengl_submit.c`, `polymost_opengl_submit.h`, `sdl_opengl.c` and
`sdl_opengl_internal.h` come from
[raynorpat/duke3d](https://github.com/raynorpat/duke3d) at
`8f076354eec06b9b41d53419a801b7db93b7685d` (`source/build/`): Ken
Silverman's Polymost under [BUILDLIC.TXT](BUILDLIC.TXT), with JFBuild's,
icculus's and duke3d's changes. Commit `d8c91dd` holds them unchanged, so
`git diff d8c91dd -- source/build/polymost* source/build/sdl_opengl*` shows
every local change. `draw.c` includes `polymost.c` (as duke3d's `engine.c`
does) when CMake's `LAMEDUKE_POLYMOST` is on, and its `drawrooms`,
`drawmaskwall`, `drawmasks`, `rotatesprite` and `rotatescalesprite` call
Polymost when `rendmode` is not 0. `/p#` picks the renderer: 1 solid colour,
2 software. Renderer 3 (OpenGL) is not built yet: `polymost.c` is compiled
without `USE_OPENGL`, and `setrendermode(3)` gives the classic renderer.

Local changes, in `polymost.c` and `polymost_soft.c`:

- Records are version 5. Wall and sprite `pal` read as 0; sprites have no
  offsets. There are no slopes: floor and ceiling stat bit 2 (a heightmap
  here) is masked off, so heightmap floors draw flat, and `getzsofslope`,
  `getflorzofslope` and `getceilzofslope` (in `draw.c`) return the flat
  `ceilingz`/`floorz`.
- The visible sprites are collected by `draw.c`'s `scansprites`, the classic
  renderer's own pass, into `tspritelist`, `tspritepicnum` and `tspritepal`,
  which the game's `animatesprites` edits. `polymost_drawsprite` draws a copy
  of `sprite[tspritelist[n]]` with `tspritepicnum[n]` and its sprite number
  in `owner`, and `polymost_spritenear` passes that number to `draw.c`'s
  `spritewallfront`.
- The horizon row is `globalhoriz - startumost[0]`: `draw.c` counts rows
  from the top of the 200-row screen, duke3d from the top of the window.
- duke3d's fractional view angle and horizon (`viewyawfraction`,
  `viewhorizonfraction`), the automap marking and `hitallsprites` are gone;
  the editor pick's `hitscan` takes this engine's arguments.
- `polymost_soft.c` addresses rows as `y*bytesperline`, without `ylookup`.

`draw.c` supplies the rest of what duke3d's engine gives `polymost.c`, as
macros and small functions around the `#include`: `draw.c`'s statics are
used directly; `viewingrange`, `xyaspect` and `yxaspect` are 65536 (a 90
degree view at 320x200's aspect, as classic draws it), `xdimenscale` is
`draw.c`'s `viewingrange`, `frameplace` is the frame and `bytesperline` its
pitch; `tempbuf`, `sectorborder` and `sintable` map to `bunchgot`,
`sectorstack` and `sintable()`; `wallfront` is mapped to duke3d's 0/1
answer; `animateoffs` drops its second argument; `widescreen` is 0.
`rotatesprite(sx, sy, picnum, shade, flags, pal)` is drawn as
`polymost_dorotatesprite(sx<<16, sy<<16, 65536, 0, picnum, shade, pal,
dastat)` with `dastat` 2 (320x200 coordinates), plus 8 and the whole screen
as the clip when flag 2 (the view window) is off, 16 when flag 1 (centre) is
off and 1 for flag 4 (translucent); `rotatescalesprite` uses `dastat` 2|8
clipped to the view window. The 2D stays at 320x200.

Text, `plotpixel` and the wall-loop helpers were written for `build.c`. The 1994 renderer, map layouts and
game behavior are still being verified against the DOS executable; this is a
work in progress, not a claim of complete equivalence to the later Build
source release.
