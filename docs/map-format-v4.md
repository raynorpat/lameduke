# Build map version 4

`L9.MAP` and `M8.MAP` are version 4. `D3D.EXE` loads only version 5, so it rejects them as a missing board. `tools/convmap4` converts them. Nothing found in print documents version 4: the [MAP format wiki page](https://moddingwiki.shikadi.net/wiki/MAP_Format_(Build)) has an empty heading for it, and Ken Silverman's [kenbuild.zip](https://advsys.net/ken/buildsrc/kenbuild.zip) only converts from 5 upward. This layout was recovered from the two files and from the changelog in that archive's `BUILD.TXT`. Its 9/16/94 entry lists what CONVMAP5 changed:

> Added numextras · Added sector[].ceilingpal · Added sector[].floorpal · Added sector[].visibility · Split sector[].tag to sector[].lotag and sector[].hitag · Added sector[].extra · Expanded wall[].cstat to a short · Split wall[].tag to wall[].lotag and wall[].hitag · Added wall[].extra · Split sprite[].tag to sprite[].lotag and sprite[].hitag · Got rid of sprite[].extra (the void * mess) · Added sprite[].extra

All values are little-endian.

## Header, 26 bytes

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

## Sector, 32 bytes

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

## Wall, 32 bytes

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

## Sprite, 40 bytes

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

## Converting to version 5

- The tag splits into `lotag` (low 16 bits) and `hitag` (high 16 bits). All tags in both files fit in the low half.
- Wall `cstat` widens to a short.
- The fields v5 added take the values the shipped v5 maps use most, which are also the defaults the changelog gives for `extra`: `ceilingpal` and `floorpal` 0, `visibility` 0, and `extra` −1 on sectors, walls and sprites.
- The sprite pointer is dropped.
- The output has no extras count, like the shipped v5 maps L7, M1, M4, N2–N5 and O1.
