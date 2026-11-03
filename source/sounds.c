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
