#include "gusmidi.h"
#include "gus.h"

static int gusmidi_error;

const char *GUSMIDI_ErrorString(int code)
{
    if (code < 0)
        code = gusmidi_error;
    switch (code) {
    case 0: return "Ultrasound music ok.";
    case 1: return "Out of memory in GusMidi.";
    case 2: return "Out of conventional (640K) memory in GusMidi.";
    case 4: return GUS_ErrorString(0);
    case 5: return "Ultrasound IRQ must be 7 or less.";
    case 6: return "ULTRADIR environment variable not set.";
    case 7: return "Can't find ULTRASND.INI file.";
    default: return "Unknown Ultrasound error code.";
    }
}

int GUSMIDI_Init(void)
{
    gusmidi_error = 0;
    return 0;
}
