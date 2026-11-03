#ifndef BOARD_H
#define BOARD_H

/* Map version 5, as loadboard at 0x49127 reads it.
   Sector is 37 bytes, wall 35, sprite 38. Offsets below are the ones
   the drawers actually index (ceilingz at +12, wall point2 at +8,
   wall picnum at +10, nextsector at +21, sprite picnum at +16,
   statnum at +28, sectnum at +30). */

#pragma pack(push, 1)

typedef struct {
    short wallptr;
    short wallnum;
    short ceilingpicnum; /* +4 */
    short floorpicnum;   /* +6, the floor span samples this for color */
    short floorpic2;     /* +8, the floor span samples this as a height */
    short raw10;
    int ceilingz;
    int floorz;
    unsigned char raw20[6];
    unsigned char ceilingstat;
    unsigned char floorstat;
    unsigned char raw28;
    unsigned char visibility;
    unsigned char raw30;
    short lotag;
    short hitag;
    short extra;
} sectortype;

typedef struct {
    int x;
    int y;
    short point2;
    short picnum;
    short overpicnum;
    char shade;
    unsigned short cstat;
    unsigned char raw17;
    unsigned char repeat;
    unsigned char xpanning;
    unsigned char ypanning;
    short nextsector;
    unsigned char tail[12];
} walltype;

typedef struct {
    int x;
    int y;
    int z;
    unsigned char raw12[4];
    short picnum;
    short raw18;
    short raw20;
    short raw22;
    short raw24;
    short raw26;
    short statnum;
    short sectnum;
    short raw32;
    short raw34;
    short raw36;
} spritetype;

#pragma pack(pop)

typedef char board_sector_size[(sizeof(sectortype) == 37) ? 1 : -1];
typedef char board_wall_size[(sizeof(walltype) == 35) ? 1 : -1];
typedef char board_sprite_size[(sizeof(spritetype) == 38) ? 1 : -1];

extern sectortype *sector;
extern walltype *wall;
extern spritetype *sprite;
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

/* One input sample. fvel/svel/avel are signed, already scaled for this step. */
void player_move(int fvel, int svel, int avel);
void player_look(int dh);

#endif
