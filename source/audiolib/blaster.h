#ifndef BLASTER_H
#define BLASTER_H

const char *BLASTER_ErrorString(int code);
int BLASTER_Init(void);
int BLASTER_CardHasMixer(void);
void BLASTER_SetVolume(int volume);
void BLASTER_SetFMVolume(int volume);
int BLASTER_GetFMVolume(void);
void BLASTER_SetCallBack(void (*function)(unsigned long));

#endif
