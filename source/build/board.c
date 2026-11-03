#include "board.h"

#include "engine.h"
#include "math.h"
#include "tiles.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sectortype sector[MAXSECTORS];  /* 0x1ECF10 */
walltype wall[MAXWALLS];        /* 0x1F6310 */
spritetype sprite[MAXSPRITES];  /* 0x1C6F10 */
int numsectors;
int numwalls;
int numsprites;

int posx;
int posy;
int posz;
int horiz;
short ang;
short cursectnum;

/* Index MAXSECTORS / MAXSTATUS holds the free list head. */
short headspritesect[MAXSECTORS + 1], headspritestat[MAXSTATUS + 1];
short prevspritesect[MAXSPRITES], nextspritesect[MAXSPRITES];
short prevspritestat[MAXSPRITES], nextspritestat[MAXSPRITES];

static void free_board(void)
{
    numsectors = 0;
    numwalls = 0;
    numsprites = 0;
}

/* 0x50A72 */
static void initspritelists(void)
{
    int i;

    for (i = 0; i < MAXSECTORS; i++)
        headspritesect[i] = -1;
    headspritesect[MAXSECTORS] = 0;
    for (i = 0; i < MAXSPRITES; i++) {
        prevspritesect[i] = (short)(i - 1);
        nextspritesect[i] = (short)(i + 1);
        sprite[i].sectnum = MAXSECTORS;
    }
    prevspritesect[0] = -1;
    nextspritesect[MAXSPRITES - 1] = -1;
    for (i = 0; i < MAXSTATUS; i++)
        headspritestat[i] = -1;
    headspritestat[MAXSTATUS] = 0;
    for (i = 0; i < MAXSPRITES; i++) {
        prevspritestat[i] = (short)(i - 1);
        nextspritestat[i] = (short)(i + 1);
        sprite[i].statnum = MAXSTATUS;
    }
    prevspritestat[0] = -1;
    nextspritestat[MAXSPRITES - 1] = -1;
}

/* 0x50B55. Takes the free-list head and pushes it on the front of the sector's list. */
static int insertspritesect(short sectnum)
{
    short blanktouse;

    if (sectnum >= MAXSECTORS || headspritesect[MAXSECTORS] == -1)
        return -1;
    blanktouse = headspritesect[MAXSECTORS];
    headspritesect[MAXSECTORS] = nextspritesect[blanktouse];
    if (headspritesect[MAXSECTORS] >= 0)
        prevspritesect[headspritesect[MAXSECTORS]] = -1;
    prevspritesect[blanktouse] = -1;
    nextspritesect[blanktouse] = headspritesect[sectnum];
    if (headspritesect[sectnum] >= 0)
        prevspritesect[headspritesect[sectnum]] = blanktouse;
    headspritesect[sectnum] = blanktouse;
    sprite[blanktouse].sectnum = sectnum;
    return blanktouse;
}

/* 0x50BF3 */
static int insertspritestat(short statnum)
{
    short blanktouse;

    if (statnum >= MAXSTATUS || headspritestat[MAXSTATUS] == -1)
        return -1;
    blanktouse = headspritestat[MAXSTATUS];
    headspritestat[MAXSTATUS] = nextspritestat[blanktouse];
    if (headspritestat[MAXSTATUS] >= 0)
        prevspritestat[headspritestat[MAXSTATUS]] = -1;
    prevspritestat[blanktouse] = -1;
    nextspritestat[blanktouse] = headspritestat[statnum];
    if (headspritestat[statnum] >= 0)
        prevspritestat[headspritestat[statnum]] = blanktouse;
    headspritestat[statnum] = blanktouse;
    sprite[blanktouse].statnum = statnum;
    return blanktouse;
}

/* 0x50B40 */
int insertsprite(short sectnum, short statnum)
{
    insertspritestat(statnum);
    return insertspritesect(sectnum);
}

int loadboard(const char *filename)
{
    FILE *fp;
    int version;
    int px, py, pz;
    short startang, startsect;
    short nsec, nwal, nspr, nextra;
    int i;

    free_board();
    initspritelists();
    fp = plat_fopen(filename, "rb");
    if (!fp)
        return -1;
    if (fread(&version, 4, 1, fp) != 1 || version != 5) {
        fclose(fp);
        return -1;
    }
    if (fread(&px, 4, 1, fp) != 1 || fread(&py, 4, 1, fp) != 1 || fread(&pz, 4, 1, fp) != 1
        || fread(&startang, 2, 1, fp) != 1 || fread(&startsect, 2, 1, fp) != 1
        || fread(&nsec, 2, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    if (nsec < 1 || nsec > MAXSECTORS) {
        fclose(fp);
        return -1;
    }
    if (fread(sector, sizeof(sectortype), (size_t)nsec, fp) != (size_t)nsec) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(&nwal, 2, 1, fp) != 1 || nwal < 0 || nwal > MAXWALLS) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(wall, sizeof(walltype), (size_t)nwal, fp) != (size_t)nwal) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(&nspr, 2, 1, fp) != 1 || nspr < 0 || nspr > MAXSPRITES) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (nspr && fread(sprite, sizeof(spritetype), (size_t)nspr, fp) != (size_t)nspr) {
        fclose(fp);
        free_board();
        return -1;
    }
    /* Several shipped maps end after the sprites. The exe's read() then
       returns 0 and the count stays as it was, so the board still loads. */
    if (fread(&nextra, 2, 1, fp) != 1)
        nextra = 0;
    if (nextra < 0) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fseek(fp, (long)nextra * 10L, SEEK_CUR) != 0) {
        fclose(fp);
        free_board();
        return -1;
    }
    i = fgetc(fp);
    fclose(fp);
    if (i != EOF) {
        free_board();
        return -1;
    }

    for (i = 0; i < nspr; i++)
        insertsprite(sprite[i].sectnum, sprite[i].statnum);
    numsectors = nsec;
    numwalls = nwal;
    numsprites = nspr;
    posx = px;
    posy = py;
    posz = pz;
    ang = startang;
    cursectnum = startsect;
    horiz = ydim / 2;
    if (cursectnum >= 0 && cursectnum < numsectors)
        posz = sector[cursectnum].floorz - 9216;
    return 0;
}

int inside(int x, int y, short sect)
{
    int w;
    int first;
    int last;
    int count;

    if (sect < 0 || sect >= numsectors)
        return -1;
    first = sector[sect].wallptr;
    last = first + sector[sect].wallnum - 1;
    count = 0;
    for (w = first; w <= last; w++) {
        int x1;
        int y1;
        int p2;
        int x2;
        int y2;

        if (w < 0 || w >= numwalls)
            break;
        x1 = wall[w].x;
        y1 = wall[w].y;
        p2 = wall[w].point2;
        if (p2 < 0 || p2 >= numwalls)
            continue;
        x2 = wall[p2].x;
        y2 = wall[p2].y;
        if (x1 < x && x2 < x)
            continue;
        if (y1 > y2) {
            int t = x1;
            x1 = x2;
            x2 = t;
            t = y1;
            y1 = y2;
            y2 = t;
        }
        if (y1 <= y && y2 > y) {
            long long left = (long long)x1 * (y - y2) + (long long)(y1 - y) * x2;
            long long right = (long long)x * (y1 - y2);
            if (left <= right)
                count ^= 1;
        }
    }
    return count;
}

static int sector_at(int x, int y, short hint)
{
    int w;
    int i;

    if (inside(x, y, hint) == 1)
        return hint;
    if (hint >= 0 && hint < numsectors) {
        int first = sector[hint].wallptr;
        int last = first + sector[hint].wallnum;
        for (w = first; w < last; w++) {
            short next;
            if (w < 0 || w >= numwalls)
                break;
            next = wall[w].nextsector;
            if (next >= 0 && inside(x, y, next) == 1)
                return next;
        }
    }
    for (i = 0; i < numsectors; i++) {
        if (inside(x, y, (short)i) == 1)
            return i;
    }
    return -1;
}

static int can_stand(short from, short to)
{
    int step;
    int eye;
    int gap;

    if (to < 0 || to >= numsectors)
        return 0;
    step = sector[from].floorz - sector[to].floorz;
    if (step > 4096)
        return 0;
    gap = sector[to].floorz - sector[to].ceilingz;
    if (gap < 4096)
        return 0;
    eye = sector[to].floorz - 9216;
    if (eye < sector[to].ceilingz + 512)
        return 0;
    return 1;
}

static int try_step(int dx, int dy)
{
    int nx;
    int ny;
    int dest;

    nx = posx + dx;
    ny = posy + dy;
    dest = sector_at(nx, ny, cursectnum);
    if (dest < 0)
        return 0;
    if (dest != cursectnum && cursectnum >= 0 && !can_stand(cursectnum, (short)dest))
        return 0;
    posx = nx;
    posy = ny;
    cursectnum = (short)dest;
    posz = sector[cursectnum].floorz - 9216;
    return 1;
}

void player_move(int fvel, int svel, int avel, int tics, int bits)
{
    const short *trig;
    int co;
    int si;
    int dx;
    int dy;
    long long fs;
    long long ss;

    /* sub_31410 multiplies by the tic count once. A zero delta does not move. */
    if (tics <= 0)
        return;
    trig = sintable();
    if (!trig)
        return;
    /* Sector lotag 2 (high bit ignored) quarters a negative forward byte. */
    if (cursectnum >= 0 && cursectnum < numsectors
        && (sector[cursectnum].lotag & 0x7fff) == 2 && fvel < 0)
        fvel >>= 2;
    ang = (short)((ang + (avel * tics) / 24) & 2047);
    co = trig[(ang + 512) & 2047];
    si = trig[ang & 2047];
    fs = (long long)fvel * tics;
    ss = (long long)svel * tics;
    /* Vectors are (trig * vel * tics) >> 3, then clipmove shifts by 14.
       Positive strafe is screen-left: left arrow raises dword_1252FC. */
    dx = (int)((co * fs + si * ss) >> 17);
    dy = (int)((si * fs + (long long)trig[(ang + 1536) & 2047] * ss) >> 17);
    if (bits & 2) {
        dx >>= 1;
        dy >>= 1;
    }
    if (!try_step(dx, dy)) {
        try_step(dx, 0);
        try_step(0, dy);
    }
}

void player_look(int dh)
{
    horiz += dh;
    if (horiz < 8)
        horiz = 8;
    if (horiz > ydim - 8)
        horiz = ydim - 8;
}

/* ---- Movement and collision (0x50867..0x56414) ---- */

#define MAXCLIPNUM 512
static int clipx1[MAXCLIPNUM], clipy1[MAXCLIPNUM], clipx2[MAXCLIPNUM], clipy2[MAXCLIPNUM];
static short clipobjtag[MAXCLIPNUM], clipnum;   /* 0x132A00.., word_23DE5C */
static short clipsectorlist[MAXSECTORS], clipsectnum; /* 0x134A00, word_23DE5A */
static short hitwalls[4];                        /* 0x134E00 */
static const int stepheight = 0x4000;            /* dword_23CE2C */

static int imul32(int a, int b)
{
    return (int)((unsigned)a * (unsigned)b);
}

static int mul16(int a, int b)
{
    return (int)(((long long)a * b) >> 16);
}

/* 0x5338E */
static void addclipline(int x1, int y1, int x2, int y2, int tag)
{
    if (clipnum >= MAXCLIPNUM)
        return;
    clipx1[clipnum] = x1;
    clipy1[clipnum] = y1;
    clipx2[clipnum] = x2;
    clipy2[clipnum] = y2;
    clipobjtag[clipnum++] = (short)tag;
}

/* 0x4B137. Nonzero when the segment passes within walldist of (x,y). */
int clipinsideboxline(int x, int y, int x1, int y1, int x2, int y2, int walldist)
{
    int r = walldist << 1;

    x1 += walldist - x;
    x2 += walldist - x;
    if ((x1 < 0 && x2 < 0) || (x1 >= r && x2 >= r))
        return 0;
    y1 += walldist - y;
    y2 += walldist - y;
    if ((y1 < 0 && y2 < 0) || (y1 >= r && y2 >= r))
        return 0;
    x2 -= x1;
    y2 -= y1;
    if ((imul32(x2, -y1) - imul32(y2, -x1) <= 0) != (imul32(x2, r - y1) - imul32(y2, -x1) <= 0))
        return 1;
    if ((imul32(x2, r - y1) - imul32(y2, -x1) <= 0) != (imul32(x2, -y1) - imul32(y2, r - x1) <= 0))
        return 1;
    if ((imul32(x2, -y1) - imul32(y2, r - x1) <= 0) != (imul32(x2, r - y1) - imul32(y2, r - x1) <= 0))
        return 1;
    return 0;
}

/* The five lines that keep a mover walldist away from segment (x1,y1)-(x2,y2). */
static void keepawaylines(int x1, int y1, int x2, int y2, int tag1, int tag2, int r, int sx, int sy)
{
    int a = y2 > y1 ? -r : r, b = x2 < x1 ? -r : r;

    addclipline(x1 - sx, y1 - sx, x1 - sx, y1 + sx, tag1);
    addclipline(x2 - sx, y2 - sx, x2 - sx, y2 + sx, tag2);
    addclipline(x1 + sy, y1 - sy, x1 - sy, y1 - sy, tag1);
    addclipline(x2 + sy, y2 - sy, x2 - sy, y2 - sy, tag2);
    addclipline(x1 + a, y1 + b, x2 + a, y2 + b, tag1);
}

/* Corners of a floor sprite, in the order 0x52589 and 0x55667 build them. */
static void floorspritecorners(const spritetype *spr, int *cx, int *cy)
{
    int pic = spr->picnum;
    int xo = (signed char)(picanm[pic] >> 8), yo = (signed char)(picanm[pic] >> 16);
    int c = sintable()[(spr->ang + 512) & 2047], s = sintable()[spr->ang & 2047];
    int xs = tilesizx[pic], ys = tilesizy[pic];
    int a, b;

    if (spr->cstat & 4)
        xo = -xo;
    if (spr->cstat & 8)
        yo = -yo;
    a = ((xs >> 1) + xo) * spr->xrepeat;
    b = (yo + (ys >> 1)) * spr->yrepeat;
    cx[0] = (int)(((long long)a * s + (long long)b * c) >> 16) + spr->x;
    cy[0] = (int)(((long long)b * s + (long long)a * -c) >> 16) + spr->y;
    cx[1] = cx[0] - mul16(spr->xrepeat * xs, s);
    cy[1] = cy[0] + mul16(spr->xrepeat * xs, c);
    cx[2] = cx[1] - mul16(spr->yrepeat * ys, c);
    cx[3] = cx[0] - mul16(spr->yrepeat * ys, c);
    cy[2] = cy[1] - mul16(spr->yrepeat * ys, s);
    cy[3] = cy[0] - mul16(spr->yrepeat * ys, s);
}

static void wallspriteends(const spritetype *spr, int *x1, int *y1, int *x2, int *y2)
{
    int pic = spr->picnum;
    int xo = (signed char)(picanm[pic] >> 8);
    int xv = sintable()[spr->ang & 2047] * spr->xrepeat;
    int yv = sintable()[(spr->ang + 1536) & 2047] * spr->xrepeat;
    int k;

    if (spr->cstat & 4)
        xo = -xo;
    k = xo + (tilesizx[pic] >> 1);
    *x1 = spr->x - mul16(k, xv);
    *y1 = spr->y - mul16(k, yv);
    *x2 = *x1 + mul16(tilesizx[pic], xv);
    *y2 = *y1 + mul16(tilesizx[pic], yv);
}

/* 0x52589 */
static void getclips(int x, int y, int z, int goalx, int goaly, short sectnum,
    int walldist, int ceildist, int flordist, int cliptype)
{
    int cx = (x + goalx) >> 1, cy = (y + goaly) >> 1;
    int rad = ksqrt(imul32(goalx - x, goalx - x) + imul32(goaly - y, goaly - y)) + (walldist << 1) + 8;
    int xmin = cx - rad, ymin = cy - rad, xmax = cx + rad, ymax = cy + rad;
    int sx = goalx < x ? -walldist : walldist, sy = goaly < y ? -walldist : walldist;
    int k, i;

    clipnum = 0;
    clipsectnum = 1;
    clipsectorlist[0] = sectnum;
    for (k = 0; k < clipsectnum; k++) {
        sectortype *sec = &sector[clipsectorlist[k]];
        int w, endw = sec->wallptr + sec->wallnum - 1;

        for (w = sec->wallptr; w <= endw; w++) {
            int p2 = wall[w].point2;
            int x1 = wall[w].x, y1 = wall[w].y, x2 = wall[p2].x, y2 = wall[p2].y;
            int dy = y2 - y1, t, side, blocked;
            short ns;

            if (!(imul32(y2 - y, x1 - x) >= imul32(y1 - y, x2 - x)
                    && (x1 >= xmin || x2 >= xmin) && (x1 <= xmax || x2 <= xmax)
                    && (y1 >= ymin || ymin <= y2) && (y1 <= ymax || ymax >= y2)))
                continue;
            if (y1 >= y2) {
                t = imul32(dy, xmin - x1);
                side = x2 > x1 ? imul32(x2 - x1, ymin - y1) - t : imul32(x2 - x1, ymax - y1) - t;
            } else {
                t = imul32(xmax - x1, dy);
                side = x2 > x1 ? imul32(x2 - x1, ymin - y1) - t : imul32(x2 - x1, ymax - y1) - t;
            }
            if (side >= 0)
                continue;
            ns = wall[w].nextsector;
            blocked = ns == -1;
            if ((cliptype == 0 || cliptype == 2) && (wall[w].cstat & 1))
                blocked = 1;
            if (cliptype == 1 && (wall[w].cstat & 0x40))
                blocked = 1;
            if (ns >= 0) {
                if (!(sector[ns].ceilingstat & 1) && sector[ns].ceilingz + ceildist - 1 >= z)
                    blocked = 1;
                if (!(sector[ns].floorstat & 1)) {
                    if (sector[ns].floorz - (flordist - 1) <= z)
                        blocked = 1;
                    if (cliptype == 2 && stepheight + z < sector[ns].floorz)
                        blocked = 1;
                }
            }
            if (blocked) {
                keepawaylines(x1, y1, x2, y2, w + 0x8000, p2 + 0x8000, walldist, sx, sy);
                continue;
            }
            for (i = clipsectnum - 1; i >= 0; i--)
                if (clipsectorlist[i] == ns)
                    break;
            if (i < 0)
                clipsectorlist[clipsectnum++] = ns;
        }
    }
    for (k = 0; k < clipsectnum; k++) {
        for (i = headspritesect[clipsectorlist[k]]; i != -1; i = nextspritesect[i]) {
            spritetype *spr = &sprite[i];
            int cstat = spr->cstat;
            int height = 4 * spr->yrepeat * tilesizy[spr->picnum];

            if (!(cstat & 1))
                continue;
            switch (cstat & 0x30) {
            case 0:
                if (xmin - walldist <= spr->x && xmax + walldist >= spr->x
                    && ymin - walldist <= spr->y && ymax + walldist >= spr->y
                    && ceildist + spr->z >= z && spr->z - flordist - height <= z) {
                    int s2 = goalx < x ? -2 * walldist : 2 * walldist;
                    int t2 = goaly < y ? -2 * walldist : 2 * walldist;

                    addclipline(spr->x - s2, spr->y - s2, spr->x - s2, spr->y + s2, i + 0xC000);
                    addclipline(spr->x + t2, spr->y - t2, spr->x - t2, spr->y - t2, i + 0xC000);
                }
                break;
            case 0x10: {
                int x1, y1, x2, y2;

                if (!(ceildist + spr->z >= z && spr->z - flordist - height <= z))
                    break;
                wallspriteends(spr, &x1, &y1, &x2, &y2);
                if (clipinsideboxline(cx, cy, x1, y1, x2, y2, rad) <= 0)
                    break;
                if (imul32(y2 - y, x1 - x) < imul32(y1 - y, x2 - x)) {
                    int t;

                    t = x1;
                    x1 = x2;
                    x2 = t;
                    t = y1;
                    y1 = y2;
                    y2 = t;
                }
                keepawaylines(x1, y1, x2, y2, i + 0xC000, i + 0xC000, walldist, sx, sy);
                break;
            }
            case 0x20: {
                int fx[4], fy[4], e;

                if (!(ceildist + spr->z >= z && spr->z - flordist <= z))
                    break;
                floorspritecorners(spr, fx, fy);
                for (e = 0; e < 4; e++) {
                    int j = (e + 1) & 3;
                    int a = fy[e] > fy[j] ? -walldist : walldist;
                    int b = fx[e] < fx[j] ? -walldist : walldist;

                    if (!(imul32(fx[j] - fx[e], y - fy[e]) < imul32(fy[j] - fy[e], x - fx[e])))
                        continue;
                    if (clipinsideboxline(cx, cy, fx[e], fy[e], fx[j], fy[j], rad) <= 0)
                        continue;
                    addclipline(fx[e] - sx, fy[e] - sx, fx[e] - sx, fy[e] + sx, i + 0xC000);
                    addclipline(fx[j] - sx, fy[j] - sx, fx[j] - sx, fy[j] + sx, i + 0xC000);
                    addclipline(fx[e] + sy, fy[e] - sy, fx[e] - sy, fy[e] - sy, i + 0xC000);
                    addclipline(fx[j] + sy, fy[j] - sy, fx[j] - sy, fy[j] - sy, i + 0xC000);
                    addclipline(fx[j] + a, fy[j] + b, fx[e] + a, fy[e] + b, i + 0xC000);
                }
                break;
            }
            }
        }
    }
}

/* 0x53532. The nearest clip line the move crosses, or -1. */
static int raytrace(int x3, int y3, int xvect, int yvect, int *intx, int *inty)
{
    int x4 = x3 + (xvect >> 14), y4 = y3 + (yvect >> 14);
    int i, hit = -1;

    *intx = x4;
    *inty = y4;
    for (i = 0; i < clipnum; i++) {
        int x1 = clipx1[i], y1 = clipy1[i], x2 = clipx2[i], y2 = clipy2[i];
        int topt, bot, nintx, ninty, cnt;

        topt = imul32(y3 - y1, x2 - x1) - imul32(y2 - y1, x3 - x1);
        if (topt < 0)
            continue;
        if (imul32(x2 - x1, y4 - y1) - imul32(y2 - y1, x4 - x1) >= 0)
            continue;
        if (imul32(y1 - y3, x4 - x3) - imul32(y4 - y3, x1 - x3) > 0)
            continue;
        if (imul32(x4 - x3, y2 - y3) - imul32(y4 - y3, x2 - x3) <= 0)
            continue;
        bot = imul32(y3 - y4, x2 - x1) - imul32(x3 - x4, y2 - y1);
        if (bot == 0)
            continue;
        cnt = 256;
        do {
            nintx = (int)((long long)topt * (x4 - x3) / bot) + x3;
            ninty = (int)((long long)topt * (y4 - y3) / bot) + y3;
            cnt--;
            topt--;
            if (cnt < 0) {
                *intx = x3;
                *inty = y3;
                return i;
            }
        } while (imul32(ninty - y1, x2 - x1) - imul32(nintx - x1, y2 - y1) <= 0);
        if ((x3 - nintx < 0 ? nintx - x3 : x3 - nintx) + (y3 - ninty < 0 ? ninty - y3 : y3 - ninty)
            < (x3 - *intx < 0 ? *intx - x3 : x3 - *intx) + (y3 - *inty < 0 ? *inty - y3 : y3 - *inty)) {
            *intx = nintx;
            *inty = ninty;
            hit = i;
        }
    }
    return hit;
}

/* 0x533CF */
static void keepaway(int *x, int *y, int w)
{
    int x1 = clipx1[w], y1 = clipy1[w], dx = clipx2[w] - x1, dy = clipy2[w] - y1;
    int ox = dx > 0 ? 1 : dx < 0 ? -1 : 0, oy = dy > 0 ? -1 : dy < 0 ? 1 : 0;
    int first = (dx < 0 ? -dx : dx) <= (dy < 0 ? -dy : dy);

    while (imul32(*y - y1, dx) - imul32(dy, *x - x1) <= 0) {
        if (first)
            *y += ox;
        else
            *x += oy;
        first = !first;
    }
}

/* 0x5225C. Returns the object tag of the first thing hit, or 0. */
int clipmove(int *x, int *y, int *z, short *sectnum, int xvect, int yvect,
    int walldist, int ceildist, int flordist, int cliptype)
{
    int goalx, goaly, intx, inty, hitwall = 0, cnt, retval = 0, i;
    int oxvect = xvect, oyvect = yvect;

    if ((xvect | yvect) == 0)
        return 0;
    goalx = (xvect >> 14) + *x;
    goaly = (yvect >> 14) + *y;
    getclips(*x, *y, *z, goalx, goaly, *sectnum, walldist, ceildist, flordist, cliptype);
    for (cnt = 3; (xvect | yvect) && hitwall >= 0 && cnt > 0; cnt--) {
        hitwall = raytrace(*x, *y, xvect, yvect, &intx, &inty);
        if (hitwall >= 0) {
            int lx = clipx2[hitwall] - clipx1[hitwall], ly = clipy2[hitwall] - clipy1[hitwall];
            int t = imul32(ly, ly) + imul32(lx, lx);

            if (t > 0) {
                int k = (int)(((long long)(imul32(lx, goalx - intx) + imul32(ly, goaly - inty)) << 20) / t);

                xvect = (int)(((long long)k * lx) >> 6);
                yvect = (int)(((long long)k * ly) >> 6);
            }
            for (i = cnt + 1; i <= 3; i++) {
                int j = hitwalls[i];

                if ((imul32(ly, oyvect >> 6) + imul32(oxvect >> 6, lx) < 0)
                    != (imul32(oxvect >> 6, clipx2[j] - clipx1[j]) + imul32(oyvect >> 6, clipy2[j] - clipy1[j]) < 0))
                    return retval;
            }
            goalx = (xvect >> 14) + intx;
            goaly = (yvect >> 14) + inty;
            keepaway(&goalx, &goaly, hitwall);
            xvect = (goalx - intx) << 14;
            yvect = (goaly - inty) << 14;
            if (cnt == 3)
                retval = (unsigned short)clipobjtag[hitwall];
            hitwalls[cnt] = (short)hitwall;
        }
        *x = intx;
        *y = inty;
    }
    for (i = 0; i < clipsectnum; i++) {
        if (inside(*x, *y, clipsectorlist[i]) == 1) {
            *sectnum = clipsectorlist[i];
            return retval;
        }
    }
    for (i = 0; i < numsectors; i++) {
        if (inside(*x, *y, (short)i) == 1) {
            *sectnum = (short)i;
            return retval;
        }
    }
    *sectnum = -1;
    return retval;
}

/* 0x55667 */
void getzrange(int x, int y, int z, short sectnum, int *ceilz, int *ceilhit, int *florz, int *florhit,
    int walldist, int cliptype)
{
    int r = 2 * walldist + 1;
    int xmin = x - r, ymin = y - r, xmax = x + r, ymax = y + r;
    int k, i;

    *ceilz = sector[sectnum].ceilingz;
    *florz = sector[sectnum].floorz;
    *ceilhit = sectnum + 0x4000;
    *florhit = sectnum + 0x4000;
    clipsectorlist[0] = sectnum;
    clipsectnum = 1;
    for (k = 0; k < clipsectnum; k++) {
        sectortype *sec = &sector[clipsectorlist[k]];
        int w, endw = sec->wallptr + sec->wallnum - 1;

        for (w = sec->wallptr; w <= endw; w++) {
            short ns = wall[w].nextsector;
            int p2, x1, y1, x2, y2, side;

            if (ns < 0)
                continue;
            p2 = wall[w].point2;
            x1 = wall[w].x;
            y1 = wall[w].y;
            x2 = wall[p2].x;
            y2 = wall[p2].y;
            if ((x1 < xmin && x2 < xmin) || (x1 > xmax && x2 > xmax) || (y1 < ymin && y2 < ymin)
                || (y1 > ymax && y2 > ymax) || imul32(y2 - y, x1 - x) < imul32(x2 - x, y1 - y))
                continue;
            if (y2 <= y1)
                side = x2 > x1 ? imul32(x2 - x1, ymin - y1) - imul32(xmin - x1, y2 - y1)
                               : imul32(ymax - y1, x2 - x1) - imul32(xmin - x1, y2 - y1);
            else
                side = x2 > x1 ? imul32(x2 - x1, ymin - y1) - imul32(xmax - x1, y2 - y1)
                               : imul32(ymax - y1, x2 - x1) - imul32(xmax - x1, y2 - y1);
            if (side >= 0)
                continue;
            if ((cliptype == 0 || cliptype == 2) && (wall[w].cstat & 1))
                continue;
            if (cliptype == 1 && (wall[w].cstat & 0x40))
                continue;
            if (!((sector[ns].ceilingstat & 1) || sector[ns].ceilingz + 768 < z))
                continue;
            if (!((sector[ns].floorstat & 1)
                    || (sector[ns].floorz - 768 > z && (cliptype != 2 || stepheight + z >= sector[ns].floorz))))
                continue;
            for (i = clipsectnum - 1; i >= 0; i--)
                if (clipsectorlist[i] == ns)
                    break;
            if (i >= 0)
                continue;
            clipsectorlist[clipsectnum++] = ns;
            if (!((x1 >= walldist + xmin || x2 >= walldist + xmin) && (x1 <= xmax - walldist || x2 <= xmax - walldist)
                    && (walldist + ymin <= y1 || y2 >= walldist + ymin) && (ymax - walldist >= y1 || y2 <= ymax - walldist)
                    && imul32(x1 - x, y2 - y) >= imul32(x2 - x, y1 - y)))
                continue;
            if (y2 <= y1)
                side = x2 > x1 ? imul32(x2 - x1, walldist + ymin - y1) - imul32(walldist + xmin - x1, y2 - y1)
                               : imul32(x2 - x1, ymax - walldist - y1) - imul32(walldist + xmin - x1, y2 - y1);
            else
                side = x2 > x1 ? imul32(x2 - x1, walldist + ymin - y1) - imul32(y2 - y1, xmax - walldist - x1)
                               : imul32(x2 - x1, ymax - walldist - y1) - imul32(y2 - y1, xmax - walldist - x1);
            if (side < 0) {
                if (sector[ns].ceilingz > *ceilz) {
                    *ceilz = sector[ns].ceilingz;
                    *ceilhit = ns + 0x4000;
                }
                if (sector[ns].floorz < *florz) {
                    *florz = sector[ns].floorz;
                    *florhit = ns + 0x4000;
                }
            }
        }
    }
    for (k = 0; k < clipsectnum; k++) {
        for (i = headspritesect[clipsectorlist[k]]; i != -1; i = nextspritesect[i]) {
            spritetype *spr = &sprite[i];
            int top = 0, bot = 0, use = 0;

            if (!(spr->cstat & 1))
                continue;
            switch (spr->cstat & 0x30) {
            case 0:
                if (spr->x < xmin || spr->x > xmax || spr->y < ymin || spr->y > ymax)
                    break;
                top = spr->z + 1024;
                bot = spr->z - 1024 - 4 * tilesizy[spr->picnum] * spr->yrepeat;
                use = 1;
                break;
            case 0x10: {
                int x1, y1, x2, y2;

                wallspriteends(spr, &x1, &y1, &x2, &y2);
                if (clipinsideboxline(x, y, x1, y1, x2, y2, walldist + 1) <= 0)
                    break;
                top = spr->z + 1024;
                bot = spr->z - 1024 - 4 * spr->yrepeat * tilesizy[spr->picnum];
                use = 1;
                break;
            }
            case 0x20: {
                int fx[4], fy[4];

                floorspritecorners(spr, fx, fy);
                if (clipinsideboxline(x, y, fx[0], fy[0], fx[1], fy[1], walldist + 1) > 0
                    || clipinsideboxline(x, y, fx[1], fy[1], fx[2], fy[2], walldist + 1) > 0
                    || clipinsideboxline(x, y, fx[2], fy[2], fx[3], fy[3], walldist + 1) > 0
                    || clipinsideboxline(x, y, fx[3], fy[3], fx[0], fy[0], walldist + 1) > 0
                    || (imul32(fx[1] - fx[0], y - fy[0]) >= imul32(x - fx[0], fy[1] - fy[0])
                        && imul32(y - fy[1], fx[2] - fx[1]) >= imul32(x - fx[1], fy[2] - fy[1])
                        && imul32(y - fy[2], fx[3] - fx[2]) >= imul32(x - fx[2], fy[3] - fy[2])
                        && imul32(y - fy[3], fx[0] - fx[3]) >= imul32(x - fx[3], fy[0] - fy[3]))) {
                    top = spr->z + 1024;
                    bot = spr->z - 1024;
                    use = 1;
                }
                break;
            }
            }
            if (!use)
                continue;
            if (top < z && top > *ceilz) {
                *ceilz = top;
                *ceilhit = i + 0xC000;
            }
            if (bot > z && bot < *florz) {
                *florz = bot;
                *florhit = i + 0xC000;
            }
        }
    }
}

/* 0x50CA3 */
static void deletespritesect(short s)
{
    short sect = sprite[s].sectnum;

    if (sect == MAXSECTORS)
        return;
    if (headspritesect[sect] == s)
        headspritesect[sect] = nextspritesect[s];
    if (prevspritesect[s] >= 0)
        nextspritesect[prevspritesect[s]] = nextspritesect[s];
    if (nextspritesect[s] >= 0)
        prevspritesect[nextspritesect[s]] = prevspritesect[s];
    if (headspritesect[MAXSECTORS] >= 0)
        prevspritesect[headspritesect[MAXSECTORS]] = s;
    prevspritesect[s] = -1;
    nextspritesect[s] = headspritesect[MAXSECTORS];
    headspritesect[MAXSECTORS] = s;
    sprite[s].sectnum = MAXSECTORS;
}

/* 0x50D7E */
static void deletespritestat(short s)
{
    short stat = sprite[s].statnum;

    if (stat == MAXSTATUS)
        return;
    if (headspritestat[stat] == s)
        headspritestat[stat] = nextspritestat[s];
    if (prevspritestat[s] >= 0)
        nextspritestat[prevspritestat[s]] = nextspritestat[s];
    if (nextspritestat[s] >= 0)
        prevspritestat[nextspritestat[s]] = prevspritestat[s];
    if (headspritestat[MAXSTATUS] >= 0)
        prevspritestat[headspritestat[MAXSTATUS]] = s;
    prevspritestat[s] = -1;
    nextspritestat[s] = headspritestat[MAXSTATUS];
    headspritestat[MAXSTATUS] = s;
    sprite[s].statnum = MAXSTATUS;
}

/* 0x50C8F */
void deletesprite(short s)
{
    deletespritestat(s);
    deletespritesect(s);
}

/* 0x50E57. Reuses the slot just freed, as the exe does. */
int changespritesect(short s, short sectnum)
{
    if ((unsigned short)sectnum > MAXSECTORS || sprite[s].sectnum == sectnum || sprite[s].sectnum == MAXSECTORS)
        return -1;
    deletespritesect(s);
    insertspritesect(sectnum);
    return 0;
}

/* 0x50EA0 */
int changespritestat(short s, short statnum)
{
    if ((unsigned short)statnum > MAXSTATUS || sprite[s].statnum == statnum || sprite[s].statnum == MAXSTATUS)
        return -1;
    deletespritestat(s);
    insertspritestat(statnum);
    return 0;
}

/* 0x50867 */
int setsprite(short s, int x, int y, int z)
{
    short sect = sprite[s].sectnum;

    sprite[s].x = x;
    sprite[s].y = y;
    sprite[s].z = z;
    updatesector(x, y, &sect);
    if (sect < 0)
        return -1;
    if (sect != sprite[s].sectnum)
        changespritesect(s, sect);
    return 0;
}

/* 0x53861 */
void rotatepoint(int xpivot, int ypivot, int x, int y, short daang, int *x2, int *y2)
{
    int dx = x - xpivot, dy = y - ypivot;
    int c = sintable()[(daang + 2560) & 2047], s = sintable()[(daang + 2048) & 2047];

    *x2 = (int)(((long long)c * dx + (long long)s * (ypivot - y)) >> 14) + xpivot;
    *y2 = (int)(((long long)c * dy + (long long)s * dx) >> 14) + ypivot;
}

/* 0x51F01. mode 1 also accepts a crossing that only touches the second
   segment's far side, as hitscan wants. */
static int lintersect(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int x4, int y4,
    int *intx, int *inty, int *intz, int mode)
{
    int t, s1, s2, d1, d2, bot, top, ok = 0;

    if (mode == 0
        && ((x1 < x3 && x1 < x4 && x2 < x3 && x2 < x4) || (x1 > x3 && x1 > x4 && x2 > x3 && x2 > x4)
            || (y1 < y3 && y1 < y4 && y3 > y2 && y2 < y4) || (y1 > y3 && y1 > y4 && y3 < y2 && y2 > y4)))
        return 0;
    t = imul32(y2 - y1, x4 - x1) - imul32(y4 - y1, x2 - x1);
    s1 = imul32(x3 - x1, y2 - y1) - imul32(x2 - x1, y3 - y1);
    if ((s1 > 0 || t < 0) && (s1 < 0 || t > 0)) {
        /* the segments do not straddle */
    } else {
        d1 = imul32(y4 - y3, x1 - x3) - imul32(x4 - x3, y1 - y3);
        d2 = imul32(x2 - x3, y4 - y3) - imul32(x4 - x3, y2 - y3);
        if ((d1 <= 0 && d2 >= 0) || (d1 >= 0 && d2 <= 0))
            ok = 1;
        if (mode == 1) {
            s2 = d2 < 0 ? imul32(x4 - x3, y2 - y3) - imul32(x2 - x3, y4 - y3) : d2;
            t = d1 < 0 ? imul32(x4 - x3, y1 - y3) - imul32(y4 - y3, x1 - x3) : d1;
            if (s2 < t)
                ok = 1;
        }
    }
    if (ok != 1)
        return 0;
    bot = imul32(x2 - x1, y3 - y4) - imul32(x3 - x4, y2 - y1);
    if (bot == 0)
        return 0;
    top = imul32(y3 - y4, x3 - x1) - imul32(y3 - y1, x3 - x4);
    *intx = (int)((long long)top * (x2 - x1) / bot) + x1;
    *inty = (int)((long long)top * (y2 - y1) / bot) + y1;
    *intz = (int)((long long)top * (z2 - z1) / bot) + z1;
    return 1;
}

static int manhattan(int x, int y, int ax, int ay)
{
    return (x - ax < 0 ? ax - x : x - ax) + (y - ay < 0 ? ay - y : y - ay);
}

/* 0x511C4. The ray runs from (xs,ys,zs) to (xs+vx, ys+vy, zs+vz). */
void hitscan(int xs, int ys, int zs, short sectnum, int vx, int vy, int vz,
    short *hitsect, short *hitwall, short *hitsprite, int *hitx, int *hity, int *hitz)
{
    int x2 = vx + xs, y2 = vy + ys, z2 = vz + zs;
    short tempshort, k;
    int cnt = 1;

    if (sectnum == -1)
        return;
    *hitsect = -1;
    clipsectorlist[0] = sectnum;
    for (tempshort = 0; tempshort < cnt; tempshort++) {
        short dasector = clipsectorlist[tempshort];
        int startwall = sector[dasector].wallptr;
        int endwall = startwall + sector[dasector].wallnum - 1;
        int z;

        for (z = startwall; z <= endwall; z++) {
            walltype *wal = &wall[z];
            int x1 = wal->x, y1 = wal->y, wx2 = wall[wal->point2].x, wy2 = wall[wal->point2].y;
            int intx, inty, intz;
            short ns;

            if (imul32(wy2 - ys, x1 - xs) < imul32(y1 - ys, wx2 - xs))
                continue;
            if (lintersect(xs, ys, zs, x2, y2, z2, x1, y1, wx2, wy2, &intx, &inty, &intz, 1) != 1)
                continue;
            ns = wal->nextsector;
            if (!(vz == 0 || (intz > sector[dasector].ceilingz && intz < sector[dasector].floorz))) {
                if (*hitsect == -1 || manhattan(intx, inty, xs, ys) < manhattan(*hitx, *hity, xs, ys)) {
                    *hitsect = dasector;
                    *hitwall = -1;
                    *hitsprite = -1;
                    *hitz = vz <= 0 ? sector[dasector].ceilingz : sector[dasector].floorz;
                    *hitx = (int)((long long)vx * (*hitz - zs) / vz) + xs;
                    *hity = (int)((long long)vy * (*hitz - zs) / vz) + ys;
                }
                continue;
            }
            if (ns == -1 || (wal->cstat & 0x40)
                || intz <= sector[ns].ceilingz || intz >= sector[ns].floorz) {
                if (*hitsect == -1 || manhattan(intx, inty, xs, ys) < manhattan(*hitx, *hity, xs, ys)) {
                    *hitsect = dasector;
                    *hitwall = (short)z;
                    *hitsprite = -1;
                    *hitx = intx;
                    *hity = inty;
                    *hitz = intz;
                    continue;
                }
                /* A farther miss through a portal still opens the sector behind it. */
                if (ns == -1 || (wal->cstat & 0x40))
                    continue;
            }
            for (k = 0; k < cnt; k++)
                if (clipsectorlist[k] == ns)
                    break;
            if (k == cnt)
                clipsectorlist[cnt++] = ns;
        }
        for (k = headspritesect[dasector]; k != -1; k = nextspritesect[k]) {
            spritetype *spr = &sprite[k];
            int topt, bot, intz, dx, dy, sx, sy;

            if (!(spr->cstat & 1) || (spr->cstat & 0x30) == 0x20)
                continue;
            topt = imul32(spr->y - ys, vy) + imul32(spr->x - xs, vx);
            if (topt <= 0)
                continue;
            bot = imul32(vy, vy) + imul32(vx, vx);
            if (bot == 0)
                continue;
            intz = (int)((long long)topt * vz / bot) + zs;
            if (!(spr->z >= intz && spr->z - ((tilesizy[spr->picnum] * spr->yrepeat) << 8 >> 6) <= intz))
                continue;
            dx = (int)((long long)(imul32(spr->y - ys, vx) - imul32(spr->x - xs, vy)) * vx / bot);
            dy = (int)((long long)(imul32(spr->y - ys, vx) - imul32(spr->x - xs, vy)) * vy / bot);
            sx = tilesizx[spr->picnum];
            if (imul32(dy, dy) + imul32(dx, dx) > (imul32(sx * spr->xrepeat, sx * spr->xrepeat) >> 7))
                continue;
            sx = (int)((long long)topt * vx / bot) + xs;
            sy = (int)((long long)topt * vy / bot) + ys;
            if (*hitsect == -1 || manhattan(sx, sy, xs, ys) < manhattan(*hitx, *hity, xs, ys)) {
                *hitsect = dasector;
                *hitwall = -1;
                *hitsprite = k;
                *hitx = sx;
                *hity = sy;
                *hitz = intz;
            }
        }
    }
}

/* 0x508C9. Moves sprite s by the change times tics; returns what clipmove
   hit, or sector+0x4000 when the new z leaves the sector. */
int movesprite(short s, int xchange, int ychange, int zchange, int walldist, int ceildist, int flordist,
    int cliptype, int tics)
{
    spritetype *spr = &sprite[s];
    short sect = spr->sectnum;
    int z = spr->z - 2 * tilesizy[spr->picnum] * spr->yrepeat;
    int retval;

    retval = clipmove(&spr->x, &spr->y, &z, &sect, (xchange * tics) << 11, (ychange * tics) << 11,
        walldist, ceildist, flordist, cliptype);
    if (sect != spr->sectnum && sect >= 0)
        changespritesect(s, sect);
    z = ((tics * zchange) >> 3) + spr->z;
    if (sect < 0)
        return retval; /* the exe reads sector[-1] here */
    if (z <= sector[sect].ceilingz || z > sector[sect].floorz) {
        if (retval == 0)
            return sect + 0x4000;
    } else {
        spr->z = z;
    }
    return retval;
}

/* 0x51016. Both sectors start the search list; a wall the line crosses must
   open onto a sector whose span contains the crossing height. */
int cansee(int x1, int y1, int z1, short sect1, int x2, int y2, int z2, short sect2)
{
    short list[MAXSECTORS];
    int cnt = 2, k;

    if (x1 == x2 && y1 == y2)
        return 1;
    list[0] = sect1;
    list[1] = sect2;
    for (k = 0; k < cnt; k++) {
        short dasect = list[k];
        int w, end = sector[dasect].wallptr + sector[dasect].wallnum - 1;

        for (w = sector[dasect].wallptr; w <= end; w++) {
            int ix, iy, iz, j;
            short ns;

            if (lintersect(x1, y1, z1, x2, y2, z2, wall[w].x, wall[w].y, wall[wall[w].point2].x,
                    wall[wall[w].point2].y, &ix, &iy, &iz, 0) != 1)
                continue;
            ns = wall[w].nextsector;
            if (ns == -1 || iz <= sector[dasect].ceilingz || iz >= sector[dasect].floorz
                || iz <= sector[ns].ceilingz || iz >= sector[ns].floorz)
                return 0;
            for (j = 0; j < cnt; j++)
                if (list[j] == ns)
                    break;
            if (j == cnt && cnt < MAXSECTORS)
                list[cnt++] = ns;
        }
    }
    return 1;
}

/* 0x5221E. The wall whose point2 is w. */
short lastwall(short w)
{
    short i = w;
    int cnt = 0x2000;

    while (wall[i].point2 != w && cnt > 0) {
        i = wall[i].point2;
        cnt--;
    }
    return cnt > 0 ? i : w;
}

/* 0x52122. Moves wall point w and every point joined to it through red
   walls, first clockwise, then (if that hit a white wall) the other way. */
void dragpoint(short w, int x, int y)
{
    short t = w;
    int cnt = 0x2000;

    wall[w].x = x;
    wall[w].y = y;
    while (wall[t].nextwall >= 0) {
        t = wall[wall[t].nextwall].point2;
        wall[t].x = x;
        wall[t].y = y;
        if (t == w || (short)--cnt <= 0)
            return;
    }
    t = w;
    for (;;) {
        short lw = lastwall(t);

        if (wall[lw].nextwall < 0)
            return;
        t = wall[lw].nextwall;
        wall[t].x = x;
        wall[t].y = y;
        if (t == w || (short)--cnt <= 0)
            return;
    }
}

/* 0x4AF61. Whether wall w passes within dist of (x,y). */
int clipinsidebox(int x, int y, short w, int dist)
{
    int r2 = dist << 1, x1 = wall[w].x + dist - x, y1 = wall[w].y + dist - y;
    int x2 = wall[wall[w].point2].x + dist - x, y2 = wall[wall[w].point2].y + dist - y, a, b, c;

    if ((x1 < 0 && x2 < 0) || (x1 >= r2 && x2 >= r2))
        return 0;
    if ((y1 < 0 && y2 < 0) || (y1 >= r2 && y2 >= r2))
        return 0;
    c = imul32(y2 - y1, -x1);
    a = imul32(x2 - x1, -y1) - c;
    b = imul32(r2 - y1, x2 - x1) - c;
    if ((a <= 0) != (b <= 0))
        return 1;
    a = imul32(-y1, x2 - x1) - imul32(y2 - y1, r2 - x1);
    if ((b <= 0) != (a <= 0))
        return 1;
    return (a <= 0) != (imul32(x2 - x1, r2 - y1) - imul32(y2 - y1, r2 - x1) <= 0);
}
