#include "sounds.h"

#include <stdio.h>

/* 0x3BBD0. Runs after FX_SetupCard and before FX_Init. */
void loadsounds(void)
{
    puts("Reading sounds...");
}

/* Passed to FX_SetCallBack. The original handler is 0x3C190. */
void sound_callback(unsigned long value)
{
    (void)value;
}

/* 0x3BE70. The VOC playback this starts arrives with the sound task. */
void spritesound(int num, short i)
{
    (void)num;
    (void)i;
}

/* 0x3C040 */
void stopsound(int num)
{
    (void)num;
}
