# Polymost for LameDuke — Design

Date: 2026-11-04
Status: Approved

## Intent

LameDuke's classic renderer draws the 1994 world at 320x200, or at 640x480
in setup mode 7. The user wants to play it at **higher resolutions, with
texture filtering that can be switched on and off** *(user)*. The way to get
there is to port Ken Silverman's Polymost renderers as they now stand in
[raynorpat/duke3d](https://github.com/raynorpat/duke3d) at `8f07635`
(`source/build/polymost*.c`, `sdl_opengl*.c`). That tree took JFBuild's 2004
Polymost through three phases: the game renderers, the editor, and a
classic-shading palette shader, plus later JFBuild fixes. **All of it comes
over** *(user)*: lameduke should end up where duke3d is now, fitted to this
beta's engine.

The classic renderer stays the default and doesn't change.

The earlier rebuild design and `VENDORED.md` say Polymost is not part of this
binary and duke3d is a guide, not a source. This design deliberately changes
that for the renderer *(user's request)*. `VENDORED.md` records the import
the same way it records `build.c`.

## Goals

1. **Renderers in `d3d`**, numbered as in Polymost: 0 classic (the
   reconstructed 1994 renderer, unchanged), 1 Polymost solid colour (debug),
   2 Polymost software (into the 8-bit frame), 3 Polymost OpenGL.
2. **Renderer 3 at high resolution.** It draws the 3D world at the chosen
   resolution, with Polymost's true perspective. HUD, status bar, menus,
   text, fades and flashes, the mirror, camera screens, the 2D map, and
   screenshots all work, so a whole game can be played there.
3. **Texture filtering** Nearest (the default), Bilinear or Trilinear, as
   in duke3d *(user)*, switchable live. On the GL 2.1 path Nearest keeps the
   exact palette-index drawing, and Bilinear and Trilinear switch to colour
   drawing with shade still from the shade tables, as duke3d does.
4. **Classic shading in renderer 3**, as in duke3d phase 3: the GL 2.1 path
   draws palette indices and resolves them through the game's shade tables
   and palette, so distance darkening, sector shade, palette swaps and
   fades match classic. The GL 1.1 and 1.5 paths come along as duke3d has
   them, as fallbacks.
5. **A new Video Options page** *(user)*, reached from a new **Video
   Options** line on Game Options, drawn in the beta's menu style:

   | Line | Choices | Applies under |
   |---|---|---|
   | Renderer | Classic (0), Polymost Software (2), Polymost OpenGL (3) | always |
   | Resolution | the display's modes from SDL, plus the desktop size | 2 and 3; dimmed under Classic |
   | Display | Windowed, Fullscreen, Borderless | always |
   | Texture Filtering | Nearest, Bilinear, Trilinear | 3 only; dimmed otherwise |
   | Mip Level | 0–15 | 3 only; dimmed otherwise |
   | VSync | On, Off | always |
   | Contrast | slider, 0.5–1.5, default 1.0 | 3 on the GL 2.1 path; dimmed otherwise |
   | Saturation | slider, 0–2, default 1.0 | 3 on the GL 2.1 path; dimmed otherwise |
   | Supersample | Off, 2K, 4K, 8K | 3 on the GL 2.1 path; dimmed otherwise |
   | Back | | |

   VSync, Contrast, Saturation, Supersample and Mip Level are added at the
   user's request and behave as on duke3d's VIDEO page (`docs/rendering.md`
   there): supersampling draws at 1440, 2160 or 4320 pixels tall and scales
   down, a level not taller than the window is dimmed, and a frame the GPU
   can't make turns it off with a message. Mip Level drops that many
   mipmap levels from every texture, as duke3d's `GLTextureMipLevel` does.

   The debug renderer (1) isn't on the list *(user)*. Options that don't
   apply to the current renderer are dimmed *(user)*. Each change takes
   effect at once, without a restart *(my call, as on duke3d's VIDEO page)*.
   Switching to or from OpenGL may rebuild the window once.
6. **`duke.cfg`** *(user)*: a new plain-text file in the game's working
   directory holding every Video Options setting.
   It's read at startup and written when a setting changes or the game
   exits. A missing file means defaults (renderer 0, Nearest, windowed, 640x480
   for renderers 2/3). Values out of range are clamped and unknown keys
   ignored. `SETUP.DAT` stays as it is: it's still read, never written, and
   still picks classic's 320x200 or 640x480. In renderers 2 and 3 the
   resolution comes from `duke.cfg` and the mode byte is ignored, so the
   game paths that test `setup_byte(0) == 7` (`player.c`, `premap.c`) go
   through a helper that knows the renderer *(my call)*.
7. **Command line**: `/p<n>` (`/p0`–`/p3`) picks the renderer for one run
   and isn't saved *(the switch letter is my call; `/r` is demo record,
   and `/p` is unused)*. `/p1` is the only way to reach the debug renderer.
   `show_help` lists it.
8. **The BUILD editor** gets Polymost in its 3D mode, as duke3d phase 2 did,
   with **its own `build.cfg`** *(user)*: renderer, 3D resolution, display
   mode, filtering and the other Video Options settings *(the extra keys are
   my call; the editor has no menu for them)*. The editor has no menu, so Alt+K cycles renderers in
   3D as in duke3d *(my call; it's free in this `build.c`)*, and the choice is
   written back. 2D mode stays 8-bit software at 640x480. The editor's
   tile browser and prompts work under GL (duke3d's
   `polymost_drawtilescreen` and last-frame redraw).
9. **The JFBuild fixes duke3d ported** come along with its Polymost files,
   along with the GL map view, fullbright lights (palette indices 240–254) and
   non-power-of-two tiles, where they apply to this engine.
10. **Game speed** *(user)*: the game runs too fast. It does so in DOSBox
   and on a real Pentium too, but plays right on a 486 *(user)*. So the
   original is CPU-speed dependent: something in the 1994 loop advances per
   frame drawn rather than per clock tick, and faster machines draw more
   frames. **The target is the speed it plays at on a 486**, in every
   renderer and at any frame rate.
   - The cause is found first, in `D3D.EXE` and in `game.c`/`actors.c`/
     `player.c`, before anything changes: which logic runs once per frame
     (or per `domovethings` call) instead of scaling with `synctics`.
     `play_level` calls `domovethings` once per presented frame with however
     many ticks passed, so per-call logic speeds up with the frame rate. The
     music-byte 6-step clock (`SETUP.DAT` byte 2 is set in the shipped file)
     is a second thing to check, not the assumed cause.
   - The fix paces that logic at a fixed rate equal to what a 486 managed
     *(my call: the rate is measured, e.g. DOSBox at 486-class cycles, and
     recorded with the measurement)*. Drawing may still run faster, so
     high frame rates under Polymost keep their benefit.
   - The recorded demos keep playing in sync and at the same speed they
     play now (`demo_hz` already tells 120 Hz and 720 Hz recordings apart).
   - This is its own task, early in the plan, independent of Polymost.
   - **Smooth drawing** *(user, added after Task 2)*: as in duke3d, each
     frame draws the camera, sprites and moving sectors interpolated
     between the last two logic steps (`smoothratio`), in every renderer
     *(the scope is my call)*. Interpolation is draw-only: game state and
     demos are unchanged.
11. **Docs**: README covers renderers, the Video Options page, `duke.cfg`,
   `/p`, and the editor's `build.cfg` and Alt+K. `VENDORED.md` records
   the import commit and every local change, as it does for `build.c`.

## Non-goals

- Changing the classic renderer, or adding resolutions to it.
- md2/hightile/texture packs, texture cache, OSD console, voxels.
- duke3d extras not asked for: internal-resolution scaling
  (`GLInternalWidth`/`Height`), a brightness row, screenshot formats,
  widescreen status bar art.
- Multiplayer (still stubbed).
- A 2D map editor mode in GL.

## Anti-goals

- Classic renderer output changing in any pixel, or demo sync breaking in
  Classic. The demos `MYDEMO_0`–`7` must play the same there.
- Renderers 1–3 may drift slightly from Classic's game state *(user)*. In
  this beta, drawing changes the game: `animatesprites` (`view.c`) updates
  the real sprites `drawrooms` found visible (`cstat` bits, `hittype`
  counters), and the mirror pass depends on `gotpic`. Polymost finds a
  different visible set. Making renderers 1–3 match Classic exactly is a
  later phase *(user)*, not this one.
- `SETUP.DAT` getting written or its layout changing.
- A build without OpenGL (`LAMEDUKE_OPENGL=OFF`) failing to compile or run.
  Under it, renderers 1–3 fall back to 0 and the menu offers Classic only.
- The editor writing maps any differently than it does now.

## How this engine differs from duke3d's (what the port must handle)

- **Records**: v5 sector 37 bytes, wall 35, sprite 38 (`board.h`). There are
  no slopes (`heinum` fields are heightmap tile numbers), no wall or sprite
  `pal`, and no sprite `clipdist` or offsets. Polymost's slope code works
  with flat sectors. Missing `pal` reads as 0.
- **Heightmap floors** (`floorstat & 2`): `draw.c` ray-casts these from a
  colour tile and a height tile. Polymost has nothing like them. In renderer 3
  they must show their shape, not a flat floor; how (for example a mesh
  built from the 64x64 height tile) is left to the builder. Renderer 2 may
  draw them flat *(my call)*.
- **Double-linked walls** (`nextsector2`/`nextwall2`): the classic renderer
  ignores them, and so may Polymost.
- **Renderer state is `static` in `draw.c`** (`globalposx`, `gotsector`,
  bunches and so on) and `rotatesprite` has the 1994 signature
  `(sx, sy, picnum, shade, flags, pal)` plus `rotatescalesprite`. Polymost's
  entry points (`polymost_drawrooms`, `_drawmasks`, `_dorotatesprite`) hook
  in through `drawrooms`, `drawmasks` and the two sprite calls when the
  renderer isn't 0, the way duke3d's `engine.c` does.
- **The 320-wide game screen.** The HUD, status bar, menus and text are laid
  out for 320x200. Some game code reads the frame directly (the
  `view.c` reactor/plasma loops at pitch 320, screenshots in `game.c`). In
  renderers 2 and 3 the game keeps laying out its 2D in 320x200
  coordinates, but **the 2D is drawn sharp at the full resolution**
  *(user)*: `rotatesprite`/`rotatescalesprite` go through
  `polymost_dorotatesprite`, scaled from 320x200, and follow the filtering
  setting in renderer 3. Text, fills, fades and anything else the game
  draws in 2D must come out at the full resolution too, not as a chunky
  layer. Effects that read the frame keep working in classic and are
  skipped or emulated in 2/3; that's left to the builder, but each must be
  listed in `VENDORED.md`.
- **Wide screens: Hor+** *(user)*. In renderers 2 and 3, a resolution wider
  than 4:3 widens the 3D view's horizontal field of view and keeps the
  vertical one, including when looking up or down (duke3d's `widescreen`
  handling is the model). The 3D view fills the whole width. The 320x200
  2D layout (weapon, status bar, text) is centred in a 4:3 area over it.
  Full-screen 2D pictures with no 3D behind them (menus over the title,
  level screens) get black bars at the sides *(my call)*. Classic
  can't widen: in Fullscreen or Borderless its 320x200 or 640x480 image is
  scaled up to fit and centred with black bars, keeping its 4:3 shape.
- **Presentation** is an `SDL_Renderer` streaming texture
  (`sdl_window.c`), and `sdl_opengl.c` is a stub. Renderer 3 needs a GL
  window. As in duke3d, once a GL context exists it stays, and renderers 0–2
  present through it.
- **Build**: one static `buildengine` library shared by the game, the editor
  and the tools. `LAMEDUKE_OPENGL` (default ON) already links OpenGL.

## Constraints

- Windows (MSVC), macOS and Linux build through the existing CMake presets.
- GL entry points beyond 1.1 are loaded at runtime, so the binary runs on a
  1.1 driver.
- AGENTS.md commit rules. As with `build.c`, the first commit adds duke3d's
  files unchanged, so `git diff` shows every local change.

## Success check

- **The user's check:** start `d3d`, open Game Options → Video Options, pick
  Polymost OpenGL at 1920x1080 Borderless, toggle filtering, and play L1;
  quit and restart and the settings are kept. Then do the same in the editor
  with `build.cfg` and Alt+K.
- **Automated:** the classic renderer's frame for a fixed L1 viewpoint and
  the demo playback are pinned before Polymost lands and stay identical
  after. `duke.cfg`/`build.cfg` read, write and clamp are tested headless.

## Left to the builder

- `duke.cfg` and `build.cfg` key names and format (duke3d's `key = value`
  style is the obvious model).
- How text, fills and fades reach full resolution in renderers 2 and 3
  (duke3d's GL 2D paths are the model).
- How the Resolution list is built and stepped, and what Fullscreen does
  when the chosen size isn't a display mode (duke3d's rules are the model).
- The heightmap floor method in renderer 3.
- Splitting the work into commits and tasks.
