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
| `sdl_main.c`, `sdl_input.c`, `sdl_window.c`, `sdl_sys.c`, `sdl_opengl.c` and headers | SDL2 startup, input, timing and framebuffer presentation. The OpenGL presentation function is a stub; there is no Polymost world renderer. |
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

Text, `plotpixel` and the wall-loop helpers were written for `build.c`. The 1994 renderer, map layouts and
game behavior are still being verified against the DOS executable; this is a
work in progress, not a claim of complete equivalence to the later Build
source release.
