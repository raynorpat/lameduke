#ifndef DSL_H
#define DSL_H

/* SDL stand-in for the DMA drivers (blaster, pas16, guswave, sndsrc). */

int DSL_Init(unsigned rate, int channels, int bits);
void DSL_Shutdown(void);
/* Stand in for the cli/sti around voice list changes. */
void DSL_Lock(void);
void DSL_Unlock(void);
/* The card's voice volume, 0-31 at 2 dB a step (SB16 mixer registers 0x32/0x33). */
void DSL_SetVoiceLevel(int level);

#endif
