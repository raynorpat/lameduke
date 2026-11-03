#ifndef GAMEDEF_H
#define GAMEDEF_H

#include "engine.h"

/* GAME.CON is compiled at startup by 0x401E0. The exe stores absolute
   pointers in the script; here every code reference is an index into script[], and index 0 is never used. */

#define MAXSCRIPTSIZE 16384
#define MAXLABELS 512
#define NUMKEYWORDS 60
#define NUMCONSOUNDS 38

int compilecon(void);
/* 0x42260: runs sprite i's actor code against player p at distance dist. */
void execute(short i, short p, int dist);

extern int script[MAXSCRIPTSIZE];   /* 0x976CC */
extern int scriptsize;              /* (0x976C8 - script) / 4 */
extern int actorscrptr[MAXTILES];   /* 0x8AE78: 0 when the tile has no actor */
extern char labels[MAXLABELS][64];  /* 0x12523C */
extern int labelcode[MAXLABELS];    /* 0x125244 */
extern int labelcnt;                /* 0x125238 */
extern const char *const keyw[NUMKEYWORDS];   /* 0x82B10 */
extern const char *const consounds[NUMCONSOUNDS]; /* 0x82838 */
/* music <vol> names...: [0] is volume 0 (0x125308), [v] is 0x8AC70 + 130*(v-1). */
extern char musicfn[5][10][13];

#endif
