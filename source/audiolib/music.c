#include "music.h"

#include "al_midi.h"
#include "awe32.h"
#include "blaster.h"
#include "gusmidi.h"
#include "ll_man.h"
#include "mpu401.h"
#include "pas16.h"
#include "sndcards.h"

int MUSIC_ErrorCode = MUSIC_Ok;
static int MUSIC_Card = -1;
static int music_volume;

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
        status = AL_Init(SoundCard);
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

void MUSIC_SetVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    if (volume > 255)
        volume = 255;
    music_volume = volume;
}
