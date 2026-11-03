#ifndef TILES_H
#define TILES_H

#include "engine.h"

/* loadpics at 0x4ABCB. ART version must be 1.
   tiles000.art, tiles001.art, ... up to MAXTILES.
   Pixels are column-major: data[x * sizy + y]. */

void loadpics(const char *filename);

/* NULL when the tile was not in any ART file. */
const unsigned char *tile_data(int pic, int *sizx, int *sizy);

#endif
