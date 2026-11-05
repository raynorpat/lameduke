/* Sprite creation: EGS 0x12E90 and spawn 0x1F160. */

#include "actors.h"
#include "demo.h"
#include "global.h"
#include "gamedef.h"
#include "sector.h"
#include "sounds.h"
#include "game.h"
#include "draw.h"
#include "music.h"
#include "view.h"

#include "board.h"
#include "buildmath.h"
#include "tiles.h"

#include <stdlib.h>
#include <string.h>

int hittype[MAXSPRITES][5];
int msx[1024], msy[1024];                          /* 0x122830, 0x123830 */
int mspos;                                         /* dword_8AC60 */
short randomstash;

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
    bposx[i] = x;
    bposy[i] = y;
    bposz[i] = z;
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
    case 30: case 313: case 314: case 1123:
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

/* Damage to player p: armor soaks 4 a hit unless /d is on. */
static void hurt(short p, int amount)
{
    if (debugmode || ps[p].b96 == 0)
        ps[p].w54 = ps[p].w54 <= 0 ? 0 : (short)(ps[p].w54 - amount);
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
                hurt(p, sprite[i].extra);
            }
            break;
        case 563:
            if (s->extra < 0)
                break;
            if (nearplayer(i, p, (short)dist)) {
                spritesound(13, ps[p].i);
                kickview(p, i);
                hurt(p, sprite[i].extra);
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
                    hurt(p, sprite[i].extra);
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

static int movealong(short i, int zchange, int cliptype)
{
    spritetype *s = &sprite[i];

    return movesprite(i, (sintable()[(s->ang + 512) & 2047] * s->xvel) >> 14,
        (s->xvel * sintable()[s->ang & 2047]) >> 14, zchange, 128, 1024, 1024, cliptype, synctics);
}

static void fallto(spritetype *s)
{
    if (s->z >= sector[s->sectnum].floorz) {
        s->zvel = 0;
        s->z = sector[s->sectnum].floorz;
    } else {
        if (s->zvel < 2048)
            s->zvel += 256;
        s->z += s->zvel;
    }
}

/* One tic of a tumbling gib of the four-frame run first..first+3: it drips
   blood (whose heading is t0 << angshift, shl 0x40 being a no-op shift on
   the 386) and lands as frame first+1 once it has bounced twice within
   landdist of the floor. */
static void gibstep(short i, short sect, short first, int zadd, int angshift, int landdist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i];

    t[0]++;
    if (t[0] & 1)
        s->picnum++;
    if (s->picnum == first + 4)
        s->picnum = first;
    s->xvel = s->xvel <= 0 ? 0 : (short)(s->xvel - 1);
    s->zvel = (short)(s->zvel + zadd);
    if (movealong(i, s->zvel, 0)) {
        t[1]++;
        s->zvel = (short)-s->zvel;
    }
    if ((krand() >> 8) >= 239) {
        int r1 = krand() & 0x20, zv = -(krand() & 511), xv = r1 + (krand() & 31);

        EGS(s->sectnum, s->x, s->y, s->z, 0, 488, -16, 24, 13, (short)(((short)t[0] << angshift) + (krand() & 127)),
            (short)xv, (short)zv, i, 5);
    }
    if (s->z + landdist > sector[sect].floorz && t[1] > 1) {
        s->picnum = first + 1;
        s->z = sector[sect].floorz - 512;
        t[2] = 1;
        s->cstat |= krand() & 4;
    }
}

/* A landed gib waits 256 tics, then goes once no player is near. */
static int gibrest(short i, int dist)
{
    int *t = hittype[i];

    if (t[4] < 256)
        t[4]++;
    else if (dist > 4096) {
        deletesprite(i);
        return 1;
    }
    return 0;
}

/* 0x394E0. Effects (stat 5): casings, gibs, debris, bubbles and the
   explosion that throws them. */
void moveeffects(void)
{
    short i, nexti;

    for (i = headspritestat[5]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        int *t = hittype[i], dist;
        short sect = s->sectnum, p;

        nexti = nextspritestat[i];
        p = findplayer(i, &dist);
        switch (s->picnum) {
        case 215:
            s->ang = krand() & 2047;
            s->xvel = (short)(16 - (krand() & 31));
            /* fall through */
        case 214:
        case 488:
        case 686:
        case 1359:
        case 1360:
        case 1370:
        case 1404:
            execute(i, p, dist);
            continue;
        }
        if (s->picnum == 1165) {
            if (movealong(i, s->zvel, 0) == 0) {
                if (s->zvel < 512)
                    s->zvel += 64;
                if (s->xvel > 0)
                    s->xvel = s->xvel - 4 < 0 ? 0 : (short)(s->xvel - 4);
                else if (s->xvel < 0)
                    s->xvel = s->xvel + 4 > 0 ? 0 : (short)(s->xvel + 4);
            } else if (t[1]) {
                deletesprite(i);
            } else {
                s->xvel = (short)-s->xvel;
                s->zvel = (short)-s->zvel;
                t[1] = 1;
            }
            continue;
        }
        if (s->picnum == 1190) {
            if ((sector[sect].lotag & 0x7FFF) != 1 || (t[0] += 128) > 2047) {
                deletesprite(i);
                continue;
            }
            s->cstat ^= 4;
            s->xrepeat = s->yrepeat = (unsigned char)((sintable()[t[0]] >> 11) + 8);
        }
        if (s->picnum >= 970 && s->picnum <= 973) {
            fallto(s);
            movealong(i, s->zvel, 0);
            if (abs(s->z - sector[sect].floorz) < 2048) {
                s->zvel = -1024;
                s->xrepeat >>= 1;
                s->yrepeat >>= 1;
                t[0]++;
            }
            if (t[0] > 2) {
                deletesprite(i);
                continue;
            }
        }
        if (s->picnum >= 1201 && s->picnum <= 1205) {
            fallto(s);
            if (s->xvel <= 0) {
                s->xvel = 0;
                if ((krand() >> 8) >= 127)
                    s->cstat ^= 0x80;
            } else {
                if (s->zvel > 512) {
                    if (s->picnum == 1204)
                        s->picnum = 1202;
                    if (s->picnum == 1205)
                        s->picnum = 1203;
                }
                if (s->picnum == 1202 || s->picnum == 1203)
                    s->cstat ^= 4;
                s->xvel -= 6;
                if ((krand() >> 8) >= 191)
                    spawn(i, 686);
            }
            if (movealong(i, s->zvel, 0) && sector[sect].floorz - 1024 < s->z) {
                s->zvel = (short)-s->zvel;
                if (++t[0] == 3) {
                    s->z = sector[sect].floorz - 512;
                    changespritestat(i, 0);
                    continue;
                }
            }
        }
        if (s->picnum >= 870 && s->picnum <= 873) {
            if (t[2]) {
                gibrest(i, dist);
                continue;
            }
            gibstep(i, sect, 870, 192, 0, 2048);
        }
        if (s->picnum >= 874 && s->picnum <= 877) {
            if (t[2]) {
                gibrest(i, dist);
                continue;
            }
            gibstep(i, sect, 874, 128, 7, 2048);
        }
        if (s->picnum >= 878 && s->picnum <= 881) {
            if (t[2]) {
                gibrest(i, dist);
                continue;
            }
            gibstep(i, sect, 878, 192, 7, 4096);
        }
        if (s->picnum == 174) {
            int a;

            if (t[0] == 0)
                t[1] = (krand() & 3) + 1;
            t[0]++;
            s->xrepeat = s->xrepeat == 96 ? 191 : (unsigned char)(s->xrepeat - 48);
            s->yrepeat -= 3;
            if (t[0] == t[1])
                for (a = 0; a < 2048; a += 768) {
                    s->ang = (short)((unsigned char)randomstash + a);
                    spawn(i, 1370);
                }
            if (t[0] > 3)
                deletesprite(i);
            continue;
        }
        if (s->picnum >= 235 && s->picnum <= 246) {
            short old = s->picnum;

            s->picnum = old == 238 ? 235 : old == 242 ? 239 : old == 246 ? 243 : (short)(old + 1);
            if (abs(sector[sect].ceilingz - sector[sect].floorz) < 7680)
                s->xvel = 64;
            if (movealong(i, s->zvel, 1) == 0 && s->z < sector[sect].floorz) {
                if ((s->zvel >> 2) < 1024)
                    s->zvel += 128;
                s->z += s->zvel;
                if (s->xvel > 16)
                    s->xvel -= 4;
            } else if (++t[0] != 2) {
                s->zvel = (short)-s->zvel;
            } else {
                deletesprite(i);
            }
        }
    }
}

/* 0x24FB0. The first player standing in sector sect, or -1. */
static short playerinsect(short sect)
{
    short p;

    for (p = connecthead; p >= 0; p = connectpoint2[p])
        if (ps[p].cursectnum == sect)
            return p;
    return -1;
}

static short findinsect(short sect, short pic)
{
    short j;

    for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j])
        if (sprite[j].picnum == pic)
            return j;
    return -1;
}

/* The crane (1175..1178). t0 is its step, t1 the sector it fetches from,
   t4 its three msx/msy slots: home x,y; pickup x,y; rope height and the
   rope sprite. What it holds (owner, or -2 for a player) rides below it. */
static void movecrane(short i)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i], d;
    short sect = s->sectnum, p, j;

    s->xrepeat = s->yrepeat = 64;
    switch (t[0]) {
    case 0:
        if (playerinsect((short)t[1]) < 0) {
            j = findinsect((short)t[1], 627);
            if (j >= 0) {
                s->ang = (short)getangle(msx[t[4] + 1] - s->x, msy[t[4] + 1] - s->y);
                setsprite(j, msx[t[4] + 1], msy[t[4] + 1], sprite[j].z);
                t[0]++;
                return;
            }
        } else {
            s->ang = (short)getangle(msx[t[4] + 1] - s->x, msy[t[4] + 1] - s->y);
            t[0]++;
        }
        break;
    case 1:
        if (s->xvel < 184) {
            s->picnum = 1176;
            s->xvel += 8;
        }
        movealong(i, s->zvel, 0);
        if (sect == t[1])
            t[0]++;
        break;
    case 2:
    case 7:
        s->z += 1536;
        if (t[0] == 2) {
            if (sector[sect].floorz - s->z < 12288 && s->picnum > 1175)
                s->picnum--;
            if (sector[sect].floorz - s->z < 5120)
                t[0]++;
        }
        if (t[0] == 7 && sector[sect].floorz - s->z < 5120) {
            if (s->picnum > 1175)
                s->picnum--;
            else {
                if (s->owner == -2) {
                    p = findplayer(i, &d);
                    if (ps[p].w82 == i)
                        ps[p].w82 = -1;
                }
                t[0]++;
                s->owner = -1;
            }
        }
        break;
    case 3:
        if (++s->picnum != 1177)
            break;
        p = playerinsect((short)t[1]);
        if (p >= 0 && ps[p].b8f) {
            s->owner = -2;
            ps[p].ang = (short)getangle(s->x - ps[p].posx, s->y - ps[p].posy);
            ps[p].w82 = i;
        } else {
            j = findinsect((short)t[1], 627);
            if (j >= 0)
                s->owner = j;
        }
        t[2] = 0;
        t[0]++;
        return;
    case 4:
        if (++t[2] > 10)
            t[0]++;
        break;
    case 5:
    case 8:
        if (t[0] == 8 && s->picnum < 1177 && sector[sect].floorz - s->z > 0x2000)
            s->picnum++;
        if (s->z >= msx[t[4] + 2])
            s->z -= 1536;
        else {
            t[0]++;
            s->xvel = 0;
        }
        break;
    case 6:
        if (s->xvel < 192)
            s->xvel += 8;
        s->ang = (short)getangle(msx[t[4]] - s->x, msy[t[4]] - s->y);
        movealong(i, s->zvel, 0);
        if ((s->y - msy[t[4]]) * (s->y - msy[t[4]]) + (s->x - msx[t[4]]) * (s->x - msx[t[4]]) < 0x4000)
            t[0]++;
        break;
    case 9:
        t[0] = 0;
        break;
    }
    setsprite((short)msy[t[4] + 2], s->x, s->y, s->z - 8704);
    if (s->owner == -1)
        return;
    p = findplayer(i, &d);
    if (ifhitweapon(i) >= 0) {
        if (s->owner == -2 && ps[p].w82 == i)
            ps[p].w82 = -1;
        s->owner = -1;
        s->picnum = 1175;
    }
    if (s->owner == -2) {
        playertype *pl = &ps[p];

        if (pl->w82 == i && p == myconnectindex)
            s->xrepeat = s->yrepeat = 0;
        pl->posx = s->x - (sintable()[(pl->ang + 512) & 2047] >> 6);
        pl->posy = s->y - (sintable()[pl->ang & 2047] >> 6);
        pl->posz = s->z - 7168;
        clipmove(&pl->posx, &pl->posy, &pl->posz, &pl->cursectnum, 0, 0, 192, 1024, 1024, 0);
        setsprite(pl->i, pl->posx, pl->posy, pl->posz);
    } else if (s->owner >= 0) {
        setsprite(s->owner, s->x, s->y, s->z);
    }
}

/* 0x3AB90. Standing things (stat 6). */
void movestandables(void)
{
    short i, nexti, j;

    for (i = headspritestat[6]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        int d;
        short sect = s->sectnum, p;

        nexti = nextspritestat[i];
        if (s->picnum == 57) {
            /* A force field post; one that dies sets every post of its hitag to
               -32, and each of those then bursts. */
            if (s->shade == -32) {
                spawn(i, 1370);
                s->z -= 0x2000;
                shoot(i, 1210, 1);
                s->z += 0x2000;
                spawn(i, 1370);
                spawn(i, 1370);
                deletesprite(i);
            } else if ((ifhitweapon(i) >= 0 && s->extra < 0) || s->shade == -31) {
                for (j = headspritestat[6]; j >= 0; j = nextspritestat[j])
                    if (s->hitag == sprite[j].hitag && sprite[j].picnum == 57)
                        sprite[j].shade = -32;
            }
        } else if (s->picnum >= 1175 && s->picnum <= 1178) {
            movecrane(i);
        } else if (s->picnum == 1200) {
            fallto(s);
            j = ifhitweapon(i);
            if (j >= 0) {
                if (s->lotag)
                    EGS(sect, s->x, s->y, s->z - 4096, 1, s->lotag, -16, 32, 32, 0, 0, -1536, sprite[j].owner, 1);
                EGS(sect, s->x, s->y, s->z - 4096, 1, 1201, s->shade, 32, 32, sprite[j].ang, 128, -1024,
                    sprite[j].owner, 5);
                EGS(sect, s->x, s->y, s->z, 1, 1204, s->shade, 28, 32, sprite[j].ang - 512, 128, -1024,
                    sprite[j].owner, 5);
                EGS(sect, s->x, s->y, s->z, 1, 1205, s->shade, 28, 32, sprite[j].ang + 512, 128, -1024,
                    sprite[j].owner, 5);
                deletesprite(i);
            }
        } else {
            p = findplayer(i, &d);
            if (s->picnum == 627 || s->picnum == 905 || s->picnum == 974)
                execute(i, p, d);
        }
    }
}

short viewscreen = -1; /* word_12538A: the 1408 the local player last stood near */

/* 0x29540. Arms every countdown activator (51) of this lotag. */
static void armactivators(short lotag)
{
    short i;

    for (i = headspritestat[1]; i >= 0; i = nextspritestat[i])
        if (sprite[i].picnum == 51 && sprite[i].lotag == lotag && sprite[i].yvel == 0)
            sprite[i].yvel = 1;
}

/* The flying drone (1280): t1 counts tics, t2 is its mood (0 wander, 1
   chase, 2 cling to the player's face). */
static void movedrone(short i, short p, int dist)
{
    static const unsigned char frames[4] = { 0, 1, 0, 3 };
    spritetype *s = &sprite[i];
    int *t = hittype[i];
    playertype *pl = &ps[p];
    short turn;

    if (movealong(i, s->zvel, 0))
        s->ang += 1024;
    t[1]++;
    turn = ifhitweapon(i);
    if (turn >= 0) {
        if (s->extra < 0) {
            ps[myconnectindex].f36 += 5000;
            spawn(i, 1370);
            deletesprite(i);
            return;
        }
        t[2] = -1;
        s->ang += 1024;
    }
    switch (t[2]) {
    case -1:
    case 0:
        if (t[2] == -1)
            t[2] = 0;
        else if ((krand() >> 8) >= 251)
            t[2] = 1;
        /* turn keeps the hit's sprite index when the drone homes instead */
        if ((krand() >> 8) < 247) {
            turn = (short)(sintable()[(16 * t[1]) & 2047] >> 8);
            s->ang += turn;
        } else
            s->ang += getincangle(s->ang, getangle(pl->posx - s->x, pl->posy - s->y), 2);
        if (turn >= -32 && turn <= 32) {
            t[0] = frames[t[1] & 3];
            if (s->xvel < 112)
                s->xvel += 8;
        } else {
            t[0] = turn >= 0 ? 2 : 4;
            if (s->xvel > 64)
                s->xvel -= 8;
        }
        break;
    case 1:
        if (pl->posz + 768 < s->z)
            s->z -= 1024;
        else if (pl->posz - 768 > s->z)
            s->z += 1024;
        s->ang += getincangle(s->ang, getangle(pl->posx - s->x, pl->posy - s->y), 3);
        if (s->xvel >= 144)
            t[0] = 0;
        else {
            t[0] = frames[t[1] & 3];
            s->xvel += 16;
        }
        if (dist < 768) {
            t[2] = (krand() >> 8) < 247 ? 0 : 2;
            s->ang += 1024;
        }
        break;
    case 2:
        s->x = (sintable()[(pl->ang + 512) & 2047] >> 6) + pl->posx;
        s->y = pl->posy + (sintable()[pl->ang & 2047] >> 6);
        s->z = pl->posz + 3072;
        t[0] = frames[t[1] & 3];
        pl->ang += sintable()[((unsigned short)t[1] << 8) & 2047] >> 12;
        s->ang = pl->ang + 1024;
        if ((krand() >> 8) >= 223)
            t[2] = 0;
        break;
    }
}

/* The flickering light (305..308): t0/t1 hold its size while it is dark. */
static void movelight(short i, short sect, int dist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i];

    if (dist > 0x2000)
        return;
    if (t[3] == 0)
        t[3] = sector[sect].floorshade;
    for (;;) {
        if (t[2]) {
            t[2]--;
            sector[sect].floorshade = sector[sect].ceilingshade = 20;
            return;
        }
        if ((s->yrepeat | s->xrepeat) == 0) {
            s->xrepeat = (unsigned char)t[0];
            s->yrepeat = (unsigned char)t[1];
            break;
        }
        if (krand() & 8)
            break;
        t[0] = s->xrepeat;
        t[1] = s->yrepeat;
        t[2] = randomstash & 4;
        s->xrepeat = s->yrepeat = 0;
    }
    s->xrepeat = (unsigned char)((randomstash & 7) + 8);
    s->picnum++;
    if (randomstash & 1)
        s->cstat ^= 2;
    if (dist < 512)
        spritesound(24, i);
    if (s->picnum == 309)
        s->picnum = 305;
    sector[sect].floorshade = sector[sect].ceilingshade = (s->picnum & 1) ? 0 : 20;
}

/* The reactor (620..625). t1 counts its meltdown once it is shot dead;
   at 23 it is gone and pays 100000. */
static void movereactor(short i, short sect, int dist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i], k, n;

    s->picnum = (short)(((krand() >> 8) < 63 ? 623 : 620) + (short)t[2]);
    if (++t[2] == 3)
        t[2] = 0;
    if (dist >= 4096) {
        t[3] = 0;
        stopsound(26);
    } else {
        t[0] += 128;
        overlaypal(246, 18, 611, (sintable()[t[0] & 2047] >> 11) + 8, 1);
        if (t[3] == 0) {
            spritesound(26, i);
            t[3] = 1;
        }
    }
    if (t[1]) {
        n = t[1]++;
        if (n == 23) {
            ps[myconnectindex].f36 += 100000;
            stopsound(26);
            deletesprite(i);
            return;
        }
        s->ang += 300;
        s->z -= 6144;
        shoot(i, 0, 1);
        s->z += 6144;
        n = t[1];
        if (n == 2)
            for (k = headspritestat[3]; k >= 0; k = nextspritestat[k])
                if (sprite[k].lotag == 3)
                    hittype[k][4] = 1;
        if (n == 2 || n == 4 || n == 7 || n == 10 || n == 15) {
            EGS(s->sectnum, s->x, s->y, s->z - (krand() & 0x3FFF) + 5120, 2, 174, -127, 96, 48, 0, 0, 0, i, 5);
            spawn(i, 1370);
            for (k = 0; k < 16; k += 2) {
                short zv = (short)-(krand() & 1023);

                EGS(sect, s->x, s->y, s->z + (k << 9), 0, 235, -8, 32, 32, s->ang, (short)(krand() + 128), zv, i, 5);
            }
        }
    }
    if (t[1] == 0 && ifhitweapon(i) >= 0 && s->extra < 0)
        t[1] = 1;
}

/* The ceiling or floor turret (930 floor, 931 ceiling). t3 is its rest
   height; t0 0 rises toward the player's height, 1 aims and fires, 2 sinks
   back, 4.. waits. */
static void moveturret(short i, short p, short sect, int dist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i];
    playertype *pl = &ps[p];

    if (ifhitweapon(i) >= 0) {
        t[4] = 0;
        if (s->extra < 0) {
            pl->f36 += 1000;
            spawn(i, 1370);
            /* the exe counts this loop in its distance variable */
            for (dist = 0; dist < 30; dist += 3) {
                short zv = (short)(-(krand() & 511) - 512), xv = (short)((krand() & 128) + 64), a = (short)(krand() & 2047);

                EGS(sect, s->x, s->y, s->z, 0, (short)(krand() % 11 + 235), -8, 32, 32, a, xv, zv, i, 5);
            }
            pl->baf++;
            t[0] = 2;
            s->xrepeat = s->yrepeat = 0;
        }
    }
    if (t[0] == 0) {
        t[4] = 0;
        if (s->xrepeat == 0) {
            deletesprite(i);
            return;
        }
        if (dist < 4096) {
            if (abs(s->z - pl->posz) >= 2048) {
                short step = (short)(pl->posz - s->z > 0 ? 2048 : -2048);

                s->z += step;
                if (s->picnum == 930)
                    sector[sect].floorz += step;
                else
                    sector[sect].ceilingz += step;
            } else {
                t[1] = 0;
                t[0]++;
            }
        }
    }
    if (t[0] == 1) {
        s->ang += 2 * getincangle(s->ang, getangle(pl->posx - s->x, pl->posy - s->y), 1);
        s->ang &= (short)0xFF80;
        if (++t[1] >= 8) {
            s->shade = 4;
            t[1] = 0;
            if (dist > 1024) {
                t[0] = 4;
                goto wait;
            }
        } else {
            t[4] ^= 1;
            if (t[4]) {
                s->z -= 0x2000;
                shoot(i, 389, 0);
                s->shade = -16;
                s->z += 0x2000;
            }
        }
    }
    if (t[0] == 2) {
        t[4] = 0;
        if (s->picnum == 930) {
            s->z += 2048;
            sector[sect].floorz += 2048;
            if (sector[sect].floorz > t[3]) {
                t[0] = 0;
                return;
            }
        } else {
            s->z -= 2048;
            sector[sect].ceilingz -= 2048;
            if (sector[sect].ceilingz < t[3]) {
                t[0] = 0;
                return;
            }
        }
    }
    if (t[0] <= 3)
        return;
wait:
    t[0]++;
    if (dist > 4096)
        t[0] = 2;
    else if (t[0] > 10)
        t[0] = 1;
    s->ang = (short)getangle(pl->posx - s->x, pl->posy - s->y);
}

/* The pipe bomb (721): owner's t0 names the player whose b90 detonates it.
   Squished by a closing sector, it is a dud; otherwise it slides to rest,
   can be picked back up, and once detonated burns for 20 tics. */
static void movebomb(short i, short p, int dist)
{
    spritetype *s = &sprite[i];
    /* shortcut: the exe reads hittype[-1] for an ownerless bomb; ownerless ones use player 0 */
    short own = (short)(s->owner >= 0 ? hittype[s->owner][0] : 0);
    int a;

    if (ifsquished(i, p) >= 0 && s->extra < 0)
        ps[own].b90 = 0;
    fallto(s);
    if (s->xvel <= 0)
        s->xvel = 0;
    else {
        s->xvel -= 4;
        if (s->xvel & 8)
            s->cstat ^= 4;
    }
    movealong(i, s->zvel, 0);
    if (ps[own].b90 || ps[own].i != s->owner) {
        if (dist > 512 && dist < 768) {
            ps[p].w50++;
            addweapon(p, 721, 1);
            deletesprite(i);
        }
        return;
    }
    s->yvel++;
    if (s->xrepeat || s->yrepeat) {
        s->xrepeat = s->yrepeat = 0;
        hitradius(i, 3092, s->extra);
    }
    if (s->yvel & 1)
        for (a = 0; a < 2047; a += 256)
            EGS(s->sectnum, s->x, s->y, s->z - (s->yvel << 10), 0, 1370, -127, 48, 48, (short)a, 0, 0, i, 5);
    if (s->yvel > 20)
        deletesprite(i);
}

/* The bug (263): hops at the player and nips; shot dead it becomes 273. */
static void movebug(short i, short p, int dist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i];
    playertype *pl = &ps[p];

    if (dist > 4096)
        return;
    if (ifhitweapon(i) >= 0 && s->extra < 0) {
        s->picnum = 273;
        t[0] = 0;
        s->cstat &= ~1;
        return;
    }
    switch (t[0]) {
    case 0:
        if (dist < 4096)
            t[0]++;
        break;
    case 1:
        t[0]++;
        s->ang = (short)getangle(pl->posx - s->x, pl->posy - s->y);
        if (dist != 0)
            s->zvel = (short)(((pl->posz + 2048 - s->z) << 8) / dist);
        s->xvel = 16;
        break;
    case 2:
        if (s->zvel > -1024)
            s->zvel -= 64;
        s->z += s->zvel;
        if (s->xvel < 256)
            s->xvel += 16;
        if (movealong(i, s->zvel, 0))
            t[0]++;
        if (dist < 768) {
            s->ang += 1024 - (randomstash & 2047);
            hurt(p, 1);
        }
        break;
    case 3:
        if (movealong(i, s->zvel, 0)) {
            s->ang += 512;
            t[0] = 1;
        }
        break;
    }
}

/* The face-hugging parasite (961..968). t0: 0 crawl, 1 leap to the
   ceiling, 2 drop, 3 fall, -1 burst, -2 feed on a body, -3 shrivel, -4
   on the player's face, where t3 steps the bite frames. */
static void moveparasite(short i, short p, short sect, int dist)
{
    static const short bite[8] = { 5, 5, 6, 6, 7, 7, 6, 5 };
    spritetype *s = &sprite[i];
    int *t = hittype[i];
    playertype *pl = &ps[p];
    short j, xv;

    t[1] += 128;
    if (t[0] == -4) {
        setsprite(i, s->x, s->y, s->z);
        s->cstat &= ~8;
        s->ang = pl->ang;
        if (syncbits[p] & 4) {
            pl->w7c = 0;
            s->xvel = 128;
            s->zvel = (short)(16 * (100 - pl->horiz) - 512);
            t[0] = 0;
            pl->w80 = -1;
        }
        s->z = pl->posz - t[2] + 2048;
        s->z += 16 * (100 - pl->horiz);
        if (t[2] > 512)
            t[2] -= 128;
        if (t[2] < 348)
            t[2] += 128;
        if (t[3] <= 0) {
            s->picnum = 966;
            if ((krand() >> 8) >= 223)
                t[3] = 1;
        } else {
            s->picnum = bite[t[3]] + 961;
            if (t[3] == 4) {
                hurt(p, 5);
                spritesound(13, pl->i);
            }
            t[3] = t[3] >= 7 ? 0 : t[3] + 1;
        }
        s->xrepeat = (unsigned char)((sintable()[t[1] & 2047] >> 13) + 20);
        s->yrepeat = (unsigned char)((sintable()[t[1] & 2047] >> 13) + 15);
        s->x = pl->posx + (sintable()[(pl->ang + 512) & 2047] >> 7);
        s->y = (sintable()[pl->ang & 2047] >> 7) + pl->posy;
        return;
    }
    if (s->xvel < 64 && dist < 512 && pl->w80 == -1) {
        pl->w80 = i;
        t[2] = (t[0] == 3 || t[0] == 2) ? 3072 : -3328;
        t[0] = -4;
    } else if (t[0] == -3) {
        if (s->xrepeat > 4)
            s->xrepeat -= 4;
        if (s->yrepeat > 4)
            s->yrepeat -= 4;
        else
            deletesprite(i);
        return;
    }
    j = ifhitweapon(i);
    if (j >= 0) {
        pl->f36 += 1000;
        if (sprite[j].picnum == 389) {
            t[0] = -3;
            return;
        }
        if ((krand() >> 8) >= 251 && abs(sector[sect].floorz - sector[sect].ceilingz) < 49152
            && (t[0] == 0 || t[0] == 2))
            t[0]++;
    }
    if (t[0] == -1) {
        fallto(s);
        s->picnum = 965;
        s->cstat &= 0xF7;
        if (s->yrepeat > 62)
            guts(i);
        if (s->xrepeat > 32)
            s->xrepeat -= krand() & 7;
        if (s->yrepeat <= 16) {
            s->xrepeat = 40;
            s->yrepeat = 16;
            s->owner = -1;
            t[0] = 0;
        } else
            s->yrepeat -= krand() & 7;
        return;
    }
    if (t[0] == -2) {
        spritetype *o;

        fallto(s);
        o = &sprite[s->owner];
        o->xvel = 0;
        s->z = o->z;
        s->x = (sintable()[(o->ang + 512) & 2047] >> 11) + o->x;
        s->y = (sintable()[o->ang & 2047] >> 11) + o->y;
        s->picnum = (short)((randomstash & 1) + 963);
        if (s->yrepeat < 64)
            s->yrepeat += 2;
        else if (s->xrepeat < 32)
            s->xrepeat += 4;
        else {
            t[0] = -1;
            if (abs(s->x - o->x) + abs(s->y - o->y) < 768)
                deletesprite(s->owner);
        }
        return;
    }
    if ((krand() >> 8) >= 223)
        for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j]) {
            short pic = sprite[j].picnum;

            if (pic != 408 && pic != 490 && pic != 1536 && pic != 1680 && pic != 1820)
                continue;
            if (abs(s->x - sprite[j].x) + abs(s->y - sprite[j].y) < 768 && abs(s->z - sprite[j].z) < 0x2000) {
                s->owner = j;
                t[0] = -2;
                t[1] = 0;
                return;
            }
        }
    if (t[0] == 0 || t[0] == 2) {
        s->picnum = 961;
        if (t[0] == 2) {
            s->zvel = 0;
            s->cstat &= ~8;
            if (sector[sect].ceilingz + 6144 < s->z) {
                s->z += 2048;
                t[0]++;
                return;
            }
        } else {
            s->cstat |= 8;
            fallto(s);
        }
        movealong(i, s->zvel, 0);
        xv = s->xvel;
        if (xv > 96) {
            s->xvel = xv - 2;
            return;
        }
        s->xvel = (short)(64 - (sintable()[((unsigned short)t[1] + 512) & 2047] >> 9));
        s->ang += getincangle(s->ang, getangle(pl->posx - s->x, pl->posy - s->y), 3);
        s->xrepeat = (unsigned char)((sintable()[((unsigned short)t[1] + 512) & 2047] >> 11) + 36);
        s->yrepeat = (unsigned char)((sintable()[t[1] & 2047] >> 13) + 16);
        if ((krand() >> 8) >= 251 && abs(sector[sect].floorz - sector[sect].ceilingz) < 49152) {
            s->zvel = 0;
            t[0]++;
        }
    }
    if (t[0] == 1) {
        s->picnum = 961;
        if (s->yrepeat < 40)
            s->yrepeat += 8;
        if (s->xrepeat > 8)
            s->xrepeat -= 4;
        if (s->zvel > -3072)
            s->zvel -= 348;
        s->z += s->zvel;
        if (sector[sect].ceilingz + 4096 > s->z) {
            s->z = sector[sect].ceilingz + 4096;
            s->xvel = 0;
            t[0]++;
        }
    }
    if (t[0] == 3) {
        s->picnum = 962;
        if (s->zvel < 3072)
            s->zvel += 348;
        s->z += s->zvel;
        if (sector[sect].floorz - 0x2000 >= s->z) {
            if (s->yrepeat < 36)
                s->yrepeat += 8;
            if (s->xrepeat > 8)
                s->xrepeat -= 4;
        } else {
            s->yrepeat -= 4;
            s->xrepeat += 2;
        }
        if (sector[sect].floorz - 2048 < s->z) {
            s->z = sector[sect].floorz - 2048;
            t[0] = 0;
            s->xvel = 0;
        }
    }
}

/* 0x35F10. Actors (stat 1): the hard-coded ones here, the rest through
   their CON code. */
void moveactors(void)
{
    short i, nexti, j;

    for (i = headspritestat[1]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        int *t = hittype[i], dist, d;
        short p, sect = s->sectnum;

        nexti = nextspritestat[i];
        p = findplayer(i, &dist);
        switch (s->picnum) {
        case 1408:
            if (dist < 2048)
                viewscreen = i;
            continue;
        case 51:
            /* A countdown activator: armed by lotag, it fires when hitag runs out. */
            if (s->yvel != 1 || --s->hitag > 0)
                continue;
            operatesectors(sect, i);
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j]) {
                if (sprite[j].statnum == 6 && sprite[j].picnum == 57)
                    sprite[j].shade = -31;
                if (sprite[j].statnum != 3)
                    continue;
                if (sprite[j].lotag == 2 || sprite[j].lotag == 21)
                    hittype[j][0] = 1;
                else if (sprite[j].lotag == 3)
                    hittype[j][4] = 1;
                else if (sprite[j].lotag == 23)
                    s->shade = 1;
            }
            deletesprite(i);
            continue;
        case 1280:
            movedrone(i, p, dist);
            continue;
        case 1122:
            if (++t[0] > 1024 || dist > 0x2000) {
                deletesprite(i);
                continue;
            }
            d = abs(sector[sect].ceilingz - sector[sect].floorz);
            s->yrepeat = (unsigned char)(d >> 9);
            s->xrepeat = (unsigned char)(25 - ((short)(d >> 9) >> 1));
            s->z = sector[sect].floorz;
            continue;
        }
        if (s->picnum >= 961 && s->picnum <= 968)
            moveparasite(i, p, sect, dist);
        else if (s->picnum == 960) {
            /* A drip: grows on the ceiling, falls, splashes. */
            if (dist > 0x2000)
                continue;
            if (t[0] == 0) {
                if (t[1] > 0)
                    t[1]--;
                else {
                    s->yrepeat++;
                    s->xrepeat -= 2;
                    if (s->yrepeat > 8) {
                        t[0] = 1;
                        s->xrepeat = 8;
                    }
                }
            }
            if (t[0] == 1) {
                fallto(s);
                if (sector[sect].floorz - 2048 < s->z) {
                    t[0] = 2;
                    spritesound(17, i);
                }
            }
            if (t[0] == 2) {
                s->xrepeat += 8;
                s->yrepeat -= 2;
                if (s->yrepeat < 2) {
                    s->z = sector[sect].ceilingz + 512;
                    t[0] = 0;
                    t[1] = krand() & 31;
                    s->xrepeat = 16;
                    s->yrepeat = 0;
                }
            }
        } else if (s->picnum == 930 || s->picnum == 931)
            moveturret(i, p, sect, dist);
        else if (s->picnum == 721)
            movebomb(i, p, dist);
        else if (s->picnum == 1123) {
            if (dist <= 0x2000) {
                d = abs(sector[sect].ceilingz - sector[sect].floorz);
                s->xrepeat = 16;
                s->yrepeat = (unsigned char)((d >> 9) + 8);
                s->z = sector[sect].floorz;
            }
        } else if (s->picnum == 32) {
            /* A proximity trigger: lotag+hitag<<16 >> 13 is its range, or with no
               hitag, a player standing in its sector. */
            int range = (s->lotag + (s->hitag << 16)) >> 13, near;

            if (range != 0 && s->hitag != 0)
                near = dist < range && abs(s->z - ps[p].posz) < 12288;
            else {
                short q = playerinsect(sect);

                near = q >= 0 && ps[q].b8f;
            }
            if (!near)
                t[0] = 0;
            else if (t[0] == 0) {
                t[0] = 1;
                armactivators(s->lotag);
                activatebylotag(s->lotag);
            }
        } else if (s->picnum >= 305 && s->picnum <= 308)
            movelight(i, sect, dist);
        else if (s->picnum >= 620 && s->picnum <= 625)
            movereactor(i, sect, dist);
        else if (s->picnum == 265) {
            s->owner += 8;
            if (s->owner < 128)
                s->ang += 8;
            else if (s->owner < 384)
                s->ang -= 8;
            else if (s->owner < 512)
                s->ang += 8;
            else {
                s->owner = 8;
                s->ang += 16;
            }
        } else if (s->picnum == 263)
            movebug(i, p, dist);
        else if (s->picnum == 273) {
            if (dist > 4096)
                continue;
            fallto(s);
            if (++t[0] > 1024)
                deletesprite(i);
            else if (movealong(i, s->zvel, 0)) {
                s->z = sector[sect].floorz;
                if (sector[sect].lotag == 1)
                    deletesprite(i);
            }
        } else
            execute(i, p, dist);
    }
}

/* 0x35750. Moves effector i along ang/xvel, then redraws its sector's
   walls around it from the msx/msy offsets at t1, turned by t2. */
static void movesector(short i)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i], j = t[1], rx, ry;
    short w, end;

    s->x += (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
    s->y += (sintable()[s->ang & 2047] * s->xvel) >> 14;
    w = sector[s->sectnum].wallptr;
    end = (short)(w + sector[s->sectnum].wallnum);
    for (; w < end; w++, j++) {
        rotatepoint(0, 0, msx[j], msy[j], (short)(t[2] & 2047), &rx, &ry);
        dragpoint(w, s->x + rx, s->y + ry);
    }
    setsprite(i, s->x, s->y, s->z);
}

static int sgn(int v)
{
    return v > 0 ? 1 : v < 0 ? -1 : 0;
}

/* 0x12E40. The first locator (stat 7) with this lotag, in sector sect
   unless sect is -1. */
static short findlocator(short lotag, short sect)
{
    short i;

    for (i = headspritestat[7]; i >= 0; i = nextspritestat[i])
        if ((sect == -1 || sect == sprite[i].sectnum) && lotag == sprite[i].lotag)
            return i;
    return -1;
}

/* 0x35BE0. The first effect sprite (stat 5) in i's sector that is not a
   casing (214), or -1. */
static short effectinsect(short i)
{
    short j;

    for (j = headspritestat[5]; j >= 0; j = nextspritestat[j])
        if (sprite[i].sectnum == sprite[j].sectnum && sprite[j].picnum != 214)
            return j;
    return -1;
}

static void shadewalls(short sect, signed char shade)
{
    int w, end = sector[sect].wallptr + sector[sect].wallnum;

    for (w = sector[sect].wallptr; w < end; w++)
        wall[w].shade = shade;
}

/* The two-sided light (3 and 4): hitag sets the rate, owner the palettes. */
static void flickerpal(short i, short sect, int lit)
{
    spritetype *s = &sprite[i];

    if (lit) {
        sector[sect].ceilingpal = (unsigned char)((unsigned short)s->owner >> 8);
        sector[sect].floorpal = (unsigned char)s->owner;
    } else
        sector[sect].ceilingpal = sector[sect].floorpal = 0;
}

/* The subway (6) and its cars (14), at LABEL_148: follow the locators of
   lotag t3 in sector t0, carrying riders and sprites around. */
static void movetrain(short i, short sect, short lotag)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i], d, dang;
    short p, j;

    if (s->owner == -1)
        s->owner = findlocator((short)t[3], (short)t[0]);
    d = abs(sprite[s->owner].x - s->x) + abs(sprite[s->owner].y - s->y);
    if (d < 1024) {
        if (lotag == 6 && (sprite[s->owner].hitag & 1))
            t[4] = 256;
        t[3]++;
        s->owner = findlocator((short)t[3], (short)t[0]);
        if (s->owner == -1) {
            t[3] = 0;
            s->owner = findlocator(0, (short)t[0]);
        }
    }
    if (s->xvel == 0)
        return;
    dang = getincangle(s->ang, getangle(sprite[s->owner].x - s->x, sprite[s->owner].y - s->y), 3);
    t[2] += dang;
    s->ang += dang;
    /* byte_125692 is never written, so the sound test always runs */
    if (s->xvel == 256) {
        d = 1;
        if (!(sector[sect].floorstat & 1) && !(sector[sect].ceilingstat & 1))
            spritesound(32, i);
    }
    if (s->xvel <= 64 && !(sector[sect].floorstat & 1) && !(sector[sect].ceilingstat & 1))
        stopsound(32);
    for (p = connecthead; p >= 0; p = connectpoint2[p]) {
        playertype *pl = &ps[p];

        if (sector[pl->cursectnum].hitag != sector[sect].hitag || sector[pl->cursectnum].hitag == -1)
            continue;
        rotatepoint(s->x, s->y, pl->posx, pl->posy, (short)dang, &pl->posx, &pl->posy);
        if (d)
            pl->posz -= 1024;
        pl->posx += (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
        pl->posy += (s->xvel * sintable()[s->ang & 2047]) >> 14;
        pl->ang += dang;
        setsprite(pl->i, pl->posx, pl->posy, pl->posz + 9216);
    }
    for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j]) {
        short pic = sprite[j].picnum;

        /* 0x1CBAB compares the sign-extended hitag with 0xFFFF, so hitag -1
           never skips a sprite here. */
        if (pic == 175 || pic == 34 || pic == 283)
            continue;
        rotatepoint(s->x, s->y, sprite[j].x, sprite[j].y, (short)dang, &sprite[j].x, &sprite[j].y);
        sprite[j].x += (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
        sprite[j].y += (s->xvel * sintable()[s->ang & 2047]) >> 14;
        sprite[j].ang += dang;
        setsprite(j, sprite[j].x, sprite[j].y, sprite[j].z);
    }
    movesector(i);
}

/* 0x1BA30. Sector effectors (stat 3), keyed by lotag. scratch and speed
   stand for two stack slots the exe reuses across effectors in one call:
   SE 23 and SE 5 read them before setting them. */
void moveeffectors(void)
{
    static playertype ghost; /* SE 18 reads ps[-1], 176 bytes of zeroed memory before ps */
    int scratch = 0, speed = 0;
    short i, nexti;

    for (i = headspritestat[3]; i >= 0; i = nexti) {
        spritetype *s = &sprite[i];
        int *t = hittype[i], k, d;
        short sect = s->sectnum, lotag = s->lotag, j, p;

        nexti = nextspritestat[i];
        switch (lotag) {
        case 0: {
            int dir = 0;

            for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
                if (s->hitag == sprite[j].hitag && (sprite[j].lotag == 1 || sprite[j].lotag == 5))
                    break;
            if (j >= 0) {
                speed = sector[sect].extra >> 3;
                scratch = dir = sprite[j].ang <= 1024 ? 1 : -1;
                if (t[3] == 0)
                    t[3] = abs(s->x - sprite[j].x) + abs(s->y - sprite[j].y);
                s->xvel = (short)t[3];
                s->x = sprite[j].x;
                s->y = sprite[j].y;
                s->ang += speed * dir;
                t[2] += speed * dir;
            }
            movesector(i);
            break;
        }
        case 2:
            if (t[4] > 0 && t[0] == 0) {
                if (s->hitag <= t[4]) {
                    t[0] = 1;
                    s->xvel = 16;
                } else
                    t[4]++;
            }
            if (t[0] <= 0)
                break;
            if (++t[0] > 24) {
                t[0] = -1;
                t[4] = -1;
                break;
            }
            s->xvel++;
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == sect && ps[p].b8f) {
                    ps[p].posx += (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
                    ps[p].posy += (s->xvel * sintable()[s->ang & 2047]) >> 14;
                }
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j])
                if (sprite[j].picnum != 175) {
                    sprite[j].x += (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
                    sprite[j].y += (sintable()[s->ang & 2047] * s->xvel) >> 14;
                    setsprite(j, sprite[j].x, sprite[j].y, sprite[j].z);
                }
            movesector(i);
            break;
        case 3:
            if (t[4] == 0)
                break;
            k = (randomstash / (s->hitag + 1)) & 1;
            flickerpal(i, sect, k);
            t[0] = k ? s->shade : t[3];
            sector[sect].ceilingshade = sector[sect].floorshade = (signed char)t[0];
            for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
                if (!(wall[k].hitag & 1))
                    wall[k].shade = (signed char)t[0];
            break;
        case 4:
            k = (randomstash / (s->hitag + 1)) & 1;
            if (k) {
                t[1] = s->shade;
                t[0] = s->shade;
            } else {
                t[1] = t[2];
                t[0] = t[3];
            }
            flickerpal(i, sect, k);
            sector[sect].floorshade = sector[sect].ceilingshade = (signed char)t[1];
            for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
                if (!(wall[k].hitag & 1))
                    wall[k].shade = (signed char)t[0];
            break;
        case 5: {
            /* The boss ship: hunts locators (stat 7, lotag t4) nearest the player. */
            int dist;

            p = findplayer(i, &dist);
            if (s->owner == -1) {
                t[4] = 0;
                scratch = 0x7FFFFFFF;
                for (;;) {
                    s->owner = findlocator((short)t[4], -1);
                    if (s->owner == -1)
                        break;
                    d = abs(sprite[ps[p].i].x - sprite[s->owner].x) + abs(sprite[ps[p].i].y - sprite[s->owner].y);
                    if (d < scratch) {
                        speed = s->owner;
                        scratch = d;
                    }
                    t[4]++;
                }
                s->owner = (short)speed;
                s->zvel = (short)(16 * sgn(sprite[speed].z - s->z));
            }
            if (abs(sprite[s->owner].x - s->x) + abs(sprite[s->owner].y - s->y) < 1024) {
                s->owner = -1;
                break;
            }
            s->xvel = 256;
            speed = getincangle(s->ang, getangle(sprite[s->owner].x - s->x, sprite[s->owner].y - s->y), 3);
            s->ang += speed;
            if ((krand() >> 8) < 223) {
                t[2] += getincangle(t[2] + 512, getangle(ps[p].posx - s->x, ps[p].posy - s->y), 2);
                sector[sect].ceilingshade = 0;
            } else {
                t[2] += speed;
                sector[sect].ceilingshade = 127;
            }
            if (ifhitweapon(i) >= 0) {
                t[3]++;
                ps[p].f36 += 10000;
                if (t[3] == 5) {
                    s->zvel += 1024;
                    ps[p].f36 += 1000000;
                    quote(7);
                }
            }
            s->z += s->zvel;
            sector[sect].ceilingz += s->zvel;
            sector[t[0]].ceilingz += s->zvel;
            movesector(i);
            break;
        }
        case 6:
            if (t[4] <= 0)
                s->xvel = 256;
            else {
                t[4]--;
                if (t[4] >= 224)
                    s->xvel -= 8;
                if (t[4] > 127 && t[4] < 224)
                    s->xvel = 0;
                if (t[4] < 128)
                    s->xvel += 8;
                if (t[4] < 96) {
                    t[4] = 0;
                    s->xvel = 256;
                }
            }
            for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
                if (sprite[j].lotag == 14 && sprite[j].hitag == s->hitag && hittype[j][0] == t[0]) {
                    sprite[j].xvel = s->xvel;
                    hittype[j][4] = t[4];
                }
            /* fall through */
        case 14:
            movetrain(i, sect, lotag);
            break;
        case 8:
        case 9: {
            int dir;

            if (getanimationgoal(&sector[sect].ceilingz) < 0)
                break;
            dir = (sector[sect].lotag & 0x8000) ? -t[3] : t[3];
            if (lotag == 9)
                dir = -dir;
            for (j = headspritestat[3]; j >= 0; j = nextspritestat[j]) {
                short s2 = sprite[j].sectnum;
                int minshade = sprite[j].shade, *u = hittype[j];

                if (sprite[j].lotag != lotag || sprite[j].hitag != s->hitag)
                    continue;
                for (scratch = sector[s2].wallptr; scratch < sector[s2].wallptr + sector[s2].wallnum; scratch++) {
                    if (wall[scratch].hitag & 1)
                        continue;
                    wall[scratch].shade += dir;
                    if (wall[scratch].shade < minshade)
                        wall[scratch].shade = (signed char)minshade;
                    else if (wall[scratch].shade > u[2])
                        wall[scratch].shade = (signed char)u[2];
                }
                sector[s2].floorshade += dir;
                sector[s2].ceilingshade += dir;
                if (sector[s2].floorshade < minshade)
                    sector[s2].floorshade = (signed char)minshade;
                else if (sector[s2].floorshade > u[0])
                    sector[s2].floorshade = (signed char)u[0];
                if (sector[s2].ceilingshade < minshade)
                    sector[s2].ceilingshade = (signed char)minshade;
                else if (sector[s2].ceilingshade > u[1])
                    sector[s2].ceilingshade = (signed char)u[0]; /* exe: clamps to t0, not t1 */
            }
            break;
        }
        case 10:
            if ((sector[sect].lotag & 0x7FFF) == 23) {
                if ((sector[sect].lotag & 0x8000) && s->hitag < ++t[0]) {
                    operatesectors(sect, i);
                    t[0] = 0;
                }
            } else if (sector[sect].floorz > sector[sect].ceilingz) {
                if (t[1] == 0 && s->hitag < ++t[0]) {
                    operatesectors(sect, i);
                    t[1] = 1;
                }
            } else {
                t[0] = 0;
                t[1] = 0;
            }
            break;
        case 11:
            /* A swinging door: t3 is its direction, t4 how far it has turned. */
            if (t[4] != 0) {
                short step = (short)(t[3] * (s->yvel >> 3));
                int w, end = sector[sect].wallptr + sector[sect].wallnum;

                t[2] += step;
                t[4] += step;
                movesector(i);
                for (w = sector[sect].wallptr; w < end; w++)
                    for (p = connecthead; p >= 0; p = connectpoint2[p])
                        if (clipinsidebox(ps[p].posx, ps[p].posy, (short)w, 128)) {
                            t[2] -= step;
                            t[4] -= step;
                            movesector(i);
                        }
                if (t[4] <= -511 || t[4] >= 512) {
                    t[4] = 0;
                    t[2] &= ~0xFF;
                    movesector(i);
                }
            }
            break;
        case 12:
            if (t[0] == 1) {
                shadewalls(sect, (signed char)t[1]);
                sector[sect].floorshade = (signed char)t[1];
                sector[sect].ceilingshade = (signed char)t[2];
            } else if (t[0] != 2)
                break;
            else if (sector[sect].floorshade > s->shade) {
                sector[sect].floorshade -= 2;
                sector[sect].ceilingshade -= 2;
                for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
                    wall[k].shade -= 2;
                break;
            }
            t[0] = 0;
            break;
        case 13:
            /* A blast ceiling: dropped force fields of its hitag set it off; at
               t2 4 it explodes. */
            if (t[2] != 0) {
                if (s->ang == 512) {
                    if (s->owner != 0)
                        sector[sect].ceilingz += (t[0] - sector[sect].ceilingz) >> 1;
                    else
                        sector[sect].floorz += (t[1] - sector[sect].floorz) >> 1;
                } else {
                    sector[sect].ceilingz += (t[0] - sector[sect].ceilingz) >> 1;
                    sector[sect].floorz += (t[1] - sector[sect].floorz) >> 1;
                }
                if (t[3] == 1) {
                    t[3] = 2;
                    sector[sect].ceilingstat ^= 1;
                    if (s->ang == 512) {
                        shadewalls(sect, s->shade);
                        sector[sect].floorshade = s->shade;
                        if (ps[0].w84 >= 0) {
                            sector[sect].ceilingpicnum = sector[ps[0].w84].ceilingpicnum;
                            sector[sect].ceilingshade = sector[ps[0].w84].ceilingshade;
                        }
                    }
                }
                if (++t[2] > 7) {
                    if (t[3] != 2)
                        sector[sect].ceilingshade = 127;
                    if (s->ang != 512)
                        sector[sect].floorshade = 127;
                    deletesprite(i);
                    break;
                }
            } else {
                for (j = headspritestat[6]; j >= 0; j = nextspritestat[j])
                    if (sprite[j].picnum == 57 && sprite[j].shade == -32 && sprite[j].hitag == s->hitag)
                        break;
                if (j >= 0)
                    for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
                        if (sprite[j].lotag == 13 && sprite[j].hitag == s->hitag)
                            hittype[j][2] = 1;
            }
            if (t[2] == 4 && s->ang != 512) {
                nuke(s->x, s->y, sector[sect].floorz - 12288 - ((krand() & 0x20) << 6), i);
                for (k = 0; k < 30; k += 3) {
                    int z = sector[sect].floorz - (krand() & 0x1000);
                    short pic = (short)(krand() % 11 + 235), a = (short)(krand() & 2047);
                    short xv = (short)((krand() & 0x80) + 64), zv = (short)(-(krand() & 511) - 512);

                    EGS(sect, s->x, s->y, z, 0, pic, -8, 24, 24, a, xv, zv, i, 5);
                }
            }
            break;
        case 15:
            if (t[4] == 0)
                break;
            s->xvel = 16;
            if (t[4] == 1) {
                if ((s->yvel >> 3) < t[3]) {
                    t[4] = 0;
                    break;
                }
                t[3]++;
            }
            if (t[4] == 2 && --t[3] < 1)
                t[4] = 0;
            movesector(i);
            break;
        case 16:
            t[2] += 32;
            if (sector[sect].floorz < sector[sect].ceilingz)
                s->shade = 0;
            else if (sector[sect].ceilingz < t[3]) {
                if (nextspritesect[headspritesect[sect]] == -1) {
                    deletesprite(i);
                    break;
                }
                s->shade = 1;
            }
            if (s->shade)
                sector[sect].ceilingz += 1024;
            else
                sector[sect].ceilingz -= 512;
            movesector(i);
            break;
        case 17:
            /* A lift: t0 is its direction; riders move to the next floor's lift. */
            speed = 4 * s->yvel * t[0];
            sector[sect].ceilingz += speed;
            sector[sect].floorz += speed;
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j])
                if (sprite[j].statnum < 3)
                    sprite[j].z += speed;
            if (t[0] == 0)
                break;
            if (abs(sector[sect].floorz - t[2]) <= s->yvel) {
                movelift(i, 0);
                break;
            }
            if (t[0] == -1) {
                s->ang += 128;
                if (sector[sect].floorz > t[3])
                    break;
            } else {
                s->ang -= 128;
                if (sector[sect].ceilingz < t[4])
                    break;
            }
            if (t[1] == 0)
                break;
            t[1] = 0;
            for (p = connecthead; p >= 0; p = connectpoint2[p]) {
                if (ps[p].cursectnum != sect)
                    continue;
                for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
                    if (sprite[j].lotag == 17 && sector[sect].hitag - t[0] == sector[sprite[j].sectnum].hitag
                        && sprite[j].hitag == s->hitag) {
                        ps[p].posx += sprite[j].x - s->x;
                        ps[p].cursectnum = sprite[j].sectnum;
                        ps[p].posy += sprite[j].y - s->y;
                        setsprite(ps[p].i, ps[p].posx, ps[p].posy, ps[p].posz + 9216);
                    }
            }
            break;
        case 18: {
            /* A pull toward the effector; the player loop's empty body leaves p -1,
               so the pull lands on the record before ps. */
            static const unsigned char stats[3] = { 1, 4, 5 };
            int r = 16 * s->yvel;
            playertype *pl = &ghost;

            if (t[0] == 0)
                break;
            for (k = 0; k < 3; k++)
                for (j = headspritestat[stats[k]]; j >= 0; j = nextspritestat[j]) {
                    scratch = abs(s->x - sprite[j].x) + abs(s->y - sprite[j].y);
                    if (scratch >= r)
                        continue;
                    sprite[j].x += sgn(s->x - sprite[j].x) << 7;
                    sprite[j].y += sgn(s->y - sprite[j].y) << 7;
                    sprite[j].z += sgn(s->z - sprite[j].z) << 12;
                    sprite[j].xvel = 0;
                    sprite[j].ang = (short)getangle(s->x - sprite[j].x, s->y - sprite[j].y);
                }
            if (abs(sprite[pl->i].x - s->x) + abs(sprite[pl->i].y - s->y) < r) {
                pl->posx += sgn(s->x - pl->posx) << 8;
                pl->posy += sgn(s->y - pl->posy) << 8;
                clipmove(&pl->posx, &pl->posy, &pl->posz, &pl->cursectnum, 0, 0, 192, 1024, 1024, 0);
            }
            break;
        }
        case 19:
            if (t[0] == 0) {
                if (effectinsect(i) >= 0) {
                    quote(8);
                    for (scratch = headspritestat[3]; scratch >= 0; scratch = nextspritestat[scratch])
                        if ((sprite[scratch].lotag & 0x7FFF) == 19 && sprite[scratch].hitag == s->hitag
                            && hittype[scratch][0] == 0)
                            hittype[scratch][0] = 1;
                }
            } else if (sector[sect].ceilingz < sector[sect].floorz)
                sector[sect].ceilingz += s->yvel;
            else {
                sector[sect].ceilingz = sector[sect].floorz;
                deletesprite(i);
            }
            break;
        case 20: {
            /* A sliding sector: t1 and t2 are its two moving wall points. */
            int dx, dy;

            if (t[0] == 0)
                break;
            s->xvel = t[0] == 1 ? 128 : -128;
            dx = (s->xvel * sintable()[(s->ang + 512) & 2047]) >> 14;
            dy = (s->xvel * sintable()[s->ang & 2047]) >> 14;
            t[3] += s->xvel;
            s->x += dx;
            s->y += dy;
            if (t[3] <= 0 || (t[3] >> 6) == (s->yvel >> 6)) {
                s->x -= dx;
                s->y -= dy;
                setsprite(i, s->x, s->y, s->z);
                t[0] = 0;
                break;
            }
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j])
                if (sprite[j].statnum != 3) {
                    sprite[j].x += dx;
                    sprite[j].y += dy;
                }
            dragpoint((short)t[1], wall[t[1]].x + dx, wall[t[1]].y + dy);
            dragpoint((short)t[2], wall[t[2]].x + dx, wall[t[2]].y + dy);
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == sect && ps[p].b8f) {
                    ps[p].posx += dx;
                    ps[p].posy += dy;
                }
            sector[sect].floorxpanning -= dx >> 3;
            sector[sect].floorypanning -= dy >> 3;
            sector[sect].ceilingxpanning -= dx >> 3;
            sector[sect].ceilingypanning -= dy >> 3;
            break;
        }
        case 21: {
            /* exe: also leaves a pointer in the scratch slot; not modelled */
            int *z;

            if (t[0] == 0)
                break;
            z = s->ang == 1536 ? &sector[sect].ceilingz : &sector[sect].floorz;
            if (t[0] == 1) {
                s->xvel = (short)(16 * s->yvel * sgn(s->z - *z));
                t[0]++;
            }
            if (sector[sect].extra != 0) {
                sector[sect].extra--;
                break;
            }
            *z += s->xvel;
            if (abs(*z - s->z) < 1024) {
                *z = s->z;
                deletesprite(i);
            }
            break;
        }
        case 22:
            if (t[1] == 0)
                break;
            if (getanimationgoal(&sector[t[0]].ceilingz) >= 0)
                sector[sect].ceilingz += synctics * s->yvel;
            else
                t[1] = 0;
            break;
        case 23:
            /* A flooding sector: once an effect lands in it, the sectors listed
               at msx[t1..] rise to the highest floor among them. */
            if (s->shade == 0) {
                if (effectinsect(i) >= 0)
                    s->shade = 1;
                break;
            }
            if (s->shade != 1)
                break;
            /* exe: reads the scratch slot here before setting it.
               shortcut: a stale slot outside the sector array, or a white wall's
               sector -1, reads exe memory we do not have; both are skipped. */
            if (scratch >= 0 && scratch < MAXSECTORS)
                for (k = sector[scratch].wallptr; k < sector[scratch].wallptr + sector[scratch].wallnum; k++)
                    if (wall[k].nextsector >= 0 && sector[wall[k].nextsector].hitag == sector[scratch].hitag)
                        wall[k].picnum = 49;
            d = (int)0x80000000;
            for (k = 0; k < t[0]; k++) {
                scratch = msx[k + t[1]];
                if (d < sector[scratch].floorz && (sector[scratch].lotag & 1)) {
                    d = sector[scratch].floorz;
                    t[2] = scratch;
                }
            }
            for (k = 0; k < t[0]; k++) {
                scratch = msx[k + t[1]];
                if (sector[scratch].floorpicnum != 49)
                    sector[scratch].floorpicnum = 49;
                if ((sector[scratch].lotag & 0x7FFF) == 2)
                    sector[scratch].ceilingz = sector[t[2]].floorz;
                else {
                    sector[scratch].lotag |= 1;
                    if (sector[scratch].floorz >= sector[t[2]].floorz)
                        sector[scratch].floorz = sector[t[2]].floorz = sector[scratch].floorz - s->yvel;
                    if (sector[scratch].floorz < sector[scratch].ceilingz)
                        sector[scratch].floorz = sector[scratch].ceilingz;
                }
            }
            if (sector[sect].floorz < s->z)
                deletesprite(i);
            break;
        case 24: {
            /* A conveyor. */
            int dx = (sintable()[(s->ang + 512) & 2047] * s->yvel) >> 18;
            int dy = (sintable()[s->ang & 2047] * s->yvel) >> 18;

            scratch = dy;
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j]) {
                short st = sprite[j].statnum;

                if (st == 9 || st == 0 || st == 3 || st == 10 || (sprite[j].picnum >= 1175 && sprite[j].picnum <= 1178))
                    continue;
                sprite[j].x += dx;
                sprite[j].y += dy;
                setsprite(j, sprite[j].x, sprite[j].y, sprite[j].z);
                if (sect != sprite[j].sectnum) {
                    sprite[j].x += 4 * dx;
                    sprite[j].y += 4 * dy;
                }
            }
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == sect && ps[p].b8f) {
                    ps[p].posx += dx;
                    ps[p].posy += dy;
                    clipmove(&ps[p].posx, &ps[p].posy, &ps[p].posz, &ps[p].cursectnum, 0, 0, 192, 1024, 1024, 0);
                    setsprite(ps[p].i, ps[p].posx, ps[p].posy, ps[p].posz);
                }
            sector[sect].floorxpanning += dy >> 2;
            sector[sect].floorypanning += dx >> 2;
            break;
        }
        case 25:
            if (s->ang != 0)
                t[2] += 64;
            if (effectinsect(i) >= 0 && ++t[3] == 4) {
                deletesprite(i);
                break;
            }
            if (sector[sect].floorz <= sector[sect].ceilingz)
                s->shade = 0;
            else if (sector[sect].ceilingz <= t[4])
                s->shade = 1;
            if (s->shade) {
                sector[sect].ceilingz += 16 * s->yvel;
                if (sector[sect].ceilingz > sector[sect].floorz)
                    sector[sect].ceilingz = sector[sect].floorz;
            } else {
                sector[sect].ceilingz -= 16 * s->yvel;
                if (sector[sect].ceilingz < t[4])
                    sector[sect].ceilingz = t[4];
            }
            movesector(i);
            break;
        case 26: {
            /* A shaking or rolling sector that returns home (t3,t4) every 8 tics. */
            int dx = (32 * sintable()[(s->ang + 512) & 2047]) >> 14, dy = (32 * sintable()[s->ang & 2047]) >> 14;

            scratch = dx;
            s->xvel = 32;
            s->shade++;
            if (s->shade <= 7)
                sector[sect].floorz += s->zvel;
            else {
                s->x = t[3];
                s->y = t[4];
                sector[sect].floorz -= s->shade * s->zvel - s->zvel;
                s->shade = 0;
            }
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j])
                if (sprite[j].statnum != 3 && sprite[j].statnum != 10) {
                    sprite[j].x += dx;
                    sprite[j].y += dy;
                    setsprite(j, sprite[j].x, sprite[j].y, sprite[j].z + s->zvel);
                }
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == sect && ps[p].b8f) {
                    ps[p].posx += dx;
                    ps[p].posy += dy;
                    ps[p].posz += s->zvel;
                    setsprite(ps[p].i, ps[p].posx, ps[p].posy, ps[p].posz + 9216);
                }
            for (k = sector[sect].wallptr; k < sector[sect].wallptr + sector[sect].wallnum; k++)
                if (wall[k].lotag == 0)
                    wall[k].xpanning += 4;
            movesector(i);
            break;
        }
        case 27: {
            /* A demo camera: while a demo plays it takes the view when it can see
               the player from beyond its hitag. */
            int dist;

            p = findplayer(i, &dist);
            if (ps[p].w54 <= 0)
                break;
            if (t[0] < 0) {
                t[0]++;
                cameraspr = i;
                break;
            }
            if (demomode != 2 || ps[p].w56 != -1)
                break;
            if (cansee(s->x, s->y, s->z, s->sectnum, ps[p].posx, ps[p].posy, ps[p].posz, ps[p].cursectnum)) {
                if ((unsigned)s->hitag <= (unsigned)dist) {
                    if (t[0] == 999) {
                        t[0] = i == cameraspr ? 0 : -20;
                        cameraspr = i;
                    }
                } else {
                    t[0] = 999;
                    cameraspr = i;
                    s->ang = (short)getangle(ps[p].posx - s->x, ps[p].posy - s->y);
                    s->yvel = (short)((s->z - ps[p].posz) / 257 + 100);
                }
            } else if (t[0] == 999) {
                t[0] = i == cameraspr ? 0 : -20;
                cameraspr = i;
            }
            break;
        }
        case 128: {
            /* Breaking glass on wall t2: shards, then the wall clears after t1 tics. */
            short w = (short)t[2], nw;

            wall[w].cstat &= 0xDF;
            wall[w].cstat |= 0x10;
            nw = wall[w].nextwall;
            wall[nw].cstat &= 0xDF;
            wall[nw].cstat |= 0x10;
            wall[w].overpicnum++;
            wall[wall[w].nextwall].overpicnum++;
            if (t[0] == 0)
                for (k = 0; k < 15; k++) {
                    int z = s->z - (krand() & 0xFFF);
                    short zv, a;

                    if (z < sector[s->sectnum].ceilingz)
                        z = s->z;
                    zv = (short)(1024 - (krand() & 2047));
                    a = (short)(s->ang - 256 + (krand() & 511));
                    EGS(s->sectnum, s->x, s->y, z, 0, (short)((k & 3) + 970), -32, 18, 18, a, 64, zv, s->owner, 5);
                }
            if (t[0] < t[1]) {
                t[0]++;
                break;
            }
            wall[w].cstat &= 0xAE;
            wall[wall[w].nextwall].cstat &= 0xAE;
            deletesprite(i);
            break;
        }
        case 129: {
            static const unsigned char frames[12] = { 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0 };

            wall[t[2]].overpicnum = (short)(frames[t[0]] + 790);
            wall[wall[t[2]].nextwall].overpicnum = (short)(frames[t[0] + 2] + 790);
            if (++t[0] > 9)
                deletesprite(i);
            break;
        }
        }
    }
}

/* The water and teleport pairs (stat 9), the first part of 0x18EA0: sprites
   resting on a lotag 0 sector's floor jump to the partner of the same hitag,
   and players cross between surface (1), underwater (2) and plain (0)
   sectors. The partner search keeps the last skipped candidate's sector
   lotag as the one it tests next, as the exe does. */
static void moveteleporters(void)
{
    short i, j, k, p;

    for (i = headspritestat[9]; i >= 0; i = nextspritestat[i]) {
        spritetype *s = &sprite[i];
        short sect = s->sectnum;

        if (s->z > sector[sect].floorz)
            s->z = sector[sect].floorz;
        if (sector[sect].lotag == 0 && sector[sect].floorz == s->z)
            for (j = headspritesect[sect]; j >= 0; j = nextspritesect[j]) {
                short st = sprite[j].statnum;

                if (hittype[i][1]) {
                    hittype[i][1]--;
                    break;
                }
                if (st == 3 || st == 9 || sprite[j].picnum == 215 || sprite[j].picnum == 283)
                    continue;
                for (k = headspritestat[9]; k >= 0; k = nextspritestat[k])
                    if (k != i && s->hitag == sprite[k].hitag) {
                        sprite[j].x = sprite[k].x;
                        sprite[j].y = sprite[k].y;
                        sprite[j].z = sprite[k].z;
                        setsprite(j, sprite[j].x, sprite[j].y, sprite[j].z);
                        hittype[k][1] = 10;
                        hittype[i][1] = 10;
                    }
            }
        for (p = connecthead; p >= 0; p = connectpoint2[p]) {
            playertype *pl = &ps[p];
            int lt = sector[sect].lotag & 0x7FFF, cl;

            if (sect != pl->cursectnum)
                continue;
            if (lt == 0) {
                if (abs(s->z - pl->posz) > 4096 && s->z != sector[sect].floorz)
                    continue;
            } else if (lt == 2) {
                if (sector[sect].ceilingz + 1280 < pl->posz)
                    continue;
            } else if (lt == 1 && sector[sect].floorz - 1280 > pl->posz)
                continue;
            if (hittype[i][0]) {
                hittype[i][0]--;
                continue;
            }
            for (k = headspritestat[9]; k >= 0; k = nextspritestat[k]) {
                int old;

                if (k == i || s->hitag != sprite[k].hitag)
                    continue;
                old = lt;
                lt = sector[sprite[k].sectnum].lotag;
                if ((old == 1 && (lt == 0 || lt == 1)) || (old == 2 && lt == 2))
                    continue;
                break;
            }
            if (k < 0)
                continue;
            cl = lt;
            if (cl == 0) {
                pl->posx = sprite[k].x;
                pl->posy = sprite[k].y;
                pl->posz = sprite[k].z == s->z ? sector[sprite[k].sectnum].floorz - 9216 : sprite[k].z;
                hittype[k][0] = 20;
                hittype[i][0] = 20;
            }
            if (cl == 1) {
                if (pl->b8f)
                    spritesound(28, pl->i);
                pl->posx += sprite[k].x - s->x;
                pl->posy += sprite[k].y - s->y;
                pl->posz = sprite[k].z - 1536;
                hittype[k][0] = 1;
                hittype[i][0] = 1;
            }
            if (cl == 2) {
                spritesound(10, pl->i);
                pl->posx += sprite[k].x - s->x;
                pl->posy += sprite[k].y - s->y;
                pl->posz = sector[sprite[k].sectnum].ceilingz + 1536;
                hittype[k][0] = 1;
                hittype[i][0] = 1;
            }
            pl->cursectnum = sprite[k].sectnum;
            if (sector[sprite[k].sectnum].lotag & 0x7FFF) {
                for (j = 0; j < 20; j++)
                    spawn(pl->i, 215);
                spawn(pl->i, 1190);
            }
        }
    }
}

/* 0x18EA0. One game tic of synctics. */
void domovethings(void)
{
    short p;

    randomstash = (short)krand();
    cameraspr = -1;
    lockclock += synctics;
    doanimations(synctics);
    if (!gamepaused) {
        if (demomode == 1)
            demo_record();
        moveeffects();
        moveteleporters();
    }
    for (p = connecthead; p >= 0; p = connectpoint2[p]) {
        playerkeys(p);
        if (!gamepaused) {
            processinput(p);
            checksectors(p);
        }
    }
    if (!gamepaused) {
        moveplayers();
        moveweapons();
        movestandables();
        moveeffectors();
        wakeactors();
        moveactors();
        movemusic();
    }
    synctics = 0;
}

/* duke3d's interpolations, for frames drawn between two logic steps: every
   wall, sector, sprite and view is drawn part of the way from where the
   last step found it, and put back before the next step. */
static walltype owall[MAXWALLS], bwall[MAXWALLS];
static sectortype osector[MAXSECTORS], bsector[MAXSECTORS];
static spritetype bsprite[MAXSPRITES];
static playertype ops[MAXPLAYERS], bps[MAXPLAYERS];

/* From o toward n; 65536 is n exactly. */
static int blend(int o, int n, int r)
{
    return n - (int)((long long)(n - o) * (65536 - r) >> 16);
}

/* A jump this far in one step (a teleport, a monitor) is not drawn in between. */
static int jumped(int dx, int dy)
{
    return abs(dx) + abs(dy) >= 8192;
}

void updateinterpolations(void)
{
    int i;

    memcpy(owall, wall, sizeof owall);
    memcpy(osector, sector, sizeof osector);
    memcpy(ops, ps, sizeof ops);
    for (i = 0; i < MAXSPRITES; i++) {
        bposx[i] = sprite[i].x;
        bposy[i] = sprite[i].y;
        bposz[i] = sprite[i].z;
    }
}

void dointerpolations(int r)
{
    int i;
    short p;

    memcpy(bwall, wall, sizeof bwall);
    memcpy(bsector, sector, sizeof bsector);
    memcpy(bsprite, sprite, sizeof bsprite);
    memcpy(bps, ps, sizeof bps);
    for (i = 0; i < numwalls; i++) {
        wall[i].x = blend(owall[i].x, wall[i].x, r);
        wall[i].y = blend(owall[i].y, wall[i].y, r);
    }
    for (i = 0; i < numsectors; i++) {
        sector[i].ceilingz = blend(osector[i].ceilingz, sector[i].ceilingz, r);
        sector[i].floorz = blend(osector[i].floorz, sector[i].floorz, r);
    }
    for (i = 0; i < MAXSPRITES; i++) {
        spritetype *s = &sprite[i];

        if (jumped(s->x - bposx[i], s->y - bposy[i]))
            continue;
        s->x = blend(bposx[i], s->x, r);
        s->y = blend(bposy[i], s->y, r);
        s->z = blend(bposz[i], s->z, r);
    }
    for (p = connecthead; p >= 0; p = connectpoint2[p]) {
        playertype *pl = &ps[p], *o = &ops[p];

        if (jumped(pl->posx - o->posx, pl->posy - o->posy))
            continue;
        pl->posx = blend(o->posx, pl->posx, r);
        pl->posy = blend(o->posy, pl->posy, r);
        pl->posz = blend(o->posz, pl->posz, r);
        pl->horiz = blend(o->horiz, pl->horiz, r);
        /* the short way round */
        pl->ang = (short)blend(pl->ang - ((((pl->ang - o->ang) & 2047) ^ 1024) - 1024), pl->ang, r);
        pl->lookang = (short)blend(o->lookang, pl->lookang, r);
    }
}

/* Only the blended fields: drawing's own writes (animatesprites' cstat) stay. */
void restoreinterpolations(void)
{
    int i;
    short p;

    for (i = 0; i < numwalls; i++) {
        wall[i].x = bwall[i].x;
        wall[i].y = bwall[i].y;
    }
    for (i = 0; i < numsectors; i++) {
        sector[i].ceilingz = bsector[i].ceilingz;
        sector[i].floorz = bsector[i].floorz;
    }
    for (i = 0; i < MAXSPRITES; i++) {
        sprite[i].x = bsprite[i].x;
        sprite[i].y = bsprite[i].y;
        sprite[i].z = bsprite[i].z;
    }
    for (p = connecthead; p >= 0; p = connectpoint2[p]) {
        ps[p].posx = bps[p].posx;
        ps[p].posy = bps[p].posy;
        ps[p].posz = bps[p].posz;
        ps[p].horiz = bps[p].horiz;
        ps[p].ang = bps[p].ang;
        ps[p].lookang = bps[p].lookang;
    }
}
