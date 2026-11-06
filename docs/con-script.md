# CON scripts

The beta drives most of its actors with a small script language. At startup `compilecon` (0x401E0) compiles `game.con` into an array of ints. Each logic step, the actor loops call `execute` (0x42260), which runs one actor's code. This is the first form of the CON language that shipped with Duke Nukem 3D in 1996. It has 60 keywords and no variables.

The code is `source/gamedef.c` and `gamedef.h`. The scripts are in the repo root:

| File | Lines | Contents |
|---|---|---|
| `GAME.CON` | 1629 | the actors: 9 states and 49 actors, with their actions and moves |
| `DEFS.CON` | 195 | `define` lines naming tiles, such as `define APLAYER 283` |
| `MUSIC.CON` | 10 | the `music` lists |

`GAME.CON` starts with `include defs.con` and `include music.con`. The map editor reads `DEFS.CON` for its tile names (see [map-editor.md](map-editor.md)).

## The compiler

`compilecon` reads `game.con` whole. As in the exe, it ends the text two bytes before the end of the file. Then it calls `parsecommand` (0x3ED60) until that returns nonzero.

Tokens are runs of letters, digits and `{ } / * - _ .`. Anything else separates them. A token is looked up as a keyword first (`keyw[]` at 0x82B10), then as a label; anything else is an error. Numbers are read with `atoi`.

Each keyword is compiled as its index in `keyw[]`, followed by its arguments. So `shoot SHOTSPARK1` becomes `13, 191`. `script[0]` stays 0 so that index 0 can mean "none".

| Limit | Value |
|---|---|
| `MAXSCRIPTSIZE` | 16384 ints |
| `MAXLABELS` | 512 labels of up to 63 characters |
| `NUMKEYWORDS` | 60 |
| `NUMCONSOUNDS` | 38 sound names |
| errors and warnings | compiling stops once there are more than 12 |

### Labels

`labels[]` (0x12523C) and `labelcode[]` (0x125244) hold one table for every name. A `define` stores its number. An `action`, `move` or `state` stores the script index where its data or code starts. Names are case sensitive. `define`, `action` and `move` warn about a name already in the table and keep the first one. `state` does not check.

### Definitions

These go outside any actor or state:

- `define NAME value`. Nothing is emitted.
- `action NAME start frames views inc delay` puts 5 ints in the script, padding with zeros when fewer are given.
- `move NAME xvel zvel` puts 2 ints, also padded.
- `state NAME` starts a block of code that ends at `ends`.
- `actor TILE strength action move flags...` starts an actor that ends at `enda`. It writes a 4-int header: strength, action, move, and the OR of all the move flags given. Missing values are 0. `actorscrptr[TILE]` (`MAXTILES` entries) points at the header, and the code follows it.
- `music V file file ...` fills row V (1-4) of `musicfn` with up to 10 names of 12 characters. Any other V fills row 0. If fewer than 10 names are given, the first unfilled slot is copied over the rest, so they stay empty. A level plays the song in row `volume + 1` at its level number, so `music 1` lists episode 1. MUSICANDSFX sprites pick from row 0 by lotag. See [audio.md](audio.md).
- `include file` compiles another file in place.

### Ifs and the jump slot

Every `if` command is followed by one slot and then the one command it guards, which may be a `{ }` block. After compiling that command, the compiler writes the index after it into the slot. `else` has the same form.

```
ifrnd 128  action JP_FRAMES  else action SHOOTING_UP_FRAMES

3  128  [slot] 7 <JP_FRAMES>   10 [slot] 7 <SHOOTING_UP_FRAMES>
           |                 ^          |                        ^
           +-----------------+          +------------------------+
```

At run time `doif` runs the command after the slot when the test is true. The `else` then jumps over its own command. When the test is false, it jumps to the slot's index and, if an `else` is there, runs the command after the else's slot. Comments between an `if` and its command are skipped. An `else` with no `if` before it gives a warning and is dropped.

### Comments

`//` runs to the end of the line and `/*` to the next `*/`. Both are keywords, so they need blank space after them: `//note` or `/*---` is read as one unknown token. `GAME.CON` always follows them with a space or a newline.

### Messages

The compiler prints its progress and problems to stdout:

```
Loading script...
    Compiling...
    Loading include file defs.con
  * ERROR!(L120) Symbol 'TROOP' is undefined.
  * Found 0 warning(s) and 1 error(s).
```

The messages keep the exe's spelling ("Paramater", "Dupiclate"). They are:

- errors: an undefined symbol or parameter, `actor` inside a `state` or `actor`, `enda` without `actor` (printed as 'aend'), too many `{` before `enda`, a `}` with no `{` open, an unknown sound, an unknown state, `ends` without `state`, `/*` without `*/`;
- warnings: a duplicate `action`, `move` or `define`, an `else` with no `if`.

Any error exits the game. With warnings only, it prints "Press any key to ignore, or ESC to exit." and reads one character with `getchar()`. The exe exits on anything below 32. The port reads stdin a line at a time, so it exits only on Esc (27); Enter, any other key or no input goes on. On success it prints "Total Lines" and "Code Size", which is (script ints − 1) × 4 bytes.

Compiling also stops at `end` or at the end of the text. A `}` outside any block is an error; the exe stopped compiling there without a message.

## The interpreter

`execute(i, p, dist)` runs actor sprite `i` against its nearest player `p` at distance `dist`. Its code is at `actorscrptr[picnum] + 4`. It:

1. Clears `killit`, the last weapon hit and the line-of-sight cache.
2. Steps the action. It adds `synctics` to `sprite.lotag`. When that passes the action's delay, `t[2]` goes up by one, `lotag` goes back to 0 and `t[3]` steps by the action's increment. `t[3]` goes back to 0 at `frames × inc`.
3. Calls `parse` (0x408B0) until a command returns 1 (`enda`).
4. Deletes the sprite if `killit` was run.
5. Applies the move when one is set (`t[1]`) and the flags (`sprite.hitag`) are nonzero. `t[0]` goes up, then the flags act in this order:

| Flag | GAME.CON name | Effect |
|---|---|---|
| 0x200 | `on_owner` | stand on the owner, if it is an actor (stat 1) |
| 0x1 | `slow_to_stop` | halve `xvel` |
| 0x2 | `face_player` | turn to face the player |
| 0x8 | `geth` | `xvel` moves halfway toward the move's xvel |
| 0x10 | `getv` | `zvel` moves halfway toward 16 × the move's zvel |
| 0x20 | `houver` | `zvel` adds or takes 16 × zvel to stay 0x2000 above the floor |
| 0x40 | `stick_to_ceiling` | z = ceiling + 2048 |
| 0x400 | `stick_to_floor` | z = floor − 2048 |
| 0x80 | `face_to_player` | turn part of the way to the player |
| 0x100 | `rhr` | when the move is blocked, turn 256 and try again (not in lotag-1 water) |
| 0x4 | `face_exit` | on the `move` command itself: face the longest clear line of 8 |

A sprite with any velocity is moved by `ssp` (clip type 1).

`parse` runs one command at `insptr` and returns 1 to end a block. `{` and `state` call `parse` until it returns 1. When `killit` is set, every later command returns 1.

The per-actor values:

| Value | Use |
|---|---|
| `hittype[i][0]` | move count (`ifmovecount`, `resetmovecount`) |
| `hittype[i][1]` | current move, a script index |
| `hittype[i][2]` | action count (`ifactioncount`, `resetactioncount`) |
| `hittype[i][3]` | frame offset within the action |
| `hittype[i][4]` | current action, a script index |
| `sprite.extra` | strength; for APLAYER it is the player's health (`ps.w54`) |
| `sprite.hitag` | move flags |
| `sprite.lotag` | action delay timer |

When a sprite spawns, `actorinit` copies the header into these: strength into `extra`, action into `t[4]`, move into `t[1]`, and flags into `hitag` (except for tile 57). The exe stores `t[1]` and `t[4]` as pointers; the port uses indices and converts them in save files (see [menus.md](menus.md)).

The action's 5 ints are read by the sprite drawing in `view.c`. The tile drawn is picnum + start + `t[3]` × views + the angle's frame. `views` is 1, 2, 3, 5, 7 or 8 angles. 3, 5 and 7 mirror the other side, and a negative value flips the sprite.

### Who runs scripts

`execute` is called from `moveactors` (every stat-1 actor without its own code), `moveplayers` (player sprites, with distance 0), `moveweapons` (tiles 191, 1210, 940 and 1406), `moveeffects` (tiles 214, 215, 488, 686, 1359, 1360, 1370 and 1404) and `movestandables` (tiles 627, 905 and 974). A tile without an actor does nothing in `execute`. All of these are in `actors.c`; see [game-speed.md](game-speed.md) for how often they run.

## Keywords

All 60 entries of `keyw[]`, in order. "Form" is what the compiler emits after the keyword. `n` is a number or `define`, `T` a tile, `[s]` the jump slot.

| # | Keyword | Form | Effect |
|---|---|---|---|
| 0 | `end` | none | stops compiling; its word becomes 0x80000000 |
| 1 | `actor` | header | starts an actor (see above) |
| 2 | `addbatteryammo` | n | `ps.w4e += n` |
| 3 | `ifrnd` | n [s] | true when `(krand() >> 8) >= 255 - n` |
| 4 | `enda` | | ends the actor; ends `execute` |
| 5 | `ifcansee` | [s] | player within ±128 of the actor's angle and in sight |
| 6 | `ifhitweapon` | [s] | the actor was hit; remembers the weapon sprite |
| 7 | `action` | action | `t[4]` = action, `t[2]` = `t[3]` = 0. Outside an actor it defines one |
| 8 | `ifpdistl` | n [s] | `dist < n` |
| 9 | `ifpdistg` | n [s] | `dist > n` |
| 10 | `else` | [s] | see the jump slot |
| 11 | `strength` | n | sets `extra`, or the player's health |
| 12 | `break` | | ends the block or state |
| 13 | `shoot` | T | `shoot(i, T, 0)` |
| 14 | `addscore` | n | player score `ps.f36 += n` |
| 15 | `sound` | name | plays one of the 38 `consounds` names, such as `squish1.voc`. Case sensitive |
| 16 | `fall` | | falls: `zvel` + 256 up to 2048, stops on the floor |
| 17 | `state` | index | calls a state. Outside an actor it starts one |
| 18 | `ends` | | ends the state |
| 19 | `define` | | compile time only |
| 20 | `//` | | line comment |
| 21 | `nuke` | | spawns tile 174 (stat 5) 5120 below the actor, then tile 1370 |
| 22 | `killit` | | deletes the actor after this run |
| 23 | `addweapon` | T | `addweapon(p, T, 0)` |
| 24 | `weapon` | n | does nothing |
| 25 | `addhealth` | n | adds n to health if alive; with `ps.b96` set and no `/d` it takes 4 from `b96` instead |
| 26 | `ifdead` | [s] | strength below 0 (player health below 1) |
| 27 | `ifsquished` | [s] | `ifsquished(i, p)` |
| 28 | `cactor` | T | changes the actor's tile |
| 29 | `{` | | starts a block |
| 30 | `}` | | ends a block |
| 31 | `spawn` | T | `spawn(i, T)` |
| 32 | `move` | move flags | `t[1]` = move, `t[0]` = 0, `hitag` = flags. Outside an actor it defines one |
| 33 | `ifwasweapon` | T [s] | the weapon from `ifhitweapon` is tile T |
| 34 | `ifaction` | action [s] | `t[4]` is that action |
| 35 | `ifactioncount` | n [s] | `t[2] >= n` |
| 36 | `resetactioncount` | | `t[2] = 0` |
| 37 | `debris` | n | n scrap sprites, tiles 235-245 |
| 38 | `ifskill` | n [s] | skill below n |
| 39 | `/*` | | block comment |
| 40 | `cstat` | n | sets `cstat` to n's low byte |
| 41 | `ifmove` | move [s] | `t[1]` is that move |
| 42 | `resetplayer` | | `resetplayer(p)` |
| 43 | `ifonwater` | [s] | sector lotag 1 and less than `yrepeat << 7` above its floor |
| 44 | `ifinwater` | [s] | sector lotag 2 |
| 45 | `ifplayersweapon` | T [s] | the player's weapon `ps.w7e` is T |
| 46 | `ifmovecount` | n [s] | `t[0] >= n` |
| 47 | `resetmovecount` | | `t[0] = 0` |
| 48 | `addinventory` | n | sets one of five item flags (0-4: `b99`, `b9a`, `b97`, `b9b` holoduke, `b98` jetpack) |
| 49 | `addammo` | n | `ps.w4c += n` |
| 50 | `hitradius` | r damage | damage sprites for every actor (stat 1, 6, 10) closer than r |
| 51 | `ifp` | flags [s] | true when any flag matches the player (below) |
| 52 | `addrammo` | n | `ps.w52 += n` |
| 53 | `sink` | | z + 4096, `xvel` halved |
| 54 | `music` | | compile time only |
| 55 | `include` | | compile time only |
| 56 | `ifstrength` | n [s] | strength at most n |
| 57 | `blood` | n | n blood sprites (tile 488) |
| 58 | `guts` | | five gib sprites (tiles 870, 874, 878) |
| 59 | `ifspawnedby` | T [s] | `owner == T`. The exe compares the owner's sprite index with a tile number |

A word with no case in `parse`, such as `end`'s 0x80000000, stops the game with the actor's tile number.

`ifp`'s flags, with the names `GAME.CON` defines: `pstanding` 1, `pwalking` 2, `prunning` 4, `pducking` 8, `pfalling` 16, `pjumping` 32, `phigher` 64 (the actor is more than 12288 below the player), `pwalkingback` 128, `prunningback` 256, `plookingleft` 512, `plookingright` 1024, `pfacingframes` 2048, `pshooting` 4096, `ponground` 8192, `pjetpack` 16384. Walking and running are read from `syncfvel + syncsvel` and the run bit (see [network.md](network.md)). 2048 is tested only when no earlier flag matched, and when it is in the mask, 4096-16384 are not tested.

## An example

From `GAME.CON`, the trooper's moves, some of its actions, and a state its actor calls:

```
move   TROOP1HIT        -32   -8
move   TROOP1STAYPUT

action RUNNING_FRAMES            0     6    5    1   15
action JP_FRAMES                 59    1    5

state troopshoot
    ifp phigher
    {
        ifrnd 128
        {
            action JP_FRAMES
            break
        }
        else action SHOOTING_UP_FRAMES
    }
    else action SHOOTING_MIDDLE_FRAMES

    move TROOP1STAYPUT face_to_player slow_to_stop
ends

actor TROOP1 MEDIUMSTRENGTH STANDING_FRAME TROOP1STAYPUT
    fall
    ifsquished
    {
        sound squish1.voc
        spawn OOZ
        blood 10
        killit
    }
    ...
enda
```

`RUNNING_FRAMES` starts at the actor's tile, has 6 frames drawn from 5 angles, steps 1 frame at a time, and moves on once `sprite.lotag` passes 15 ticks. `TROOP1STAYPUT` is a move of zero speed, which the flags then use to face the player and slow down.

## Known limits

How the compiler handles its limits:

- A script that reaches `MAXSCRIPTSIZE` − 8 ints stops compiling with "Script is larger than 16384 words." Each command checks this before it runs, because the `actor` header, `end` and a `state` call write a few words past `scriptptr` without `emit`'s bound. The exe had no such check and wrote past the array.
- A 513th label is an error ("More than 512 labels."). `labels[]` has one spare row, so the name being read always has somewhere to go; the exe read one past the end here.
- Line numbers in messages match the file. The exe counted the newline after a `//` comment twice; the port counts it once. The "Total Lines" figure adds each include's lines to the total.

The shipped scripts stay well inside these limits: the port reports 1843 lines (the exe's count, with doubled comment lines, is 1868) and 13336 bytes (3334 of 16384 ints) with no warnings.
