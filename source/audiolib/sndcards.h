#ifndef SNDCARDS_H
#define SNDCARDS_H

/* Card numbers are the switch in FX_SetupCard / MUSIC_Init (D3D.EXE).
   8 is GUSWAVE and GusMidi in this binary. duke3d's later sndcards.h
   puts SoundScape at 8 and UltraSound at 9.
   The setup menu table at 0x828F8 is {-1,0,1,3,4,7,9,10,12}. Slot 6
   (the Gravis line in SETUP.EXE) stores 9, which this switch treats
   as Sound Source. Left as recovered. */

enum {
    SoundBlaster = 0,
    ProAudioSpectrum = 1,
    SoundMan16 = 2,
    Adlib = 3,
    GenMidi = 4,
    SoundCanvas = 5,
    Awe32 = 6,
    WaveBlaster = 7,
    UltraSound = 8,
    SoundSource = 9,
    TandySoundSource = 10,
    PC = 11,
    NumSoundCards = 12
};

#endif
