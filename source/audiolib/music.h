#ifndef MUSIC_H
#define MUSIC_H

enum {
    MUSIC_Warning = -2,
    MUSIC_Error = -1,
    MUSIC_Ok = 0,
    MUSIC_SoundCardError = 1,
    MUSIC_InvalidCard = 2,
    MUSIC_MidiError = 3,
    MUSIC_TaskManError = 4,
    MUSIC_FMNotDetected = 5,
    MUSIC_DPMIError = 6
};

extern int MUSIC_ErrorCode;

const char *MUSIC_ErrorString(int code);
int MUSIC_Init(int SoundCard, int Address);
void MUSIC_SetVolume(int volume);

#endif
