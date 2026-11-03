#ifndef ACTORS_H
#define ACTORS_H

#include "board.h"

extern unsigned char show2dsprite[(MAXSPRITES + 7) >> 3];
extern int msx[1024], msy[1024], mspos;

short EGS(short whatsect, int x, int y, int z, int cstat, short picnum, int shade, int xrepeat,
    int yrepeat, short ang, short xvel, short zvel, short owner, short stat);
short spawn(short j, short pn);
short nextsectorneighborz(short sectnum, int thez, short topbottom, short direction);

short findplayer(short i, int *dist);
void addweapon(short p, short weapon, int force);
void shoot(short i, short atwith, int mode);
void processinput(short snum);
void resetplayer(short p);

int getincangle(int a, int na, int shift);
int ssp(short i, int cliptype);
void hitradius(short i, int r, int damage);
void nuke(int x, int y, int z, short i);
short ifsquished(short i, short p);
short ifhitweapon(short i);
void guts(short i);

void wakeactors(void);
void moveplayers(void);
void moveweapons(void);
void movemusic(void);
void moveeffects(void);
void movestandables(void);

extern short randomstash; /* word_1253A0: a krand drawn at the start of every tic */

#endif
