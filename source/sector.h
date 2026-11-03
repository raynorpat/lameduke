#ifndef SECTOR_H
#define SECTOR_H

#define MAXANIMATES 512
#define MAXANIMWALLS 512

typedef struct {
    short wallnum;
    int tag;
} animwalltype;

extern short animatesect[MAXANIMATES];
extern int *animateptr[MAXANIMATES];
extern int animategoal[MAXANIMATES], animatevel[MAXANIMATES], animatecnt;
extern animwalltype animwall[MAXANIMWALLS];
extern short numanimwalls;

void doanimations(int tics);
void activatebylotag(short lotag);
int setanimate(short sect, int *ptr, int goal, int vel);
void operatesectors(short sn, short ii);
void checkhitswitch(short w);
void checkhitwall(short w, int x, int y, int z, short atwith);
int checkhitceiling(short sn);
void quote(short q);
extern const char fta_quotes[13][32];

#endif
