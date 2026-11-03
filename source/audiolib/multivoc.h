#ifndef MULTIVOC_H
#define MULTIVOC_H

const char *MV_ErrorString(int code);
int MV_Init(int card, unsigned rate, int voices, int channels, int bits);
void MV_SetVolume(int volume);
void MV_SetCallBack(void (*function)(unsigned long));

extern int MV_ErrorCode;

#endif
