# Audio

`D3D.EXE` links Jim Dose's Apogee Sound System (AudioLib). The port keeps its structure in `source/audiolib/`: the same `FX_`, `MUSIC_`, `MV_`, `MIDI_` and `AL_` entry points, and the same switches on the card number. Underneath, one SDL device stands in for a Sound Blaster 16. Digital voices go through its DAC and MIDI music through an emulated OPL3.

## The library

| File | What it is |
|---|---|
| `fx_man.c` | Real. `FX_SetupCard`, `FX_Init`, `FX_SetVolume`, `FX_SetCallBack`, `FX_PlayVOC3D` (0x6E660), `FX_Pan3D` (0x6E788), `FX_StopSound` (0x6E898), `FX_StopAllSounds` (0x6E978). Playing, panning and stopping only reach multivoc cards (Sound Blaster, PAS, SoundMan 16, AWE32, Sound Source, Tandy). Other cards get `FX_InvalidCard`. |
| `multivoc.c` | Real. The voice lists, VOC block walker (0x6A461), 3D pan table (0x6AA13), volume table (0x6A906) and the 16-bit stereo mixer (0x6967E). Only the 16-bit stereo mode is lifted, the one `main` asks for. |
| `mv_mix.c` | Real, port-only. Feeds `dsl.c` frames, 256 at a time from `MV_ServiceVoc` (0x6983D), turning the unsigned buffers into signed samples. |
| `pitch.c` | Real. `PITCH_GetScale` (0x66C70) with the 2^(k/300) table. The game always passes pitch offset 0. |
| `music.c` | Real. `MUSIC_Init`, `MUSIC_PlaySong` (0x68E94), `MUSIC_SetVolume` (0x68DE4), `MUSIC_Pause` (0x68E2C), `MUSIC_Continue` (0x68E24). |
| `midi.c` | Real. The MIDI file player of 0x6B330-0x6C6C0. Format 0 and 1 files. |
| `al_midi.c` | Real. The AdLib/OPL MIDI driver (0x70AB0-0x71C74): 9 voices, the timbre bank, percussion map, pitch bend. A Sound Blaster is taken to be an SB16 and driven as two stereo OPL3 banks; an AdLib (card 3) is one mono OPL2. |
| `oplemu.c` | Real, port-only. A YMF262 emulator covering what AL_MIDI uses: 18 two-operator channels, waveforms 0-3, no rhythm mode, no 4-op. Runs at 49716 Hz (`OPL_RATE`). |
| `task_man.c` | Real but small. One task and its rate. `TS_Advance` runs it from the audio callback, not from a timer interrupt. |
| `dsl.c` | Real, port-only. Opens the device and mixes the DAC and OPL streams (below). |
| `blaster.c` | Mostly stub. Error strings, plus SB16 mixer volume (0x6FBB4, 0x6FD04, 0x6FC60) mapped onto `dsl.c` gains. `BLASTER_CardHasMixer` (0x6FDB0) always says yes. |
| `ll_man.c`, `user.c` | Stubs. Memory locking does nothing; `USRHOOKS_` is `malloc`/`free`. |
| `guswave.c` | Stub. `GUSWAVE_Init` opens the device, but nothing plays through GUSWAVE. |
| `gus.c`, `gusmidi.c`, `pas16.c`, `adlibfx.c`, `pcfx.c`, `sndsrc.c` | Stubs. Error strings and an `_Init` that returns 0. Volume and callback setters do nothing or just store the pointer. |
| `awe32.c` | Stub. `AWE32_Init` returns 0. |
| `mpu401.c` | Stub. `MPU_Init` calls `MIDI_Init`, which returns 0. No MIDI driver table is set, so songs do not play. |

`dsl.h` and `sndcards.h` have no exe counterpart beyond the card numbers.

## Picking the card

`setup.dat` is read by `source/build/engine.c`. `setup_byte(1)` is the digitized sound menu choice and `setup_byte(2)` is the music on/off byte. `game.c` maps the menu choice through the table at 0x828F8, `{-1,0,1,3,4,7,9,10,12}`. Choice 0 means no digital sound. The result is used for both FX and music:

1. If `setup_byte(1)` is set: `FX_SetupCard`, `loadsounds`, `FX_Init(card, 4, 2, 16, 11000)`, `FX_SetCallBack(sound_callback)`, `FX_SetVolume(sound_volume)`. Four voices, stereo, 16-bit, 11000 Hz.
2. If `setup_byte(2)` is set: `MUSIC_Init(card, 0)`, then `MUSIC_SetVolume(music_volume)`.

Any error goes to `errorexit`. Some menu choices fail this way: card 12 is past the end of the card list, and cards 4 and 7 (General MIDI, WaveBlaster) pass `FX_SetupCard` but `FX_Init` rejects them. `sndcards.h` notes that choice 6 (Gravis in `SETUP.EXE`) stores 9, which this switch reads as Sound Source; that is left as recovered. Music only plays on cards 0, 1, 2 and 3, the OPL cards. Whether the exe has a separate music card byte has not been checked; the port uses the one card.

## SDL output

`source/build/sdl_audio.c` opens one device with `SDL_OpenAudioDevice`: 44100 Hz, `AUDIO_S16SYS`, stereo, 512-sample buffers. It is opened once, on the first `DSL_Init` or `DSL_InitMusic`, and SDL pulls samples through its callback. There is no queue.

The callback is `dsl_fill` in `dsl.c`. For each output frame it:

- steps the DAC stream at the mix rate (11000 Hz) and pulls a frame from `MV_Mix` when due, with linear interpolation between the last two;
- with music on, calls `TS_Advance(44100)`, which runs `MIDI_Service` as often as the task rate fits, then averages the OPL samples owed at 49716 Hz;
- scales each by its mixer gain and clamps to 16 bits.

The gains come from the SB16 mixer levels: `BLASTER_SetVolume` and `BLASTER_SetFMVolume` keep the top five bits, and `dsl.c` turns level L into 10^(-(31-L)/10). Game code locks the device with `DSL_Lock`/`DSL_Unlock` around calls that touch voices or the song.

## Sound effects

`source/sounds.c` has a fixed table of 39 file names (off_82838). `loadsounds` (0x3BBD0) runs after `FX_SetupCard` and before `FX_Init`. Each file is read whole into the cache (`allocache`) with 16 zeroed bytes of `SLACK` after it, because the parsers read a few bytes past a block end as the exe does. `vocvalid` checks the "Creative Voice File" header and that every block multivoc's `nextblock` walks lies inside the file. A file that is missing, short or bad leaves its slot empty. The exe plays whatever is at address 0 for such a slot; the port skips it.

CON scripts name sounds with `sound <file>`. `gamedef.c` looks the name up in `consounds` (0x82838, 38 entries, the same list without `rpg.voc`) and emits the index. There is no `definesound` in this version. `rpg.voc` (38) is only played from code (`player.c`).

| Call | What it does |
|---|---|
| `soundxyz` (0x3BCB0, 0x3BE70) | Skipped if sound is off or three copies already play. Measures from the listener: the demo camera, or the viewed player's position and `ang + lookang`. Distance is \|dx\|+\|dy\|, times 32 for sound 17. Not started if distance >> 6 is over 255. Calls `FX_PlayVOC3D(ptr, 0, angle >> 4, distance >> 6, priority, num)`; priority is 255 for sound 32, else 0. z is ignored. |
| `spritesound` | `soundxyz` at a sprite. A call with owner -1 (walls, sectors) records player 0's sprite as owner. |
| `pansounds` (0x3C090) | Once a drawn frame, re-pans every playing sound to its owner sprite with `FX_Pan3D`. |
| `stopsound` (0x3C040) | Stops the last handle started for that sound and lowers its count. |
| `sound_callback` (0x3C190) | multivoc calls it when a voice ends or is killed; lowers the count. |
| `FX_StopAllSounds` | Called when a level is entered (`premap.c`) and when S turns sound off (`game.c`). |

The angle passed is `angle >> 4`, 0-127, and multivoc uses `angle & 31`, so the pan wraps four times around the circle. The exe does the same (`sar ebx, 4` at 0x3BDD5).

`player.c` lowers the FX volume by 32 under water. The menu sliders set `sound_volume` and `music_volume`, saved in `userdefs.tmp`.

## Music

The CON command `music N file ...` fills a row of `char musicfn[5][10][13]`. `music 1` to `music 4` fill rows 1-4 and `music 0` (or anything else) fills row 0. Up to ten names of 12 characters each; a short list copies its last read slot into the rest, as the exe does. The shipped `MUSIC.CON` gives row 0 `FASTWAY.MID BROWNEYE.MID BROWNEYE.MID` and fills rows 1-4 almost entirely with `fastway.mid`.

`enterlevel` in `premap.c` sets `ps.musicptr = 130*(volume+1) + 13*level`, the offset of row volume+1, slot level, and calls `playmusic`. `movemusic` (0x3A860) in `actors.c` drives music sprites (stat 11): below lotag 1000 a sprite starts the level's song or row 0 slot lotag, fades out with `MUSIC_SetVolume` when the player comes within hitag, and restores the volume when they leave. At lotag 1000 and up it loops sound lotag-1000 while the player is near.

`playmusic` (0x3BC40) does nothing unless `setup_byte(2)` is set. It reads the whole file into a buffer with `SLACK`, checks it with `midivalid` (the `MThd` header and every `MTrk` chunk lie inside the file), and calls `MUSIC_PlaySong(song, 1)` to loop it. A file that fails leaves the old song playing. M pauses and resumes the song (`musicon`).

`MIDI_PlaySong` (0x6BD48) reads the division and schedules `MIDI_Service` with TaskMan at `120 * division / 60` ticks a second, 120 BPM. A tempo meta event (0x51) changes the rate to `60000000 / tempo * division / 60`. `MIDI_Service` (0x6B6A0) runs once a MIDI tick: it walks every track, sends note, controller, program and bend events to the driver table (AL_MIDI here), handles controller 7 as channel volume itself, and rewinds all tracks when the last one ends. A meta event's length is read as one byte, and a sysex as a status with no data, as in the exe.

Starting a song sets `taskman = 1`. That switches the game clock to the 18.2 Hz chain TaskMan leaves behind. See [game-speed.md](game-speed.md).

## Files shipped

The repo root has 34 VOC files and 6 MIDI files: `BROWNEYE.MID`, `E2M1.MID`, `FASTWAY.MID`, `FASTWAY1.MID`, `FASTWAY2.mid`, `MISTACHE.MID`. Only `FASTWAY.MID` and `BROWNEYE.MID` are named in `MUSIC.CON`; file names are opened case-insensitively by `plat_fopen`.

Six names in the sound table have no file: `xoptor.voc` (4), `waterfal.voc` (11), `bodybloa.voc` (14), `bodyblor.voc` (15), `switch.voc` (25) and `subway.voc` (32). Those sounds are silent. Sound 25 is the one `sector.c` plays when a wall switch (tile 395 or 396) is used.

## What is incomplete

The README says sound effects and music are not yet complete. The audio code has no `pending` comments. What is there now:

- Sound effects play on the multivoc cards, which covers the shipped `SETUP.DAT`. GUS, AdLib FX and PC speaker effects are stubs.
- Music plays only through AL_MIDI and the OPL emulator. General MIDI, Sound Canvas, WaveBlaster, AWE32 and GUS music are stubs, and Sound Source or Tandy as the card ends in `errorexit` when music is on.
- `oplemu.c` is not a full OPL3: no rhythm mode or 4-operator voices. AL_MIDI does not use them.
- The patch pre-scan (0x6C3AC) is not lifted; AL_MIDI does not load patches.
- How close the mix sounds to a real SB16 has not been measured.
