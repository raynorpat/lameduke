# BUILD map editor for LameDuke

## Intent

The person working in this repo wants to make new maps for the December 1994 `D3D.EXE` beta and edit the shipped ones (`L1`–`L9`, `M1`–`M8`, `N1`–`N8`, `O1`–`O4`, `WAR1`, `WAR2`). Two of those, `L9.MAP` and `M8.MAP`, are version 4, which neither the exe nor the editor reads, so a v4-to-v5 converter is part of this work. They used Ken Silverman's BUILD editor a long time ago and want that editor: the same 2D overhead mode, 3D mode, and keys.

No BUILD.EXE from this beta exists to lift from, so the editor is Ken's released `build.c` taken from [raynorpat/duke3d](https://github.com/raynorpat/duke3d) (`source/build/build.c`, with its `bstub.h` hooks) and adapted to this repo's reconstructed 1994 engine and version-5 maps. This is a decision of the repo owner and replaces the rebuild design's "no map editor" non-goal and `VENDORED.md`'s "not a source for copying" line for this one file.

Success: open any shipped map, change it or build a new one in the editor, save, and play it in this port and in the original `D3D.EXE` under DOSBox, with every sprite and tag meaning what the editor said it meant.

## Goals

- A second executable, `build`, built from the same CMake tree as `d3d` on Windows, macOS, and Linux, in C and SDL2, linking this repo's `buildengine` library. It runs from the game directory and reads the loose files the game reads (`TILES000.ART`–`TILES007.ART`, `PALETTE.DAT`, `LOOKUP.DAT`, `TABLES.DAT`, the maps). No GRP.
- It works like the BUILD editor: 2D mode with the grid, sector/wall/sprite drawing, splitting, joining, dragging, tags, and the tile picker; 3D mode for flying through the map and setting pictures, shade, panning, repeat, and tags on what the mouse points at. Classic keys as `build.c` has them.
- 3D mode draws with this repo's 1994 renderer (`draw.c`), so the editor's view looks like the game's.
- Maps load and save through version-5 files as `D3D.EXE` reads them (`loadboard`): 37-byte sectors, 35-byte walls, 38-byte sprites, and the trailing 10-byte records after the sprites (`boardextra`), which are kept as loaded.
- The editor's save writes sprites in index order. The exe's `saveboard` (`0x49337`) walks the status lists, which after a load reverses them and renumbers every sprite. A map that ended right after its sprites (L7, M1, M4, N2–N5, O1) is saved without the trailing count, as it was loaded. (My call: the result is still a v5 file the original loads, and nothing gets renumbered.)
- Opening a shipped map and saving it with no edits writes a file byte-identical to the original. This holds for every shipped map.
- A map saved by the editor loads in the original `D3D.EXE`.
- Fields the 1996 `build.c` does not know about are kept: `nextsector2`/`nextwall2` on walls, sector `extra`, the heinum fields (heightmap tiles under `floorstat`/`ceilingstat` bit 2 in this beta, not slopes), and `boardextra`.
- `nextsector2`/`nextwall2` are opaque: nothing in `D3D.EXE` reads them, and shipped maps hold only `(-1,-1)` or `(0,0)`. They are never renumbered. A newly drawn wall gets `(-1,-1)`. A split or copied wall keeps the source wall's values, as `build.c` copies every field.
- When an edit inserts, deletes, or renumbers walls or sectors, `nextwall`, `nextsector`, `point2` and `wallptr` are renumbered as `build.c` already does.
- A heightmap tool for floors. This beta's renderer draws a floor as a heightmap when `floorstat` bit 2 is set: the floor picture gives the colours and the tile numbered in `floorheinum` gives the heights (`draw.c`, the `floorstat & 2` path). In 3D mode the tool turns that bit on or off for the floor under the mouse and picks the height tile with the tile picker. With the bit off, `floorheinum` is left as it was. Ceiling heightmaps are not drawn by this renderer, so the tool doesn't offer them. Which keys it uses is the builder's call; the freed slope keys are the obvious slot.
- The editor names sprites and tags as you edit, the way duke3d's stub labels them, with names that come only from this repo:
  - sprite and tile names from `DEFS.CON`'s `define` lines (194 names, read at startup the way the game reads them);
  - sector lotag meanings from the cases `sector.c` handles;
  - SECTOREFFECTOR (tile 175) lotag meanings from `moveeffectors` (lotags 0–29, 128, 129);
  - other tag meanings the game code acts on, where `actors.c`/`sector.c` make the meaning clear.
  Anything without a known meaning is shown as its plain number.
- The editor never changes a tag, picnum, or other field the user did not edit.

## The v4 converter

`L9.MAP` (32,130 bytes) and `M8.MAP` (13,306 bytes) are version 4. The exe rejects them as a missing board. The owner wants them convertible so they can be edited and played.

- No document found gives the v4 layout. The [MAP format wiki page](https://moddingwiki.shikadi.net/wiki/MAP_Format_(Build)) has an empty v4 heading. Ken's [kenbuild.zip](https://advsys.net/ken/buildsrc/kenbuild.zip) has `BUILDINF.TXT`, `BUILD2.TXT` and converters for 5→6, 6→7, 7→6 and 6→5 only. Its `CONVMAP6.C` does confirm this repo's v5 layout field for field, and names the 10-byte tail records "numextras / extratype".
- So the v4 layout is recovered from the two files. Fitting the v5 header and count positions against both files with any record sizes from 16 to 64 bytes finds no consistent fit, so v4 differs in more than record size. Strong checks are available: every wall loop must close through `point2`, every `nextwall` must point back, every sprite's `sectnum` must be in range, heights sit on 1024-unit steps, and the start position must fall inside the start sector. The layout counts as recovered when both files decode with every one of those holding.
- The converter is a small command-line tool, in the spirit of Ken's `CONVMAP6`: `convmap4 IN OUT` reads a v4 map and writes a v5 map through the editor's save path. It never overwrites its input. The shipped `L9.MAP` and `M8.MAP` are not changed in the repo. (My call: a separate tool keeps v4 out of the editor and the game.)
- Fields v4 holds that v5 can't are reported on the console, not dropped silently. Fields v5 has that v4 lacks get the values the shipped v5 maps use most (for example `nextsector2`/`nextwall2` of `(-1,-1)`).
- These two maps have heightmap sectors (26 in L9, 12 in M8). They convert to v5 heightmap sectors (`floorstat`/`ceilingstat` bit 2 with the tile in heinum) if v4 stores them the same way.
- Done when both converted maps load in the editor, pass the checks above, and load in this port and in `D3D.EXE` under DOSBox (the last is an owner check).

## Non-goals

- Polymost, OpenGL, or any renderer beyond `draw.c`.
- Duke3D 1996 sprite, tile, or tag names, or duke3d's `astub.c` game knowledge.
- Launching the map in `d3d` from inside the editor. (Builder's call to add later.)
- A config file or key rebinding. Keys are `build.c`'s defaults. (My call; easy to add later.)
- Slopes, sprite `clipdist`, and other 1996 fields: the v5 records do not hold them. Their tools are removed or replaced (the slope keys by the heightmap tool), not stored somewhere the original would not read.
- New map features the 1994 engine does not draw or simulate.

## Anti-goals

- A label that is wrong: a sprite, tile, or tag named for what it does in 1996 Duke3D, or a guess presented as a meaning.
- A tag or picnum changed by the editor without the user asking.
- A saved map that the original `D3D.EXE` rejects or loads differently, including broken `nextwall2`/`nextsector2` links.
- Bringing duke3d's 1996 engine (`engine.c`, `polymost.c`, its draw code) in alongside this one. Only `build.c` and the stub interface come across; what it needs from an engine is provided by this repo's engine.

## What the adaptation involves

`build.c` is about 6,900 lines and calls roughly 70 functions this engine does not have yet. They fall into groups the builder plans:

- **2D mode drawing:** grid, lines, pixels, text (`draw2dgrid`, `drawline16`-style calls, `printext16`, `printext256`, `clear2dscreen`, `fillscreen16`). This engine already has `drawline16` and `draw2dscreen` lifted from the exe in game mode (no grid) and the 640×350 `setmapmode`.
- **3D mode picking:** what the mouse points at (`searchit`, `searchsector`, `searchwall`, `searchstat`). The rebuild did not port the exe's `searchit` pick because the game never used it.
- **Math helpers and engine housekeeping:** the `*scale` family, `klabs`, `loopnumofsector`, `sectorofwall`, `setfirstwall`, `updatecrc16`, timer and key init and shutdown, `nextpage`, `uninitengine`.
- **Slope calls** (`getceilzofslope`, `alignceilslope`, and the like): removed with the slope tools, since v5 heinum is a heightmap.
- **Polymost and renderer cycling** (`editorcyclerenderer`, `polymost_drawtilescreen`, `setpolymost2dview`, `glredrawlastframe`): removed.
- **Struct fields:** every access to a 1996 field the v5 layout lacks or types differently (sprite `cstat` is one byte here) is adapted or removed.

The game-specific hooks (`ExtInit`, `ExtGetSectorCaption`, `ExtShowSpriteData`, and the rest of `bstub.h`) get a LameDuke stub, the editor-side counterpart of duke3d's `astub.c`, holding the names above.

## Verification

- A check script in `.superpowers/checks/` loads every shipped map in the editor's load path, saves it, and compares the bytes with the original.
- A check that an inserted and then deleted wall or sector leaves a map's `nextwall`/`nextsector`/`point2` links valid, and its `nextwall2`/`nextsector2` values unchanged.
- The owner loads an editor-saved map in `D3D.EXE` under DOSBox (the session here cannot drive DOSBox).

## Licensing and records

`build.c` and `bstub.h` are under BUILDLIC (`source/build/BUILDLIC.TXT`, already in the repo) with their existing headers kept. `source/build/VENDORED.md` records where they came from (the duke3d path and commit) and what was changed. The rebuild design's "a map editor, or a content converter" non-goal, and its "`L9.MAP` and `M8.MAP` ... are not converted" line, are updated to point here. The game itself still rejects v4, as the exe does.

## Left to the builder

- File layout: where the editor and its stub live, the executable's name casing, and how CMake builds it.
- Which of the missing engine functions are lifted from `D3D.EXE` (where the exe has them), taken from the 1996 code's behaviour, or written fresh, as long as the anti-goals hold.
- How the editor shows names (status line, captions, help screens), following `build.c`'s existing places for them.
- The 3D mode's screen size.
