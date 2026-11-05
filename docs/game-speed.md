# Game speed

`D3D.EXE` plays at different speeds on different machines. It feels right on a 486 and too fast on a Pentium or in DOSBox. The port plays at the 486's pace whatever the frame rate.

## Why the original depends on the CPU

The single-player loop (0x16F4A) runs once per frame drawn:

1. getinput (0x30E60) sets `synctics = totalclock - lockclock`.
2. The inlined domovethings advances `lockclock` by `synctics`, then moves everything once.
3. The frame is drawn.

Only some of the game scales with `synctics`: the player's movement and turning (`processinput`), the sector animations (`doanimations`) and a few actor moves. The rest moves once per call, however many ticks went by: actors and their CON scripts, weapon and hit counters, effectors and spawn timers. Drawing has its own per-frame steps: animated walls and the plasma tile (0x25550), the viewscreen every twelfth frame (0x25410), sprite frame counters in animatesprites, the quote timers and the HUD pulse. So a faster CPU draws more frames, and everything moved per call or per frame speeds up. The loop does not skip a call when `synctics` is 0. On a fast machine most frames have 0 ticks, and actors still move on every one.

## The clock

main (0x15BE7) sets the PIT to divisor 0x26D6 (120 Hz) and installs sub_102A0 on IRQ 0. sub_102A0 adds 1 to `totalclock` (0x23CE28), and 5 more when music is on (byte_23DE42). The input ramps step the same number of times. When the first song starts, the MIDI player calls TaskMan's startup (0x65E2C). TaskMan saves sub_102A0 as the old IRQ 0 handler, sets the PIT to the music task's rate and chains to the saved handler only when its count passes 0x10000 (0x65CCC). From then on sub_102A0 runs at 1193182 / 65536 = 18.2 Hz, so with music `totalclock` advances 6 × 18.2 = 109.2 ticks a second, not 720. Without music it stays at 120 a second.

## Measurement

A scratch copy of `D3D.EXE` had its getinput call at 0x16F5A pointed at a code cave in the dead sub_5B480. The cave calls getinput, then writes `totalclock` and `synctics` to `SPEED.BIN`, one record per loop. It ran in DOSBox 0.74-3 (SB16, the shipped `SETUP.DAT` with sound and music on) with `d3d /v1 /l1`, standing at the L1 start. The first tenth of each run is left out.

| cycles | loops | clock ticks | loops/s at 109.2 Hz | synctics per loop |
|---|---|---|---|---|
| 26800 (486 DX2-66), 50 s | 591 | 4284 | 15.07 | 6 ×469, 12 ×123 (mean 7.25) |
| 26800 (486 DX2-66), 80 s | 1011 | 7260 | 15.21 | 6 ×812, 12 ×200 (mean 7.19) |
| 60000 (Pentium class) | 1314 | 4422 | 32.5 | 0 ×578, 6 ×737 |
| max | 5134 | 4398 | 127.5 | 0 ×4402, 6 ×733 |

The two 486 runs differ by 30 s of wall time and by 3312 clock ticks, which is 110 a second. That confirms the 18.2 Hz chain (at 120 Hz × 6 the difference would be 21600). On the 486 every loop sees one or two chained calls. On a Pentium-class CPU 44% of loops see none, and the game runs about twice as fast.

## What the port does

- `play_level` runs the loop at `GAME_LOGIC_HZ` = 15 a second, the 486's rate. `synctics` is the ticks the clock gives in 1/15 s: 6 or 12 with music (mean 7.28; on the 486 it was 7.19 to 7.25), and 8 without. Frames are drawn as often as they come.
- A frame drawn between two loops leaves the drawing's per-frame steps alone (`viewtic` in `view.c`), so walls, quotes, the HUD and the menu's title spin and slider keys keep the 486's pace too.
- The clock follows the exe: 120 Hz until the first song, then 18.2 Hz chained calls when music is on. The episode map screen (0x19650) uses the same clock.
- Below 15 frames a second the game slows down, as it did on the 486.
- With `LAMEDUKE_FRAME` set (a frame dump), one loop runs per frame, so frame N always shows the same picture.
- Frames between two loops are drawn in between, as in the 1996 game (`smoothratio`). Before each loop `updateinterpolations` keeps every wall's x/y, sector's ceiling and floor z, sprite's x/y/z and player's view. A frame drawn a share of the way to the next loop blends each from that copy to where the loop left it, draws, and puts the loop's values back. So the view lags one loop behind and never passes where the loop left it, and the game state is the same with or without the blend. Anything that moved 8192 or more in one loop (a teleport, a monitor) is drawn where it is. The menu, a pause, a frame dump and `LAMEDUKE_NOSMOOTH=1` draw the loop's own positions. Attract demos blend between samples the same way. The first frame after a loop is drawn near the last loop's camera, so the sprites `animatesprites` ticks (`hittype[i][0]` of tile 1165) can differ with the blend on and off; no game logic reads that counter.

## Attract demos

The exe's demo loop (0x15E53) runs samples while `totalclock` is ahead of `lockclock`. So demos play at the playing machine's clock, whatever clock recorded them. The same DOSBox hook on the demo loop's domovethings call (0x15EB0) saw 1391 samples of demo 0 (5082 tics) go by in about 6180 clock ticks. That is close to real time at 109 ticks a second. The port paces demos with the same clock: 109.2 ticks a second with music (demo 0 takes about 69 s, where it used to take 10.5 s at 720), and 120 without. Only the wall pacing changed. Every demo shows the same frames and samples as before.

## By eye

Doors, enemies and the fire rate on L1 still have to be compared by eye against DOSBox at 26800 cycles.
