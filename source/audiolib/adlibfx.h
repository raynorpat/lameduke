#ifndef ADLIBFX_H
#define ADLIBFX_H

const char *ADLIBFX_ErrorString(int code);
int ADLIBFX_Init(void);
void ADLIBFX_SetVolume(int volume);
void ADLIBFX_SetCallBack(void (*function)(unsigned long));

#endif
