# Network and the sync stream

`D3D.EXE` was built with multiplayer in mind: a ring of players, per-player input arrays and a network init at startup. The port keeps the data structures and plays one player. There is no serial, modem or IPX code, and no way to start a network game.

## What the exe had

- `initmultiplayers` (0x57AD7) runs at startup after "Checking for multiplayer...". It takes a comm type and a rate. 0 is single player, 1 to 4 pick a serial port and a baud rate from 2400, 4800, 9600, 14400, 19200 or 28800, and 5 and above open IPX. main passes `SETUP.DAT` bytes 4 and 5.
- The players form a linked ring at 0x23E130: `connecthead` is the first player, `connectpoint2[p]` the next one, and -1 ends it. `numplayers` counts them. There are 17 slots (`MAXPLAYERS`).
- `myconnectindex` (word_12538C) is the local player. `screenpeek` is the player the view follows.
- Input travels as four arrays indexed by player: `syncfvel`, `syncsvel` and `syncavel` (signed bytes for forward, strafe and turn) and `syncbits` (an int of buttons). `synctics` is how many clock ticks the step covers.
- The level frame's message prompt (T, 0x16217) also sends opcode 4 and the typed text (0x58C55) to every other player.

The command line (0x154F0, `check_args` in `premap.c`) has no network switches. The flags are `/?`, `/l#`, `/v#`, `/s#`, `/q`, `/d`, `/r`, `/oDIR` and `/e`. The port adds `/p#` for the renderer.

## What the port does

`source/build/network.c` is a stub. `initmultiplayers` still takes `SETUP.DAT` bytes 4 and 5, but only looks the baud rate up and throws it away. It always ends with one player: `connecthead = 0`, `connectpoint2[0..15] = -1`, `numplayers = 1`. Nothing is sent or received. This matches the README's "multiplayer is stubbed out".

Everything that walks the ring (`domovethings`, `moveplayers`, the save code and others) loops `for (p = connecthead; p >= 0; p = connectpoint2[p])`, so it runs once for player 0.

What stays from the multiplayer design:

| Where | What it does with one player |
|---|---|
| `enterlevel` (`premap.c`, 0x249B0) | Copies `ps[0]` to all 17 slots, clears the sync arrays, and pairs each ring player with a player sprite (stat 10) in turn, giving each a palette (0, 3, 4, ...). Players are moved to their sprites only when `numplayers > 1`. |
| `enterlevel` | `/* pending: 0x11120 holds every node at the start line (multiplayer task) */`. The wait for every node to load is not done. |
| `prelevel` (0x22210) | Sets `myconnectindex = screenpeek`. `screenpeek` is never assigned, so both are 0. |
| `functionkeys` (`game.c`, 0x14CD0) | K moves `myconnectindex` to the next ring player, wrapping to `connecthead`. With one player it stays 0. The key still sets the video mode that player is in (3D or map) and redoes the view size; for 3D it first shows tile 1170 for two frames. |
| `typemessage` (`view.c`) | Types and shows the message. Nothing is sent. |

Player start spots: `enterlevel` spawns one APLAYER (tile 283) at the map's start position. A map can also carry its own APLAYER sprites; `spawn` puts them in stat 10. The ring walk pairs player 0 with the first sprite in stat 10. Which sprite that is when a map has several has not been checked.

## Single-player input, one step at a time

`play_level` (`game.c`) runs a logic step every `TICSPERFRAME` (8) ticks of a 120 Hz clock; see [game-speed.md](game-speed.md). Each step:

1. `functionkeys()` handles the keys that never go through the sync stream (F10, F12, T, D, M, S, V, K, W).
2. With the menu open (`gm & 1`) the input is zero and the ramps stop (`input_clear`). Otherwise `input_sample(8, ...)` runs the keyboard ramps 8 times, adds the mouse, and builds the button bits from fixed scancodes (not `SETUP.DAT`).
3. The results go in `syncfvel[0]`, `syncsvel[0]`, `syncavel[0]` and `syncbits[0]`, and `synctics = 8`.
4. `domovethings` (0x18EA0) runs. For each ring player it calls `playerkeys`, `processinput` and `checksectors`, which read `sync*[p]`. CON's `ifp` reads them too.

The button bits, from `syncbits_from_keys` in `sdl_input.c`. Uses are listed only where the code reading them was checked.

| Bit | Keys | Read by |
|---|---|---|
| 0x1 | A | Instructions page shows "Jump" |
| 0x2 | Z | `ifp pducking`; Instructions shows "Duck" |
| 0x4 | Ctrl | fire (`player.c`) |
| 0x8 | none | Instructions shows "Open" |
| 0x10 | Space | use: doors and switches (`checksectors`) |
| 0x20 | Shift | run (`ifp`) |
| 0x100-0x1000 | 1-5 | weapon select |
| 0x100000 | `-`, keypad `-` | view size |
| 0x200000 | P | pause |
| 0x400000 | Tab | 2D map |
| 0x800000 | U | `/d`: open every door |
| 0x1000000-0x10000000 | F1-F5 | inventory; without `/d`, F1-F3 open Instructions, Save and Restore |
| 0x80000000 | Esc | opens the menu |

## Demos use the same stream

A demo is the sync stream of player 0 written to disk. `demo.c` reads and writes `mydemo_N.dmo` (0x1B810 and 0x1B920):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | volume |
| 1 | 1 | level |
| 2 | 1 | skill |
| 3 | 4 | count |
| 7 | count | forward bytes (`syncfvel`) |
| | count | strafe bytes (`syncsvel`) |
| | count | turn bytes (`syncavel`) |
| | count | tic bytes (`synctics`) |
| | 4 × count | bits ints (`syncbits`) |

`DEMO_MAX` is 16384. A file with a larger or negative count is refused. On load, every bits value is ANDed with 0xFFE7FFFF, clearing 0x80000 and 0x100000, as playback at 0x15EA2 does. That keeps the view-size keys out of a demo.

Recording (`/r`, `demomode = 1`):

- `domovethings` calls `demo_record` once per step when the game is not paused (0x18EEB). It stores `sync*[0]` and `synctics`.
- The count stops at 0x3FFF. After that, each new sample overwrites the slot after the last one kept.
- On the way out (`quitgame` or the window closing), `demo_write` drops the last sample and writes `mydemo_0.dmo` when the game is on `/v1 /l1`, otherwise `mydemo_1.dmo`. Those names are two of the shipped attract demos, so recording replaces one of them.
- The port records `synctics = 8` for every sample. Demos from the exe hold the tic count of each frame, often 0, 6 or 12.

Playback (`play_demo`, `demomode = 2`) sets `sync*[0]` and `synctics` from each sample and calls `domovethings`, so the game runs the same code as in play. The attract loop starts at demo 1 (or `LAMEDUKE_DEMO`), goes through the eight demos in order and wraps. Each one starts a new game from its 3-byte header. A missing demo ends the game with "Demo not found."
