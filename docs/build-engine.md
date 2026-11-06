# Build engine

`source/build/` is the Build engine linked into the Dec 30 1994 `D3D.EXE`, rebuilt from the executable. It is older than Ken Silverman's released Build source. Routines carry the exe address they were recovered from in a comment. The engine is a static library shared by the game, the editor and the map tools. [VENDORED.md](../source/build/VENDORED.md) records provenance, the imported files and the local changes.

## Data model

`board.h` declares the three map records inside `#pragma pack(push, 1)`. Compile-time checks (`board_sector_size` and the others) fail the build if a size drifts.

| Record | Size | Notes |
|---|---|---|
| `sectortype` | 37 bytes | `ceilingheinum`/`floorheinum` (+8, +10) hold heightmap tiles (`floorstat & 2`), not slopes. `ceilingpal`/`floorpal` +28, `visibility` +30, `lotag`/`hitag`/`extra` +31. |
| `walltype` | 35 bytes | `cstat` is a short at +15. No `pal`. `nextsector2`/`nextwall2` at +25; nothing in `D3D.EXE` reads them. `lotag`/`hitag`/`extra` +29. |
| `spritetype` | 38 bytes | `cstat` is one byte (+12). No `pal`, `clipdist` or offsets. `sectnum` +28 and `statnum` +30, as insertsprite (0x50B40) uses them. `lotag`/`hitag`/`extra` +32. |

The later release's `build.h` has 40-byte sectors, 32-byte walls and 44-byte sprites.

| Limit | Value |
|---|---|
| `MAXSECTORS` | 1024 |
| `MAXWALLS` | 8192 |
| `MAXSPRITES` | 4096 |
| `MAXSTATUS` | 1024 |
| `MAXTILES` (engine.h) | 4096 |

The arrays sit at the exe's addresses: `sprite` 0x1C6F10, `sector` 0x1ECF10, `wall` 0x1F6310. So `sector[-1]` is the last 37 bytes of `sprite[]`, which matters for `movesprite` below.

## Map format, version 5

All values are little-endian. `loadboard` (board.c) reads:

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | version, must be 5 |
| 4 | int32 ×3 | start x, y, z |
| 16 | int16 | start angle |
| 18 | int16 | start sector |
| 20 | int16 | number of sectors (1..1024) |
| 22 | 37 × n | sectors |
| | int16 | number of walls (0..8192), then 35 × n walls |
| | int16 | number of sprites (0..4096), then 38 × n sprites |
| | int16 | number of extra records (0..4096), optional |
| | 10 × n | extra records |

The extra records are word_23DE7A (count) and unk_1BCF10 (data) in the exe. Nothing reads them. The engine keeps them only to write them back.

Several shipped maps end right after the sprites (L7, M1, M4, N2–N5, O1, per [map-editor.md](map-editor.md)). The exe's `read()` returns 0 and the count stays as it was, so the board still loads. The port does the same and sets `boardhadextra` to 0. Anything after the extras is ignored, as in the exe.

After the read, `loadboard` calls `initspritelists`, then `insertsprite(sectnum, statnum)` for each sprite in order. The free lists start in index order, so sprite `i` gets slot `i`. It returns 0 on success, -1 otherwise.

There are two writers:

- `saveboard` is the exe's (0x49337). It walks the status lists, so sprites go out in status order and get renumbered. It always writes the extras count.
- `writeboard` is the editor's. Sprites go out in index order: every slot with `statnum < MAXSTATUS`. The extras count is written only if the loaded board had one (`boardhadextra`) or there are extras. So an unedited map saves byte for byte. It writes `filename.tmp`, checks `ferror` and `fclose`, then `plat_rename`s it over `filename`. On any failure it removes the `.tmp`, and the old map stays whole. `newboardextra` clears the extras for a new board.

`checkboard` reports broken wall loops, unpaired `nextwall`/`nextsector`, sprites outside a real sector and a start outside its sector. Several shipped maps have a few, and the game loads them. `sectorofwall`, `loopnumofsector` and `setfirstwall` are not in the exe; they were written for the editor.

Version 4 (`L9.MAP`, `M8.MAP`) is not loaded: `loadboard` rejects any version but 5, as the exe does. `tools/convmap4` converts them. See [map-format.md](map-format.md#version-4).

## Sprite lists

Each sprite is on one sector list and one status list. Both are doubly linked (`headspritesect`/`prevspritesect`/`nextspritesect`, and the same for `stat`). Index `MAXSECTORS` of `headspritesect` and `MAXSTATUS` of `headspritestat` hold the free-list heads.

| Routine | Address | What it does |
|---|---|---|
| `initspritelists` | 0x50A72 | Empties every list and chains all 4096 sprites onto both free lists in index order. A free sprite has `sectnum` `MAXSECTORS` and `statnum` `MAXSTATUS`. |
| `insertsprite` | 0x50B40 | Calls `insertspritestat` (0x50BF3), then `insertspritesect` (0x50B55). Each takes its free-list head and pushes it on the front of the target list. Returns the sector insert's result, or -1. |
| `deletesprite` | 0x50C8F | `deletespritestat` (0x50D7E) then `deletespritesect` (0x50CA3). Each unlinks the sprite and pushes it on the front of the free list. |
| `changespritesect` | 0x50E57 | Deletes from the sector list and inserts again. The slot just freed is the free-list head, so the same index comes back, as in the exe. |
| `changespritestat` | 0x50EA0 | The same for status. |
| `setsprite` | 0x50867 | Sets x/y/z, runs `updatesector`, and changes sector if it moved. -1 when outside every sector. |

## Collision and queries

Hit values use Build's tags: sector + 0x4000, wall + 0x8000, sprite + 0xC000.

| Routine | Address | What it does |
|---|---|---|
| `clipmove` | 0x5225C | `getclips` (0x52589) gathers clip lines from walls and blocking sprites near the move. `raytrace` (0x53532) finds the nearest one crossed. The move slides along it, up to three times; `keepaway` (0x533CF) pushes the goal off the line. Returns the first thing hit, then finds the new sector among the clip sectors, then all sectors, else -1. |
| `getzrange` | 0x55667 | Ceiling and floor z (and what they belong to) for a box of `2*walldist+1` around a point. Walks neighbouring sectors the box reaches, then blocking face, wall and floor sprites in them. |
| `hitscan` | 0x511C4 | Follows a ray through sectors. Nearest hit by Manhattan distance among walls, floors/ceilings and sprites with `cstat & 1` (not floor sprites). Walls with `cstat & 0x40` stop it. The comment notes the exe multiplies in 32 bits before dividing. Returns without touching outputs when `sectnum` is -1. |
| `neartag` | 0x51895 | What a use press reaches: nearest tagged sector beyond a wall, tagged wall and tagged sprite within range, with the distance along the view to the last one found. |
| `cansee` | 0x51016 | Line of sight. Both sectors start the search list. Each wall the line crosses must open onto a sector whose ceiling-to-floor span contains the crossing height. |
| `inside` | 0x4B977 | Even-odd point-in-sector test. 1 inside, 0 outside, -1 for an invalid sector. Uses 64-bit products. |
| `movesprite` | 0x508C9 | `clipmove` on `change * tics << 11`, starting from the sprite's top (`z - 2*tilesizy*yrepeat`), then z by `tics * zchange >> 3`. Rejects a z outside the sector's span; returns sector + 0x4000 then if nothing was hit. |
| `updatesector` | 0x53797 (draw.c) | Keeps the sector if still inside, then tries neighbours, then every sector, else -1. |

In `getclips` a wall blocks when it is one-sided, when the next sector's ceiling or floor (unless parallaxed) leaves less than `ceildist`/`flordist` at `z`, and by `cliptype`: types 0 and 2 block on wall `cstat & 1`, type 1 on `cstat & 0x40`. Type 2 also blocks where the next floor is more than `stepheight` (0x4000, dword_23CE2C) below `z`.

When `clipmove` leaves `sect` at -1, `movesprite` reads the ceiling and floor of `sector[-1]`, as the exe does. That is the last 37 bytes of `sprite[MAXSPRITES - 1]`; the port reads those bytes explicitly.

Other helpers: `rotatepoint` (0x53861), `lastwall` (0x5221E), `dragpoint` (0x52122), `clipinsidebox` (0x4AF61), `clipinsideboxline` (0x4B137) and `lintersect` (0x51F01).

## Math

`math.c` and `buildmath.h`. `loadtables` reads `TABLES.DAT`, 11904 bytes; the exe reads it in six slices at 0x49518 (4096, 4096, 640, 1024, 1024, 1024).

| Offset | Size | Use in the port |
|---|---|---|
| 0 | 4096 | `sintable()`: 2048 shorts, sin(90°) = 16383 |
| 4096 | 4096 | Not read by anything found in the port |
| 8192 | 640 | 320-entry arctangent table for `getangle` (0x4BA86) |
| 8832 | 1024 | Large text font (draw.c) |
| 9856 | 1024 | Small text font (draw.c) |
| 10880 | 1024 | Brightness table, `setbrightness` (display.c) |

`sintable()` returns NULL until `loadtables` succeeds. Angles are 0..2047; cosine is `sintable[(a + 512) & 2047]`, written as `+2560` in the code.

`sqrttable` (dword_180E90) is 2048 Newton square roots (0x4BBA8) built by `initmath`. `ksqrt` (0x4BB6C) scales the input into the table. `krand` (0x55647) is `randomseed = (21 * randomseed + 1) & 0xFFFF`. `wrand` is Watcom's `rand()`, which the game also calls; nothing seeds it.

## ART tiles

`loadpics` (0x4ABCB, tiles.c) loads `tiles000.art`, `tiles001.art` and on until one is missing. The file name argument is ignored. Each file starts with four int32s: version (must be 1), a count, first tile and last tile. Then come `tilesizx` and `tilesizy` (shorts) and `picanm` (int32) for that range, then the pixels.

- Pixels are column-major: `waloff[pic][x * tilesizy[pic] + y]`. An empty tile has `waloff` NULL.
- Each ART file is one `calloc` block, in file order, with `ART_PAD` (65536) bytes extra. The drawers read past a tile's last row when its height is not a power of two. In the exe that read lands in whatever the cache holds next; the pad keeps it inside the block.
- `picsiz` holds floor(log2) of each size: y in the high nibble, x in the low.

Tiles do not go through the cache.

## Palette, lookups and translucency

display.c.

- `loadpalette` (0x49647) reads `PALETTE.DAT`: 768 VGA bytes (6-bit), `numpalookups` shade tables of 256, then an optional translucency table stored as its upper triangle (255 rows, 32640 bytes). The count comes from the file size: `(len - 768) >> 7`, and if that is odd there is a translucency table. The triangle is mirrored into the full 64 KB `transluc`, with the diagonal set to the colour itself.
- `setbrightness` (0x57A5A) maps each 6-bit colour through row `level` of the brightness table, the last 1024 bytes of `TABLES.DAT`, and expands it to 8 bits. Level is capped at 15.
- `loadlookup` (0x13760) reads `LOOKUP.DAT`: a count byte, then records of a palookup number and a 256-byte remap of palookup 0 (makepalookup 0x57239 with no fog colour). It errors out with "Palette remaps not found." when the file is missing. It also sets `pskybits` to 3.
- `setmapmode` (0x53F42) and `qsetmode640480` (0x53FF8) set the 640x350 and 640x480 16-colour modes with the EGA palette. `screencapture` (0x4B390) writes `captNNNN.pcx` from the 320x200 frame.

## Cache

cache.c is the exe's "BootLeg CacheMaster" (0x3BB30 / 0x3BA30). It is a bump allocator over 0x493E0 bytes with 128 slots, not the later hash cache. Nothing is ever freed. `allocache` exits with "Clog Error." past 127 slots and "Out of memory!" when the space runs out. `allocache_store` (0x3BB60) stores through a pointer the decompiler lost, so the port only allocates. In the port only `sounds.c` allocates from it; `game.c` calls `initcache`.

## Engine init

`initengine` (0x4A0C0, engine.c) loads `TABLES.DAT` and `SETUP.DAT`, then calls `setview` over the full screen (639x479 in setup mode 7, else 319x199) and `draw_init`.

`SETUP.DAT` is read as 6 config bytes then 17 key bytes. A short read zeroes the part. `setup_byte` gives the config bytes: 0 graphics mode, 1 digitized-sound menu, 2 music on/off, 3 mouse, 4 comm option, 5 comm speed. The port never writes `SETUP.DAT`. `setgamemode` (0x49845) turns mode 7 into 640x480 and modes 0–6 into 320x200.

`errorexit` prints the message and exits with 0. The exe (0x10C60) also tears down timer and video and prints a CP437 bar first.

`network.c`'s `initmultiplayers` is single-player only: it maps the comm rate to a baud and sets `numplayers` to 1.

## Platform layer

| File | Role |
|---|---|
| `platform.h` | `plat_fopen`, `plat_rename`, `plat_listdir`, `plat_chdir`, `plat_getcwd`. |
| `plat_win32.c` | `plat_fopen` is plain `fopen`; NTFS already ignores case. `plat_rename` is `MoveFileExA` with replace-existing and write-through. |
| `plat_unix.c` | `D3D.EXE` asks for lowercase names and the release ships them uppercase. A read tries the exact name first, then scans the directory with `strcasecmp`. A write, and `plat_rename`'s target, reuse an existing file of any case instead of adding a twin. |
| `sdl_main.c` | `platform_init` / `platform_quit` (SDL_Init, SDL_Quit). |
| `sdl_sys.c` | Timer: `timer_init`, `timer_ticks`, `timer_delay`. |
| `sdl_window.c` | Window, presentation of the 8-bit frame, display modes, and the GL window and context for renderer 3. |
| `sdl_input.c` | `keystatus[256]` (byte_1BCA10) in PC scancode set 1, mouse, and `input_sample`, which runs the 0x102A0 key ramp and getinput (0x30E60). |
| `sdl_audio.c` | 16-bit stereo output device; the mixer fills it from the audio thread. |

## Rendering

The room and sprite drawers (`draw.c`, 0x43F18 `drawrooms` and on) and the imported Polymost renderers are described in [rendering.md](rendering.md). `updatesector` and the text fonts live in `draw.c` too.

## Differences from Ken's released source

In short (details in [VENDORED.md](../source/build/VENDORED.md)):

- Version-5 records of 37/35/38 bytes, not the release's 40/32/44. No slopes; the heinum fields are heightmap tiles. No wall or sprite `pal`, no sprite `clipdist` or offsets, one-byte sprite `cstat`.
- `MAXTILES` is 4096.
- Loose `TILESnnn.ART` and data files, no GRP.
- A bump-allocator cache, not the later hash cache.
- `TABLES.DAT` is the 11904-byte layout above.
- No sector visibility in drawing, no palette fades, no multiplayer.

No specific Build release was imported as a baseline, so this is not a file-by-file diff.
