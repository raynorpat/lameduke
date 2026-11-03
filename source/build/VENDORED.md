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
| `CMakeLists.txt` | Builds the engine as a static library for the game. |

The game reads the original loose files rather than a GRP archive. There is no
separate Build map editor in this tree. The 1994 renderer, map layouts and
game behavior are still being verified against the DOS executable; this is a
work in progress, not a claim of complete equivalence to the later Build
source release.
