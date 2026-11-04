#ifndef MIDI_H
#define MIDI_H

/* The driver table MUSIC_Init fills (unk_241B98), in the exe's order. */
typedef struct {
    void (*NoteOff)(int channel, int key, int velocity);
    void (*NoteOn)(int channel, int key, int velocity);
    void (*PolyAftertouch)(int channel, int key, int pressure);
    void (*ControlChange)(int channel, int type, int data);
    void (*ProgramChange)(int channel, int patch);
    void (*ChannelAftertouch)(int channel, int pressure);
    void (*PitchBend)(int channel, int lsb, int msb);
    void (*ReleasePatches)(void);
    void (*LoadPatch)(int patch);
    void (*SetVolume)(int volume);
    int (*GetVolume)(void);
} midifuncs;

int MIDI_Init(void);
void MIDI_SetMidiFuncs(const midifuncs *f);
int MIDI_PlaySong(const unsigned char *song, int loop);
void MIDI_StopSong(void);
int MIDI_SetVolume(int volume);
void MIDI_Service(void);

#endif
