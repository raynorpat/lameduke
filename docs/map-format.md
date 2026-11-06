# Build map format, versions 5 and 4

LameDuke's maps are Build boards. `D3D.EXE` (and the port) loads only version 5. Two shipped maps, `L9.MAP` and `M8.MAP`, are the older version 4; `tools/convmap4` converts them to version 5.

This beta's version 5 is **not** the layout later Build games use. Duke Nukem 3D 1.3D's version 7 has 40-byte sectors, 32-byte walls and 44-byte sprites with a different field order. LameDuke's v5 has 37-byte sectors, 35-byte walls and 38-byte sprites. Tools written for v7 maps will misread these files.

All values are little-endian. Records are packed with no padding (`#pragma pack(1)` in `source/build/board.h`, which also checks the three sizes at compile time).

## Version 5

### File layout

`loadboard` (0x49127, `source/build/board.c`) reads:

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | version, 5 |
| 4 | int32 | start x |
| 8 | int32 | start y |
| 12 | int32 | start z |
| 16 | int16 | start angle (0–2047) |
| 18 | int16 | start sector |
| 20 | int16 | number of sectors, 1–1024 |
| 22 | 37 × n | sectors |
| | int16 | number of walls, 0–8192 |
| | 35 × n | walls |
| | int16 | number of sprites, 0–4096 |
| | 38 × n | sprites |
| | int16 | number of extra records, 0–4096 (optional) |
| | 10 × n | extra records |

Each count sits just before its records. Version 4 puts all three counts in the header (see below).

The extra records end the file. Nothing in the game reads them; the engine keeps them only to write them back. Their meaning is unknown. Several shipped maps (L7, M1, M4, N2–N5, O1) end right after the sprites with no extras count. The exe's `read()` then returns 0 and the board still loads, so the port does the same. Bytes after the extras are ignored.

The limits are `MAXSECTORS` 1024, `MAXWALLS` 8192, `MAXSPRITES` 4096 and `MAXSTATUS` 1024. A count outside them, a short file or a version other than 5 fails the load. The game reports any failed load as "Board not found."

### Sector, 37 bytes

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | int16 | wallptr | first wall of the sector |
| 2 | int16 | wallnum | number of walls |
| 4 | int16 | ceilingpicnum | |
| 6 | int16 | floorpicnum | |
| 8 | int16 | ceilingheinum | heightmap tile when `ceilingstat & 2` (not a slope in this beta) |
| 10 | int16 | floorheinum | heightmap tile when `floorstat & 2` |
| 12 | int32 | ceilingz | z grows downward |
| 16 | int32 | floorz | |
| 20 | int8 | ceilingshade | |
| 21 | int8 | floorshade | |
| 22 | uint8 | ceilingxpanning | |
| 23 | uint8 | floorxpanning | |
| 24 | uint8 | ceilingypanning | |
| 25 | uint8 | floorypanning | |
| 26 | uint8 | ceilingstat | flags below |
| 27 | uint8 | floorstat | |
| 28 | uint8 | ceilingpal | |
| 29 | uint8 | floorpal | |
| 30 | uint8 | visibility | |
| 31 | int16 | lotag | game meaning (doors, lifts, water…) |
| 33 | int16 | hitag | |
| 35 | int16 | extra | |

Fields come in ceiling/floor pairs, not as one ceiling group and one floor group the way v7 lays them out.

**Ceiling and floor stat bits**, as `draw.c` uses them:

| Bit | Meaning |
|---|---|
| 1 | parallax sky |
| 2 | heightmap surface; the heinum field names the height tile (Classic `heightfloor` 0x4FFEE) |
| 4 | swap x and y texture axes |
| 8 | double texture scale ("smoosh") |
| 0x10 | flip x |
| 0x20 | flip y |
| 0x40 | align the texture to the sector's first wall |

### Wall, 35 bytes

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | int32 | x | the wall's start point |
| 4 | int32 | y | |
| 8 | int16 | point2 | the next wall in the loop (its start is this wall's end) |
| 10 | int16 | picnum | |
| 12 | int16 | overpicnum | masked-wall tile |
| 14 | int8 | shade | |
| 15 | int16 | cstat | flags below; a byte in version 4 |
| 17 | uint8 | xrepeat | |
| 18 | uint8 | yrepeat | |
| 19 | uint8 | xpanning | |
| 20 | uint8 | ypanning | |
| 21 | int16 | nextsector | −1 for a solid wall |
| 23 | int16 | nextwall | the matching wall in nextsector, or −1 |
| 25 | int16 | nextsector2 | carried through; nothing in the exe reads it |
| 27 | int16 | nextwall2 | likewise |
| 29 | int16 | lotag | |
| 31 | int16 | hitag | |
| 33 | int16 | extra | |

A sector's walls are `wallptr` to `wallptr + wallnum − 1`, and they form one or more closed loops through `point2`. A two-sided wall's `nextwall` points back at it and starts where this wall ends. `checkboard` in `board.c` (run by `mapcheck validate`) checks these rules. Several shipped maps break a few of them, and the game loads them anyway.

**Wall cstat bits** used by the Classic renderer and collision:

| Bit | Meaning |
|---|---|
| 1 | blocks movement (`clipmove`, `getzrange`) |
| 2 | draw the bottom part with the next sector's wall |
| 4 | align the texture to the floor instead of the ceiling |
| 8 | flip x |
| 0x10 | masked wall: draw `overpicnum` in the opening |
| 0x20 | one-way: draw as solid from this side |
| 0x40 | blocks hitscan |
| 0x80 | masked wall is translucent |
| 0x100 | flip y |

Polymost also reads 0x200 (reverse translucency), a later Build bit. The Classic renderer ignores it.

### Sprite, 38 bytes

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | int32 | x | |
| 4 | int32 | y | |
| 8 | int32 | z | |
| 12 | uint8 | cstat | flags below; a byte, not a short as in v7 |
| 13 | int8 | shade | |
| 14 | uint8 | xrepeat | |
| 15 | uint8 | yrepeat | |
| 16 | int16 | picnum | |
| 18 | int16 | ang | |
| 20 | int16 | xvel | |
| 22 | int16 | yvel | the game keeps the player number here for player sprites |
| 24 | int16 | zvel | |
| 26 | int16 | owner | |
| 28 | int16 | sectnum | |
| 30 | int16 | statnum | status list; 1024 (`MAXSTATUS`) means free |
| 32 | int16 | lotag | |
| 34 | int16 | hitag | |
| 36 | int16 | extra | |

There are no x/y offsets, clipdist or pal fields. Those came later.

**Sprite cstat bits:**

| Bit | Meaning |
|---|---|
| 1 | blocks movement and hitscan |
| 2 | translucent |
| 4 | flip x |
| 8 | flip y |
| 0x30 | type: 0 faces the camera, 0x10 wall sprite, 0x20 floor sprite |
| 0x40 | one-sided |

The editor also tests 0x80 (centre on the tile's middle). The game's Classic renderer does not use it.

### Loading and saving

On load, each sprite goes into the sector and status lists in file order (`insertsprite(sectnum, statnum)`). On an empty board sprite `i` keeps slot `i`. There are two writers:

- `saveboard` (0x49337) is the game's, used by save games. It writes sprites in status-list order, so they are renumbered, and it always writes the extras count.
- `writeboard` is the editor's, also used by `convmap4` and `mapcheck`. It writes sprites in index order and writes the extras count only if the loaded map had one, so an unedited map saves byte for byte. It writes `NAME.tmp` and then renames it over `NAME`, so a failed save leaves the old map whole.

See [build-engine.md](build-engine.md) for the sprite lists and [map-editor.md](map-editor.md) for editing.

## Version 4

`L9.MAP` and `M8.MAP` are version 4. `D3D.EXE` loads only version 5, so it rejects them as a missing board. `tools/convmap4` converts them. Nothing found in print documents version 4: the [MAP format wiki page](https://moddingwiki.shikadi.net/wiki/MAP_Format_(Build)) has an empty heading for it, and Ken Silverman's [kenbuild.zip](https://advsys.net/ken/buildsrc/kenbuild.zip) only converts from 5 upward. This layout was recovered from the two files and from the changelog in that archive's `BUILD.TXT`. Its 9/16/94 entry lists what CONVMAP5 changed:

> Added numextras · Added sector[].ceilingpal · Added sector[].floorpal · Added sector[].visibility · Split sector[].tag to sector[].lotag and sector[].hitag · Added sector[].extra · Expanded wall[].cstat to a short · Split wall[].tag to wall[].lotag and wall[].hitag · Added wall[].extra · Split sprite[].tag to sprite[].lotag and sprite[].hitag · Got rid of sprite[].extra (the void * mess) · Added sprite[].extra

### Header, 26 bytes

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | version, 4 |
| 4 | int32 | start x |
| 8 | int32 | start y |
| 12 | int32 | start z |
| 16 | int16 | start angle |
| 18 | int16 | start sector |
| 20 | uint16 | number of sectors |
| 22 | uint16 | number of walls |
| 24 | uint16 | number of sprites |

All three counts come first, unlike version 5, which puts each count just before its records. The sectors, walls and sprites follow, and the file ends after the last sprite. There is no extras count.

How it was found: the v5 header and count positions fit neither file with any record size. Searching for the stride at which each sector's `wallptr` equals the previous `wallptr + wallnum` gave 32-byte sectors starting at offset 26. The three shorts at 20, 22 and 24 are then 146, 791 and 53 for L9, and 61, 349 and 4 for M8. With 32-byte walls and 40-byte sprites, both files end exactly on the last record.

### Sector, 32 bytes

| Offset | Type | Field |
|---|---|---|
| 0 | int16 | wallptr |
| 2 | int16 | wallnum |
| 4 | uint8 | ceilingstat |
| 5 | uint8 | ceilingxpanning |
| 6 | uint8 | ceilingypanning |
| 7 | int8 | ceilingshade |
| 8 | int32 | ceilingz |
| 12 | int16 | ceilingpicnum |
| 14 | int16 | ceilingheinum |
| 16 | uint8 | floorstat |
| 17 | uint8 | floorxpanning |
| 18 | uint8 | floorypanning |
| 19 | int8 | floorshade |
| 20 | int32 | floorz |
| 24 | int16 | floorpicnum |
| 26 | int16 | floorheinum |
| 28 | int32 | tag |

The ceiling and the floor are each a 12-byte group. In both files the stat bytes hold 0, 1, 8 and 0x38; byte 3 of each group is a signed shade (18, 12, −8, −5); the z values are 1024-unit heights such as −8192 and 8192; and the picnums stay under 400. Neither file sets stat bit 2, and every heinum is 0, so neither has a heightmap sector. The two panning bytes are zero in both files, so their order (x then y, as in v5) is assumed, not shown.

### Wall, 32 bytes

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | x |
| 4 | int32 | y |
| 8 | int16 | point2 |
| 10 | uint8 | cstat |
| 11 | int8 | shade |
| 12 | uint8 | xrepeat |
| 13 | uint8 | yrepeat |
| 14 | uint8 | xpanning |
| 15 | uint8 | ypanning |
| 16 | int16 | picnum |
| 18 | int16 | overpicnum |
| 20 | int16 | nextsector |
| 22 | int16 | nextwall |
| 24 | int16 | nextsector2 |
| 26 | int16 | nextwall2 |
| 28 | int32 | tag |

With these offsets, every wall loop closes through `point2` and every `nextwall` points back to a wall that starts where this one ends: zero defects in either file. Byte 10 holds cstat bits (1, 2, 4, 16, 32), and the walls with a masked picture carry 4 or 17. Byte 11 is a signed shade. The yrepeats are mostly 8 and 16. Picnums stay under 403, and overpicnum is −1 or a tile. `nextsector2` and `nextwall2` already exist here as `(−1,−1)` or `(0,0)`, as in the v5 maps.

### Sprite, 40 bytes

| Offset | Type | Field |
|---|---|---|
| 0 | int32 | x |
| 4 | int32 | y |
| 8 | int32 | z |
| 12 | uint8 | cstat |
| 13 | int8 | shade |
| 14 | uint8 | xrepeat |
| 15 | uint8 | yrepeat |
| 16 | int16 | picnum |
| 18 | int16 | ang |
| 20 | int16 | xvel |
| 22 | int16 | yvel |
| 24 | int16 | zvel |
| 26 | int16 | owner |
| 28 | int16 | sectnum |
| 30 | int16 | statnum |
| 32 | int32 | tag |
| 36 | uint32 | extra (the `void *` the changelog removed) |

This is v5's order, ending in the 4-byte tag and the pointer. Every sector number is in range and every pointer is 0. M8's sprite 0 is tile 283 (APLAYER) at the start position.

### Converting to version 5

```sh
./build/bin/convmap4 L9.MAP L9V5.MAP
```

- The tag splits into `lotag` (low 16 bits) and `hitag` (high 16 bits). All tags in both files fit in the low half.
- Wall `cstat` widens to a short.
- The fields v5 added take the values the shipped v5 maps use most, which are also the defaults the changelog gives for `extra`: `ceilingpal` and `floorpal` 0, `visibility` 0, and `extra` −1 on sectors, walls and sprites.
- The sprite pointer is dropped.
- The output has no extras count, like the shipped v5 maps L7, M1, M4, N2–N5 and O1.

`convmap4` refuses to write over its input. See [map-editor.md](map-editor.md) for playing a converted map.

## Checking a map

```sh
./build/bin/mapcheck validate L1.MAP          # wall loops, links, sprite sectors, start
./build/bin/mapcheck roundtrip L1.MAP OUT.MAP # load and save with writeboard
./build/bin/mapcheck same L1.MAP OUT.MAP      # exit 0 when the files are identical
```
