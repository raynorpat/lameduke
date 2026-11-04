#ifndef MULTIVOC_H
#define MULTIVOC_H

const char *MV_ErrorString(int code);
int MV_Init(int card, unsigned rate, int numvoices, int channels, int bits);
void MV_SetVolume(int volume);
void MV_SetCallBack(void (*function)(unsigned long));
int MV_PlayVOC3D(const unsigned char *ptr, int pitchoffset, int angle, int distance, int priority,
    unsigned long callbackval);
int MV_Pan3D(int handle, int angle, int distance);
int MV_Kill(int handle);
/* The interrupt routine: mixes and returns the next 256 stereo frames. */
const unsigned short *MV_ServiceVoc(void);

extern int MV_ErrorCode;

#endif
