#include "gus.h"

const char *GUS_ErrorString(int code)
{
    switch (code) {
    case 0: return "Ok";
    case 1: return "No more voices available";
    case 2: return "Base port not found";
    case 3: return "Invalid IRQ - run setgus";
    case 4: return "Invalid DMA - run setgus";
    case 5: return "OS already loaded";
    case 6: return "Can't unload OS.  Not loaded";
    case 7: return "No more memory available on UltraSound card";
    case 8: return "DMA channel busy (conflict?)";
    case 9: return "Too many interrupt handlers";
    case 10: return "No response from DMA channel";
    case 11: return "UltraSound card not found";
    case 12: return "Another program is using the UltraSound card";
    case 13: return "No more DOS multiplex interrupts";
    case 14: return "Timer value must be 1 or 2";
    case 15: return "Invalid or malformed patch";
    case 16: return "Old patch, run update program";
    case 17: return "DOS error";
    case 18: return "File not found";
    default: return "Unknown error";
    }
}
