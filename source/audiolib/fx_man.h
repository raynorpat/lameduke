#ifndef FX_MAN_H
#define FX_MAN_H

enum {
    FX_Warning = -2,
    FX_Error = -1,
    FX_Ok = 0,
    FX_SoundCardError = 1,
    FX_InvalidCard = 2,
    FX_MultiVocError = 3,
    FX_InvalidVOCFile = 4,
    FX_DPMIError = 5
};

typedef struct {
    int MaxVoices;
    int MaxSampleBits;
    int MaxChannels;
} fx_device;

extern int FX_ErrorCode;

const char *FX_ErrorString(int code);
int FX_SetupCard(int SoundCard, fx_device *device);
int FX_Init(int SoundCard, int numvoices, int numchannels, int samplebits, unsigned mixrate);
int FX_SetCallBack(void (*function)(unsigned long));
int FX_SetVolume(int volume);
int FX_PlayVOC3D(const unsigned char *ptr, int pitchoffset, int angle, int distance, int priority,
    unsigned long callbackval);
int FX_Pan3D(int handle, int angle, int distance);
int FX_StopSound(int handle);

#endif
