#ifndef PCFX_H
#define PCFX_H

const char *PCFX_ErrorString(int code);
int PCFX_Init(void);
void PCFX_SetVolume(int volume);
void PCFX_SetCallBack(void (*function)(unsigned long));

#endif
