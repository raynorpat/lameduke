#include "adlibfx.h"

static int adlib_error;
static void (*adlib_callback)(unsigned long);

const char *ADLIBFX_ErrorString(int code)
{
    if (code < 0)
        code = adlib_error;
    switch (code) {
    case 0: return "Adlib FX ok.";
    case 1: return "No free voices available to Adlib FX.";
    case 2: return "No voice with matching handle found.";
    case 3: return "DPMI Error in AdlibFX.";
    default: return "Unknown Adlib FX error code.";
    }
}

int ADLIBFX_Init(void)
{
    adlib_error = 0;
    return 0;
}

void ADLIBFX_SetVolume(int volume)
{
    (void)volume;
}

void ADLIBFX_SetCallBack(void (*function)(unsigned long))
{
    adlib_callback = function;
}
