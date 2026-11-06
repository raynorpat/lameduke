# Menus

The menus are `menus()` in `source/menus.c` (0x3CCF0). They draw over the game frame every frame while `ps.gm` has bit 0. `menupage` (dword_1252F0) picks the page. The README says the menus are not complete; this page lists what the code does and what is missing.

## Game state flags

`ps[screenpeek].gm` holds the game's mode. The bits the code tests:

| Bit | Meaning | Set by |
|---|---|---|
| 0x01 | menu open | Esc in play or in a demo, F1-F3 without `/d`, `menupage` changes |
| 0x02 | attract mode / quit to main menu | `enterlevel(2)` for demos; in-game menu "Quit to Main Menu" |
| 0x04 | playing a level | `enterlevel(4)`, "Return to Game", `loadplayer` |
| 0x08 | level end | `checksectors` on a sector with lotag 0xFFFF |
| 0x10 | typing a message | T (`functionkeys`); Enter clears it (`typemessage`) |
| 0x80 | quit | main menu Quit or Esc; in-game menu Q |

Most writes replace the whole value. "Return to Game" sets `gm = 4`, which also closes the menu. Main menu "Demo" sets `gm = 6`, which the attract loop reads as "next demo" (0x16007). `play_level` checks bit 3 after each logic step and starts the next level (0x17411). After each frame, bit 1 returns to the attract loop (0x17482) and 0x80 quits (0x16036).

The menu does not pause the game. While it is open, `play_level` keeps running logic steps but sends zero input (getinput 0x30E60 does the same). Actors keep moving behind the menu.

## Opening the menu

- In play, Esc reaches `playerkeys` as `syncbits` bit 31. It opens page 0 with `gm |= 1`, unless the player is paused, viewing through a camera (`ps.w56 >= 0`) or `gm` has bit 1. If the 2D map was up, the game switches back to 3D.
- In an attract demo, Esc opens page 0 directly (0x15F51).
- Without `/d`, F1 opens Instructions (page 20), F2 Save (page 50) and F3 Restore (page 10). With `/d` those keys only work the inventory.
- Page 0 opened while `gm` has bit 2 becomes the in-game menu (page 100). So "Back" on any sub-page returns to the main menu in the attract loop and to the in-game menu in a level.

When a page opens, the pointer goes to (160, 100) and the title restarts its spin. On a page with items starting at y=40 or 46, that puts the pointer on the fourth line.

## Keys and mouse

`menuinput` (0x3C6F0) handles every list page:

| Input | Result |
|---|---|
| Up / Down (main or keypad) | move one line, wrapping at the ends |
| Enter, Space, left click | pick the line under the pointer |
| Esc | -2, usually Back |
| Q, right click | -1 |

`drawpointer` (0x1A6F0) moves the pointer with the mouse, scaled by Mouse Sensitivity, and draws tile 614 when the view is 3D. A click counts when the button is released. The line under the pointer pulses (`hudshade`) and dims while a button is held. Letters are tiles: digits at `c+101`, lowercase at `c+26`, uppercase at `c+32`. Any other character repeats the glyph before it.

The page title spins and grows in. `titlespin` goes up by 348 each logic step until it reaches 7844, then holds at 0x2000. Frames drawn between logic steps leave it alone (see [game-speed.md](game-speed.md)).

## The menu tree

Main menu, page 0 (title tile 1129). Esc and Q both quit here.

| Item | Goes to |
|---|---|
| Begin a New Game | Episode (page 1, tile 616): Mr. Caliber, Mission Cockroach, Suck Hole, Hard Landing, Back. Then Skill (page 2, tile 617): Easy, Medium, Hard, Harder, Back. A skill calls `newgame(volume, 0, skill)` and `enterlevel(4)`. |
| Restore a Game | page 10 (tile 613): eight slots and Back |
| Instructions | page 20 (tile 615): Back |
| Game Options | page 30 (tile 1128) |
| Ordering Info | page 40 (tile 1127): Back |
| Demo | `gm = 6`: the next attract demo |
| High Scores | page 60 (tile 1138) |
| Quit | `gm = 0x80` |

In-game menu, page 100 (tile 1129). Esc returns to the game; Q quits the game.

| Item | Goes to |
|---|---|
| Return to Game | `gm = 4` |
| How to Play Me | page 20 |
| Save a Game | page 50 (tile 1137): eight slots and Back |
| Restore a Game | page 10 |
| Game Options | page 30 |
| Quit to Main Menu | `gm = 2`: back to the attract demos |

Game Options, page 30:

- Sound Volume, Music Volume and Mouse Sensitivity are sliders of 32 segments (tiles 678 and 679), 0-255. With the pointer on a slider's row, Left and Right move the pointer 8 pixels and set the value, and a held mouse button sets it from the pointer's x. The volumes go to `FX_SetVolume` and `MUSIC_SetVolume` every frame.
- Joystick does nothing.
- Video Options opens page 31.

Instructions shows "Jump", "Duck", "Shoot" or "Open" for `syncbits[0]` bits 0x1, 0x2, 0x4 and 0x8. In a level the input is zero while the menu is open, so the words only show up while a demo plays behind the menu.

High Scores draws the table from `drawscores` (0x1A8B0): 12 rows of quote and score, the top one pulsing. Only Esc, Q or a right click leave it.

## Video Options

Page 31 is a port addition. Game Options had five items before it was added. Its rows: Renderer, Resolution, Display, Texture Filtering, Mip Level, VSync, Contrast, Saturation, Supersample, Back.

- Up and Down pick a row, and the pointer's y picks it too. Esc, Q or a right click go back to Game Options.
- Left and Right change the row's value. Enter steps it forward.
- Resolution is staged with Left and Right and applied with Enter. The list is the display's modes plus the desktop size and the current size.
- Renderer skips Polymost Debug, renderers this build lacks, and OpenGL after it has failed.
- Rows that do nothing with the current renderer are dimmed and ignore keys: Resolution under Classic, Filtering and Mip Level without OpenGL, Contrast, Saturation and Supersample without OpenGL's shader path.
- A change is applied at the start of the next frame (`menus_applyvideo`). When it works, that row is copied into the saved settings and `duke.cfg` is written. When it fails, the page shows "Video setting unavailable" (or "OpenGL could not start").

The README's "Renderers and Video Options" section covers `duke.cfg` and the renderers; see also [rendering.md](rendering.md).

## Save and restore

`config.c` writes two files per slot (1-8):

- `SAVE___n.SAV` (0x1B630): all 17 `ps` records, `hittype`, the 256 animated walls (6 bytes each) and the moving sector points `msx` and `msy`. The port keeps script and music positions as indices; on save it turns them back into the exe's addresses (`script` at 0x976CC, `musicfn` at 0x8AC70, plus the DOS/4GW load offset 0x194000), so the slot holds what the exe would write. Restore converts them back.
- `SAVE_vln.SAV`: the board, through `saveboard`, with v the episode, l the level and n the slot, all counting from 1.

`loadplayer` (0x1B700) builds the board name from the episode and level being played now, before it reads the slot. This is the exe's behaviour. Restoring a slot saved on another level loads that slot's player data onto the wrong board, or exits with "Board not found." when no such file exists. From the main menu, "now" is the attract demo's level. The board load also renumbers sprites while `hittype` keeps the saved order, as in the exe; the port zeroes any action or move that is no longer a valid script index.

Picking a save slot saves and returns to the menu. Picking a restore slot loads and sets `gm = 4`, which closes the menu and, from the attract loop, starts play.

## userdefs.tmp

`load_user_stats` (0x1B520) and `save_user_stats` (0x1B5D0) keep 21 bytes, then the 12 score quotes (33 bytes each) and 12 scores. The bytes hold visibility, the `/q` quote flag, brightness, sound on, music on, then 16-bit `debugmode`, sound volume, music volume, `gamepaused`, `cameraspr` and mouse sensitivity. Bytes 17-20 are kept but never read. Without the file the defaults are visibility 13, volumes 255, mouse 128, sound and music on, brightness 1. The file is written on every exit.

## Quit and high scores

Quit (`gm = 0x80`) calls `quitgame` (0x1749E). Unless `/d` is on or the player came from Quit to Main Menu, a score that makes the table asks for a quote at that row. `newscore` (0x24E80) moves rows 1-10 down one place but overwrites the landing row instead of moving it, as the exe does. So the score it replaces is lost and the row below appears twice. The table stays up until Esc, Space or Enter. Then the demo (if recording) and `userdefs.tmp` are written and the game exits with "Game Over, or is it?".

## Function keys

`functionkeys` (0x14CD0) runs once per logic step in levels and once per sample in demos, outside the sync stream:

| Key | Action |
|---|---|
| F12 | screen capture |
| F10 | brightness 0-4 |
| T | type a message (`gm |= 0x10`) |
| D | toggle `debugmode` |
| M | pause or resume music (only with music set up) |
| S | sound effects on or off (only with sound set up) |
| V | visibility, 16 down to 7, then wraps to 16 |
| K | next player in the ring; see [network.md](network.md) |
| W | warp screen, with `/d` on and no message prompt |

## Help quotes

With `/q` (or the flag saved in `userdefs.tmp`), `quote(q)` (0x12630) shows one of 13 lines from `fta_quotes`, such as "Go get the gun." or "You found a secret room..!". The line stays for 4 logic steps per character and can't restart until it has run out. `displayquotes` draws it at y=166, or 189 when the view size `ps.w4a` is negative.

## Title and attract loop

main goes straight into the attract loop. It plays the demos from `mydemo_1.dmo` on, wrapping after 7 (see [network.md](network.md) for the format). Each demo starts with `enterlevel(2)`, which shows tile 482 with "Entering..." instead of the episode map. Esc opens the main menu over the running demo. A new game or a restore leaves the loop for `play_level`, and Quit to Main Menu comes back to it.

## Pending

- Joystick has no handler.
- The save and restore lists always read "empty"; slot contents are not shown.
- Instructions and Ordering Info show only their title tile and Back.
- The status bar redraws after the map and at level start are marked pending (`premap.c` 0x17890, `player.c` 0x14050). They are not menu code, but they affect what is drawn when a menu closes.
