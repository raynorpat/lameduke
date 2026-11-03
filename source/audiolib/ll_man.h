#ifndef LL_MAN_H
#define LL_MAN_H

/* 0x6C75C / 0x6C748, called around FX_Init and MUSIC_Init.
   The DOS build locked the mixer in conventional memory. */

int LL_LockMemory(void);
void LL_UnlockMemory(void);

#endif
