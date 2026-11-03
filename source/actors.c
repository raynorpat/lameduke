/* Sprite creation: EGS 0x12E90 and spawn 0x1F160. */

#include "actors.h"
#include "global.h"
#include "gamedef.h"

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
