#include "fx_man.h"

#include "adlibfx.h"
#include "blaster.h"
#include "guswave.h"
#include "ll_man.h"
#include "multivoc.h"
#include "pas16.h"
#include "pcfx.h"
#include "sndcards.h"
#include "sndsrc.h"

int FX_ErrorCode = FX_Ok;
static int FX_Card;

const char *FX_ErrorString(int code)
{
    if (code < 0)
        code = FX_ErrorCode;
    switch (code) {
    case FX_Ok:
        return "Fx ok.";
    case FX_SoundCardError:
        switch (FX_Card) {
        case SoundBlaster:
        case Awe32:
            return BLASTER_ErrorString(-1);
        case ProAudioSpectrum:
        case SoundMan16:
            return PAS_ErrorString(-1);
        case Adlib:
            return ADLIBFX_ErrorString(-1);
        case UltraSound:
            return GUSWAVE_ErrorString(-1);
        case SoundSource:
        case TandySoundSource:
            return SNDSRC_ErrorString(-1);
        case PC:
            return PCFX_ErrorString(-1);
        default:
            return "Unknown Fx error code.";
        }
    case FX_InvalidCard:
        return "Invalid Sound Fx device.";
    case FX_MultiVocError:
        return MV_ErrorString(-1);
    case FX_InvalidVOCFile:
        return "Invalid VOC file.";
    case FX_DPMIError:
        return "DPMI Error in FX_MAN.";
    default:
        return "Unknown Fx error code.";
    }
}

static void device_clear(fx_device *device)
{
    device->MaxVoices = 0;
    device->MaxSampleBits = 0;
    device->MaxChannels = 0;
}

int FX_SetupCard(int SoundCard, fx_device *device)
{
    FX_Card = SoundCard;
    device_clear(device);
    switch (SoundCard) {
    case SoundBlaster:
    case Awe32:
        if (BLASTER_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            return -1;
        }
        device->MaxVoices = 8;
        break;
    case ProAudioSpectrum:
    case SoundMan16:
        if (PAS_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            return -1;
        }
        device->MaxVoices = 8;
        break;
    case Adlib:
    case PC:
        device->MaxVoices = 1;
        device->MaxChannels = 1;
        break;
    case GenMidi:
    case SoundCanvas:
    case WaveBlaster:
        break;
    case UltraSound:
        if (GUSWAVE_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            return -1;
        }
        device->MaxVoices = 8;
        break;
    case SoundSource:
    case TandySoundSource:
        if (SNDSRC_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            return -1;
        }
        device->MaxVoices = 8;
        device->MaxSampleBits = 8;
        device->MaxChannels = 1;
        break;
    default:
        FX_ErrorCode = FX_InvalidCard;
        return -1;
    }
    FX_ErrorCode = FX_Ok;
    return 0;
}

int FX_Init(int SoundCard, int numvoices, int numchannels, int samplebits, unsigned mixrate)
{
    int status = 0;

    if (LL_LockMemory() != 0) {
        FX_ErrorCode = FX_DPMIError;
        return -1;
    }
    FX_Card = SoundCard;
    switch (SoundCard) {
    case SoundBlaster:
    case ProAudioSpectrum:
    case SoundMan16:
    case Awe32:
    case SoundSource:
    case TandySoundSource:
        if (MV_Init(SoundCard, mixrate, numvoices, numchannels, samplebits) != 0) {
            FX_ErrorCode = FX_MultiVocError;
            status = -1;
        }
        break;
    case Adlib:
        if (ADLIBFX_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            status = -1;
        }
        break;
    case UltraSound:
        if (GUSWAVE_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            status = -1;
        }
        break;
    case PC:
        if (PCFX_Init() != 0) {
            FX_ErrorCode = FX_SoundCardError;
            status = -1;
        }
        break;
    default:
        FX_ErrorCode = FX_InvalidCard;
        status = -1;
        break;
    }
    if (status != 0)
        LL_UnlockMemory();
    return status;
}

int FX_SetCallBack(void (*function)(unsigned long))
{
    switch (FX_Card) {
    case SoundBlaster:
    case ProAudioSpectrum:
    case SoundMan16:
    case Awe32:
    case SoundSource:
    case TandySoundSource:
        MV_SetCallBack(function);
        return 0;
    case Adlib:
        ADLIBFX_SetCallBack(function);
        return 0;
    case UltraSound:
        GUSWAVE_SetCallBack(function);
        return 0;
    case PC:
        PCFX_SetCallBack(function);
        return 0;
    default:
        FX_ErrorCode = FX_InvalidCard;
        return -1;
    }
}

int FX_SetVolume(int volume)
{
    switch (FX_Card) {
    case SoundBlaster:
    case Awe32:
        /* 0x6E338: a card with a mixer sets it, otherwise multivoc scales */
        if (BLASTER_CardHasMixer())
            BLASTER_SetVolume(volume);
        else
            MV_SetVolume(volume);
        return 0;
    case ProAudioSpectrum:
    case SoundMan16:
        PAS_SetVolume(volume);
        return 0;
    case Adlib:
        ADLIBFX_SetVolume(volume);
        return 0;
    case UltraSound:
        GUSWAVE_SetVolume(volume);
        return 0;
    case SoundSource:
    case TandySoundSource:
        MV_SetVolume(volume);
        return 0;
    case PC:
        PCFX_SetVolume(volume);
        return 0;
    default:
        return 0;
    }
}

static int multivoc_card(void)
{
    switch (FX_Card) {
    case SoundBlaster:
    case ProAudioSpectrum:
    case SoundMan16:
    case Awe32:
    case SoundSource:
    case TandySoundSource:
        return 1;
    default:
        return 0;
    }
}

/* 0x6E660. Only the multivoc cards are lifted; the shipped setup picks one. */
int FX_PlayVOC3D(const unsigned char *ptr, int pitchoffset, int angle, int distance, int priority,
    unsigned long callbackval)
{
    int handle;

    if (!multivoc_card()) {
        FX_ErrorCode = FX_InvalidCard;
        return FX_Error;
    }
    handle = MV_PlayVOC3D(ptr, pitchoffset, angle, distance, priority, callbackval);
    if (handle == -1) {
        FX_ErrorCode = FX_MultiVocError;
        return FX_Warning;
    }
    return handle;
}

/* 0x6E788 */
int FX_Pan3D(int handle, int angle, int distance)
{
    if (!multivoc_card()) {
        FX_ErrorCode = FX_InvalidCard;
        return FX_Error;
    }
    if (MV_Pan3D(handle, angle, distance) != 0) {
        FX_ErrorCode = FX_MultiVocError;
        return FX_Warning;
    }
    return FX_Ok;
}

/* 0x6E898 */
int FX_StopSound(int handle)
{
    if (!multivoc_card()) {
        FX_ErrorCode = FX_InvalidCard;
        return FX_Ok;
    }
    if (MV_Kill(handle) != 0) {
        FX_ErrorCode = FX_MultiVocError;
        return FX_Warning;
    }
    return FX_Ok;
}

/* 0x6E978 */
int FX_StopAllSounds(void)
{
    if (multivoc_card() && MV_KillAllVoices() != 0) {
        FX_ErrorCode = FX_MultiVocError;
        return FX_Warning;
    }
    return FX_Ok;
}
