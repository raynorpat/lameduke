#ifndef DSL_H
#define DSL_H

/* SDL stand-in for the DMA drivers (blaster, pas16, guswave, sndsrc). */

/* Start the DAC output of multivoc's buffers at its mix rate. */
int DSL_Init(unsigned rate, int channels, int bits);
void DSL_Shutdown(void);
/* Stand in for the cli/sti around voice list changes. */
void DSL_Lock(void);
void DSL_Unlock(void);
/* The card's voice volume, 0-31 at 2 dB a step (SB16 mixer registers 0x32/0x33). */
void DSL_SetVoiceLevel(int level);
/* The card's FM volume, the same scale (SB16 mixer registers 0x34/0x35). */
void DSL_SetFMLevel(int level);
/* Start the OPL3 output next to the DAC. */
int DSL_InitMusic(void);

#endif
