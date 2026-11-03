# LameDuke first rebuild

## Intent

LameDuke is the December 30, 1994 `D3D.EXE` beta ("DUKE DUKEM 3D Beta version 1.3.95"). This rebuild exists so that beta can be played on a current machine the way that executable plays it.

The person this is for is the one working in this repo. Success is sitting down with the files that already ship beside the executable, watching the included demos, and playing an episode locally, with the picture, the rules, the sound effects, and the music matching that beta.

The source of behavior is `D3D.EXE`. The duke3d tree is only a guide for how a modern port is arranged. Its game code is not pasted in. This beta is not the 1996 1.5 source, and that source is copyrighted.

## Goals

The first rebuild is done when all of these are true:

- With no level chosen, the attract loop plays the included `mydemo_N.dmo` recordings the way the executable does. Esc opens the menu. It does not exit.
- The executable's whole menu works. From the attract loop that is Begin a New Game, Restore a Game, Instructions, Game Options, Ordering Info, Demo, High Scores, and Quit. During play that is Return to Game, How to Play Me, Save a Game, Restore a Game, Game Options, and Quit to Main Menu, in that order. New Game leads through the episode names (Mr. Caliber, Mission Cockroach, Suck Hole, Hard Landing, Back) and the skills (Easy, Medium, Hard, Harder, Back). Game Options is Sound Volume, Music Volume, Mouse Sensitivity, Joystick, and Back. Save and Restore each list eight slots (shown as `empty` until used) and Back, stored in the executable's `SAVE_*.SAV` files. Quit, and Quit to Main Menu, do what the executable does with them. The command line the executable accepts can start that same local game. That surface is the executable's whole help list: `/?`, `/l#`, `/v#`, `/s#` (0-3), `/q` (help quotes), `/d` (debug modes), `/r` (record a demo), `/oDIR` (reroute output), and `/e` (encode `L?.MAP`). An unknown flag prints `Unknown command line paramater '%s'` and the help, as the executable does. The shipped `GO.BAT` runs `d3d /d`, so `/d` is not optional.
- One local player gets the executable's gameplay: movement, weapons, actors, doors, sector motion, and the CON scripts that drive them.
- Classic mode draws slopes, masked walls, shade, palette, and sprites the way the executable draws them, into the framebuffer mode the executable selects. The result matches a December 1994 frame or comes very close. The SDL window shows that buffer.
- VOC sound effects play when the game plays them. MID songs play when the game plays them. Output is the machine's audio device.
- These files are read as they ship, with no conversion step and no new container: maps, `TILES*.ART`, `GAME.CON`, `DEFS.CON`, `MUSIC.CON`, VOC files, MID files, the eight demos (`MYDEMO_0.DMO` to `MYDEMO_7.DMO`), `PALETTE.DAT`, `LOOKUP.DAT`, `TABLES.DAT`, and `SETUP.DAT`.
- The executable asks for lowercase names (`tiles000.art`, `mydemo__.dmo`, `game.con`, `elev1.voc`). The files ship uppercase, and `FASTWAY2.mid` is mixed. Opening a file matches its name case-insensitively on every platform, so the same files load on Windows, macOS, and Linux without renaming.
- A file the executable names but the release does not ship (for example `waterfal.voc`, `bodybloa.voc`, `switch.voc`, `subway.voc`, `xoptor.voc`, `FILE_ID.DIZ`) is handled the way the executable handles a missing file. It does not crash, and it does not get a placeholder file.
- Keys are the seventeen scancodes in `setup.dat`, plus the hardcoded keys the executable ORs in. The executable's mouse turns and moves. A and Z look up and down.
- The program stays C, SDL2, and CMake, and builds for Windows, macOS, and Linux.
- Game code from the executable is lifted into the engine, audio, and game files already in this tree.

## Non-goals

- Polymost, or any second world renderer. It may come later. This rebuild does not start it.
- Multiplayer that talks to anything. Serial and IPX stay as stubs: the functions and call sites remain, they do not open a port or a socket, and startup does not search for another player.
- A port of `SETUP.EXE`, or any new setup UI. Modernizing setup is a later project. The game reads the `setup.dat` that tool already writes.
- Mouse-look as the current port implements it (mouse Y changes the horizon).
- A GRP archive, a map editor, or a content converter.
- Copying duke3d's game, CON, or rendering implementation into this tree.

## Anti-goals

- A game that feels like the beta while following different rules, a different draw, or different data layouts.
- Treating the current portal approximation as the classic renderer.
- Silently dropping a shipped file the executable accepts, or rewriting it into another format so the port can load it.
- Letting a stub claim success. A stub that prints a line and returns is unfinished game code, not a finished system.

## How it plays

Launch with no level and the attract demos run. Esc opens the menu. Quit on that menu is what leaves to the shell.

Launch with the menu, or with the executable's command line, and a local game starts on the chosen episode, level, and skill. During play, Esc opens the in-game menu, and Return to Game comes back to the level. That game uses the same simulation the demos use. Demos are recordings of that simulation, not a separate camera path. The Demo entry on the main menu plays a demo the way the executable's menu does.

`setup.dat` supplies the graphics mode, the sound and music card choices, the comm bytes, and the key scancodes. The shipped file in this repo has sound and music on. The game honors that file. It does not grow a second binding layer.

## Decisions already made

These came from the person who owns the repo:

- Faithful to `D3D.EXE`, including the classic picture, which must match or come very close.
- Playback of the included demos is required.
- Local play and the attract demos are both in. Multiplayer is stubs for now.
- The menu and the command line are both ways into a local game. Every page of that menu is in this rebuild, not only New Game.
- Existing content loads with no conversion.
- Sounds and music are in, heard on the machine's audio device.
- Polymost is acceptable later and out of scope for this rebuild.
- Setup modernization is a later project.
- The modern stack already in the tree stays: C, SDL2, CMake, Windows, macOS, and Linux.

## Constraints

- Behavior that is still unclear is recovered from `D3D.EXE` (the IDA database is local and stays uncommitted). It is not filled in from the 1996 duke3d source.
- Minimum code. No system the executable does not have. No abstraction for a single caller.
- `D3D.EXE`, `DOS4GW.EXE`, and `SETUP.EXE` stay untracked. So do IDA databases.
- The three existing libraries stay the home for lifted code: `source/build` (engine and platform), `source/audiolib` (AudioLib), and `source/` (game, CON, menus, demos, sounds).
- Platform differences stay in the existing platform files. The game and the classic drawer do not call Win32, Cocoa, or a Linux-only API.
- Commit subjects, when a commit is requested, stay short, with a subsystem prefix (`build:`, `audio:`, `game:`), and no trailer metadata.

## Facts already recovered

Treat these as given. A later reading of the executable can correct one. Do not replace them with the layout from a newer Build game.

- The executable is Watcom DOS/4GW. There are no source filenames in it. Code runs from `0x10000` to about `0x7d000`. `main` is `0x15B60`.
- Map version must be 5. The loader rejects anything else, including `L9.MAP` and `M8.MAP` (both version 4), as a missing board. Sector is 37 bytes, wall 35, sprite 38, packed. A good v5 file is fully consumed. `+z` is down. Floor color is sector `+6`. Sector `+31` is the lotag. Spawn eye height used by the loader is `floorz - 9216`.
- In the actor walker, sprite `+28` is a sector index. The labels in `spritetype` that call `+28` statnum and `+30` sectnum are not safe to trust until checked against one real sprite.
- ART version must be 1. Files are `tiles000.art` through `tiles007.art`. Pixels are column-major. Index 255 is transparent on masks and sprites.
- `PALETTE.DAT` begins with 768 VGA 6-bit entries. `TABLES.DAT` is 11904 bytes. The first 4096 are 2048 signed 16-bit sine values: index 0 is 0, 512 is 16383, 1024 is 0, 1536 is -16383.
- Demo file `mydemo_N.dmo`: uint8 volume, level, skill, int32 count, then count bytes of forward, strafe, turn, and tics, then count int32 bit words. Playback clears bits 19 and 20 (`0xFFE7FFFF`). Record cap is 16384. Velocities are signed bytes and are the values after the timer ramp, not raw key holds.
- The PIT runs at 120 Hz (divisor `0x26D6`). Each interrupt ramps once and adds 1 to the game clock. When the music byte in `setup.dat` is nonzero, it ramps five more times and adds 5, six steps total. Forward and strafe accelerate by 8 and drag by 2. Turn accelerates by 16 and drags by 10. Clamp is -128..127. Held forward settles at 125. Held turn settles at 117. Left strafe increases the strafe byte. Right decreases it. Positive strafe is screen-left under the view basis below.
- One demo sample is applied once. Angle updates first: `ang += (turn * tics) / 24`, wrapped to 0..2047. Position uses `(trig * velocity * tics) >> 17` after the clipmove shift of 14 on a vector already shifted by 3. Bit 1 of the sync word halves that step. Lotag 2 with the high bit masked quarters a negative forward byte. `clipmove` is called with wall distance `0x90` and ceiling and floor distances `0x400`.
- View basis at draw time: cos is `sintable[(ang+512) & 2047]`, sin is `sintable[ang & 2047]`. Forward is `(dx*cos + dy*sin) >> 14`. Lateral is `(dy*cos - dx*sin) >> 14`. At 320 wide, screen Y is `horiz + zdiff * 8 / forward`. Increasing angle turns from +X toward +Y, and +Y is screen-right.
- Demos 0 and 1 use only tic counts that are multiples of 6, which is the six-step clock (720 tic-units per second). The other included demos contain tic counts that are not multiples of 6, so their clock is 120 Hz. Each sample carries its own tic count, so the simulation does not depend on which clock was used. Only the wall-clock pace of playback does (see Left to the builder).
- `setup.dat` is 6 config bytes, then 17 key scancodes. Byte 0 is graphics, byte 1 is the sound menu index, byte 2 is music, bytes 4 and 5 are the comm type and rate. Byte 3 has not been identified yet. The shipped file begins `00 01 01 01 00 00`. The sound menu index maps through `{ -1, 0, 1, 3, 4, 7, 9, 10, 12 }` before it reaches AudioLib. In this executable, digitized card 8 is Gravis Ultrasound. It is not SoundScape, and it is not the later duke3d numbering.
- The digitized init from `main` is channels 2, 16-bit, mix rate 11000. Music init is the same card lookup with address 0. Both are skipped when their `setup.dat` byte is 0.
- Movement keys consult `setup.dat` and also the hardcoded arrows. The other sync bits are hardcoded scancodes. Bits 0 and 1 (A and Z) drive look. Bit 1 also halves the position step. Bits 19 and 20 are cleared during demo playback.
- Demo headers: 0 is volume 0 level 0, 1 is 0/5, 2 is 3/3, 3 is 3/0, 4 is 0/5, 5 is 0/4, 6 is 1/6, 7 is 0/1, all skill 0. Volumes and levels are 0-based, so volume 3 uses the `O?.MAP` files.
- `GAME.CON` is compiled by the executable at startup. Weapons, actors, and most interactive behavior live there. The port's compiler is still a stub that only opens the file.

## Where the tree is now

CMake already configures and links `d3d.exe`. From the repo root the startup text runs through the video mode, art and maps load, and a local level or an attract demo presents a picture. The movement step and the key ramp match the facts above. That match is in the uncommitted working-tree change to `board.c`, `sdl_input.c`, and `game.c`. It is not in `1d65a0e`. File opens mix cases (`"PALETTE.DAT"` next to `"lookup.dat"`), so on a case-sensitive filesystem the tree only works with some of the shipped files. `/d` and `/e` are parsed and do nothing. The picture is a software portal approximation: flat floors and ceilings, no slopes, approximate wall and floor sampling, billboard sprites. It is not the executable's column drawers. CON does not execute. The AudioLib mixer, VOC playback, and MIDI playback do not produce samples. The menu does not run. Multiplayer stores the comm bytes and returns.

That approximation is scaffolding. Classic mode replaces it. It is not kept as a second renderer in this rebuild.

## Left to the builder

These are calls made while writing this down, not choices the owner stated. Change one if the executable disagrees.

- The SDL window nearest-scales the classic framebuffer. It does not change the game's internal resolution. The mode still comes from `setup.dat` through the executable's mode select. The shipped file selects 320×200, shown in a larger window.
- The Joystick page keeps the executable's axes, buttons, and calibration. The device under it is the machine's joystick through SDL. With no joystick connected, that page behaves as the executable behaves with no joystick.
- OpenGL, if the existing present path uses it, only puts that framebuffer on screen. No Polymost-style world rendering is added.
- Digitized and MIDI output is mixed in process and handed to SDL audio. The card id from `setup.dat` still selects the executable's branch. The port does not need a Sound Blaster, GUS, or MPU-401.
- Save and restore are required, because those menu pages are in. The slot filenames and the bytes inside a slot are whatever the executable writes. The `/e` map encode path, and any other command the executable implements, are lifted with the code that implements them. Nothing in that set is invented, and nothing in that set is dropped because it is awkward.
- Esc opens the menu, in the attract loop and during play, because that is how this executable reaches the menu. Quit is the path back to the shell. This replaces an earlier note that Esc itself exited.
- Demo playback is paced by the clock the file was recorded on, 720 tic-units per second for demos 0 and 1 and 120 for the rest, judged by whether every tic count is a multiple of 6. The executable would play every demo at whatever clock its own `setup.dat` selects. This port chooses the recording's pace instead so the demos look as they were recorded. Change this if a capture of the executable shows otherwise.
- Reference captures come from `D3D.EXE` running under DOSBox on this machine, with the shipped `SETUP.DAT`. They stay local and untracked, like the IDA database. "Matches the executable" in Acceptance means it matches those captures.
- There is no new test framework. Checks are the game itself: the included demos, a local level started from the menu and from the command line, and a captured classic frame. The `LAMEDUKE_FRAME` dump can stay as a debug aid until a captured frame is no longer how drawing is checked. It must not fire unless that variable is set.
- Work lands in the existing files. A new file is added only when the executable's own split, or a platform boundary, calls for it.

## Acceptance

1. No arguments, from the repo root: attract demos run on their own clock, the picture is classic mode, and the VOC and MID for that session are audible. Esc opens the main menu. Quit on that menu returns to the shell.
2. Every main-menu page and every in-game menu page does what the executable's page does, including new game, save, restore, instructions, game options, ordering info, demo, high scores, and quit. The menu and `/v` `/l` `/s` each start a local game on a shipped map. Weapons, actors, doors, and CON match the executable's behavior on that map.
3. A classic frame of the level 1 start, and a classic frame of the attract demo, shows the slopes, masks, and shade the executable draws. The portal approximation is gone.
4. Opening a shipped VOC or MID by converting it first is not part of the pipeline. The game opens the files by the names it already uses.
5. `setup.dat` keys, A and Z, and the executable's mouse are the controls. Mouse Y does not look up and down.
6. Multiplayer startup does not open a serial device or a network socket.
7. `L9.MAP` and `M8.MAP` fail the version check the executable fails. They are not converted to version 5.
8. The existing CMake build still produces `d3d.exe` for the host, with the Windows, macOS, and Linux platform files left in a state that builds. On a case-sensitive filesystem the shipped, uppercase files load unchanged.
9. Every command-line flag in the executable's help list does what the executable does with it, `/d` (from `GO.BAT`) and `/e` included.
