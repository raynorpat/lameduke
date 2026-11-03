#include "guswave.h"
#include "dsl.h"
#include "gus.h"

static int guswave_error;
static void (*guswave_callback)(unsigned long);

const char *GUSWAVE_ErrorString(int code)
{
    if (code < 0)
        code = guswave_error;
    switch (code) {
    case 0: return "GUSWAVE ok.";
    case 1: return GUS_ErrorString(0);
    case 2: return "GUSWAVE not installed.";
    case 3: return "No free voices available to GUSWAVE.";
    case 4: return "Not enough Ultrasound memory available for GUSWAVE.";
    case 5: return "No voice with matching handle found.";
    default: return "Unknown GUSWAVE error code.";
    }
}

int GUSWAVE_Init(void)
{
    guswave_error = 0;
    return DSL_Init(11000, 1, 8);
}

void GUSWAVE_SetVolume(int volume)
{
    (void)volume;
}

void GUSWAVE_SetCallBack(void (*function)(unsigned long))
{
    guswave_callback = function;
}
