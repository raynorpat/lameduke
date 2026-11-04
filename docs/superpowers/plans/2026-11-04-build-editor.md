# BUILD Map Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A `build` map editor (Ken's `build.c` from raynorpat/duke3d, adapted to this repo's 1994 engine and v5 maps) with LameDuke names, a floor heightmap tool, and a `convmap4` v4-to-v5 converter.

**Architecture:** `build.c` and `bstub.h` come from duke3d commit `2f141cac5a0d2106780dff15fc2e2b8757618bfc` (`source/build/`). Everything they need from an engine is added to this repo's `buildengine` library, lifted from `D3D.EXE` wherever the exe's engine has it (the engine in the exe was shared with the 1994 editor, so editor-mode branches such as the `searchit` pick at `word_23DE46` exist there). A LameDuke stub (`source/editor/lstub.c`) implements `bstub.h`. A plain-C checker (`tools/mapcheck.c`) is the test harness: it loads, writes, compares and validates maps.

**Tech Stack:** C, SDL2, CMake (as the game). IDA on `D3D.EXE` for lifts.

**Spec:** `docs/superpowers/specs/2026-11-04-build-editor-design.md`

## Global Constraints

- Maps are version 5: 37-byte sectors, 35-byte walls, 38-byte sprites, then the optional 10-byte `boardextra` records. A file `D3D.EXE` loads must still load.
- Opening a shipped v5 map and saving it with no edits gives a byte-identical file.
- The editor saves sprites in index order, and leaves out the trailing extra count when the loaded map had none.
- `nextsector2`/`nextwall2` are opaque: never renumbered. A newly drawn wall gets `(-1,-1)`. A copied or split wall keeps the source's values.
- A label is a name from `DEFS.CON`, a sector lotag `sector.c` handles, an SE lotag `moveeffectors` handles, or another meaning `actors.c`/`sector.c` make clear. Anything else is a plain number. No 1996 Duke3D names.
- The editor never changes a field the user did not edit.
- The 3D view draws with `draw.c`. No Polymost, no renderer cycling, no `build.cfg`.
- Only `build.c` and `bstub.h` come from duke3d. None of its engine, Polymost or game files.
- The game's `saveboard` (0x49337, used by `SAVE_VLN.SAV`) stays exe-faithful; the editor's save is a separate function.
- The game still rejects v4 maps. The shipped `L9.MAP` and `M8.MAP` are not modified.
- Builds on Windows, macOS and Linux. Existing checks (`args`, `con`, `demo`, `maps`) keep passing.
- Commit messages: `subsystem: what it does`, one or two lines.

## Review Focus

1. A map with sprites whose `owner` or other index fields name other sprites must survive a round trip unchanged. Index-order save covers it; the round-trip check over all shipped maps pins it (Task 1).
2. Deleting sector 0 or wall 0 must not rewrite the `(0,0)` `nextwall2`/`nextsector2` pairs that L1 and M5 hold on hundreds of walls (Task 4).
3. Sprite `cstat` is one byte in v5: a 1996 key that sets a bit above 7 (for example 0x100 or 0x8000) must not silently truncate into a different meaning (Task 3).
4. A sector lotag the stub names must be one `sector.c` really handles; one it doesn't must show as a number (Task 5).
5. A converted v4 map must give the game a valid start: the start position inside the start sector, every sprite in a real sector (Task 7).

---

### Task 1: Editor save path and the map checker

**Files:**
- Modify: `source/build/board.c`, `source/build/board.h`
- Create: `tools/mapcheck.c`, `tools/CMakeLists.txt`, `.superpowers/checks/editor.sh`
- Modify: `CMakeLists.txt` (add `tools`)

**Interfaces:**
- Produces: `int writeboard(const char *filename, int px, int py, int pz, short ang, short sect);` sprites in index order, `boardextra` written only if the loaded board had the count. `extern unsigned char boardhadextra;` set by `loadboard`. `int checkboard(char *why, int len);` returns 0 when every wall loop closes through `point2`, every `nextwall` points back, `nextsector` matches the other wall's sector, every sprite `sectnum` is in range, and the start is inside `cursectnum`; otherwise 1 and a reason.
- `mapcheck roundtrip IN OUT` (load, writeboard, exit 0), `mapcheck validate MAP` (load, checkboard, print, exit 0/1), `mapcheck same A B` (exit 0 if byte-identical).

- [ ] **Step 1:** Write `.superpowers/checks/editor.sh`: for every `*.MAP` with version 5, `mapcheck roundtrip` into the scratch dir, then `mapcheck same` against the original, and `mapcheck validate`. Prints `editor: all pass` or a FAIL line per map.
- [ ] **Step 2:** Run it. Expected: fails (no `mapcheck`).
- [ ] **Step 3:** Implement `writeboard`, `boardhadextra`, `checkboard` in `board.c`, and `tools/mapcheck.c`. `mapcheck` links `buildengine` only and needs no window.
- [ ] **Step 4:** Run `editor.sh`. Expected: `editor: all pass` for all 29 v5 maps. Run the four existing checks. Expected: all pass.
- [ ] **Step 5:** Commit `build: add an index-order map writer and a map checker`.

### Task 2: Engine services the editor needs

**Files:**
- Modify: `source/build/draw.c`, `draw.h`, `display.c`, `display.h`, `math.h`, `board.c`, `board.h`, `engine.c`, `engine.h`, `sdl_input.c`, `sdl_input.h`

**Interfaces:**
- Consumes: `build.c`'s calls, listed by the survey: `printext16`, `printext256`, `draw2dgrid`, `drawline16`, `drawline256`, `plotpixel`, `clear2dscreen`, `fillscreen16`, `clearview`, `clearbuf`, `clearbufbyte`, `nextpage`, `qsetmode640480`, `setview`, `loopnumofsector`, `sectorofwall`, `setfirstwall`, `updatecrc16`, `klabs`, `min`/`max`, the `mulscale`/`divscale`/`dmulscale` variants, `uninitengine`, `initkeys`/`uninitkeys`, `inittimer`/`uninittimer`, `_idle`, `getmousearea`, and the globals `editstatus`, `searchit`, `searchx`, `searchy`, `searchsector`, `searchwall`, `searchstat`.
- Produces: each of those with `build.c`'s 1996 signature.

- [ ] **Step 1:** For each name, check IDA first: if `D3D.EXE` has it (search its engine for the routine or the editor branch), lift it with its address in a comment, in the house style. Known: the `searchit` pick (`word_23DE46`, set to 2 for one frame; `drawrooms` then records what lies under `searchx`,`searchy`), `drawline16` (0x5393D), `draw2dscreen` (0x5445E). Text uses the fonts in `TABLES.DAT`: the 8x8 font at offset 8832 and the small font at 9856 (both 1024 bytes, 8 bytes a character).
- [ ] **Step 2:** What the exe lacks is written from the 1996 behaviour `build.c` expects, kept small.
- [ ] **Step 3:** Verify with `mapcheck pick MAP X Y`, a mode added here: load, draw one 3D frame at the map start with `searchit = 2`, `searchx/searchy = X,Y`, print `searchstat searchsector searchwall`. Run it on L1 at the screen centre and at a corner. Expected: centre names a wall or sprite in front of the start (compare with the frame dump of L1 at that position), a top corner names the ceiling (`searchstat 1`) of the start sector.
- [ ] **Step 4:** Run all checks. Expected: all pass, and the demo trace unchanged.
- [ ] **Step 5:** Commit `build: lift the editor's search pick, text, grid and helpers`.

### Task 3: build.c adapted, building and running

**Files:**
- Create: `source/editor/build.c`, `source/editor/bstub.h` (from duke3d at the pinned commit, headers kept), `source/editor/lstub.c` (empty `Ext*` hooks for now), `source/editor/CMakeLists.txt`
- Modify: `source/CMakeLists.txt` or root `CMakeLists.txt` (add `source/editor`)

**Interfaces:**
- Consumes: Task 1 `writeboard`, `boardhadextra`; Task 2 services; existing `loadboard`, `draw2dscreen`, `drawrooms`, `drawmasks`, `setmapmode`, `setgamemode`, `loadpics`, `initengine`, `getmousevalues`, `keystatus`.
- Produces: executable target `buildeditor` with `OUTPUT_NAME build`. Dev aids as in `d3d`: `LAMEDUKE_FRAME=file` / `LAMEDUKE_FRAME_AT=n` dump the nth presented frame and exit; `LAMEDUKE_KEYS=frame:scancode,...` presses keys on given frames. These are the harness for Tasks 4-6.

- [ ] **Step 1:** Copy the two files. Remove: Polymost and renderer cycling (`editorcyclerenderer`, `editorclamprenderer`, `polymost_drawtilescreen`, `setpolymost2dview`, `glredrawlastframe`, `setmmxoverlay`, `getrendermode`), `editormode.h`/`build.cfg` (keys are `build.c`'s defaults), `-renderer`. Replace slope code (`getceilzofslope`/`getflorzofslope` become `ceilingz`/`floorz`, the `alignceilslope`/`alignflorslope` keys are removed; Task 6 reuses their slots). Remove `wall/sprite .pal`, sprite `clipdist`, `xoffset`, `yoffset`, the sector `filler`. Map field names to the v5 struct (`nextsector`/`nextwall` stay; `nextsector2`/`nextwall2` are never touched). Sprite `cstat` keys that set bits above 7 are removed. `loadnames` (names.h) becomes a call into the stub (Task 5); for now no names. Save calls `writeboard`.
- [ ] **Step 2:** Build. Expected: no errors, no new warnings.
- [ ] **Step 3:** Run `build L1` with a frame dump at frame 5. Expected: the 2D view of L1 with grid, walls and sprites. Press keypad Enter by `LAMEDUKE_KEYS` and dump a later frame. Expected: the 3D view at the start, matching `d3d`'s first L1 frame apart from the editor overlay.
- [ ] **Step 4:** Save without edits via `LAMEDUKE_KEYS` (Ctrl+S, then Esc to quit; scancodes from `build.c`), then `mapcheck same` against `L1.MAP`. Expected: identical. Run all checks.
- [ ] **Step 5:** Commit `editor: bring Ken's build.c from duke3d and adapt it to the v5 engine`.

### Task 4: Index fields stay right through edits

**Files:**
- Modify: `source/editor/build.c` (`insertpoint`, `deletepoint`, `deletesector`, `copysector`, the sector join/split paths), `.superpowers/checks/editor.sh`

**Interfaces:**
- Consumes: Task 3 key harness, Task 1 `checkboard`.
- Produces: `mapcheck diff A B` lists changed fields per record (used to prove nothing unedited changed).

- [ ] **Step 1:** Add to `editor.sh`: on L1 and M5, scripted edits (insert a point on wall 0 and delete it again; delete sector 0) then save. `mapcheck validate` must pass and `mapcheck diff` must show only the records the edit touched, with every `nextwall2`/`nextsector2` value identical to the original's after renumbering.
- [ ] **Step 2:** Run. Expected: fails until the edit paths copy v5 fields whole (whole-struct copies already do; any field-by-field 1996 copy misses `nextwall2`/`nextsector2`/`extra`), and new walls get `(-1,-1)`.
- [ ] **Step 3:** Fix the edit paths.
- [ ] **Step 4:** Run `editor.sh` and all checks. Expected: all pass.
- [ ] **Step 5:** Commit `editor: keep v5 wall fields through insert, delete and copy`.

### Task 5: LameDuke names

**Files:**
- Modify: `source/editor/lstub.c`
- Create: `.superpowers/checks/names.sh`

**Interfaces:**
- Produces: `const char *tilename(short picnum);` from `DEFS.CON`'s `define` lines whose value is a tile the game uses as a picnum (read at startup with the game's CON reader rules, case-insensitive open). Captions: `ExtGetSectorCaption`, `ExtGetWallCaption`, `ExtGetSpriteCaption`, and the `ExtShow*Data` panels. Tables `sectorlotagname[]` and `selotagname[]` with a comment per entry giving the `sector.c`/`actors.c` function and case it comes from. `LAMEDUKE_NAMES=1` prints every table entry and exits (for the check).

- [ ] **Step 1:** Write `names.sh`: runs `build` with `LAMEDUKE_NAMES=1` and checks that every sector lotag number it names appears as a `case N:` in the sector-lotag switches it cites in `sector.c`, every SE lotag in `moveeffectors`, and every tile name is a `define` in `DEFS.CON` with that value.
- [ ] **Step 2:** Run. Expected: fails (no names).
- [ ] **Step 3:** Fill the tables from `sector.c` (operatesectors and the lotag cases), `actors.c`/`sector.c` `moveeffectors` (0-29, 128, 129), and other cases whose meaning the code makes clear. Unknown numbers print as numbers. Tile picker and 2D sprite labels use `tilename`.
- [ ] **Step 4:** Run `names.sh`, `editor.sh`, and a frame dump of L1's 2D view with sprite captions on. Expected: pass; the dump shows DEFS.CON names (for example `SECTOREFFECTOR`, `APLAYER`).
- [ ] **Step 5:** Commit `editor: name tiles, sectors and effectors from DEFS.CON and the game code`.

### Task 6: Floor heightmap tool

**Files:**
- Modify: `source/editor/build.c` (3D mode keys, on the slots the slope keys had)

**Interfaces:**
- Consumes: `draw.c`'s `floorstat & 2` path (colours from `floorpicnum`, heights from tile `floorheinum`), the tile picker.

- [ ] **Step 1:** Add to `editor.sh`: on a copy of L1, point at the start sector's floor, toggle the heightmap on, pick a height tile by number, save. `mapcheck diff` shows only that sector's `floorstat` (bit 2 set) and `floorheinum` changed. Toggle off and save: only `floorstat` differs from the original.
- [ ] **Step 2:** Run. Expected: fails.
- [ ] **Step 3:** Implement: one key toggles `floorstat` bit 2 on the floor under the mouse; another opens the tile picker and stores the choice in `floorheinum`. Ceilings are not offered. The status line shows the heightmap state and tile.
- [ ] **Step 4:** Run `editor.sh`. Dump a 3D frame after turning it on. Expected: pass; the floor is drawn as a heightmap.
- [ ] **Step 5:** Commit `editor: add the floor heightmap tool on the old slope keys`.

### Task 7: The v4 converter

**Files:**
- Create: `tools/convmap4.c`, `docs/map-format-v4.md`
- Modify: `tools/CMakeLists.txt`, `.superpowers/checks/editor.sh`

**Interfaces:**
- Consumes: Task 1 `writeboard`, `checkboard`.
- Produces: `convmap4 IN OUT`. Refuses if OUT names IN. Prints any v4 field v5 cannot hold.

- [ ] **Step 1:** Recover the v4 layout from `L9.MAP` and `M8.MAP` (and the exe's loader, in case a rejected-version path still knows v4). Known so far: the header starts `04 00 00 00`, then x, y, z, angle; the v5 count positions and any 16-64 byte record sizes give no consistent fit. Record the layout and how each field was identified in `docs/map-format-v4.md`.
- [ ] **Step 2:** Add to `editor.sh`: convert both maps into the scratch dir, `mapcheck validate` each. Run. Expected: fails (no tool).
- [ ] **Step 3:** Implement `convmap4`. Heightmap sectors (26 in L9, 12 in M8) map to `floorstat` bit 2 and `floorheinum` if v4 stores them that way; missing v5 fields take the commonest shipped values (`nextsector2`/`nextwall2` `(-1,-1)`).
- [ ] **Step 4:** Run `editor.sh`. Expected: both validate. Copy each converted map over `L1.MAP` in a scratch copy of the game data and dump frame 2 of `d3d /v1 /l1`. Expected: a sensible view from inside the level. Run all checks.
- [ ] **Step 5:** Commit `tools: convert the v4 maps L9 and M8 to v5`.

### Task 8: Records

**Files:**
- Modify: `source/build/VENDORED.md`, `docs/superpowers/specs/2026-11-03-lameduke-rebuild-design.md`, `README.md`
- Create: `docs/map-editor.md`

- [ ] **Step 1:** `VENDORED.md`: `build.c` and `bstub.h` from raynorpat/duke3d at the pinned commit, BUILDLIC, and the list of changes. Rebuild design: the "map editor, content converter" non-goal and the L9/M8 line point to the editor spec. `docs/map-editor.md`: running `build`, keys that differ from duke3d (heightmap tool, removed slope and renderer keys), `convmap4`. README: one line each for `build` and `convmap4`.
- [ ] **Step 2:** Run all checks. Expected: all pass.
- [ ] **Step 3:** Commit `docs: record the editor, its source and the v4 converter`.

Owner checks after Task 8: load an editor-saved map and a converted L9/M8 in `D3D.EXE` under DOSBox; build on macOS and Linux.
