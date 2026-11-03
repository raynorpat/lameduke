#include "sndsrc.h"

static int sndsrc_error;

const char *SNDSRC_ErrorString(int code)
{
    if (code < 0)
        code = sndsrc_error;
    switch (code) {
    case 0: return "Sound Source ok.";
    case 1: return "Could not detect Sound Source.";
    case 2: return "No sound playing.";
    case 3: return "DPMI Error in SndSrc.";
    default: return "Unknown Sound Source error code.";
    }
}

int SNDSRC_Init(void)
{
    sndsrc_error = 0;
    return 0;
}

void SNDSRC_SetVolume(int volume)
{
    (void)volume;
}
