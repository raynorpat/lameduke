#include "mpu401.h"
#include "midi.h"

int MPU_Init(int card)
{
    (void)card;
    return MIDI_Init();
}
