#ifndef GUSWAVE_H
#define GUSWAVE_H

const char *GUSWAVE_ErrorString(int code);
int GUSWAVE_Init(void);
void GUSWAVE_SetVolume(int volume);
void GUSWAVE_SetCallBack(void (*function)(unsigned long));

#endif
