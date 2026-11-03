#ifndef TILES_H
#define TILES_H

#include "engine.h"

/* loadpics at 0x4ABCB. ART version must be 1.
   tiles000.art, tiles001.art, ... up to MAXTILES.
   Pixels are column-major: waloff[pic][x * tilesizy[pic] + y]. */
void loadpics(const char *filename);

extern short tilesizx[MAXTILES];  /* word_1AAB90 */
extern short tilesizy[MAXTILES];  /* word_1ACB90 */
extern int picanm[MAXTILES];      /* dword_1A2B90 */
extern unsigned char picsiz[MAXTILES]; /* byte_187E90 */
extern unsigned char *waloff[MAXTILES]; /* dword_19EB90; NULL when the tile is empty */

#endif
