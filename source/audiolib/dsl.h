#ifndef DSL_H
#define DSL_H

/* SDL stand-in for the DMA drivers (blaster, pas16, guswave, sndsrc). */

int DSL_Init(unsigned rate, int channels, int bits);
void DSL_Shutdown(void);

#endif
