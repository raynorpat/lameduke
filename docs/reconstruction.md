# Reconstruction

LameDuke is a C rebuild of `D3D.EXE`, the December 30, 1994 Duke Nukem 3D beta. The exe calls itself "DUKE DUKEM 3D Beta version 1.3.95" (`banner` in `source/game.c`). The port is built from the exe's own code, read in IDA. It does not reuse the 1996 Duke3D game source. For what the game contains, see [game-sheet.md](game-sheet.md).

## Goals

The [rebuild design](superpowers/specs/2026-11-03-lameduke-rebuild-design.md) sets them:

- Read the beta's loose files as they ship: maps, `TILES*.ART`, the three CON files, VOC and MID files, the eight demos, `PALETTE.DAT`, `LOOKUP.DAT`, `TABLES.DAT` and `SETUP.DAT`. No GRP, no conversion step.
- Play the attract demos, the whole menu, the command line and local single-player games the way the exe does.
- Draw the classic picture as the exe draws it, into the mode `SETUP.DAT` selects.
- Play VOC effects and MID songs when the game plays them.
- Build with C, SDL2 and CMake on Windows, macOS and Linux.

Later designs added the [map editor](superpowers/specs/2026-11-04-build-editor-design.md) and [Polymost, Video Options and the speed fix](superpowers/specs/2026-11-04-polymost-design.md).

## Method

Each routine is lifted from the exe and written as plain C. The source comments tie the C back to the exe:

- A function starts with its exe address, for example `/* 0x249B0. Loads the board of ps[0] ... */` above `enterlevel` in `source/premap.c`.
- Globals name the exe variable they stand for, using IDA's names: `byte_1257A8`, `word_1253A4`, `dword_B56D8`, `off_82838`. Struct fields keep offset names (`ps.b9d`, `ps.w54`) until their purpose is plain.
- Address ranges show where the exe inlined code, for example "0x29720. Player p's one-shot keys".

The Build engine half is lifted the same way. `source/build/draw.c` alone cites about 100 exe addresses. [VENDORED.md](../source/build/VENDORED.md) lists what each engine file holds.

The exe is the reference whenever the 1996 code differs. Map records stay at version 5 (37-byte sectors, 35-byte walls, 38-byte sprites). Tiles and tags are named by how this beta uses them, not by their 1996 meaning.

### Where the port differs from the exe, and why

Comments mark each place. Grep for `the exe`, `Not in the exe`, `shortcut:` and `pending:`.

| Kind | Examples |
|---|---|
| Out-of-bounds reads the exe gets away with | `sector[-1]`, `wall[-1]`, `sprite[-1]` and `hittype[-1]` reads are skipped or given the value the exe would have read (`sector.c`, `player.c` `checksectors`, `actors.c`, `board.c`). |
| Uninitialised values | Where the exe leaves stack garbage, the port picks a fixed value and says so (`draw.c`, `sector.c`). |
| Pointers kept as indices | The exe stores script and actor addresses. The port keeps indices, and save files convert between the two (`config.c`, `gamedef.h`). |
| Files that are missing | A sound that did not load plays whatever is at address 0 in the exe. The port skips it (`sounds.c`). Truncated VOC and MID files are rejected, not played. |
| Added for this port | `/p#` (renderer for this run), the Video Options page, `duke.cfg`, Polymost, and helpers written for the editor (`board.c` writer, `draw.c` text). Each says "Not in the exe". |
| Timing | The exe's logic speed depends on the CPU. The port runs a fixed 120 Hz clock at 8 tics a step. See [game-speed.md](game-speed.md). |

## Source tree

| Path | Contents |
|---|---|
| `source/` | The game: `game.c` (main, loops, function keys), `premap.c` (command line, `newgame`, `enterlevel`), `actors.c` (spawn and every mover), `player.c` (input, weapons, use key), `sector.c` (doors, lifts, switches), `gamedef.c` (CON compiler and interpreter), `view.c` (3D view, HUD, overlays), `menus.c`, `demo.c`, `config.c` (saves, `userdefs.tmp`), `sounds.c`, `videocfg.c` (`duke.cfg`). |
| `source/build/` | The 1994 Build engine (`board.c`, `draw.c`, `display.c`, `tiles.c`, `math.c`, `engine.c`), Polymost, the stubbed `network.c`, and the platform layer. See [VENDORED.md](../source/build/VENDORED.md). |
| `source/audiolib/` | Apogee Sound System as the exe links it: `fx_man`, `multivoc`, `music`, `midi`, `al_midi` and an OPL emulator (`oplemu.c`). The DOS card drivers for GUS, AWE32, MPU-401, PAS and others are thin stubs. |
| `source/editor/` | The BUILD editor (`build.c`, `bstub.h` from raynorpat/duke3d), its glue (`edglue.c`, `editor.h`) and LameDuke names (`lstub.c`). See [map-editor.md](map-editor.md). |
| `tools/` | `mapcheck`, `convmap4` and `cfgcheck`, described below. |
| `docs/superpowers/` | The design specs and task plans. |

Other docs cover parts in depth: [build-engine.md](build-engine.md), [rendering.md](rendering.md), [audio.md](audio.md), [network.md](network.md), [menus.md](menus.md) and [con-script.md](con-script.md).

## What came from elsewhere

[VENDORED.md](../source/build/VENDORED.md) has the details. In short:

- The engine was rebuilt from `D3D.EXE`. No Build source release is its baseline.
- `source/editor/build.c` and `bstub.h` are Ken Silverman's editor from raynorpat/duke3d at `2f141ca`, held unchanged in commit `8035cc7`.
- `polymost*.c`, `sdl_opengl.c` and their headers come from raynorpat/duke3d at `8f07635`, held unchanged in commit `d8c91dd`.
- `git diff` against those two commits shows every local change.
- Ken Silverman's Build license is in [BUILDLIC.TXT](../source/build/BUILDLIC.TXT).

## Platform layer

The game and the drawer never call an OS API directly.

| File | Job |
|---|---|
| `sdl_main.c` | `platform_init` and `platform_quit` (SDL init and shutdown). |
| `sdl_window.c` | The window, presenting the 8-bit frame, display modes, the GL window and context, frame capture. |
| `sdl_opengl.c` | duke3d's GL loader and draw paths, built only with OpenGL and Polymost. |
| `sdl_input.c` | Keys into `keystatus[]` by DOS scancode, the mouse, the turn and move ramps, and `syncbits`. |
| `sdl_sys.c` | Timer (`timer_ticks`, `timer_delay`). |
| `sdl_audio.c` | Opens the SDL audio device (16-bit stereo) and calls the mixer from its callback. |
| `plat_win32.c`, `plat_unix.c` | File open, rename, directory listing and the current directory. On Unix, `plat_fopen` matches a file name in any case, because the exe asks for `tiles000.art` and the files ship as `TILES000.ART`. NTFS already ignores case. |

## Environment variables for testing

None of these exist in the exe. They let scripts drive the game and the tools.

| Variable | Read in | What it does |
|---|---|---|
| `LAMEDUKE_FRAME=file` | `game.c`, `editor/edglue.c` | Write one shown frame and exit. The game writes a BMP; the editor writes a PPM. While set, the game runs one logic step per frame, so frame N is always the same picture. |
| `LAMEDUKE_FRAME_AT=n` | `game.c`, `editor/edglue.c` | Which frame `LAMEDUKE_FRAME` writes (default 1). |
| `LAMEDUKE_DEMO=n` | `game.c` | The first attract demo to play (`n & 7`). Default 1. |
| `LAMEDUKE_TRACE=1` | `game.c` | Print `sample i x y z ang sect` for player 0 after each demo sample or logic step. |
| `LAMEDUKE_SPEED=1` | `game.c`, `view.c` | Print `logic`, `frame` and `cam` lines with timestamps, for the pacing checks in [game-speed.md](game-speed.md). |
| `LAMEDUKE_NOSMOOTH=1` | `game.c` | Draw each logic step's own positions, with no blend between steps. |
| `LAMEDUKE_KEYS=...` | `sdl_input.c`, `editor/edglue.c` | Scripted keys. In the game, `a,b,c` holds one hex scancode per input sample, and `frame:code,...` holds a key on that frame. The editor's form is in [map-editor.md](map-editor.md#testing-aids). |
| `LAMEDUKE_SWITCH=frame:renderer,...` | `game.c` | Switch renderer after that many frames, as the menu would. |
| `LAMEDUKE_RESOLUTION=WxH` | `premap.c` | Override the render size whenever the renderer is set, `/p` included. |
| `LAMEDUKE_GL_FORCEFAIL=1` | `sdl_window.c` | Make the GL context fail, to test the fallback to Classic. |
| `LAMEDUKE_NAMES=1` | `editor/lstub.c` | Print the editor's name tables and exit. |
| `MAPCHECK_FRAME=file` | `tools/mapcheck.c` | With `mapcheck pick`, also write the drawn frame as a PPM. |

## Check tools

Built by `tools/CMakeLists.txt` next to `d3d`.

- **`mapcheck`** links the engine.
  - `mapcheck roundtrip IN OUT` loads a map and writes it back with `writeboard`.
  - `mapcheck same A B` exits 0 when two files are byte-identical.
  - `mapcheck validate MAP` loads a map and runs `checkboard` on its links.
  - `mapcheck pick MAP X Y` draws one 3D frame at the map start with the editor's search pick at (X, Y), and prints `searchstat`, `searchsector` and `searchwall`.
- **`convmap4 IN OUT`** converts a version-4 map (`L9.MAP`, `M8.MAP`) to version 5. It never writes over its input and reports any v4 field v5 can't hold. See [map-format.md](map-format.md#version-4).
- **`cfgcheck read FILE`** prints a `duke.cfg` as parsed and clamped. `cfgcheck roundtrip IN OUT` reads one and writes it back.

## History

`git log` has 125 commits. In order:

1. A first playable C port with a portal renderer (`1d65a0e`), then the design spec and plan (`8f91fbb`).
2. Task by task, the exe's code replaced it: case-insensitive files, the command line, `drawrooms` and `drawmasks` (the portal renderer was dropped), the CON compiler, `processinput`, `moveactors`, the effectors, `domovethings`, demos, sound, music, menus, saves and high scores.
3. The BUILD editor, the map checker and the v4 converter.
4. Game speed at the 486's pace, then interpolation between logic steps.
5. Polymost's software and OpenGL renderers, Video Options and `duke.cfg`.
6. The newest commit (`c300098`) paces logic with Duke3D's fixed 120 Hz loop at 8 tics a step.

## Status

From the code as it stands:

**Works**

- The attract loop plays `MYDEMO_1` to `MYDEMO_7` and `MYDEMO_0`, in turn. Esc opens the main menu.
- Every main-menu and in-game-menu page the exe has: new game (episode and skill), restore, instructions, game options, ordering info, demo, high scores, save, quit. Eight save slots use the exe's `SAVE___n.SAV` and `SAVE_vln.SAV` names.
- The exe's command line, plus `/p#`.
- Single-player play through the lifted CON interpreter, actors, effectors, sectors and weapons.
- Sound effects through `multivoc` and SDL. Music through the OPL emulator when `SETUP.DAT` picks an FM card. The shipped `SETUP.DAT` picks Sound Blaster with sound and music on.
- Classic, Polymost Software and Polymost OpenGL renderers.
- The editor, the checker and the converter.

**Pending or incomplete**

- `pending:` 0x17890, the status bar draw in `enterlevel` (`premap.c`), and 0x14050, its redraw when the map closes (`player.c`).
- `pending:` 0x11120, which holds every node at the start line. It belongs to the multiplayer task (`premap.c`).
- Multiplayer: `network.c` reads `SETUP.DAT`'s comm bytes and always reports one player. No port or socket opens.
- Joystick: the Game Options row is there, but choosing it does nothing, and there is no joystick input.
- Music on the MPU-401, AWE32 and Gravis card choices: their init calls are stubs, so no song is heard. Only the FM (OPL) path makes sound.
- Doors, enemies and fire rate still need a by-eye comparison with DOSBox ([game-speed.md](game-speed.md)).
- Polymost's known drift from Classic is listed in [VENDORED.md](../source/build/VENDORED.md).
