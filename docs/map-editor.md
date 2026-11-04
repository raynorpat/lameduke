# Map editor

`build` is Ken Silverman's BUILD editor, adapted to this repo's 1994 engine and the version-5 maps the beta reads. It's built next to `d3d` (`build/bin/build`, or `build\bin\build.exe` on Windows). Run it from the game directory, where the ART, palette, tables and `DEFS.CON` files are:

```sh
./build/bin/build L1        # L1.MAP; the .map is added when you leave it off
./build/bin/build MYMAP     # a name that doesn't exist starts a new board
```

The window title shows `BUILD Editor - L1.map` while a map is open, and `BUILD Editor` for a new board until it is saved. The editor starts in 3D mode on a loaded map. Keypad Enter switches between 2D and 3D. In 2D mode, Esc opens the file menu: (N)ew, (L)oad, (S)ave, save (A)s, (Q)uit.

## What it saves

Maps are saved as version 5, which the original `D3D.EXE` loads:

- Opening a map and saving it with no edits writes a byte-identical file. This holds for every shipped map.
- Sprites are written in index order, so a save renumbers nothing.
- A map that ended right after its sprites (L7, M1, M4, N2–N5, O1) is saved without the trailing count, as it was loaded.
- The start position is saved as loaded. The start only moves when you set it.
- A sprite is moved into a different sector on save only if you moved or placed it this session. Some shipped sprites deliberately sit outside the sector they belong to, effectors among them.
- `nextwall2` and `nextsector2` are carried unchanged. Nothing in `D3D.EXE` reads them. A newly drawn wall gets `(-1,-1)`.

## Names

Captions name things the way this beta's code uses them, never by 1996 Duke3D meanings:

- Tiles and sprites get the names from `DEFS.CON`'s `define` lines.
- Sector lotags get a name when the game acts on them. Examples are 1 water, 2 underwater, 9 star door, 16/17 floor down/up, 18/19 elevator down/up, 20 ceiling door, 22 split door, 23 swinging door, 25 sliding door, and 32767 secret.
- SECTOREFFECTOR (tile 175) lotags are named where the effector's code makes the purpose plain. Examples are 0 rotate, 11 swinging door, 17 lift, 24 conveyor, and 27 demo camera.
- Any other tag shows as its number.

F5 and F6 show what the cursor is on. `source/editor/lstub.c` cites the source of each name.

## Keys that differ from duke3d's editor

- **[ and ]** (3D, pointing at a floor) are the floor heightmap tool, which replaces 1996's slope keys. `[` turns `floorstat` bit 2 on or off. `]` picks the height tile with the tile picker (G, type the number, Enter). The renderer reads both the heights and the floor picture as 64×64 grids, and the status line warns when either isn't 64×64. Turning the heightmap off leaves the height tile set. Version 5 has no slopes, and the renderer draws no ceiling heightmaps.
- **T** toggles translucency on a sprite (bit 2) or a masked wall (bit 128). There is only one level, because v5 sprite `cstat` is one byte and the 1994 renderer tests only one wall bit.
- **B** toggles blocking only, and **H** leaves sprites alone. v5 has no hitscan bit.
- **F12** captures the 3D screen to `captNNNN.pcx`. There is no 2D capture.
- There is no **Alt+K** renderer cycling. The 3D view is the game's 1994 renderer, in the mode `SETUP.DAT` selects.
- There is no `build.cfg`. The keys are `build.c`'s defaults.
- Wall and sprite `pal`, sprite `clipdist` and sprite x/y offsets are 1996 fields that v5 can't store. They aren't shown. Pasting a copied wall or sprite onto a ceiling or floor leaves that surface's pal alone.
- The file menu (Esc, L) starts in the folder you ran the editor from.

## The v4 maps

`L9.MAP` and `M8.MAP` are version 4, which neither the game nor the editor reads. The editor refuses to open them, and it never saves over a file that isn't a version-5 map. A failed save says so and keeps your changes marked unsaved. Convert them first:

```sh
./build/bin/convmap4 L9.MAP L9V5.MAP
```

`convmap4` never writes over its input, and it reports any v4 field that version 5 can't hold. The layout and how it was recovered are in [map-format-v4.md](map-format-v4.md). A converted map can be edited, and it plays under a map name the game loads. M8's start lies outside its sector, as in the shipped N8, which uses the same start.

## Testing aids

The check scripts drive the editor with environment variables, as they drive `d3d`:

- `LAMEDUKE_FRAME=file` and `LAMEDUKE_FRAME_AT=n` write the nth frame shown (a PPM) and exit.
- `LAMEDUKE_KEYS=step:code,...` scripts the input. Each entry happens at that `_idle` step:
  - a hex scancode presses that key until the editor handles it;
  - `~code` releases a held key;
  - `@x/y` moves the cursor;
  - `px/y` centres the 2D view;
  - `bN` sets the mouse buttons.
- `LAMEDUKE_NAMES=1` prints every name table and exits.
