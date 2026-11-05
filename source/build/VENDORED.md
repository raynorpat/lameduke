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
| `math.c`, `buildmath.h` | Sine-table loading and engine math helpers. |
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
2 software, 3 OpenGL. `polymost.c` gets `USE_OPENGL` (and so
`polymost_opengl.c`) when CMake's `LAMEDUKE_OPENGL` and `LAMEDUKE_POLYMOST`
are both on, which also builds `sdl_opengl.c` and `polymost_opengl_submit.c`.
Otherwise, or when the GL window can't be made, `setrendermode(3)` gives the
classic renderer.

Local changes, in `polymost.c` and `polymost_soft.c`:

- Records are version 5, with no wall or sprite `pal`. As in classic, walls
  and masked walls take their sector's `floorpal` and sprites `tspritepal`
  (the sector's pal, or the player colour `animatesprites` sets). Sprites
  have no offsets, and their shade is classic's `shade + 6 + sectorshade`. There are no slopes: floor and ceiling stat bit 2 (a heightmap
  here) is masked off in the flat surface pass; renderer 3 adds the terrain
  mesh described below, while renderer 2 draws it flat. `getzsofslope`,
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
used directly; renderer 1 uses the 90-degree 320x200 aspect, while
renderers 2/3 use the target dimensions and Hor+ aspect described below.
`xdimenscale` scales by view width, `frameplace` chooses the renderer's
frame and `bytesperline` its pitch; `tempbuf`, `sectorborder` and `sintable` map to `bunchgot`,
`sectorstack` and `sintable()`; `wallfront` is mapped to duke3d's 0/1
answer; `animateoffs` drops its second argument. Renderer 1 keeps its original
320x200 aspect; renderers 2/3 use the Hor+ and 2D transforms below.
`rotatesprite(sx, sy, picnum, shade, flags, pal)` is drawn as
`polymost_dorotatesprite(sx<<16, sy<<16, 65536, 0, picnum, shade, pal,
dastat)` with `dastat` 2 (320x200 coordinates), plus 8 and the whole screen
as the clip when flag 2 (the view window) is off, 16 when flag 1 (centre) is
off and 1 for flag 4 (translucent); `rotatescalesprite` uses `dastat` 2|8
clipped to the view window. The game keeps its 320x200 layout coordinates.

Known drift from classic in renderers 1 and 2: Polymost's software path has
no distance shading (upstream), and `rotatescalesprite` centres on the tile's
picanm offsets plus half its size where classic centres on its middle (the
same for tiles with no offsets).

Renderer 3, local changes for the OpenGL backend:

- `sdl_window.c` is this engine's, with duke3d's GL window and context
  ported into it: `setgamemode_gl`, the draw paths through
  `gl_initdrawpath`, the shader path's present and supersampling, the last
  frame for `glredrawlastframe`, and the 8-bit frame shown through the GL
  context (4:3, black bars) once there is one. The context is made once and
  kept until shutdown. Display modes are 0 windowed, 1 fullscreen, 2
  borderless (duke3d numbers them otherwise); the SDL_Renderer path keeps
  4:3 in full screen. `LAMEDUKE_GL_FORCEFAIL` set makes the context fail
  (duke3d's `gl_forcefail` 4). Left out: duke3d's internal size, brightness,
  fades around a change, the overlay, Alt+Enter, the mode-list cleanup and
  screenshots. `platform_capture` reads renderer 3's frame for
  `LAMEDUKE_FRAME`.
- `sdl_opengl.c` and `polymost_opengl_submit.c` include `sdl_window.h`
  (`glinfo`, `xres`, `yres`) instead of duke3d's `build.h`.
- `draw.c` gives the GL backend `palette_t` and `curpalette`, copied from
  `palette[]` as Polymost draws (the 1.1/1.5 textures remade on a change,
  as duke3d's `VBE_setPalette` does), `getfadedpalette`, `setpolymost2dview`
  (from duke3d's `engine.c`) and `MAXPALOOKUPS`. This engine has no palette
  fades, so `palfadedelta` is 0, and its sky no distance shade
  (`globalpisibility` 0). `asm1`, `asm2` and `globalpolytype` exist only for
  `polymost_fillpolygon`, which nothing calls.
- `glview_begin`/`glview_end` temporarily expose the actual target size to
  renderers 2/3 and restore the game's logical view afterward. Renderer 3
  uses `xres`/`yres`; renderer 2 has a separate indexed buffer. Hor+ uses
  viewingrange = width*3/(height*4), with the 4:3 pixel aspect and the horizon
  scaled by height, preserving vertical field of view while looking up/down.
  Sprite sorting uses those same projected coordinates, without scaling
  them a second time. Face sprites use the wall pass's eye-depth units
  (`gxyaspect*gyxscale`), preserving distance fog under the new aspect.
- `polymost_fogvis` shades at this engine's classic wall rate
  (`depth>>(visibility-3)` shades, `draw.c`'s `wallscan`) instead of
  duke3d's `globalvisibility` and sector visibility, which the 1994 engine
  doesn't have. Sprite fog is twice the wall rate, matching Classic's
  `depth>>(visibility+4)` on a depth scaled by 256. Flat floors and ceilings
  use 4/5 of the wall rate, matching `hlinedriver`'s distance scale.
- HUD sprites, text and pixels draw at full target resolution in a centered
  4:3 area, with 320x200 game coordinates. `polymost_dorotatesprite` uses
  duke3d's widescreen path and the beta's 1.2 pixel aspect. `printext256`
  emits scaled glyph pixels; `plotpixel` emits indexed software rectangles
  or GL quads, including the index-FBO path. Opaque 320x200 full-screen
  pictures clear their sides to black. The beta's actual status tile 1381
  is 320x22 with zero picanm offsets (not 32 rows); its original placement
  centered at (160,189) is preserved, about 119 rows tall at 1080p.
- `setrenderresolution(w,h)` (draw.h) allocates the renderer-2 frame and
  applies an active renderer's target size, returning -1 on invalid sizes,
  allocation or video failure. Accepted sizes are 320..8192 by 200..8192;
  defaults are 640x480. It does not change the saved logical view. The
  software rasterizer's row-edge scratch array supports 8192 rows.
  `LAMEDUKE_RESOLUTION=WxH` is an inert game test hook until Video Options
  calls the API. `setgamemode` ignores the setup mode byte in renderers 2/3.
  Crossing between Classic and hires rebuilds the game's logical dimensions
  and view rectangle: mode 7 returns to its 640x480 full view, while hires
  restores the player's 320x200 view size. Renderer 2's SDL window uses the
  requested size even at 320x200; only the legacy software presentation
  stretches a 320x200 frame into a 960x720 window.
- Heightmap floors in renderer 3 add a depth-tested mesh after the opaque
  world pass. The 64x64 height tile repeats in world coordinates, with one
  sample per 16 map units and z = floorz - (sample << 8), matching the beta's
  26.6 ray-grid coordinates. Inner wall loops are joined to the outer
  boundary by visible bridges; coincident bridge corners are excluded from
  strict ear-interior tests. The resulting boundary is ear-triangulated; each
  triangle clips the 16-unit grid, split along each cell's diagonal, using
  linear height interpolation. The existing polygon mapper supplies texture
  filtering, palette shading, pitch and near clipping. Renderer 2 remains
  flat. The mesh uses the heightmap's direct color-tile coordinates, as the
  beta does, rather than the flat floor's slope/panning rules.
- Frame readers: `view.c`'s 320-pitch reflective-floor postprocess (tile 572)
  is skipped in 2/3. The 96x96 mirror (570) and camera-monitor (1408) tile
  copies are emulated by temporarily selecting Classic for that offscreen
  render, copying its indexed pixels, and restoring the selected renderer.
  Plasma (49) edits tile memory, not the high-resolution frame, and remains
  enabled. `invalidatetile` refreshes palette and index texture caches after
  plasma or camera/mirror edits, including edits while another renderer is
  active. The fixed 320x200 F12 PCX writer is skipped in 2/3; the debug BMP
  capture uses full-resolution presentation. `game_hires` gates player and
  premap setup-mode-7 tests. SETUP.DAT is never written.
- GL clears only the 3D viewport's color before drawing it: rasterization
  gaps cannot retain a previous camera's pixels after an FBO recreation or
  renderer switch. `platform_capture` resolves the same present shader as
  the window, then reads the drawable, so supersampling captures remain
  window-sized and include color correction. Renderer 2 fills the window
  through either SDL or retained GL; Classic's retained-GL presentation
  keeps its 4:3 bars.

`source/build/math.h` is now `buildmath.h`: with `source/build` on the
include path, MSVC found it for `polymost.c`'s `<math.h>`.

Text, `plotpixel` and the wall-loop helpers were written for `build.c`. The 1994 renderer, map layouts and
game behavior are still being verified against the DOS executable; this is a
work in progress, not a claim of complete equivalence to the later Build
source release.

- `draw.c`: map editor logical picking coordinates into the Polymost drawable
  while drawing, and provide the GL tile-browser view and logical pixel mapping.
- `editor/build.c`: restore configured 3D modes, cycle renderers, use the GL
  tile view, and redraw the last frame beneath 3D quit prompts.
