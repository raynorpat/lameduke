#ifndef POLYMOST_H
#define POLYMOST_H

/* duke3d's Polymost (polymost.c, compiled inside draw.c). drawrooms,
   drawmasks, rotatesprite and rotatescalesprite call these when rendmode
   is not 0. */

void polymost_drawrooms(void);
void polymost_drawmaskwall(int damaskwallcnt);
void polymost_drawsprite(int snum);
void polymost_dorotatesprite(int sx, int sy, int z, short a, short picnum, signed char dashade,
    unsigned char dapalnum, unsigned short dastat, int cx1, int cy1, int cx2, int cy2);

#endif
