#include "pcfx.h"

static int pcfx_error;
static void (*pcfx_callback)(unsigned long);

const char *PCFX_ErrorString(int code)
{
    if (code < 0)
        code = pcfx_error;
    switch (code) {
    case 0: return "PC FX ok.";
    case 1: return "No free voices available to PC FX.";
    case 2: return "No voice with matching handle found.";
    case 3: return "DPMI Error in PCFX.";
    default: return "Unknown PC FX error code.";
    }
}

int PCFX_Init(void)
{
    pcfx_error = 0;
    return 0;
}

void PCFX_SetVolume(int volume)
{
    (void)volume;
}

void PCFX_SetCallBack(void (*function)(unsigned long))
{
    pcfx_callback = function;
}
