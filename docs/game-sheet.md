# LameDuke game sheet

The facts below come from the shipped data files and from the code lifted from `D3D.EXE`. How the rebuild was made is in [reconstruction.md](reconstruction.md).

## Release

| | |
|---|---|
| Name | LameDuke (`LAMEDUKE.ZIP`) |
| What it is | A very early Duke Nukem 3D beta |
| EXE date | December 30, 1994 |
| Released | January 29, 1997, a year after the Duke Nukem 3D shareware release |
| Title in the exe | `DUKE DUKEM 3D Beta version 1.3.95 -- DO NOT DISTRIBUTE!!! That would be bad.` |
| Platform | DOS, Watcom with DOS/4GW (`DOS4GW.EXE` ships beside it) |
| Credits in `LAMEDUKE.TXT` | George Broussard, Todd Replogle, Allen Blum, Dirk Jones, Douglas R. Wood, Randy Pitchford, Brian Martel, Keith Schuler, Stephen Hornback, Lee Jackson, Ken Silverman and Bobby Prince |

## Files

All files are loose in one directory. There is no GRP.

| Kind | Files |
|---|---|
| Programs | `D3D.EXE`, `SETUP.EXE`, `DOS4GW.EXE`, `GO.BAT` (runs `d3d /d`). The three EXEs are not tracked in this repo. |
| Maps | `L1`–`L9`, `M1`–`M8`, `N1`–`N8`, `O1`–`O4`, `WAR1`, `WAR2` (`.MAP`) |
| Art | `TILES000.ART`–`TILES007.ART` |
| Tables | `PALETTE.DAT`, `LOOKUP.DAT`, `TABLES.DAT` |
| Scripts | `GAME.CON`, `DEFS.CON`, `MUSIC.CON` |
| Sounds | 34 `.VOC` files |
| Music | `FASTWAY.MID`, `FASTWAY1.MID`, `FASTWAY2.mid`, `BROWNEYE.MID`, `E2M1.MID`, `MISTACHE.MID` |
| Demos | `MYDEMO_0.DMO`–`MYDEMO_7.DMO` |
| Settings | `SETUP.DAT` (written by `SETUP.EXE`) |
| Notes | `LAMEDUKE.TXT` |

The game also writes `userdefs.tmp` (visibility, quotes, brightness, sound, music and mouse settings), the save files `SAVE___n.SAV` and `SAVE_vln.SAV`, and the high score table. The port adds `duke.cfg`.

## Episodes and levels

`enterlevel` (0x249B0, `source/premap.c`) builds the map name as `'L' + volume` and `'1' + level`, both counted from 0. So volume 1 is `L`, volume 2 is `M`, volume 3 is `N` and volume 4 is `O`. Episode names come from the New Game menu.

| `/v` | Episode | Maps | Notes |
|---|---|---|---|
| 1 | Mr. Caliber | `L1`–`L9` | `L9` is version 4 |
| 2 | Mission Cockroach | `M1`–`M8` | `M8` is version 4 |
| 3 | Suck Hole | `N1`–`N8` | |
| 4 | Hard Landing | `O1`–`O4` | |

- The exe loads only version-5 maps. `L9` and `M8` are version 4, so loading them stops with "Board not found." `convmap4` converts them. See [map-format.md](map-format.md#version-4).
- An exit (sector lotag `0xFFFF`) moves to the next level number. After `L8` that is `L9`, so the game stops there. The same happens after `M7`, and after the last map of each episode.
- No volume number builds the name `WAR1.MAP` or `WAR2.MAP`. The game cannot reach them; the editor can open them.
- The menu always starts a new game at level 1 of the chosen episode.
- `LAMEDUKE.TXT` says "levels 1-10 exist for volumes 1-3". The files hold 9, 8 and 8, plus a fourth volume of 4.
- Skills on the menu: Easy, Medium, Hard, Harder.

### Demos

Each demo's header holds its volume, level and skill. The attract loop starts at demo 1, plays 1 to 7, then 0, and repeats.

| Demo | Map | Samples |
|---|---|---|
| 0 | L1 | 2064 |
| 1 | L6 | 2959 |
| 2 | O4 | 213 |
| 3 | O1 | 774 |
| 4 | L6 | 2218 |
| 5 | L5 | 1649 |
| 6 | M7 | 5267 |
| 7 | L2 | 3550 |

All eight are recorded at skill 0.

## Command line

From the exe's help (0x154F0) and `check_args` in `source/premap.c`. Flags start with `/` and ignore case.

| Switch | Effect |
|---|---|
| `/?` | Print the help and exit. |
| `/v#` | Volume # (1–4). |
| `/l#` | Level #, and start that level instead of the demos. |
| `/s#` | Skill. The help says "0-3", but the code subtracts 1, so `/s1`–`/s4` give the four skills. |
| `/q` | Help quotes on. |
| `/d` | Debug mode. Starts with all ammo raised, unlocks every weapon key and inventory item, and gives the U door cheat. |
| `/r` | Record a demo. On exit it writes `mydemo_0.dmo` for volume 1 level 1, otherwise `mydemo_1.dmo`. These overwrite the shipped demos. |
| `/oDIR` | Prints "I/O path rerouted to DIR." Nothing reads the path. |
| `/e` | Prints "Encoding L1.MAP" through "Encoding N9.MAP". It writes nothing. |
| `/p#` | Not in the exe. Renderer for this run: 0 Classic, 1 solid colour, 2 Polymost, 3 OpenGL. |

Any other argument prints `Unknown command line paramater` (sic) and the help.

## Enemies

From `GAME.CON` `actor` lines, `DEFS.CON` and the hard-coded actors in `source/actors.c`. Tiles marked "code" have no CON actor; their behavior is built into the exe.

| Name in `DEFS.CON` | Tile | Driven by | Notes |
|---|---|---|---|
| `FEMANOID` | 408 | CON, strength 5 | |
| `MANDROID` | 490 | CON, strength 5 | |
| `TROOP1` | 1536 | CON, strength 10 | Has a jetpack hover |
| `CAPTAIN` | 1680 | CON, strength 30 | |
| `JELLYFISH` | 1820 | CON, strength 20 | |
| `DRONE1` | 579 | code | Counted as an enemy at spawn |
| `SHARK` | 1280 | code | The code moves it as a flying drone that can cling to the player's face |
| `GREENSLIME` | 961–968 | code | A face-hugging parasite |
| `FLOORTURRIT`, `CEILINGTURRIT` | 930, 931 | code | Turrets |
| `BOUNCEMINE` | 940 | CON | |
| `DRUNKGUY1` | 631 | CON | Not counted as an enemy |

`LAMEDUKE.TXT` says E1L4 shows the early OctaBrain. Which tile that is has not been checked.

## Weapons

Keys 1–5 pick a weapon. `GAME.CON` names them `WEAPON1`–`WEAPON5`. The player starts with weapon 2.

| Key | Tile | `DEFS.CON` name | Pickup | Ammo |
|---|---|---|---|---|
| 1 | 1230 | `CLUB` | always held | battery |
| 2 | 191 | `SHOTSPARK1` | `FIRSTGUNSPRITE` (1125) | clips: `AMMO` (723) gives 48 |
| 3 | 389 | `BULLET` | `GUN2ASPRITE` (38) | battery: `BATTERYAMMO` (1120) gives 20 |
| 4 | 721 | `HEAVYHBOMB` | none in `GAME.CON` | bombs |
| 5 | 563 | `RPG` | `RPGSPRITE` (1121) | rockets: `RPGAMMO` (1119) gives 5 |

Starting ammo is 24 clips, 9 battery, 2 bombs and 5 rockets. With `/d` it is 144, 120, 696 and 99. `LAMEDUKE.TXT` describes weapon 1 as "a tazer type stick", weapon 3 as becoming the three-barrel chaingun, and weapon 5 as "the BIG rocket launcher". It also mentions a laser sight on the pistol (`LASERSITE`, tile 214, has a CON actor).

## Items and inventory

| Item | Tile | Effect |
|---|---|---|
| `SIXPAK` | 722 | +60 health |
| `COKE` | 60 | +10 health, +100 score |
| `STEROIDS` | 724 | Inventory; F1 uses it |
| `SHIELD` | 726 | Inventory; F2 turns it on, and it takes damage in place of health |
| `AIRTANK` | 727 | Scuba gear; F3 |
| `HOLODUKE` | 720 | Inventory; F4 places a decoy player |
| `JETPACK` | 728 | Inventory; F5 turns it on and off |

The player starts with 100 health, the `APLAYER` actor's strength. I shows the inventory bar. Without `/d`, F1, F2 and F3 also open How to Play, Save and Restore.

## Music

`MUSIC.CON` fills the song table with `music N` lines. `music 0`–`3` are episodes 1–4. `music 4` is the table that music sprites pick by lotag.

| Episode | Songs by level |
|---|---|
| 1 | L1 `FASTWAY.MID`, L2 and L3 `BROWNEYE.MID`. L4–L9 name no song, so the last song keeps playing. |
| 2–4 | `FASTWAY.MID` on every level |
| Music sprites | `FASTWAY.MID` |

`FASTWAY1.MID`, `FASTWAY2.mid`, `E2M1.MID` and `MISTACHE.MID` ship but no script names them.

## Sounds

The exe loads 39 names (`off_82838`, `source/sounds.c`). Six are not shipped: `xoptor`, `waterfal`, `bodybloa`, `bodyblor`, `switch` and `subway`. A missing sound stays silent.

`elev1`, `shooting`, `running`, `expl1`, `xoptor`, `glass`, `door1`, `knuckle`, `jump`, `land`, `scuba`, `waterfal`, `getweapn`, `bodyblop`, `bodybloa`, `bodyblor`, `onBoard`, `dripping`, `jetPakOn`, `jetPakI`, `fta_hm`, `fta_hf`, `fta_r`, `liteflck`, `biglite`, `switch`, `reactor`, `jetpakof`, `runningw`, `squish1`, `rebound`, `gun1`, `subway`, `clipin`, `clipout`, `elev3`, `elev2`, `door2`, `rpg`.

`COPTER.VOC` ships but is not on the list. `GAME.CON` plays only `getweapn`, `squish1`, `expl1` and `biglite` itself; the rest come from the code. See [audio.md](audio.md).

## Controls

Movement keys come from `SETUP.DAT` bytes 6–9 (forward, back, left, right) and byte 11 (strafe). The keypad and grey arrows always move. Mouse X turns and mouse Y moves. Every other key is a fixed scancode (`syncbits_from_keys`, `source/build/sdl_input.c`; `playerkeys`, `functionkeys`).

| Key | Action |
|---|---|
| Arrows | Move and turn. The shipped `SETUP.DAT` uses the arrow keys and Right Ctrl to strafe. |
| Left Ctrl | Fire |
| Space | Open, use |
| A, Z | Jump, crouch (swim or fly up and down) |
| Shift | Run |
| 1–5 | Weapons |
| Page Up / Home, Page Down / End | Look up, look down |
| Keypad 5 | Centre the view |
| Insert, Delete | Look left, look right |
| C | Crack knuckles (replays the weapon raise) |
| Enter | Lower or raise the weapon |
| I | Inventory bar |
| F1–F5 | Inventory items, above |
| Tab | 2D map |
| Keypad +, Keypad − / − | View size, or map zoom |
| P | Pause |
| T | Type a message |
| Esc | Menu |
| F10 | Brightness |
| F12 | Screen capture |
| M, S | Music on/off, sound on/off |
| V | Visibility |
| K | View the next player |
| D | Debug mode on/off, the same as `/d` |
| W | Warp to a level (debug mode only) |
| U | Open every door and flip the force fields (debug mode only) |

`LAMEDUKE.TXT` says the team did not know of any cheat codes. In the code, D toggles debug mode during play.

## Quirks from `LAMEDUKE.TXT`

- "It might not work on all systems." No support was offered.
- Levels may need to be loaded by hand; nobody was sure the exit switches worked.
- "The timer is going nuts." On faster machines the game runs too fast. See [game-speed.md](game-speed.md) for why and what the port does.
- There is no Dukematch. The exe reads comm settings, but the port's network is a stub ([network.md](network.md)).
- Sound is "probably SoundBlaster only".
- Parts of the levels were reused for the Plutonium PAK's "Derelict".
