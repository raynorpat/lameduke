#include "pas16.h"

static int pas_error;

const char *PAS_ErrorString(int code)
{
    if (code < 0)
        code = pas_error;
    switch (code) {
    case 0: return "Pro AudioSpectrum ok.";
    case 1: return "MVSOUND.SYS not loaded.";
    case 2: return "Invalid Pro AudioSpectrum Irq.";
    case 3: return "Unable to set Pro AudioSpectrum Irq.";
    case 4: return "Unsupported Pro AudioSpectrum Irq.";
    case 5: return "No sound playing.";
    case 6: return "Could not find Pro AudioSpectrum.";
    case 7: return "DPMI Error in PAS16.";
    case 8: return "Out of conventional memory in PAS16.";
    default: return "Unknown Pro AudioSpectrum error code.";
    }
}

int PAS_Init(void)
{
    pas_error = 0;
    return 0;
}

void PAS_SetVolume(int volume)
{
    (void)volume;
}
