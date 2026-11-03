#include "al_midi.h"
#include "oplemu.h"

int AL_Init(int card)
{
    (void)card;
    return OPL_Init();
}
