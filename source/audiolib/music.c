#include "music.h"

#include "al_midi.h"
#include "awe32.h"
#include "blaster.h"
#include "dsl.h"
#include "gusmidi.h"
#include "ll_man.h"
#include "midi.h"
#include "mpu401.h"
#include "pas16.h"
#include "sndcards.h"

#include <stddef.h>

int MUSIC_ErrorCode = MUSIC_Ok;
static int MUSIC_Card = -1;

/* unk_241B98, filled by 0x68F94 for the OPL cards. */
static midifuncs al_funcs = {
    AL_NoteOff, AL_NoteOn, NULL, AL_ControlChange, AL_ProgramChange, NULL, AL_SetPitchBend,
    NULL, NULL, NULL, NULL,
};

const char *MUSIC_ErrorString(int code)
{
    if (code < 0)
        code = MUSIC_ErrorCode;
    switch (code) {
    case MUSIC_Ok:
        return "Music ok.";
    case MUSIC_SoundCardError:
        switch (MUSIC_Card) {
        case SoundBlaster:
            return BLASTER_ErrorString(-1);
        case ProAudioSpectrum:
        case SoundMan16:
            return PAS_ErrorString(-1);
        case Adlib:
            return "Adlib error.";
        case GenMidi:
        case SoundCanvas:
        case WaveBlaster:
            return "MPU-401 error.";
        case Awe32:
            return "AWE32 error.";
        case UltraSound:
            return GUSMIDI_ErrorString(-1);
        default:
            return "Invalid Music device.";
        }
    case MUSIC_InvalidCard:
        return "Invalid Music device.";
    case MUSIC_MidiError:
        return "Error playing MIDI file.";
    case MUSIC_TaskManError:
        return "TaskMan error.";
    case MUSIC_FMNotDetected:
        return "Could not detect FM chip.";
    case MUSIC_DPMIError:
        return "DPMI Error in MUSIC.";
    default:
        return "Unknown Music error code.";
    }
}

int MUSIC_Init(int SoundCard, int Address)
{
    int status;

    (void)Address;
    if (LL_LockMemory() != 0) {
        MUSIC_ErrorCode = MUSIC_DPMIError;
        return -1;
    }
    MUSIC_Card = SoundCard;
    switch (SoundCard) {
    case SoundBlaster:
    case ProAudioSpectrum:
    case SoundMan16:
    case Adlib:
        /* 0x68F94; the FM chip is always found here */
        status = AL_Init(SoundCard);
        if (SoundCard == SoundBlaster && BLASTER_CardHasMixer()) {
            al_funcs.SetVolume = BLASTER_SetFMVolume;
            al_funcs.GetVolume = BLASTER_GetFMVolume;
        }
        MIDI_SetMidiFuncs(&al_funcs);
        DSL_InitMusic();
        break;
    case GenMidi:
    case SoundCanvas:
    case WaveBlaster:
        status = MPU_Init(SoundCard);
        break;
    case Awe32:
        status = AWE32_Init();
        break;
    case UltraSound:
        status = GUSMIDI_Init();
        break;
    default:
        MUSIC_ErrorCode = MUSIC_InvalidCard;
        status = -1;
        break;
    }
    if (status != 0)
        LL_UnlockMemory();
    return status;
}

/* 0x68DE4 */
void MUSIC_SetVolume(int volume)
{
    if (MUSIC_Card == -1)
        return;
    DSL_Lock();
    MIDI_SetVolume(volume);
    DSL_Unlock();
}

/* 0x68E94 */
int MUSIC_PlaySong(const unsigned char *song, int loop)
{
    int status;

    if (MUSIC_Card == -1) {
        MUSIC_ErrorCode = MUSIC_InvalidCard;
        return MUSIC_Warning;
    }
    DSL_Lock();
    status = MIDI_PlaySong(song, loop);
    DSL_Unlock();
    if (status != 0) {
        MUSIC_ErrorCode = MUSIC_MidiError;
        return MUSIC_Warning;
    }
    return MUSIC_Ok;
}

/* 0x68E24 */
void MUSIC_Continue(void)
{
    DSL_Lock();
    MIDI_ContinueSong();
    DSL_Unlock();
}

/* 0x68E2C */
void MUSIC_Pause(void)
{
    DSL_Lock();
    MIDI_PauseSong();
    DSL_Unlock();
}
