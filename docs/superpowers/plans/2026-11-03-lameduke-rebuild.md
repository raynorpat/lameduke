# LameDuke First Rebuild Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Play the December 1994 `D3D.EXE` beta on a current machine from its shipped files, with its demos, menus, rules, classic picture, sound, and music.

**Architecture:** Behavior is lifted from `D3D.EXE` through the local IDA database into the existing three libraries: `source/build` (engine, platform), `source/audiolib`, and `source/` (game, CON, menus, demos, sounds). The portal approximation in `build/draw.c` is replaced, not kept beside the new drawer. SDL2 only presents the framebuffer, plays mixed audio, and reads input.

**Tech Stack:** C, SDL2, CMake (Windows, macOS, Linux). IDA database `D3D.EXE.i64` (local, untracked). DOSBox running `D3D.EXE` produces the reference captures.

**Spec:** `docs/superpowers/specs/2026-11-03-lameduke-rebuild-design.md`

## Global Constraints

- Source of behavior is `D3D.EXE`. Never paste or paraphrase duke3d or 1996 source.
- Minimum code. No system the executable does not have. No abstraction for a single caller.
- `D3D.EXE`, `DOS4GW.EXE`, `SETUP.EXE`, IDA databases, and DOSBox captures stay untracked.
- The game and the drawer never call Win32, Cocoa, or a Linux-only API. Platform code lives in `build/plat_win32.c`, `build/plat_unix.c`, and `build/sdl_*.c`.
- Shipped files are read as they are. No conversion, no new container, no renaming.
- Commits use `build:`, `audio:`, or `game:` style prefixes, one or two lines, and no trailer metadata (AGENTS.md overrides any default attribution).
- A stub that prints and returns is unfinished. Each task removes the stubs it touches.
- Comments cite the executable address of the code they lift (`/* 0x3BBD0 ... */`), the way the tree already does.

## Verification Method

There is no test framework, and the spec says not to add one. Every task is checked by running `build/d3d.exe` from the repo root and comparing the result with a DOSBox capture of `D3D.EXE` made with the same command line and the shipped `SETUP.DAT`. Task 1 adds two debug variables so these checks can be repeated:

For DOSBox, use a disposable copy of `D3D.EXE` and the game files. The original executable requires a matching `FILE_ID.DIZ`; if it is missing, startup prints `No good.` and deletes the executable it was launched from. Never run this check against the only copy of the executable. A local, untracked reference fixture can be reconstructed from the pattern at `0x8291C`: `check_file_id` at `0x24E00` reads 450 bytes and compares 400 pattern bytes, treating each `*` as two skipped file bytes before the next pattern byte. A fixture using CRLF for those skipped bytes allowed the copied executable to start. Do not treat the fixture as a shipped file.

- `LAMEDUKE_FRAME=out.bmp` (already exists) plus `LAMEDUKE_FRAME_AT=N`: write the Nth presented frame, then exit. Unset `LAMEDUKE_FRAME_AT` means N=1, the current behavior.
- `LAMEDUKE_TRACE=1`: after each demo sample, print `sample i x y z ang cursectnum` to stdout. This is used for repeatable checks that the tree does not regress. It is not a reference, because the executable has no such output.

Build: `cmake --preset default && cmake --build --preset default`. The binary is `build/bin/d3d.exe`, and it runs from the repo root. Commands below write it as `./build/d3d` for short.

## Review Focus

1. **Case-sensitive filesystem.** On Linux/macOS, `lookup.dat` must open `LOOKUP.DAT` and `fastway2.mid` must open `FASTWAY2.mid`. Task 1 has the test.
2. **A missing file the executable names.** Check `waterfal.voc`, `switch.voc`, an unused save slot, and a missing demo against the executable, with no crash. Tests are in Task 7 (VOC) and Task 9 (save slot). `FILE_ID.DIZ` is a special case: the executable's missing-file response is destructive, as documented above; the current port instead allows startup without it.
3. **No audio device.** If the audio device cannot be opened, or SDL audio fails, the game runs silent the way the executable does when card init fails. It does not `errorexit`. Task 7 has the test.
4. **Long stall or window drag mid-demo.** Demo state comes from the recorded tics only, so a 2-second stall must not change the trace. Task 6 has the test.
5. **`/oDIR` to a missing or read-only directory.** A save or demo record must report what the executable reports. It must never claim success. Task 2 has the test.

---

### Task 1: Case-insensitive file open and debug capture

**Files:**
- Modify: `source/build/platform.h`, `source/build/plat_unix.c`, `source/build/plat_win32.c`
- Modify: every `fopen` of a shipped file: `build/board.c:47`, `build/display.c:42,56`, `build/engine.c:15`, `build/math.c:16`, `build/tiles.c:45`, `config.c:8`, `demo.c:38`, `gamedef.c:7`, `premap.c:26`
- Modify: `source/game.c` (`present`, `play_demo`)

**Interfaces:**
- Produces: `FILE *plat_fopen(const char *name, const char *mode);` in `platform.h`. On Windows it is `fopen`. On Unix it does `fopen` first, then scans the current directory with `opendir`/`readdir`/`strcasecmp` for a case-insensitive match. Writes (`"wb"`) reuse an existing match, and otherwise create the name as given.
- Produces: `LAMEDUKE_FRAME_AT`, `LAMEDUKE_TRACE` as described in Verification Method.

- [ ] **Step 1: Reproduce.** On Linux or macOS (or WSL in the repo dir), run `./build/d3d` and expect it to fail on `lookup.dat` or `tables.dat`. Write down the failure.
- [ ] **Step 2: Implement** `plat_fopen` and replace every listed `fopen`. Spell each name the way the executable's string spells it (lowercase, for example `"palette.dat"` and not `"PALETTE.DAT"`).
- [x] **Step 3: Verify.** On Linux run `./build/d3d`: the attract demo starts. On Windows it is unchanged. The current port's `check_file_id()` returns 0 without `FILE_ID.DIZ`; this differs from the original executable, which reports `No good.` and deletes its executable when the file is missing. The port's non-destructive behavior allows the supplied files to start without the missing identification file.
- [ ] **Step 4: Implement** `LAMEDUKE_FRAME_AT` and `LAMEDUKE_TRACE` in `game.c`. When `LAMEDUKE_FRAME` is set, demo playback skips its wall-clock wait so captures are fast.
- [ ] **Step 5: Verify.** Run `LAMEDUKE_TRACE=1 ./build/d3d > t0.txt` and stop it after one demo. Run it again. The two files are identical. Run `LAMEDUKE_FRAME=f.bmp LAMEDUKE_FRAME_AT=200 ./build/d3d`: it writes `f.bmp` and exits 0.
- [ ] **Step 6: Commit.** Commit the uncommitted ramp and movement work first as its own commit (`game: lift the sync ramp and movement step`), after confirming with the owner. Then `build: open shipped files case-insensitively`.

### Task 2: Command line

**Files:** `source/premap.c`, `source/premap.h`, `source/game.c`

**Interfaces:**
- Produces globals in `premap.c`: `game_quotes` (`/q`), `game_debug` (`/d`), `game_demorec` (`/r`, exists), `game_outdir[]` (`/oDIR`). `/e` runs the encode path at the point `check_args` runs it in the executable, then exits as the executable does.

- [ ] **Step 1: In IDA,** read `check_args` and every reader of each flag. Write down what `/d` turns on (from `GO.BAT`), what `/e` writes for each `L?.MAP`, and where `/oDIR` is applied (`I/O path rerouted to %s.`).
- [ ] **Step 2: Implement** each flag's effect where its reader lives. `/oDIR` is applied by `plat_fopen` for output files only, if the executable does it that way. Otherwise follow the executable.
- [ ] **Step 3: Verify** each flag against DOSBox. `d3d /x` prints `Unknown command line paramater '/x'` and the help, with the same exit. `d3d /d` matches the DOSBox debug behavior. `d3d /e` produces the same output files and console text as DOSBox (compare the bytes). `d3d /r /v1 /l1` writes `mydemo_?.dmo` and prints `Demorec mode on...`. `d3d /o/nonexistent /r /v1 /l1` prints what DOSBox prints for an unwritable output directory, and creates nothing.
- [ ] **Step 4: Commit:** `game: implement /q /d /r /o /e as the executable does`

### Task 3: Classic renderer

**Files:**
- Modify: `source/build/draw.c` (rewrite), `source/build/draw.h`, `source/build/engine.c`, `source/build/display.c`
- Delete: the portal functions in `draw.c` (`paint_floor`, `paint_wall`, `draw_wall`, `draw_sector`, `draw_sprites`, `project_*`, `clip_near`)

**Interfaces:**
- Produces: `void drawrooms(int x, int y, int z, short ang, int horiz, short sect);` and `void drawmasks(void);`, plus the executable's wall, floor/ceiling (with slope), masked-wall, and sprite column drawers as static functions. Shade goes through `LOOKUP.DAT` palookup. `draw_view()` stays as the caller used by `game.c` and calls these two.
- Consumes: `sintable()`, `tile_data()`, `sector[]`/`wall[]`/`sprite[]`, `setgamemode()` mode from `setup_byte(0)`.

- [ ] **Step 1: In IDA,** find drawrooms from the call in the view-drawing path, and map its scan, wall, flat, slope, and mask routines plus `TABLES.DAT` beyond the sine table (the remaining 7808 bytes). Confirm sprite `+28` and `+30` (spec Facts) against one real sprite in `L1.MAP`.
- [ ] **Step 2: Capture references in DOSBox.** `d3d /v1 /l1` start frame, and attract demo 1 at about sample 200. Save as untracked `ref/l1_start.png` and `ref/demo1_200.png`.
- [ ] **Step 3: Implement** the drawers. Delete the portal code in the same change.
- [ ] **Step 4: Verify.** `LAMEDUKE_FRAME=l1.bmp ./build/d3d /v1 /l1` should match `ref/l1_start.png` at the pixel level or very close, with slopes, masked walls, and shade visible. Do the same for demo 1 at sample 200. Run every shipped v5 map with `/v /l` and confirm none crash. Resize the window: the 320×200 buffer nearest-scales and the internal resolution does not change.
- [ ] **Step 5: Commit:** `build: lift the classic drawers and drop the portal renderer`

### Task 4: CON compiler

**Files:** `source/gamedef.c`, `source/gamedef.h`

**Interfaces:**
- Produces: `int compilecon(void);` (exists, returns 0 on success). It compiles `game.con`, and through its includes `defs.con` and `music.con`, into the executable's script array and label and actor tables. Expose the array and the tables as `extern` in `gamedef.h`, using the names the executable's layout suggests.

- [ ] **Step 1: In IDA,** read the compiler from `main`'s call after `check_args`. Record its keyword table in order, its error strings, and the output array's size and layout.
- [ ] **Step 2: Get the reference.** In DOSBox (with the debugger, or by breakpoint dump) after compile, record script length, label count, and the first 64 script words. If a dump is impractical, record the console text the compiler prints.
- [ ] **Step 3: Implement** the compiler.
- [ ] **Step 4: Verify.** `./build/d3d` prints the same compile text as DOSBox, and the script length, label count, and first 64 words match Step 2. A copy of `GAME.CON` with one bad keyword prints the executable's error for it.
- [ ] **Step 5: Commit:** `game: compile GAME.CON as the executable does`

### Task 5: Simulation: CON execution, actors, sectors, player — complete

**Files:** `source/game.c`, `source/premap.c` (`enterlevel`), `source/global.c`, `source/build/board.c` (`player_move`, clipmove, getzrange as the executable has them), `source/build/sdl_input.c`. A new `source/actors.c` only if the executable's code layout shows a separate unit there. Otherwise this goes in `game.c`.

**Interfaces:**
- Consumes: Task 4 script tables, Task 3 `drawrooms`.
- Produces: `void game_tic(int fvel, int svel, int avel, int tics, int bits);`, one sync sample of the whole simulation (player, weapons, actors, sector motion, doors). It replaces the direct `player_move` calls in `play_demo` and `play_level`. `void input_sample(...)` (exists) gains the hardcoded sync-bit keys and `setup.dat` scancodes, so it returns `bits` as well.

**Completion:** The owner playtested the game, opened a door, and shot a flying enemy, and accepted Task 5 as complete. Commit `7d13557` corrects local-play clock rounding and weapon spawn positions. The port and the disposable DOSBox reference both reached the level-one start, with close initial views. A 10,000-frame attract run completed without a crash, and repeated shorter traces matched. These checks did not establish that every weapon, enemy, or interaction matches DOSBox; the broader comparisons in Step 3 remain useful follow-up verification.

**Recorded-input comparison:** A disposable demo containing 100 forward samples followed by a use-key input brought both versions up to the same wall; it did not reach a door. A second demo substituted firing for the use input, but the captures were not synchronized closely enough to compare the shot. The screenshots also differed in HUD and weapon presentation; trace the executable's demo view path before attributing that difference to simulation. Disposable inputs and captures are under `.superpowers/playtest/`.

The unchecked steps below retain the original verification detail; completion was accepted on the playtest evidence above.

- [ ] **Step 1: In IDA,** map the per-tic path called from `sub_31410`: player, weapon fire, actor walker (statnum lists), sector effectors, doors, and the CON interpreter. Map the sync bit numbers to keys (spec: bits 0 and 1 are A and Z, bit 1 halves the step, bits 19 and 20 are cleared in demos).
- [ ] **Step 2: Implement** in this order, and check each against DOSBox on `L1.MAP` before the next: player and look, weapons, doors and sector motion, actors and the CON interpreter.
- [ ] **Step 3: Verify** with `/v1 /l1` against DOSBox. Open the first door, fire each weapon, and kill the first enemy, and the same things happen. Mouse X turns. Mouse Y does not change the horizon. A and Z look.
- [ ] **Step 4: Commit** after each sub-part works: `game: lift <part> from the executable`

### Task 6: Demos and the attract loop — complete

**Files:** `source/game.c` (`play_demo`, `main`), `source/demo.c`

**Interfaces:**
- Consumes: `game_tic` from Task 5. Esc opens the menu from Task 9 (until Task 9 lands, Esc leaves the loop as it does now).

**Progress:** Commits `33bd272` and `4345aa6` correct demo pacing and reset the player before each recording. All eight shipped demos completed a fast playback cycle without crashing. In demo 4, samples 1423–1425 contain F3, which opens Restore in the original; the port's menu is deferred to Task 9. For an isolated comparison, a disposable copy suppressed that F3 input and inserted a pause after a chosen sample. The views were close at sample 100, while a later comparison around sample 500 showed a substantially different view. The fixture changes the recording and is a diagnostic, not proof that the unmodified demo matches. Continue tracing the divergence and compare the other demos before completing this task.

**Demo 1 frame comparison:** Demo 1 is `L6.MAP`. Screenshots from a single DOSBox run were matched against every frame the port drew. The screenshots were taken with `PrintWindow`, which captures only the DOSBox window, and the 320x200 image was cropped from its 640x400 client area.
- Port frames 256-355 match the original almost pixel for pixel. Those frames include the demo camera views (SE 27) and the actors.
- From frame 356 (sample 466, when camera 6 releases the view), the original shows the player's underwater view with a near-level horizon and a visible pistol. The port shows `horiz` near 46 and no weapon.
- A disposable copy of `D3D.EXE` had its SE 27 demo-camera test patched at file offset `0x3E49F` (2 to 3), so that copy always shows the player view. With that copy, distant troopers drift from about frame 21, and by about frame 160 the original has dead troopers that the port does not. SE 27 also sets `cameraspr`, which CON `ifp pfacing` reads, so a comparison against the patched copy must disable SE 27 in the port the same way.
- The following were checked against the original and match: krand use in the sound code (there is none), the `processinput` water, landing, movement and look code, zero-tic demo samples (both run every sample), and `newgame` at demo start (`0x1B810`).
- A disposable `D3D.EXE` patched to write its state at the end of every `domovethings` (seed, player position and `horiz`, all sprites, `hittype`, and the stat lists) gives a per-tic trace to compare against the port's. Three fixes made the whole of demo 1 match:
  - The shot spawn z offsets in `shoot` were wrong.
  - `furthestangle` (`0x403B0`) passes one variable as both `hitsect` and `hitsprite`. Every wall hit therefore clears `hitsect`, and the last wall crossed wins, not the nearest.
  - In `hitscan` a farther floor or ceiling crossing falls through to the wall tests, and the hit point is multiplied in 32 bits before the divide.
- The remaining differences cannot affect play. Shell casings (pic 1165) advance their frame in `animatesprites`, so they depend on how many frames were drawn. A missed hitscan spawns a sprite in sector -1 from uninitialised stack values, and that sprite is deleted on the next tic.
- Demo playback ends where the exe's catch-up loop (`0x15E4E`) ends. The exe stops after a frame once it reaches count-1 samples, and a zero-tic sample always runs on into the next one. So demo 1 plays all 2959 samples and demo 2 plays 212 of its 213.
- `movesprite` in sector -1 reads `sector[-1]`, which is the last 37 bytes of `sprite[4095]`.
- A second trace covered the whole attract cycle: 20212 tics, demos 1 to 7 and then demo 0. Four more fixes made all of it match:
  - A moving sector (`0x1CBAB`) compares the sign-extended sector hitag with `0xFFFF`. That is never equal, so it carries sprites even in hitag -1 sectors.
  - On a miss, `checkhitceiling` clears only `al`, and its callers test all of `eax`. So a miss in a sector with a nonzero hitag counts as a hit, and a shot above the ceiling stays where it is.
  - The premap gives `DOORSHOCK` (1123) `cstat |= 1`, shade -12 and stat 1.
  - `processinput` looks right on bit 7 of the sync bits, not bit 31. Bit 31 opens the menu in `playerkeys`.
- The demo frame (`0x15FC3`) and the level frame (`0x18DAD`) both draw tile 481, the logo, translucent at (268, 156) over every camera view.
- The pistol was never missing. The player drowns in demo 1 (health 0 from tic 526 to 578), and that is when the camera releases the view.
- The exe redraws while it waits for its clock, so a few fields depend on how many frames it drew: shell casing frames (pic 1165), the SE 27 camera `yvel` clamp, and the viewscreen's owner and frame counter (1408). The comparison masks these, along with the sector -1 sprite that a missed hitscan spawns from leftover stack values.

- Attract order (`0x15E01`): demo 1 plays first, the index goes up after each load, and it wraps from 8 to 0. A missing demo is `errorexit("Demo not found.")` (`0x1B810`), as in the port.
- Recording (`/r`) sets `demomode` to 1. `domovethings` (`0x18EEB`) records one sample per unpaused tic. The count stops at `0x3FFF`, and later tics overwrite the slot after the last kept sample. On exit, `0x1B920` drops the last sample and writes `mydemo_0.dmo` for `/v1 /l1`, otherwise `mydemo_1.dmo`.
- Step 3 was checked against a per-tic state trace of the whole attract cycle, not by eye. `.superpowers/checks/demo.sh` compares demo 1 with `ref/demo1.trace`. With a 2-second stall at sample 100, the trace is unchanged.

- [x] **Step 1: In IDA,** confirm the attract order (which demo index plays first and how it wraps). The tree starts at 1 and wraps at 8.
- [x] **Step 2: Implement.** Playback calls `game_tic` with the recorded bytes, and bits are masked by `0xFFE7FFFF`. `/r` records through the same path. Pacing stays per spec ("Left to the builder").
- [ ] **Step 3: Verify.** Watch all eight demos beside DOSBox. The player follows the same route and shoots the same things, and none desyncs (no walking into walls, no early end). Save `LAMEDUKE_TRACE=1` output for demo 1 as `ref/demo1.trace`.
- [x] **Step 4: Stall test.** Add a temporary 2-second `SDL_Delay` at sample 100 and run the trace again. It matches `ref/demo1.trace` exactly. Remove the delay.
- [x] **Step 5: Commit:** `game: play the attract demos through the full simulation`

### Task 7: Sound effects

**Files:** `source/audiolib/multivoc.c`, `mv_mix.c`, `fx_man.c`, `dsl.c`, `blaster.c` (the card branch the shipped setup selects), `source/sounds.c`

**Interfaces:**
- Produces: `MV_Mix` fills SDL's buffer through an `SDL_AudioSpec.callback` in `dsl.c`, at 11000 Hz, 16-bit, 2 channels (spec). `loadsounds()` loads the executable's VOC list. `sound_callback` does what `0x3C190` does. Sound play calls are whatever the executable calls `FX_*`. Lift them under their AudioLib names.

- [ ] **Step 1: In IDA,** read `0x3BBD0` (loadsounds), the VOC table, the play sites, and the multivoc mixer and VOC block parser.
- [ ] **Step 2: Implement.** A VOC the executable lists but the release does not ship (`waterfal.voc`, `switch.voc`, `subway.voc`, `bodybloa.voc`, `xoptor.voc`) is handled as `0x3BBD0` handles a failed open.
- [ ] **Step 3: Verify.** In `/v1 /l1`, firing plays `shooting.voc` and doors play `door1.voc`/`door2.voc`, matching DOSBox by ear. With `SDL_AUDIODRIVER=dummy`, and again with the audio device disabled, the game runs silent and does not `errorexit` (Review Focus 3). Setting `SETUP.DAT` byte 1 to 0 skips sound init the way the executable does.
- [ ] **Step 4: Commit:** `audio: mix VOC effects through SDL`

### Task 8: Music

**Files:** `source/audiolib/music.c`, `al_midi.c`, `oplemu.c`, `midi.c`, plus `mpu401.c`/`gusmidi.c` only to keep their branch calls compiling. `source/sounds.c` or `game.c` for the `MUSIC.CON` song selection.

**Interfaces:**
- Produces: `MUSIC_PlaySong`, `MUSIC_StopSong`, and the other `MUSIC_*` calls the executable makes, under AudioLib names. MIDI is rendered in process (OPL emulation for the AdLib/SoundBlaster branch that the shipped setup selects) and mixed into the same SDL callback as Task 7.

- [ ] **Step 1: In IDA,** map the song selection (`MUSIC.CON`, when a song starts or stops) and the AL_MIDI and OPL path.
- [ ] **Step 2: Implement** OPL emulation in `oplemu.c` from the OPL2 register behavior. If an existing permissively licensed OPL core is brought in instead, ask the owner first.
- [ ] **Step 3: Verify.** In the attract loop and in `/v1 /l1`, the song DOSBox plays starts at the same moment and sounds like it. Music Volume changes the level. `SETUP.DAT` byte 2 = 0 skips music init.
- [ ] **Step 4: Commit:** `audio: play MID songs through OPL emulation`

### Task 9: Menus, save/restore, high scores

**Files:** `source/menus.c`, `source/menus.h`, `source/game.c`, `source/config.c` (`userdefs.tmp`)

**Interfaces:**
- Produces: `int menu_run(int ingame);` returns the chosen action as the executable encodes it. The text comes from the strings at `0x9d169`–`0x9d310` that `0x3C1C0` sets up. Save and restore write `SAVE_*.SAV` byte-for-byte as the executable does (both name templates, `SAVE___0.SAV` and `SAVE_000.SAV`, appear. Find which one each path uses).

- [ ] **Step 1: In IDA,** map `0x3C1C0`, the menu loop, input handling, each page's action, the save format, the high score file (`Score`, `Hero's Quote`), and Instructions and Ordering Info content.
- [ ] **Step 2: Implement** every page listed in the spec Goals, in the executable's order.
- [ ] **Step 3: Verify** against DOSBox, page by page. A save made in this port loads in DOSBox, and a DOSBox save loads here (byte-compare the files). Restore of an `empty` slot does what the executable does. Esc in the attract loop opens the main menu, and Quit returns to the shell with exit code 0. Esc in play opens the in-game menu, and Return to Game resumes on the same tic.
- [ ] **Step 4: Commit:** `game: lift the main and in-game menus with save and restore`

### Task 10: Joystick, multiplayer stubs, and acceptance

**Files:** `source/build/sdl_input.c`, `source/build/network.c`, `source/menus.c` (Joystick page)

- [ ] **Step 1: Implement** the Joystick page's axes, buttons, and calibration on `SDL_Joystick`. With no device connected, behave as the executable does with no joystick.
- [ ] **Step 2: Verify** `initmultiplayers` with `SETUP.DAT` bytes 4 and 5 set to a serial type: no port and no socket opens (watch with Process Monitor or `strace`), and startup does not wait for a peer.
- [ ] **Step 3: Run the spec's Acceptance list, 1 through 9,** on Windows. Then build and run items 1, 7, and 8 on Linux and macOS. `./build/d3d /v1 /l9` (L9) and `/v2 /l8` (M8) fail as DOSBox does.
- [ ] **Step 4: Commit:** `game: joystick page and multiplayer stubs per the executable`
