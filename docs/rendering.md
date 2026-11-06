# Rendering

How the port draws a frame: the renderers, the Classic pipeline, the frame buffer and window, and the game's own drawing on top. Maps, sectors, walls, sprites and collision are covered in [the Build engine notes](build-engine.md). Frame pacing and interpolation are in [game speed](game-speed.md).

## The renderers

| Number | Name | Code | Notes |
|---|---|---|---|
| 0 | Classic | `source/build/draw.c` | The 1994 software renderer, reconstructed from `D3D.EXE`. Default. |
| 1 | Polymost debug | `polymost.c`, `polymost_soft.c` | Solid colours. Only through `/p1`; never saved. |
| 2 | Polymost Software | `polymost.c`, `polymost_soft.c` | Indexed buffer at the chosen resolution. |
| 3 | Polymost OpenGL | `polymost.c`, `polymost_opengl.c`, `polymost_opengl_submit.c` | Draws the world with OpenGL. |

Polymost comes from raynorpat/duke3d. [VENDORED.md](../source/build/VENDORED.md) lists the revision and every local change. The Polymost files are not compiled on their own: `draw.c` includes `polymost.c`, which includes its backends. `drawrooms`, `drawmaskwall`, `drawmasks`, `rotatesprite` and `rotatescalesprite` hand off to Polymost when `rendmode` is not 0.

### Build options

| CMake option | Default | Effect |
|---|---|---|
| `LAMEDUKE_POLYMOST` | ON | Defines `POLYMOST` and builds the Polymost files into `draw.c`. |
| `LAMEDUKE_OPENGL` | ON | With `LAMEDUKE_POLYMOST`, defines `USE_OPENGL`, links OpenGL and builds `sdl_opengl.c` and `polymost_opengl_submit.c`. |

### Choosing one

- `duke.cfg`'s `renderer` key holds the saved choice. See the [README table](../README.md#dukecfg) for every video key.
- `/p0` to `/p3` pick the renderer for one run (`premap.c`). This switch is not in the exe. It overrides the saved choice without writing it.
- A choice made in Video Options is saved.
- `LAMEDUKE_RESOLUTION=WxH` overrides the size in `game_setrenderer`. It is a test hook.

The fallback rules are in `setrendermode` and `rendereravailable` (`draw.c`) and `videocfg_apply` (`source/videocfg.c`):

- A value below 0 becomes 0; above 3 becomes 3.
- `rendereravailable` accepts 0 to 3 only when both `POLYMOST` and `USE_OPENGL` are built. Otherwise it accepts only 0. So a Polymost build without OpenGL also falls back to Classic for 1 and 2.
- Renderer 3 becomes 0 when there is no GL draw path, that is when no GL window has been made.
- `videocfg_apply` makes the GL window with `setgamemode_gl` before it selects renderer 3. If that fails, it keeps the previous renderer. If the resolution cannot be set, it keeps the previous renderer and size.
- `LAMEDUKE_GL_FORCEFAIL` set makes the GL context fail, to test this path.

`game_hires()` is true for renderers 2 and 3. Crossing between Classic and hires calls `setgamemode` again and rebuilds the view rectangle (`game_applyvideocfg` in `premap.c`).

## Classic

Classic draws into a static 640x480 indexed `frame` in `draw.c`. Rows are 320 bytes, or 640 in setup mode 7 (`pitch()`). The file comment says Mode 13h, Mode X and the linear modes give the same picture; the port draws it linearly.

### drawrooms (0x43F18)

1. Copy the view's `startumost`/`startdmost` into `umost`/`dmost`, the open rows of each column. Find the camera's sector with `updatesector` (0x53797).
2. `scansector` (0x442B8) walks the visible walls of a sector. `scanwall` (0x46BEC) projects each one into the scan list and queues the sector behind it. `scansprites` collects that sector's sprites into `tspritelist`.
3. Walls are grouped into bunches. Each pass picks the closest bunch with `bunchfront` (0x44848), which uses `wallfront` (0x445AC).
4. `drawalls` (0x4493D) draws that bunch's ceilings, floors and sky. `drawwalls` (0x45D09) draws its walls and opens the sectors behind them.
5. Repeat until no bunches or no open columns remain.

Walls go through `wallscan` (0x4796F) and `vline` (vlineasm 0x7BBE7). Ceilings and floors use `ceilscan` (0x46FEC), `florscan` (0x474A5), `hlinedriver` (0x49A7F) and `hline` (hlineasm 0x7B7BB). Parallaxed surfaces go through `drawsky`. `wallmost` (0x564CE) gives the screen row of a height along a wall.

There are no slopes. Floor stat bit 2 marks a heightmap floor instead. `heightfloor` (0x4FFEE) marches a 64x64 height tile (`floorheinum`) coloured by `floorpicnum`, one cell per 16 map units, from far to near, and fills each column up from `dmost`. Renderer 3 draws these floors as a terrain mesh (`terrainpoly` in `polymost.c`). Renderer 2 draws them flat.

### drawmasks (0x4BFDB)

Masked walls and sprites come last:

1. Shell-sort the collected sprites by depth (`spritesy`).
2. While both lists have entries, test the farthest sprite against the last masked wall with `spritewallfront` (0x447D6). If the sprite is in front, first draw every sprite overlapping that wall that is behind it, then the wall (`drawmaskwall`, 0x4C322). Otherwise draw the sprite.
3. Draw what is left of either list.

Face and wall sprites go through `drawsprite` (0x4CA84) and floor sprites through `drawfloorsprite`. The game's `animatesprites` runs between `drawrooms` and `drawmasks` and edits `tspritepicnum` and `tspritepal`.

### Tiles and reading past them

Tiles load from the loose `TILES000.ART` onward (`loadpics`, 0x4ABCB, in `tiles.c`). The column drawers read past a tile's last row when its height is not a power of two. In the exe that read lands in whatever the cache holds next. The port keeps each ART file as one block in file order, padded by `ART_PAD` (65536 bytes), so the overrun stays in memory it owns. `vline` also keeps the bits of `vplc>>shift` above the low byte in the palookup index, as the asm leaves them in `ebx`.

### Palette, shading and translucency

`display.c` loads them:

- `loadpalette` (0x49647) reads `PALETTE.DAT`: 768 VGA palette bytes, `numpalookups` shade tables, then the translucency table stored as its upper triangle. The table is only present when the file's size says so.
- `loadlookup` (0x13760) reads `LOOKUP.DAT`: each record is a palookup number and a 256-byte remap of palookup 0.
- `setbrightness` (0x57A5A) maps the 6-bit palette through the last 1024 bytes of `TABLES.DAT`.

Every drawer writes `palookup[pal][shade*256 + texel]`. Shade adds a distance term to the surface shade; in `wallscan` it is `scale >> (visibility + 2)`, clamped to the table count. Walls and masked walls take the sector's `floorpal`. Sprites take the sector's pal or the player colour `animatesprites` sets.

Translucent pixels read `transluc[dest*256 + src]`. That covers masked walls with cstat 0x80 (tvlineasm 0x7BC72), sprites with cstat 2, floor sprites, and `rotatesprite` flag 4. Without a table, sprites with cstat 2 draw opaque.

### rotatesprite (0x4F679) and the 2D map

`rotatesprite` draws a tile unscaled, with flag 1 to centre on its picanm offsets, 2 to place and scale it in the view window, and 4 for translucency. Columns step 320 bytes a row, as in the exe. `rotatescalesprite` (0x56C60) draws a tile scaled and turned.

The 2D map is `draw2dscreen` (0x5445E), with `draw2dgrid` (0x5417F), `clear2dscreen` (0x54130) and `drawline16` (0x5393D). `setmapmode` (0x53F42) gives it a 640x350 EGA mode. In the game it shows only the walls and sprites marked seen; in the editor it shows everything. Renderers 2 and 3 draw the game's map at full resolution in a centred 4:3 area.

### The editor pick

The editor sets `searchit` to 2 and `searchx`/`searchy` to the mouse, then draws a frame. In Classic, `pickwall` (0x44DA6) and `picksprite` fill `searchsector`, `searchwall` and `searchstat` as the frame draws:

| `searchstat` | Hit |
|---|---|
| 0 | Wall |
| 1 | Ceiling |
| 2 | Floor |
| 3 | Sprite |
| 4 | Masked wall |

Classic sets `searchit` to 1 once it has a hit; a sprite or masked wall drawn later overwrites it. Polymost answers the same pick with `hitscan` and sets `searchit` to 0. The [map editor notes](map-editor.md) cover the editor.

## Frame buffer and window

`nextpage` presents the frame and counts `numframes`; `showpage` presents without counting. Both call `platform_present` in `sdl_window.c`. `framebuffer()` returns renderer 2's own buffer while it is active, and Classic's `frame` otherwise.

Without a GL window, `platform_present` expands the indexed frame to RGB through `palette` and copies it to an SDL streaming texture. Classic opens its window at three times the frame size. A 320x200 frame opens a 960x720 window, the 4:3 a VGA monitor showed. Renderer 2 opens its window at its own size. Scaling is nearest-neighbour. In full screen, the SDL path keeps 4:3 with black bars.

Once a GL window exists it is kept until shutdown. SDL2 cannot add GL to a window with an SDL_Renderer, so `setgamemode_gl` makes a new window and drops the old one only after the context works. Indexed frames are then shown through GL too (`gl_present8`), fitted 4:3 in the drawable, or at renderer 2's own aspect. Renderer 3 swaps its own frame.

`gl_choosepath` (`sdl_opengl.c`) picks the best draw path the driver has:

| Path | Needs |
|---|---|
| Immediate | Anything |
| VBO | Vertex buffers |
| Shader | VBOs, GLSL, FBOs and GL 2.1 |

Contrast, saturation, supersampling and filtered presentation need the shader path. Supersampling renders at 1440, 2160 or 4320 rows (`2k`, `4k`, `8k`) at the drawable's aspect, clamped to the GPU's limits, and is offered only when that is taller than the drawable. VSync sets the GL swap interval or the SDL_Renderer's vsync. Display modes are 0 windowed, 1 fullscreen (the closest display mode to the window's size) and 2 borderless.

The game keeps its 320x200 coordinates in every renderer. In 2 and 3, the HUD, text and status bar draw at full resolution in a centred 4:3 area. The view uses Hor+ (`glview_begin`/`glview_end` in `draw.c`).

## Video configuration

`source/videocfg.c` reads and writes `duke.cfg`, and `videocfg_apply` applies it in this order: display mode, VSync, resolution, renderer, mip level, colour, supersampling, filter. A setting that fails is reset to its safe value and `videocfg_apply_failed` is set. The keys, defaults and ranges are in the [README](../README.md#dukecfg). `SETUP.DAT` still gives Classic its video mode. `setgamemode` (0x49845) ignores that byte in renderers 2 and 3.

## Game-side drawing

`source/view.c` draws each frame through `displayrooms` (0x14050):

1. `drawview` draws the 2D map instead of the world when the player has it up.
2. On a frame that follows a logic step (`viewtic`), it steps animated walls (0x25550) and the viewscreen (0x25410).
3. `dointerpolations(smoothratio)` blends positions. See [game speed](game-speed.md#what-the-port-does).
4. `drawrooms`, then `animatesprites`, then `drawmasks`, from the player or from a camera sprite.
5. `reflectfloor` shades the pixels of the reflective floor (tile 572).
6. `restoreinterpolations`.
7. Back in `displayrooms`: `displayweapon`, `displaystatus` (status tile 1381 at 160,189) and `displayquotes`. A player at a monitor gets `displaymonitor` instead.

Mirrors (tile 570, 0x21A10) and camera monitors (tile 1408) draw a 96x96 view into a tile with `rendertile` (0x21380). It always uses Classic: it switches to renderer 0, renders, copies the pixels into the tile, switches back and calls `invalidatetile` so Polymost reloads it. The plasma tile (49) edits tile memory and works in every renderer.

## Known gaps

From the code and the README:

- `reflectfloor` returns in renderers 2 and 3. It also returns in setup mode 7, where the pitch is 640.
- `screencapture`, the F12 PCX writer (0x4B390), only writes a 320x200 frame and returns in renderers 2 and 3.
- Renderer 2 draws heightmap floors flat.
- Polymost's software path has no distance shading.
- `rotatescalesprite` in Polymost centres on the tile's offsets plus half its size; Classic centres on its middle.
- The port has no palette fades.
- Some background runs with VSync on have stalled in testing. Foreground behaviour is not verified.
- The README says visual fidelity is still under investigation.
