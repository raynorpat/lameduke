#ifndef GLOBAL_H
#define GLOBAL_H

#include "game.h"
#include "board.h"
#include "network.h"

#define MAXPLAYERS 17

/* The 176-byte player record at 0x108830, one per player. Offsets match the
   exe; fields are named by offset until their use is clear. */
#pragma pack(push, 1)
typedef struct {
    short i;                 /* +00 player sprite */
    int f02, f06;
    int posx, posy, posz;    /* +0A eye position */
    int horiz;               /* +16 */
    int f1a, f1e, f22;       /* +1A last x/y/z */
    int f26;                 /* +26 z velocity */
    int oposx, oposy, oposz; /* +2A */
    int f36;
    int f3a;                 /* +3A bob counter */
    short ang, oang, f42;    /* +3E */
    short cursectnum, ocursect; /* +44 */
    short lookang;           /* +48 */
    short w4a, w4c, w4e, w50, w52, w54, w56, w58, w5a, w5c, w5e, w60;
    short w62[12];           /* +62..+79: rest of the per-weapon timers that w60 starts */
    short w7a;               /* +7A */
    short w7c;               /* +7C weapon frame */
    short w7e;               /* +7E current weapon tile */
    short w80, w82, w84;
    int musicptr;            /* +86 */
    unsigned char gm;        /* +8A game mode flags */
    unsigned char b8b;
    unsigned char skill, level, volume; /* +8C */
    unsigned char b8f, b90, b91, b92, b93, b94, b95, b96, b97, b98, b99, b9a, b9b, b9c, b9d,
        b9e, b9f, ba0, ba1, ba2, ba3, ba4, ba5, ba6, ba7, ba8, ba9, baa, bab, bac, bad, bae, baf;
} playertype;
#pragma pack(pop)

typedef char playertype_size[(sizeof(playertype) == 176) ? 1 : -1];

extern playertype ps[MAXPLAYERS];

/* Five ints per sprite at 0x1093E0: t0 counter, t1 move, t2, t3, t4 action. */
extern int hittype[MAXSPRITES][5];

/* One frame of input per player (0x1253FF.., 0x1252A0). */
extern signed char syncfvel[MAXPLAYERS], syncsvel[MAXPLAYERS], syncavel[MAXPLAYERS];
extern int syncbits[MAXPLAYERS];
extern unsigned char synctics;       /* 0x1257C1 */
extern short screenpeek;
extern int lockclock;                /* 0x125304 */
extern short gamepaused, cameraspr;  /* 0x1257B3, 0x1257B5 */
extern short myconnectindex;         /* word_12538C */
extern int demomode;                 /* dword_125258 */
#define debugmode game_debug         /* word_1257AD, set by /d */
extern int game_debug;

#endif
