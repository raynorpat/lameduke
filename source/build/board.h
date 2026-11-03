#ifndef BOARD_H
#define BOARD_H

/* Map version 5, as loadboard at 0x49127 reads it. Sector is 37 bytes,
   wall 35, sprite 38. Fields follow the v5 order; each offset the drawers
   use was checked against D3D.EXE (sprite +28 is sectnum, +30 statnum, per
   insertsprite 0x50B40). Sector +8/+10 are heightmap tiles in this beta. */

#pragma pack(push, 1)

typedef struct {
    short wallptr, wallnum;
    short ceilingpicnum, floorpicnum;   /* +4, +6 */
    short ceilingheinum, floorheinum;   /* +8, +10: heightmap tiles (floorstat&2) */
    int ceilingz, floorz;               /* +12, +16 */
    signed char ceilingshade, floorshade; /* +20 */
    unsigned char ceilingxpanning, floorxpanning; /* +22 */
    unsigned char ceilingypanning, floorypanning; /* +24 */
    unsigned char ceilingstat, floorstat; /* +26 */
    unsigned char ceilingpal, floorpal; /* +28 */
    unsigned char visibility;           /* +30 */
    short lotag, hitag, extra;          /* +31 */
} sectortype;

typedef struct {
    int x, y;
    short point2, picnum, overpicnum;   /* +8 */
    signed char shade;                  /* +14 */
    short cstat;                        /* +15 */
    unsigned char xrepeat, yrepeat, xpanning, ypanning; /* +17 */
    short nextsector, nextwall;         /* +21 */
    short nextsector2, nextwall2;       /* +25 */
    short lotag, hitag, extra;          /* +29 */
} walltype;

typedef struct {
    int x, y, z;
    unsigned char cstat;                /* +12 */
    signed char shade;
    unsigned char xrepeat, yrepeat;
    short picnum, ang, xvel, yvel, zvel, owner; /* +16 */
    short sectnum, statnum;             /* +28 */
    short lotag, hitag, extra;          /* +32 */
} spritetype;

#pragma pack(pop)

typedef char board_sector_size[(sizeof(sectortype) == 37) ? 1 : -1];
typedef char board_wall_size[(sizeof(walltype) == 35) ? 1 : -1];
typedef char board_sprite_size[(sizeof(spritetype) == 38) ? 1 : -1];

#define MAXSECTORS 1024
#define MAXWALLS 8192
#define MAXSPRITES 4096
#define MAXSTATUS 1024

extern sectortype sector[MAXSECTORS];
extern walltype wall[MAXWALLS];
extern spritetype sprite[MAXSPRITES];
extern short headspritesect[MAXSECTORS + 1], headspritestat[MAXSTATUS + 1];
extern short prevspritesect[MAXSPRITES], nextspritesect[MAXSPRITES];
extern short prevspritestat[MAXSPRITES], nextspritestat[MAXSPRITES];
extern int numsectors;
extern int numwalls;
extern int numsprites;

extern int posx;
extern int posy;
extern int posz;
extern int horiz;
extern short ang;
extern short cursectnum;

/* 0 on a version-5 board that is fully consumed. -1 otherwise. */
int loadboard(const char *filename);

/* 0x4B977. 1 when (x,y) is inside the sector, 0 when it is not, -1 if sect is invalid. */
int inside(int x, int y, short sect);

/* 0x50B40. Returns the sprite index taken from the free list, or -1. */
int insertsprite(short sectnum, short statnum);
void deletesprite(short s);
int changespritesect(short s, short sectnum);
int changespritestat(short s, short statnum);
int setsprite(short s, int x, int y, int z);
void updatesector(int x, int y, short *sectnum);
void rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2);
int clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist);
/* 0x5225C. Returns the tag of the first thing hit: wall+0x8000, sprite+0xC000, or 0. */
int clipmove(int *x, int *y, int *z, short *sectnum, int xvect, int yvect,
    int walldist, int ceildist, int flordist, int cliptype);
int movesprite(short s, int xchange, int ychange, int zchange, int walldist, int ceildist, int flordist,
    int cliptype, int tics);
int cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2);
/* 0x511C4 */
void hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz,
    short *hitsect, short *hitwall, short *hitsprite, int *hitx, int *hity, int *hitz);
/* 0x55667 */
void getzrange(int x, int y, int z, short sectnum, int *ceilz, int *ceilhit, int *florz, int *florhit,
    int walldist, int cliptype);

/* One sync sample from sub_31410. Velocities are the signed ramp bytes.
   tics is the clock delta (byte_1257C1). bits bit 1 halves the step. */
void player_move(int fvel, int svel, int avel, int tics, int bits);
void player_look(int dh);

#endif
