#include "multivoc.h"

#include "blaster.h"
#include "dsl.h"
#include "pas16.h"
#include "sndsrc.h"

int MV_ErrorCode = 0;

static void (*mv_callback)(unsigned long);
static int mv_volume;

const char *MV_ErrorString(int code)
{
    if (code < 0)
        code = MV_ErrorCode;
    switch (code) {
    case 0:
        return "Multivoc ok.";
    case 1:
        return "Selected sound card is not supported by Multivoc.";
    case 2:
        return "Multivoc not installed.";
    case 3:
        return "No free voices available to Multivoc.";
    case 4:
        return "Out of memory in Multivoc.";
    case 5:
        return "No voice with matching handle found.";
    case 6:
        return BLASTER_ErrorString(-1);
    case 7:
        return PAS_ErrorString(-1);
    case 8:
        return SNDSRC_ErrorString(-1);
    case 9:
        return "DPMI Error in Multivoc.";
    default:
        return "Unknown Multivoc error code.";
    }
}

int MV_Init(int card, unsigned rate, int voices, int channels, int bits)
{
    (void)card;
    (void)voices;
    if (DSL_Init(rate, channels, bits) != 0) {
        MV_ErrorCode = 1;
        return -1;
    }
    MV_ErrorCode = 0;
    return 0;
}

void MV_SetVolume(int volume)
{
    mv_volume = volume;
}

void MV_SetCallBack(void (*function)(unsigned long))
{
    mv_callback = function;
}
