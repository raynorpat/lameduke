/* Mix loop for multivoc. The binary's mixer is the block that feeds
   the DMA drivers. SDL playback will call this from dsl.c. */

#include "mv_mix.h"

void MV_Mix(void *dest, unsigned samples)
{
    (void)dest;
    (void)samples;
}
