#ifndef OPLEMU_H
#define OPLEMU_H

/* A YMF262 (OPL3) as AL_MIDI drives it: 18 two-operator channels in two
   register banks, waveforms 0-3, no rhythm mode, no 4-op pairs. Samples
   come at the chip's own rate. */

#define OPL_RATE 49716

void OPL_Reset(void);
/* reg 0x000-0x0FF is the first bank, 0x100-0x1FF the second. */
void OPL_Write(int reg, int val);
void OPL_Generate(int *left, int *right);

#endif
