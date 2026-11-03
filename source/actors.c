/* Sprite creation: EGS 0x12E90 and spawn 0x1F160. */

#include "actors.h"
#include "global.h"
#include "gamedef.h"
#include "sector.h"
#include "sounds.h"
#include "game.h"
#include "draw.h"
#include "music.h"

#include "board.h"
#include "math.h"
#include "tiles.h"

#include <stdlib.h>

int hittype[MAXSPRITES][5];
unsigned char show2dsprite[(MAXSPRITES + 7) >> 3]; /* byte_1B4D90 */
int msx[1024], msy[1024];                          /* 0x122830, 0x123830 */
int mspos;                                         /* dword_8AC60 */

static void markshow2d(short i)
{
    show2dsprite[i >> 3] |= (unsigned char)(1 << (i & 7));
}

/* Seeds strength, action, move and move flags from the actor's script header. */
static void actorinit(short i, short pic)
{
    int a = actorscrptr[pic];

    if (a) {
        sprite[i].extra = (short)script[a];
        hittype[i][4] = script[a + 1];
        hittype[i][1] = script[a + 2];
        if (pic != 57)
            sprite[i].hitag = (short)script[a + 3];
    } else {
        hittype[i][4] = 0;
        hittype[i][1] = 0;
    }
}

/* 0x12E90 */
short EGS(short whatsect, int x, int y, int z, int cstat, short picnum, int shade, int xrepeat,
    int yrepeat, short ang, short xvel, short zvel, short owner, short stat)
{
    short i = (short)insertsprite(whatsect, stat);
    spritetype *s = &sprite[i];

    s->y = y;
    s->cstat = (unsigned char)cstat;
    s->picnum = picnum;
    s->shade = (signed char)shade;
    s->xrepeat = (unsigned char)xrepeat;
    s->yrepeat = (unsigned char)yrepeat;
    s->ang = ang;
    s->xvel = xvel;
    s->zvel = zvel;
    s->owner = owner;
    s->yvel = 0;
    hittype[i][3] = 0;
    hittype[i][2] = 0;
    hittype[i][0] = 0;
    s->z = z;
    s->x = x;
    actorinit(i, picnum);
    return i;
}

/* 0x50EE9. The neighbour whose floor (topbottom 1) or ceiling is nearest to
   z, below it when direction is 1, above it otherwise. */
short nextsectorneighborz(short sectnum, int thez, short topbottom, short direction)
{
    int best = direction == 1 ? 0x7FFFFFFF : (int)0x80000000;
    short result = -1;
    int w, end = sector[sectnum].wallptr + sector[sectnum].wallnum - 1;

    for (w = sector[sectnum].wallptr; w <= end; w++) {
        short ns = wall[w].nextsector;
        int z;

        if (ns < 0)
            continue;
        z = topbottom == 1 ? sector[ns].floorz : sector[ns].ceilingz;
        if (direction == 1 ? (z > thez && z < best) : (z < thez && z > best)) {
            best = z;
            result = ns;
        }
    }
    return result;
}

/* The sector-effector setup of spawn (tile 175), by sprite lotag. */
static void spawneffector(short i, short sect, short j)
{
    spritetype *sp = &sprite[i];
    int k;

    (void)j;
    sp->yvel = sector[sect].extra;
    sp->xrepeat = 0;
    sp->yrepeat = 0;
    switch (sp->lotag) {
    case 3:
        hittype[i][3] = sector[sect].floorshade;
        sector[sect].floorshade = sp->shade;
        sector[sect].ceilingshade = sp->shade;
        sp->owner = (short)(sector[sect].ceilingpal << 8);
        sp->owner |= sector[sect].floorpal;
        for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
            if (!(wall[k].hitag & 1))
                wall[k].shade = sp->shade;
        break;
    case 4:
        hittype[i][2] = sector[sect].floorshade;
        sp->owner = (short)(sector[sect].ceilingpal << 8);
        sp->owner |= sector[sect].floorpal;
        for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
            if (wall[k].shade > hittype[i][3])
                hittype[i][3] = wall[k].shade;
        break;
    case 9:
        if ((sector[sect].lotag & 0x7FFF) && abs(sector[sect].ceilingz - sp->z) > 1024)
            sector[sect].lotag |= (short)0x8000;
        /* fall through */
    case 8:
        hittype[i][0] = sector[sect].floorshade;
        hittype[i][1] = sector[sect].ceilingshade;
        for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
            if (wall[k].shade > hittype[i][2])
                hittype[i][2] = wall[k].shade;
        hittype[i][3] = (sp->yvel >> 8) + 2;
        if (hittype[i][3] < 8)
            hittype[i][3] = 1;
        hittype[i][3] = 8; /* the exe overwrites the value it just computed */
        break;
    case 11:
        hittype[i][3] = sp->ang <= 1024 ? -2 : 2;
        /* fall through */
    case 0: case 2: case 5: case 6: case 14: case 15: case 16: case 25: case 26: {
        short s;

        hittype[i][1] = mspos;
        for (k = sector[sect].wallptr; k <= sector[sect].wallptr + sector[sect].wallnum - 1; k++) {
            msx[mspos] = wall[k].x - sp->x;
            msy[mspos] = wall[k].y - sp->y;
            mspos++;
        }
        if (sp->lotag == 6 || sp->lotag == 14 || sp->lotag == 5) {
            if (sector[sp->sectnum].hitag != -1) {
                sector[sect].floorstat |= 0x40;
                sector[sect].ceilingstat |= 0x40;
            }
            /* The exe passes the wall loop's end index as the first sector hint. */
            s = (short)(sector[sect].wallptr + sector[sect].wallnum);
            for (k = 0; k < 10; k++) {
                updatesector(k * (sintable()[(sp->ang + 512) & 2047] >> 5) + sp->x,
                    sp->y + k * (sintable()[sp->ang & 2047] >> 5), &s);
                if (sector[s].hitag == 0)
                    break;
                if (sector[s].hitag != -1) {
                    sector[s].ceilingstat |= 0x40;
                    sector[s].floorstat |= 0x40;
                }
            }
            sp->owner = -1;
            hittype[i][0] = s;
            hittype[i][3] = sp->hitag;
        }
        if (sp->lotag == 16)
            hittype[i][3] = sector[sect].ceilingz;
        if (sp->lotag == 25) {
            hittype[i][4] = sector[sect].ceilingz;
            sector[sect].ceilingz = sp->z;
        }
        if (sp->lotag == 26) {
            hittype[i][3] = sp->x;
            hittype[i][4] = sp->y;
            sp->zvel = sp->shade == sector[sect].floorshade ? -256 : 256;
            sector[sect].floorstat |= 0x40;
            sp->shade = 0;
        }
        break;
    }
    case 12:
        hittype[i][1] = sector[sect].floorshade;
        hittype[i][2] = sector[sect].ceilingshade;
        break;
    case 13:
        hittype[i][0] = sector[sect].ceilingz;
        hittype[i][1] = sector[sect].floorz;
        sp->owner = abs(hittype[i][0] - sp->z) < abs(hittype[i][1] - sp->z);
        if (sp->ang == 512) {
            if (sp->owner)
                sector[sect].ceilingz = sp->z;
            else
                sector[sect].floorz = sp->z;
        } else {
            sector[sect].floorz = sp->z;
            sector[sect].ceilingz = sp->z;
        }
        if (!(sector[sect].ceilingstat & 1))
            break;
        sector[sect].ceilingstat ^= 1;
        hittype[i][3] = 1;
        if (sp->owner == 0 && sp->ang == 512) {
            sector[sect].ceilingstat ^= 1;
            hittype[i][3] = 0;
        }
        sector[sect].ceilingshade = sector[sect].floorshade;
        if (sp->ang == 512) {
            for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++) {
                short ns = wall[k].nextsector;

                if (ns >= 0 && !(sector[ns].ceilingstat & 1)) {
                    sector[sect].ceilingpicnum = sector[ns].ceilingpicnum;
                    sector[sect].ceilingshade = sector[ns].ceilingshade;
                    break;
                }
            }
        }
        break;
    case 17:
        hittype[i][2] = sector[sect].floorz;
        hittype[i][3] = sector[nextsectorneighborz(sect, sector[sect].floorz, -1, -1)].ceilingz;
        hittype[i][4] = sector[nextsectorneighborz(sect, sector[sect].ceilingz, 1, 1)].floorz;
        break;
    case 20: {
        int best = 0x7FFFFFFF, w0 = -1;

        for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++) {
            int d = ksqrt((sp->y - wall[k].y) * (sp->y - wall[k].y) + (sp->x - wall[k].x) * (sp->x - wall[k].x));

            if (d < best) {
                best = d;
                w0 = k;
            }
        }
        hittype[i][1] = w0;
        best = 0x7FFFFFFF;
        for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++) {
            int d = ksqrt((sp->y - wall[k].y) * (sp->y - wall[k].y) + (sp->x - wall[k].x) * (sp->x - wall[k].x));

            if (d < best && k != hittype[i][1]) {
                best = d;
                w0 = k;
            }
        }
        hittype[i][2] = w0;
        break;
    }
    case 23:
        sp->xrepeat = 64;
        sp->yrepeat = 64;
        sp->shade = 0;
        hittype[i][1] = mspos;
        for (k = 0; k < numsectors; k++) {
            if (sector[k].hitag == sp->hitag) {
                msx[mspos++] = k;
                hittype[i][0]++;
            }
        }
        break;
    case 24:
        sp->yvel *= 2;
        sector[sect].floorstat |= 0x40;
        break;
    case 27:
        if (demomode == 1) {
            sp->xrepeat = 64;
            sp->yrepeat = 64;
        }
        break;
    }
    changespritestat(i, sp->lotag == 7 ? 9 : 3);
}

/* 0x1F160. j < 0 initialises sprite pn in place; otherwise a sprite of tile pn
   is created at sprite j. Returns the sprite. */
short spawn(short j, short pn)
{
    short i, sect;
    spritetype *sp;

    if (j < 0) {
        short k;

        i = pn;
        k = sprite[i].picnum;
        hittype[i][3] = 0;
        hittype[i][2] = 0;
        hittype[i][0] = 0;
        if (k >= 265 && k <= 269)
            k = 265;
        actorinit(i, k);
    } else {
        i = EGS(sprite[j].sectnum, sprite[j].x, sprite[j].y, sprite[j].z, 0, pn, 0, 0, 0, 0, 0, 0,
            sprite[j].picnum, 0);
    }
    sp = &sprite[i];
    sect = sp->sectnum;
    switch (sp->picnum) {
    case 5:
        sp->xrepeat = sp->yrepeat = 0;
        changespritestat(i, 11);
        break;
    case 23: case 31:
        sp->xrepeat = sp->yrepeat = 0;
        if (sp->picnum == 23)
            sector[sect].lotag ^= 0x4000;
        changespritestat(i, 8);
        break;
    case 34:
        changespritestat(i, 7);
        sp->xrepeat = sp->yrepeat = 0;
        break;
    case 30: case 313: case 314:
        sp->cstat |= 1;
        /* fall through */
    case 1122:
        sp->shade = -12;
        markshow2d(i);
        changespritestat(i, 1);
        break;
    case 305:
        hittype[i][0] = sp->xrepeat;
        hittype[i][1] = sp->yrepeat;
        /* fall through */
    case 32: case 51: case 216:
        sp->xrepeat = sp->yrepeat = 0;
        sp->yvel = 0;
        changespritestat(i, 1);
        break;
    case 265: case 266: case 267: case 268: case 269:
        sp->picnum = 265;
        /* fall through */
    case 327:
        markshow2d(i);
        sp->shade = -16;
        changespritestat(i, 1);
        break;
    case 38: case 60: case 70: case 80: case 159: case 263: case 315: case 316: case 381: case 620:
    case 677: case 720: case 721: case 722: case 723: case 724: case 726: case 727: case 728:
    case 1119: case 1120: case 1121: case 1125:
        sp->shade = -17;
        markshow2d(i);
        sp->cstat |= 1;
        changespritestat(i, 2);
        break;
    case 57:
        sp->owner = i;
        sp->shade = -16;
        if (sp->xrepeat <= 8)
            sp->xrepeat = sp->yrepeat = 0;
        markshow2d(i);
        sp->cstat |= 1;
        changespritestat(i, 6);
        break;
    case 175:
        spawneffector(i, sect, j);
        break;
    case 215:
        sp->xrepeat = sp->yrepeat = 10;
        sp->shade = -16;
        changespritestat(i, 5);
        break;
    case 283:
        sp->xrepeat = sp->yrepeat = 0;
        sp->shade = -32;
        changespritestat(i, 10);
        break;
    case 323:
        sp->z = sector[sect].floorz - (sp->yrepeat << 7);
        break;
    case 408: case 490: case 579: case 1536: case 1680: case 1820:
        ps[0].bac++;
        ps[0].bae++;
        /* fall through */
    case 631:
        sp->shade = -24;
        sp->yrepeat = 43;
        markshow2d(i);
        sp->xrepeat = 52;
        if (sp->picnum == 408)
            sp->xrepeat -= 9;
        changespritestat(i, 2);
        sp->cstat |= 1;
        break;
    case 606: case 627: case 905: case 1200:
        sp->owner = i;
        sp->shade = -16;
        markshow2d(i);
        sp->yrepeat = sp->xrepeat = 38;
        sp->cstat |= 1;
        changespritestat(i, 6);
        break;
    case 686: case 1360: case 1370:
        if (sp->picnum == 1360 && j >= 0 && sprite[j].statnum == 10)
            sp->z += 4096;
        sp->xrepeat = sp->yrepeat = sp->picnum == 1360 ? 8 : 28;
        sp->cstat = (unsigned char)(krand() & 0xC);
        sp->shade = -127;
        if (j >= 0)
            sp->ang = sprite[j].ang;
        changespritestat(i, 5);
        break;
    case 930:
        markshow2d(i);
        sp->xrepeat = sp->yrepeat = 32;
        hittype[i][3] = sector[sect].floorz;
        sp->z = hittype[i][3] + 3072;
        sp->cstat |= 1;
        ps[0].bae++;
        changespritestat(i, 2);
        break;
    case 931:
        markshow2d(i);
        sp->xrepeat = sp->yrepeat = 32;
        hittype[i][3] = sector[sect].ceilingz;
        sp->z = hittype[i][3] + 10752;
        sp->cstat |= 9;
        ps[0].bae++;
        changespritestat(i, 2);
        break;
    case 940:
        markshow2d(i);
        sp->owner = i;
        sp->xrepeat = sp->yrepeat = 24;
        sp->cstat |= 1;
        ps[0].bae++;
        changespritestat(i, 4);
        break;
    case 960:
        sp->xrepeat = 16;
        sp->yrepeat = 0;
        hittype[i][1] = krand() & 0x1F;
        sp->z = sector[sect].ceilingz + 2048;
        changespritestat(i, 2);
        break;
    case 962:
        sp->z = sector[sect].ceilingz + 6144;
        hittype[i][0] = 3;
        /* fall through */
    case 961:
        markshow2d(i);
        sp->shade = -16;
        sp->xrepeat = 32;
        sp->yrepeat = 16;
        sp->cstat |= 1;
        changespritestat(i, 2);
        break;
    case 1165:
        if (j >= 0) {
            short a;

            if (sprite[j].picnum == 283) {
                short p = sprite[j].yvel;

                a = (short)(ps[p].ang - (krand() & 0x3F));
                hittype[i][0] = krand() & 1;
                sp->x = sprite[j].x + (sintable()[(a + 512) & 2047] >> 6);
                sp->y = (sintable()[a & 2047] >> 6) + sprite[j].y;
                sp->z = sprite[j].z - 32 * ps[p].horiz + 4608;
                sp->zvel = (short)-(krand() & 0xFF);
            } else {
                a = sp->ang;
                sp->x = sprite[j].x + (sintable()[(a + 512) & 2047] >> 6);
                sp->y = (sintable()[a & 2047] >> 6) + sprite[j].y;
                sp->z = sprite[j].z - 9216;
            }
            sp->shade = -8;
            sp->ang = (short)(a - 512);
            sp->zvel = -192;
            sp->xvel = 48;
            sp->xrepeat = sp->yrepeat = 5;
            changespritestat(i, 5);
        }
        break;
    case 1175: {
        short k;

        markshow2d(i);
        sp->cstat |= 0x40;
        sp->picnum += 2;
        sp->z = sector[sect].ceilingz + 12288;
        hittype[i][4] = mspos;
        msx[mspos] = sp->x;
        msy[mspos] = sp->y;
        msx[mspos + 2] = sp->z;
        for (k = headspritestat[0]; k >= 0; k = nextspritestat[k]) {
            if (sprite[k].picnum == 1174 && sprite[k].hitag == sp->hitag) {
                hittype[i][1] = sprite[k].sectnum;
                sprite[k].xrepeat = 48;
                sprite[k].yrepeat = 128;
                msx[mspos + 1] = sprite[k].x;
                msy[mspos + 1] = sprite[k].y;
                sprite[k].x = sp->x;
                sprite[k].y = sp->y;
                sprite[k].z = sp->z;
                sprite[k].shade = sp->shade;
                msy[mspos + 2] = k;
                setsprite(k, sprite[k].x, sprite[k].y, sprite[k].z);
                break;
            }
        }
        sp->owner = -1;
        mspos += 3;
        changespritestat(i, 6);
        break;
    }
    case 1190:
        sp->xrepeat = sp->yrepeat = 20;
        sp->shade = -16;
        changespritestat(i, 5);
        break;
    case 1280:
        sp->xrepeat = sp->yrepeat = 64;
        sp->shade = -127;
        sp->cstat |= 1;
        changespritestat(i, 2);
        break;
    case 1359:
        sp->xrepeat = sp->yrepeat = 32;
        sp->cstat |= 0x22;
        sp->ang = (short)(krand() & 2047);
        sp->shade = 0;
        sp->z = sector[sect].floorz;
        changespritestat(i, 5);
        break;
    case 1404:
        if (j >= 0) {
            sp->owner = j;
            sp->xrepeat = (unsigned char)(tilesizx[sprite[j].picnum] / 3);
            sp->yrepeat = (unsigned char)(tilesizy[sprite[j].picnum] / 3);
            sp->shade = -127;
        }
        changespritestat(i, 5);
        break;
    case 1406:
        if (j >= 0) {
            sp->xrepeat = sp->yrepeat = 16;
            sp->shade = -15;
            sp->cstat |= wrand() & 0xE;
            sp->ang = sprite[j].ang;
            sp->xvel = sprite[j].xvel;
            sp->zvel = sprite[j].zvel;
            sp->owner = j;
            sp->z -= 0x2000;
            changespritestat(i, 4);
        }
        break;
    case 1408:
        sp->owner = -1;
        changespritestat(i, 1);
        break;
    }
    return i;
}

/* 0x3E700. The turn from a toward na, taking the short way round. */
int getincangle(int a, int na, int shift)
{
    na &= 2047;
    a &= 2047;
    if (abs(a - na) >= 1024)
        return (((a + 1024) & 2047) - na) >> shift;
    return (na - a) >> shift;
}

/* 0x35600. Moves sprite i along its ang, xvel and zvel; 1 when nothing was hit. */
int ssp(short i, int cliptype)
{
    spritetype *s = &sprite[i];

    return movesprite(i, (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14,
               (s->xvel * sintable()[s->ang & 2047]) >> 14, s->zvel, 128, 1024, 1024, cliptype, synctics)
        == 0;
}

/* 0x35480. Every actor (stat 1, 6, 10) within r turns away from i and gets
   a damage sprite of the given strength. */
void hitradius(short i, int r, int damage)
{
    static const unsigned char stats[3] = { 1, 6, 10 };
    int k;
    short j;

    for (k = 0; k < 3; k++)
        for (j = headspritestat[stats[k]]; j >= 0; j = nextspritestat[j]) {
            unsigned dx = (unsigned)(sprite[i].x - sprite[j].x), dy = (unsigned)(sprite[i].y - sprite[j].y);

            if ((int)(dy * dy + dx * dx) >= r * r)
                continue;
            sprite[j].ang = (short)getangle(sprite[j].x - sprite[i].x, sprite[j].y - sprite[i].y);
            if (sprite[j].extra >= 0) {
                short d = EGS(sprite[j].sectnum, sprite[j].x, sprite[j].y, sprite[j].z, 0, 46, -16, 0, 0,
                    sprite[j].ang, 32, 0, sprite[j].owner, 4);
                sprite[d].extra = (short)damage;
            }
        }
}

/* 0x35690 */
void nuke(int x, int y, int z, short i)
{
    EGS(sprite[i].sectnum, x, y, z + 5120, 2, 174, -127, 96, 48, 0, 0, 0, i, 5);
    spawn(i, 1370);
}

/* 0x35A60. A tagged sector closed below 7168 units squashes i; -1 when it
   does not. */
short ifsquished(short i, short p)
{
    sectortype *sc = &sector[sprite[i].sectnum];
    short pic;

    if (sc->floorz - sc->ceilingz >= 7168 || (sc->lotag & 0x8000) || (sc->lotag & 0x7FFF) == 0)
        return -1;
    if ((sc->lotag & 0x7FFF) != 23)
        sc->ceilingz -= 1024;
    sprite[i].z = sc->floorz;
    pic = sprite[i].picnum;
    if (pic == 408 || pic == 490 || pic == 631 || pic == 1536 || pic == 1680 || pic == 1820) {
        sprite[i].xvel = 0;
        sprite[i].cstat &= 0xFE;
        ps[p].bad++;
        quote(10);
    } else if (pic == 579) {
        sprite[i].xvel = 0;
        ps[p].bad++;
        quote(10);
    }
    return i;
}

/* 0x35C30. The first projectile (stat 4) touching i that i did not fire.
   Anything but an effector takes its damage, speed and heading. */
short ifhitweapon(short i)
{
    short j;

    for (j = headspritestat[4]; j >= 0; j = nextspritestat[j])
        if (j != i && i != sprite[j].owner
            && abs(sprite[i].x - sprite[j].x) + abs(sprite[i].y - sprite[j].y) < 768
            && abs((sprite[i].z - sprite[j].z) >> 8) <= 48)
            break;
    if (j < 0)
        return -1;
    if (sprite[i].statnum != 3) {
        sprite[i].extra -= sprite[j].extra;
        sprite[i].xvel = sprite[j].xvel;
        sprite[i].ang = sprite[j].ang;
        if ((sprite[i].statnum == 1 || sprite[i].statnum == 10) && sprite[j].picnum == 389)
            spawn(i, 1404);
    }
    return j;
}

/* 0x35D50 */
void guts(short i)
{
    spritetype *s = &sprite[i];
    int z = s->z - 9216;

    EGS(s->sectnum, s->x, s->y, z, 0, 870, -16, 24, 24, s->ang + 1536, 64, -623, i, 5);
    EGS(s->sectnum, s->x, s->y, z, 0, 870, -16, 24, 24, s->ang - 512, 48, -447, i, 5);
    EGS(s->sectnum, s->x, s->y, z + 2048, 0, 878, -16, 24, 24, s->ang + 512, 32, -508, i, 5);
    EGS(s->sectnum, s->x, s->y, z + 2048, 0, 878, -16, 24, 24, s->ang - 512, 24, -768, i, 5);
    EGS(s->sectnum, s->x, s->y, z, 0, 874, -16, 24, 24, s->ang - 1024, 32, -768, i, 5);
}

/* 0x358C0. A sleeping actor (stat 2) that can see its nearest player wakes
   up (stat 1), with its sight quote and sound. */
void wakeactors(void)
{
    short i, nexti;
    int dist;

    for (i = headspritestat[2]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        short p;

        nexti = nextspritestat[i];
        p = findplayer(i, &dist);
        if (!cansee(s->x, s->y, s->z - (s->yrepeat << 7), s->sectnum, ps[p].posx, ps[p].posy,
                ps[p].posz - 4096, ps[p].cursectnum))
            continue;
        switch (s->picnum) {
        case 60:
            quote(6);
            break;
        case 408:
            spritesound(21, i);
            quote(0);
            break;
        case 490:
        case 1680:
            spritesound(20, i);
            quote(0);
            break;
        case 38:
        case 1125:
            quote(1);
            break;
        case 579:
        case 1536:
            spritesound(22, i);
            quote(3);
            break;
        case 620:
            quote(2);
            break;
        }
        changespritestat(i, 1);
    }
}

/* 0x3A470. Player sprites (stat 10). A live one follows its player, or the
   player's last spot (and parallax goes to 2) while w56 is set; an ownerless
   one is a falling, flickering copy that drifts for 8 tics, then stays. */
void moveplayers(void)
{
    short i, nexti;

    for (i = headspritestat[10]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        playertype *p = &ps[s->yvel];

        nexti = nextspritestat[i];
        if (s->owner >= 0) {
            if (p->w56 < 0) {
                execute(i, s->yvel, 0);
                p = &ps[s->yvel];
                p->bab = 0;
                s->x = p->posx;
                s->y = p->posy;
                s->z = p->posz + 9216;
                s->ang = p->ang;
            } else {
                parallaxtype = 2;
                s->x = p->f1a;
                s->y = p->f1e;
                s->z = p->f22 + 9216;
                s->ang = p->f42;
            }
            setsprite(i, s->x, s->y, s->z);
            continue;
        }
        if (p->w58 == 0) {
            deletesprite(i);
            continue;
        }
        if (s->z >= sector[s->sectnum].floorz) {
            s->zvel = 0;
            s->z = sector[s->sectnum].floorz;
        } else {
            if (s->zvel < 2048)
                s->zvel += 256;
            s->z += s->zvel;
        }
        if ((krand() >> 8) < 239) {
            s->shade = 4;
            s->cstat &= 0xFD;
        } else {
            s->shade = 0;
            s->cstat |= 2;
        }
        if (s->yvel >= 8) {
            s->ang = 2047 - ps[s->yvel].ang;
            setsprite(i, s->x, s->y, s->z);
            continue;
        }
        s->ang = ps[s->yvel].ang;
        s->yvel++;
        s->xvel = 128;
        movesprite(i, (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14,
            (s->xvel * sintable()[s->ang & 2047]) >> 14, s->zvel, 128, 1024, 1024, 0, synctics);
    }
}

/* 0x2F2E0. A hit strong enough to matter tips player p's view toward or
   away from projectile i and kicks the look angle sideways. */
static void kickview(short p, short i)
{
    int a;

    if (sprite[ps[p].i].extra / 20 < ps[p].w54)
        return;
    a = getincangle(sprite[i].ang, getangle(sprite[i].x - ps[p].posx, sprite[i].y - ps[p].posy), 0);
    if (a >= -512 && a < 512)
        ps[p].horiz += 4;
    else
        ps[p].horiz -= 4;
    ps[p].lookang = (short)(a >= 0 ? 2 * sprite[i].extra : -2 * sprite[i].extra);
}

/* Projectile i's damage to player p: armor soaks 4 a hit unless /d is on. */
static void hurtplayer(short p, short i)
{
    if (debugmode || ps[p].b96 == 0)
        ps[p].w54 = ps[p].w54 <= 0 ? 0 : (short)(ps[p].w54 - sprite[i].extra);
    else
        ps[p].b96 -= 4;
}

static int nearplayer(short i, short p, int dist)
{
    return dist < 768 && sprite[i].owner != ps[p].i && abs((ps[p].posz - sprite[i].z) >> 8) <= 48;
}

/* 0x38F30. Projectiles (stat 4). */
void moveweapons(void)
{
    short i, nexti;

    for (i = headspritestat[4]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        int dist;
        short p;

        nexti = nextspritestat[i];
        p = findplayer(i, &dist);
        switch (s->picnum) {
        case 46:
            if (s->shade != -15) {
                s->shade = -15;
                break;
            }
            shoot(i, 1230, 1);
            if ((short)dist < 512 && s->owner != ps[p].i) {
                kickview(p, i);
                hurtplayer(p, i);
            }
            break;
        case 563:
            if (s->extra < 0)
                break;
            if (nearplayer(i, p, (short)dist)) {
                spritesound(13, ps[p].i);
                kickview(p, i);
                hurtplayer(p, i);
                s->extra = -1;
            }
            if (!ssp(i, 1)) {
                shoot(i, 563, 1);
                s->extra = -1;
            }
            break;
        case 389:
            if (s->extra >= 0) {
                if (nearplayer(i, p, (short)dist)) {
                    hurtplayer(p, i);
                    kickview(p, i);
                    spritesound(13, ps[p].i);
                    s->extra = -1;
                    continue;
                }
                if (!ssp(i, 1)) {
                    shoot(i, 389, 1);
                    s->extra = -1;
                }
            }
            break;
        case 191:
        case 1210:
            if ((short)dist < 512)
                kickview(p, i);
            if (s->shade != -15) {
                s->shade = -15;
                break;
            }
            shoot(i, s->picnum, 1);
            if (s->picnum == 1210 && s->yvel >= 0)
                spritesound(30, i);
            break;
        case 940:
        case 1406:
            break;
        default:
            continue;
        }
        execute(i, p, (short)dist);
    }
}

/* 0x3A860. Music and ambience sprites (stat 11). Below lotag 1000 a sprite
   cycles t0 through: start the level's (or its lotag's) song, wait until
   the local player comes within hitag, fade out, wait until the player
   leaves; at 1000 and up it loops sound lotag-1000 while the player is near. */
void movemusic(void)
{
    spritetype *me = &sprite[ps[myconnectindex].i];
    short i;

    for (i = headspritestat[11]; i >= 0; i = nextspritestat[i]) {
        spritetype *s = &sprite[i];
        int *t = hittype[i], d = (abs(me->x - s->x) + abs(me->y - s->y)) >> 3;

        if (s->lotag > 999) {
            if (d >= s->hitag) {
                if (t[0]) {
                    stopsound(s->lotag - 1000);
                    t[0] = 0;
                }
            } else if (t[0] == 0) {
                spritesound(s->lotag - 1000, i);
                t[0] = 1;
            }
            continue;
        }
        if (t[1] > 0) {
            t[1] -= 12;
            if (t[1] < 0) {
                if (++t[0] > 3)
                    t[0] = 0;
                t[1] = 0;
            }
            MUSIC_SetVolume(t[1]);
            continue;
        }
        switch (t[0]) {
        case 0:
        case 2: {
            playertype *p = &ps[myconnectindex];
            /* exe: 0x125308 + 13*lotag, or 0x8AC70 + 130*volume + 13*level, which
               is the next volume's row; ps.musicptr keeps the offset into musicfn. */
            int off = t[0] ? 13 * s->lotag : 130 * (p->volume + 1) + 13 * p->level;

            t[0]++;
            if (off != p->musicptr && off >= 0 && off < (int)sizeof musicfn) {
                p->musicptr = off;
                playmusic(&musicfn[0][0][0] + off);
                MUSIC_SetVolume(music_volume);
            }
            break;
        }
        case 1:
            if (d < s->hitag)
                t[1] = music_volume;
            break;
        case 3:
            if (d >= s->hitag)
                t[1] = music_volume;
            break;
        }
    }
}
