#ifndef BLASTER_H
#define BLASTER_H

const char *BLASTER_ErrorString(int code);
int BLASTER_Init(void);
void BLASTER_SetVolume(int volume);
void BLASTER_SetCallBack(void (*function)(unsigned long));

#endif
