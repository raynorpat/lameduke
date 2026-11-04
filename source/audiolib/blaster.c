#include "blaster.h"
#include "dsl.h"

static int blaster_error;
static void (*blaster_callback)(unsigned long);

const char *BLASTER_ErrorString(int code)
{
    if (code < 0)
        code = blaster_error;
    switch (code) {
    case 0: return "Sound Blaster ok.";
    case 1: return "BLASTER environment variable not set.";
    case 2: return "Missing Sound Blaster address in BLASTER environment variable.";
    case 3: return "Missing Sound Blaster interrupt in BLASTER environment variable.";
    case 4: return "Missing Sound Blaster DMA channel in BLASTER environment variable.";
    case 5: return "Missing Sound Blaster 16-bit DMA channel in BLASTER environment variable.";
    case 6: return "Sound Blaster card type not set.";
    case 7: return "Invalid parameter in BLASTER environment variable.";
    case 8: return "Sound Blaster card not ready.";
    case 9: return "No sound playing.";
    case 10: return "Invalid Sound Blaster Irq.";
    case 11: return "Unable to set Sound Blaster Irq.";
    case 12: return "DMA channel ok.";
    case 13: return "Mixer not available on selected Sound Blaster card.";
    case 14: return "DPMI Error in Blaster.";
    case 15: return "Out of conventional memory in Blaster.";
    default: return "Unknown Sound Blaster error code.";
    }
}

int BLASTER_Init(void)
{
    blaster_error = 0;
    return 0;
}

/* 0x6FDB0. The port stands in for an SB16, which has a mixer. */
int BLASTER_CardHasMixer(void)
{
    return 1;
}

/* 0x6FBB4, the SB16 branch: both voice volume registers get volume & 0xF8. */
void BLASTER_SetVolume(int volume)
{
    if (volume > 255)
        volume = 255;
    if (volume < 0)
        volume = 0;
    DSL_SetVoiceLevel((volume & 0xF8) >> 3);
}

void BLASTER_SetCallBack(void (*function)(unsigned long))
{
    blaster_callback = function;
}

static int fmlevel = 0xC0; /* SB16 mixer registers 0x34/0x35 after reset */

/* 0x6FD04, the SB16 branch: both FM volume registers get volume & 0xF8. */
void BLASTER_SetFMVolume(int volume)
{
    if (volume > 255)
        volume = 255;
    if (volume < 0)
        volume = 0;
    fmlevel = volume & 0xF8;
    DSL_SetFMLevel(fmlevel >> 3);
}

/* 0x6FC60 */
int BLASTER_GetFMVolume(void)
{
    return fmlevel;
}
