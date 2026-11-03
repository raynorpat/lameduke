/* The classic renderer from D3D.EXE: drawrooms 0x43F18 and the drawers under it.
   Addresses name the routine or table each piece comes from. Mode 13h, Mode X
   and the linear modes all produce the same picture; this draws it linearly. */

#include "draw.h"

#include "board.h"
#include "display.h"
#include "engine.h"
#include "math.h"
#include "tiles.h"

#include <string.h>

#define MAXXDIM 640
#define MAXYDIM 480
#define MAXWALLSB 4096 /* the exe; wall sprites use slot 4095 */

static unsigned char frame[MAXXDIM * MAXYDIM];

/* View state (0x23C3A4..0x23C3F8, 0x23DE6E). */
static int globalposx, globalposy, globalposz, globalhoriz, globalhorizraw;
static short globalang, globalcursectnum;
static int cosglobalang, singlobalang;
static int xdimen = 320, ydimen = 200, viewingrange = 65536, xdimscale = 65536;
static int windowx1, windowy1, windowx2, windowy2;
static int wtop, wbot;              /* 0x23C3DC, 0x23C3CC: clip rows 0 and 200 */
static int frameoffset;             /* 0x23C3E8 */
static int totalclocklock;          /* 0x23C320 */
int totalclock;                     /* 0x23CE28 */
int visibility = 13;                /* byte_23DE9E, set by initengine */
int parallaxtype = 2;               /* byte_23DE9D */
int parallaxyoffs;                  /* 0x23CE20 */

/* Per-surface state used by the drawers (0x23C35C..0x23C394, 0x23DE6A/6C). */
static int globalshade, globalpal, globalorientation;
static int flatpal;                 /* 0x23C398: the hline palookup, kept apart from globalpal */
static int globalxpanning, globalypanning, globalzd, globalyscale;
static short globalpicnum, globalshiftval;
static unsigned char *globalbufplc;
static int globalx1, globaly1, globalx2, globaly2, globalzx; /* 0x23C370..0x23C394 */
static unsigned char xshift, yshift;  /* byte_23DE9B, byte_23DE9A */

/* Column arrays. */
static int startumost[MAXXDIM], startdmost[MAXXDIM];
static int umost[MAXXDIM], dmost[MAXXDIM];   /* 0x13BB90, 0x13B190 */
static int uwall[MAXXDIM], dwall[MAXXDIM];   /* 0x138990, 0x137F90 */
static int uwall2[MAXXDIM], dwall2[MAXXDIM]; /* 0x13A790, 0x139D90 */
static int swall[MAXXDIM + 1];               /* 0x139390 */
static int lwall[MAXXDIM + 4];               /* 0x23C3FC */
static int skyswall[MAXXDIM + 1], skylwall[MAXXDIM + 1]; /* 0x137590, 0x136B90 */
static short radarang[MAXXDIM + 1];          /* word_198E90 */
static int lastx[MAXYDIM + 2];               /* 0x134E08, indexed by y */
static int horizlookup[1024];                /* dword_135AD0: 65536/(i-511), centred at 0x1362D0 */

/* Scan and bunch lists (0x188E90..0x17E590). */
static int xb1[MAXWALLSB], xb2[MAXWALLSB], yb1[MAXWALLSB], yb2[MAXWALLSB];
static int rx1[MAXWALLSB], ry1[MAXWALLSB], rx2[MAXWALLSB], ry2[MAXWALLSB];
static short thesector[MAXWALLSB], thewall[MAXWALLSB], p2[MAXWALLSB];
static short bunchfirst[MAXWALLSB], bunchlast[MAXWALLSB];
static unsigned char bunchgot[MAXWALLSB];
static short numscans, numbunches, numhits;
static short sectorstack[MAXSECTORS];
static short sectorstackcnt;
static unsigned char gotsector[(MAXSECTORS + 7) >> 3];

/* Lists drawmasks consumes. */
short maskwall[MAXWALLSB], maskwallcnt;     /* word_17A590, 23DE72 */
short smostwall[MAXWALLSB], smoststart[MAXWALLSB], smostwallcnt; /* 17C590, 15C590, 23DE74 */
short smost[40960];                         /* word_13C590 */
int smostcnt;                               /* 23DE76 */
short tspritelist[MAXSPRITES];              /* 1B4F90 */
int spritesx[MAXSPRITES], spritesy[MAXSPRITES]; /* 150590, 154590 */
short tspritepicnum[MAXSPRITES];            /* 1B8410 */
unsigned char tspritepal[MAXSPRITES];       /* 1B7010 */
int spritesortcnt;                          /* 23CE18 */

static int mulscale(int a, int b, int n)
{
    return (int)(((long long)a * b) >> n);
}

static int divscale(int a, int b, int n)
{
    if (b == 0)
        return 0;
    return (int)(((long long)a << n) / b);
}

static int dmulscale(int a, int b, int c, int d, int n)
{
    return (int)(((long long)a * b + (long long)c * d) >> n);
}

/* 32-bit imul, wrapping as the x86 does. */
static int imul(int a, int b)
{
    return (int)((unsigned)a * (unsigned)b);
}

static int ksgn(int v)
{
    return (v > 0) - (v < 0);
}

/* 0x509E2 */
static int animateoffs(short pic)
{
    int k = totalclocklock >> ((picanm[pic] >> 24) & 15);
    int frames = picanm[pic] & 63;
    int i;

    if (!frames)
        return 0;
    switch (picanm[pic] & 0xC0) {
    case 0x40:
        i = k % (frames << 1);
        return i < frames ? i : (frames << 1) - i;
    case 0x80:
        return k % (frames + 1);
    case 0xC0:
        return -(k % (frames + 1));
    }
    return 0;
}

/* byte_1AEB90. A bit per tile drawn; loadpics clears it and the game clears
   the bits it tests. */
unsigned char gotpic[(MAXTILES + 7) >> 3];

static void setgotpic(short pic)
{
    gotpic[pic >> 3] |= (unsigned char)(1 << (pic & 7));
}

/* 0x561E1. A Q15 arctan series for each column. */
static void setradarang(int xd)
{
    int limit = xd > 320 ? 512 : 256;
    int half = xd >> 1;
    int i;

    for (i = 0; i <= half; i++) {
        int v = (int)(((long long)i << 15) / half) - 0x8000;
        int term = v;
        int sq = -(int)(((long long)v * v) >> 15);
        int k = 3;

        while (k < limit - 4 * (i >> 1)) {
            int t = imul(sq, term);
            v += t / (k << 15);
            term = t >> 15;
            k += 2;
        }
        radarang[i] = (short)(((long long)v << 16) / 6588397);
        if (xd - i < xd)
            radarang[xd - i] = (short)-radarang[i];
    }
}

/* 0x562C6 */
void setview(int x1, int y1, int x2, int y2)
{
    int i;

    windowx1 = x1;
    windowx2 = x2;
    windowy1 = y1;
    windowy2 = y2;
    ydimen = y2 - y1 + 1;
    xdimen = x2 - x1 + 1;
    viewingrange = (int)(((long long)xdimen << 16) / 320);
    xdimscale = 20971520 / xdimen;
    setradarang(xdimen);
    for (i = 0; i < xdimen; i++) {
        startumost[i] = 100 - (ydimen >> 1);
        startdmost[i] = startumost[i] + ydimen;
    }
    frameoffset = x1 + (y1 - startumost[0]) * (setup_byte(0) == 7 ? 640 : 320);
    for (i = xdimen; i < 320; i++) {
        startumost[i] = 1;
        startdmost[i] = 0;
    }
    if (setup_byte(0) == 7) {
        wtop = (200 - ydimen) >> 1;
        wbot = (ydimen + 200) >> 1;
    } else {
        wtop = 0;
        wbot = 200;
    }
}

static int pitch(void)
{
    return setup_byte(0) == 7 ? 640 : 320;
}

/* 0x46BEC. Projects one wall of the sector into the scan list and queues the
   sector behind it when it is close enough to need visiting. */
static void scanwall(short sectnum, short w)
{
    int x1 = wall[w].x - globalposx, y1 = wall[w].y - globalposy;
    int x2 = wall[wall[w].point2].x - globalposx, y2 = wall[wall[w].point2].y - globalposy;
    short nextsect = wall[w].nextsector;
    int xp1, yp1, xp2, yp2;

    if (nextsect >= 0 && !(wall[w].cstat & 0x20)
        && !(gotsector[nextsect >> 3] & (1 << (nextsect & 7)))) {
        int templong = imul(x1, y2) - imul(x2, y1);
        int t2 = templong;

        if (templong < 0)
            templong = imul(x2, y1) - imul(x1, y2);
        if (templong < 0x40000
            && (int)(((long long)t2 * t2) >> 5)
                <= imul(y2 - y1, y2 - y1) + imul(x2 - x1, x2 - x1))
            sectorstack[sectorstackcnt++] = nextsect;
    }
    yp1 = dmulscale(cosglobalang, y1, singlobalang, -x1, 14);
    xp1 = dmulscale(cosglobalang, x1, singlobalang, y1, 14);
    yp2 = dmulscale(cosglobalang, y2, singlobalang, -x2, 14);
    xp2 = dmulscale(cosglobalang, x2, singlobalang, y2, 14);
    if (imul(xp2, yp1) > imul(xp1, yp2) || (xp1 <= 0 && xp2 <= 0))
        return;
    if (-xp1 > yp1) {
        int den;

        if (-xp2 > yp2)
            return;
        xb1[numscans] = 0;
        den = yp1 + xp1 - xp2 - yp2;
        if (den == 0)
            return;
        yb1[numscans] = (int)((long long)(yp1 + xp1) * (xp2 - xp1) / den) + xp1;
    } else {
        if (xp1 < yp1 || xp1 == 0)
            return;
        xb1[numscans] = (int)((long long)(xdimen >> 1) * yp1 / xp1) + (xdimen >> 1);
        if (yp1 >= 0)
            xb1[numscans]++;
        if (xb1[numscans] >= xdimen)
            xb1[numscans] = xdimen - 1;
        yb1[numscans] = xp1;
    }
    if (xp2 < yp2) {
        int den;

        if (xp1 < yp1)
            return;
        xb2[numscans] = xdimen - 1;
        den = xp1 + yp2 - yp1 - xp2;
        if (den == 0)
            return;
        yb2[numscans] = (int)((long long)(xp1 - yp1) * (xp2 - xp1) / den) + xp1;
    } else {
        if (-xp2 > yp2 || xp2 == 0)
            return;
        xb2[numscans] = (int)((long long)(xdimen >> 1) * yp2 / xp2) + (xdimen >> 1) - 1;
        if (yp2 >= 0)
            xb2[numscans]++;
        if (xb2[numscans] >= xdimen)
            xb2[numscans] = xdimen - 1;
        yb2[numscans] = xp2;
    }
    if (yb1[numscans] > 0 && yb2[numscans] > 0 && xb1[numscans] <= xb2[numscans]) {
        ry1[numscans] = xp1;
        thesector[numscans] = sectnum;
        ry2[numscans] = xp2;
        thewall[numscans] = w;
        rx1[numscans] = yp1;
        rx2[numscans] = yp2;
        numscans++;
    }
}

/* 0x442B8 */
static void scansector(short sectnum)
{
    if (sectnum == -1)
        return;
    sectorstack[0] = sectnum;
    sectorstackcnt = 1;
    do {
        short sect = sectorstack[--sectorstackcnt];
        short i;
        short w, endw, scanfirst, bunchfrst;
        short startbunch;

        for (i = headspritesect[sect]; i != -1; i = nextspritesect[i]) {
            int xs = sprite[i].x - globalposx;
            int ys = sprite[i].y - globalposy;
            int yp = (imul(ys, singlobalang) + imul(xs, cosglobalang)) >> 6;

            if (yp > 1024 || (sprite[i].cstat & 0x30)) {
                if (yp > 1024)
                    spritesx[spritesortcnt] = (int)((long long)(xdimen << 7)
                        * ((imul(ys, cosglobalang) - imul(singlobalang, xs)) >> 6) / yp)
                        + (xdimen << 7);
                tspritepicnum[spritesortcnt] = sprite[i].picnum;
                spritesy[spritesortcnt] = yp;
                tspritepal[spritesortcnt] = (sector[sect].ceilingstat & 1)
                    ? sector[sect].ceilingpal : sector[sect].floorpal;
                tspritelist[spritesortcnt++] = i;
            }
        }
        gotsector[sect >> 3] |= (unsigned char)(1 << (sect & 7));

        bunchfrst = numscans;
        scanfirst = numscans;
        endw = (short)(sector[sect].wallptr + sector[sect].wallnum - 1);
        for (w = sector[sect].wallptr; w <= endw; w++) {
            short before = numscans;

            scanwall(sect, w);
            if (numscans > before)
                p2[numscans - 1] = numscans;
            if (w > wall[w].point2) {
                if (scanfirst < numscans)
                    p2[numscans - 1] = scanfirst;
                scanfirst = numscans;
            }
        }
        startbunch = numbunches;
        for (w = bunchfrst; w < numscans; w++) {
            short nxt = p2[w];

            if (wall[thewall[w]].point2 != thewall[nxt] || xb2[w] >= xb1[nxt]) {
                bunchfirst[numbunches++] = p2[w];
                p2[w] = -1;
            }
        }
        for (i = startbunch; i < numbunches; i++) {
            for (w = bunchfirst[i]; p2[w] != -1; w = p2[w])
                ;
            bunchlast[i] = w;
        }
    } while (sectorstackcnt > 0);
}

/* 0x445AC. Which of two scan walls is in front: l1, l2, -1 (unknown), -2 (none). */
static int wallfront(int l1, int l2)
{
    walltype *wal = &wall[thewall[l1]];
    int x11 = wal->x, y11 = wal->y;
    int x21 = wall[wal->point2].x, y21 = wall[wal->point2].y;
    int x12, y12, x22, y22, dx, dy, t1, t2;

    wal = &wall[thewall[l2]];
    x12 = wal->x;
    y12 = wal->y;
    x22 = wall[wal->point2].x;
    y22 = wall[wal->point2].y;

    dx = x21 - x11;
    dy = y21 - y11;
    t1 = imul(x12 - x11, dy) - imul(y12 - y11, dx);
    t2 = imul(x22 - x11, dy) - imul(y22 - y11, dx);
    if (t1 == 0 && t2 == 0)
        return -1;
    if (t1 == 0)
        t1 = t2;
    if (t2 == 0)
        t2 = t1;
    if ((t1 < 0) == (t2 < 0)) {
        if ((t1 < 0) == (imul(globalposx - x11, dy) - imul(globalposy - y11, dx) < 0))
            return l2;
        return l1;
    }
    dx = x22 - x12;
    dy = y22 - y12;
    t1 = imul(x11 - x12, dy) - imul(y11 - y12, dx);
    t2 = imul(x21 - x12, dy) - imul(y21 - y12, dx);
    if (t1 == 0 && t2 == 0)
        return -1;
    if (t1 == 0)
        t1 = t2;
    if (t2 == 0)
        t2 = t1;
    if ((t1 < 0) == (t2 < 0)) {
        if ((t1 < 0) != (imul(globalposx - x12, dy) - imul(globalposy - y12, dx) < 0))
            return l2;
        return l1;
    }
    return -2;
}

/* 0x44848 */
static int bunchfront(int b1, int b2)
{
    int x1b1 = xb1[bunchfirst[b1]];
    int x2b1 = xb2[bunchlast[b1]] + 1;
    int x1b2 = xb1[bunchfirst[b2]];
    int x2b2 = xb2[bunchlast[b2]] + 1;
    int i;

    if (x1b1 >= x2b2 || x1b2 >= x2b1)
        return -1;
    if (x1b1 >= x1b2) {
        for (i = bunchfirst[b2]; xb2[i] < x1b1; i = p2[i])
            ;
        return wallfront(i, bunchfirst[b1]) == i ? b2 : b1;
    }
    for (i = bunchfirst[b1]; xb2[i] < x1b2; i = p2[i])
        ;
    return wallfront(i, bunchfirst[b2]) == i ? b1 : b2;
}

/* 0x564CE. Fills mostbuf with the screen y of height z along scan wall w.
   Returns 1|2 when both ends are above the window, 4|8 when below. */
static int wallmost(int *mostbuf, int w, int z)
{
    int bad;
    int ix1, ix2, iy1, iy2, y, yinc, i;
    int z8 = z << 3;

    bad = (z8 < imul(wtop - globalhorizraw, yb1[w]))
        + 2 * (z8 < imul(wtop - globalhorizraw, yb2[w]))
        + 4 * (z8 > imul(wbot - globalhorizraw, yb1[w]))
        + 8 * (z8 > imul(wbot - globalhorizraw, yb2[w]));

    if ((bad & 3) == 3) {
        for (i = xb1[w]; i <= xb2[w]; i++)
            mostbuf[i] = wtop;
        return bad;
    }
    if ((bad & 12) == 12) {
        for (i = xb1[w]; i <= xb2[w]; i++)
            mostbuf[i] = wbot;
        return bad;
    }
    ix1 = xb1[w];
    iy1 = yb1[w];
    ix2 = xb2[w];
    iy2 = yb2[w];
    if (bad & 3) {
        int inty = z8 / -globalhorizraw;
        int t = (int)((long long)(rx2[w] - rx1[w]) * (inty - ry1[w]) / (ry2[w] - ry1[w])) + rx1[w];
        int xcross = (xdimen >> 1) + (int)((long long)(xdimen >> 1) * t / inty);

        if (t > 0)
            xcross--;
        if ((bad & 3) == 2) {
            ix2 = xcross;
            iy2 = inty;
            for (i = xcross + 1; i <= xb2[w]; i++)
                mostbuf[i] = wtop;
        } else {
            ix1 = xcross;
            iy1 = inty;
            for (i = xb1[w]; i < xcross; i++)
                mostbuf[i] = wtop;
        }
        if (ix1 <= ix2) {
            int a = divscale(z, iy1, 19), b = divscale(z, iy2, 19);

            if (xdimen != 320) {
                a = mulscale(viewingrange, a, 16);
                b = mulscale(viewingrange, b, 16);
            }
            y = a + (globalhoriz << 16);
            yinc = (b - a) / (ix2 - ix1 + 1);
            for (i = ix1; i <= ix2; i++) {
                mostbuf[i] = y >> 16;
                y += yinc;
            }
        }
        if (mostbuf[xcross] < wtop)
            mostbuf[xcross] = wtop;
        return bad;
    }
    if (bad & 12) {
        int inty = z8 / (wbot - globalhorizraw);
        int t = (int)((long long)(rx2[w] - rx1[w]) * (inty - ry1[w]) / (ry2[w] - ry1[w])) + rx1[w];
        int xcross = (xdimen >> 1) + (int)((long long)(xdimen >> 1) * t) / inty;

        if (t > 0)
            xcross--;
        if ((bad & 12) == 8) {
            ix2 = xcross;
            iy2 = inty;
            for (i = xcross + 1; i <= xb2[w]; i++)
                mostbuf[i] = wbot;
        } else {
            ix1 = xcross;
            iy1 = inty;
            for (i = xb1[w]; i < xcross; i++)
                mostbuf[i] = wbot;
        }
        if (ix1 <= ix2) {
            int a = divscale(z, iy1, 19), b = divscale(z, iy2, 19);

            if (xdimen != 320) {
                a = mulscale(viewingrange, a, 16);
                b = mulscale(viewingrange, b, 16);
            }
            y = a + (globalhoriz << 16);
            yinc = (b - a) / (ix2 - ix1 + 1);
            for (i = ix1; i <= ix2; i++) {
                mostbuf[i] = y >> 16;
                y += yinc;
            }
        }
        if (mostbuf[xcross] > wbot)
            mostbuf[xcross] = wbot;
        return bad;
    }
    {
        int a = divscale(z, yb1[w], 19), b = divscale(z, yb2[w], 19);

        if (xdimen != 320) {
            a = mulscale(viewingrange, a, 16);
            b = mulscale(viewingrange, b, 16);
        }
        y = a + (globalhoriz << 16);
        yinc = (b - a) / (xb2[w] - xb1[w] + 1);
        for (i = xb1[w]; i <= xb2[w]; i++) {
            mostbuf[i] = y >> 16;
            y += yinc;
        }
    }
    return bad;
}

/* vlineasm 0x7BBE7. The bits of vplc>>shift above the low byte stay in the
   palookup index, as the asm leaves them in ebx. */
static void vline(int vinc, int vplc, const unsigned char *pal, int cnt, int shift,
    const unsigned char *col, unsigned char *dest)
{
    int n = cnt + 1;

    shift &= 31;
    while (n-- > 0) {
        unsigned idx = (unsigned)vplc >> shift;
        unsigned b = (idx & ~0xFFu) | col[idx];

        vplc += vinc;
        *dest = pal[b];
        dest += 320;
    }
}

static void loadtile(short pic)
{
    (void)pic; /* 0x4AAC9 reads the tile from its ART file on first use; loadpics already did. */
}

/* 0x4796F */
static void wallscan(int x1, int x2, const int *uwal, const int *dwal, const int *swal, const int *lwal)
{
    int x;
    int xpow2, ypow2;
    int sizx, sizy;

    sizx = tilesizx[globalpicnum];
    sizy = tilesizy[globalpicnum];
    if (sizx <= 0 || sizy <= 0)
        return;
    if (!(wbot >= uwal[x1] || wbot >= uwal[x2]))
        return;
    if (!(wtop <= dwal[x1] || wtop <= dwal[x2]))
        return;
    loadtile(globalpicnum);
    setgotpic(globalpicnum);
    if (globalorientation & 0x100) {
        globalyscale = -globalyscale;
        globalzd = -globalzd;
    }
    if (x1 < 0)
        x1 = 0;
    if (x2 >= xdimen)
        x2 = xdimen - 1;
    xpow2 = (1 << (picsiz[globalpicnum] & 15)) == sizx;
    ypow2 = (1 << (picsiz[globalpicnum] >> 4)) == sizy;
    for (x = x1; x <= x2; x++) {
        int y1, y2, scale, shade, vinc, xu;
        const unsigned char *pal;

        if (umost[x] > dmost[x] || swal[x] <= 4)
            continue;
        y1 = uwal[x] < umost[x] ? umost[x] : uwal[x];
        y2 = dwal[x] - 1 >= dmost[x] ? dmost[x] - 1 : dwal[x] - 1;
        if (y2 < y1)
            continue;
        scale = (int)((65536LL << 16) / swal[x]);
        shade = globalshade + (scale >> (visibility + 2));
        if (shade < 0)
            shade = 0;
        if (shade >= numpalookups)
            shade = numpalookups - 1;
        pal = palookup[globalpal] + (shade << 8);
        vinc = imul(globalyscale, scale);
        xu = globalxpanning + (lwal[x] >> 6);
        xu = xpow2 ? (xu & (sizx - 1)) : xu % sizx;
        vline(vinc, globalzd + imul(y1 - globalhoriz + 1, vinc), pal, y2 - y1, globalshiftval,
            waloff[globalpicnum] + (ypow2 ? xu << (picsiz[globalpicnum] >> 4) : xu * sizy),
            frame + frameoffset + y1 * pitch() + x);
    }
}

/* hlineasm 0x7B7BB, with the texture shifts patched in by 0x7B685. */
static void hline(int x1, int x2, int y, unsigned u, unsigned v, int uinc, int vinc, int shade)
{
    int xlog = picsiz[globalpicnum] & 15;
    int ylog = picsiz[globalpicnum] >> 4;
    const unsigned char *pal = palookup[flatpal] + (shade << 8);
    unsigned char *dest = frame + frameoffset + y * pitch() + x1;
    int n = x2 - x1 + 1;

    v &= 0xFFFFFF00u;
    vinc &= (int)0xFFFFFF00;
    while (n-- > 0) {
        unsigned idx = u >> ((32 - xlog) & 31);

        if (ylog)
            idx = (idx << ylog) | (v >> (32 - ylog));
        *dest++ = pal[globalbufplc[idx]];
        u += (unsigned)uinc;
        v += (unsigned)vinc;
    }
}

/* 0x49A7F */
static void hlinedriver(int x1, int x2, int y)
{
    int dist, d, s, c, u, v, uinc, vinc, half, shade;

    if (x1 > x2)
        return;
    if (globalhoriz - 1 == y)
        dist = 160;
    else
        dist = (globalhoriz - 1 - y < 0 ? y - (globalhoriz - 1) : globalhoriz - 1 - y) * 320;
    d = divscale(globalzd, dist, 14);
    s = mulscale(singlobalang, d, 4);
    c = mulscale(cosglobalang, d, 4);
    half = xdimen >> 1;
    u = imul(s, x1) - imul(s + c, half) + (globalposx << 20);
    v = imul(c, x1) + imul(s - c, half) - (globalposy << 20);
    uinc = s;
    vinc = c;
    if (globalorientation & 0x74) {
        if (globalorientation & 0x40) {
            int a = mulscale(x1 - half, s, 8) - mulscale(c, half, 8) - globalx1;
            int b = -mulscale(x1 - half, c, 8) - mulscale(s, half, 8) - globaly1;

            u = mulscale(a, globalx2, 12) + mulscale(b, globaly2, 12);
            v = mulscale(b, globalx2, 12) - mulscale(a, globaly2, 12);
            uinc = mulscale(s, globalx2, 20) + mulscale(-c, globaly2, 20);
            vinc = mulscale(-c, globalx2, 20) - mulscale(s, globaly2, 20);
        }
        if (globalorientation & 4) {
            int t = u;
            u = v;
            v = t;
            t = uinc;
            uinc = vinc;
            vinc = t;
        }
        if (globalorientation & 0x10) {
            uinc = ~uinc;
            u = ~u;
        }
        if (globalorientation & 0x20) {
            v = ~v;
            vinc = ~vinc;
        }
    }
    if (y > globalhoriz) {
        v = ~v;
        vinc = ~vinc;
    }
    uinc <<= xshift;
    u <<= xshift;
    v <<= yshift;
    vinc <<= yshift;
    v += globalypanning;
    u += globalxpanning;
    shade = globalshade + ((d < 0 ? -d : d) >> visibility);
    if (shade < 0)
        shade = 0;
    if (shade >= numpalookups)
        shade = numpalookups - 1;
    hline(x1, x2, y, (unsigned)u, (unsigned)v, uinc, vinc, shade);
}

/* The per-surface setup ceilscan and florscan share. */
static int flatsetup(short sectnum, int isfloor)
{
    sectortype *sec = &sector[sectnum];
    int i;

    flatpal = isfloor ? sec->floorpal : sec->ceilingpal;
    globalzd = isfloor ? globalposz - sec->floorz : sec->ceilingz - globalposz;
    if (globalzd > 0)
        return 0;
    globalpicnum = isfloor ? sec->floorpicnum : sec->ceilingpicnum;
    if ((unsigned)globalpicnum >= MAXTILES)
        globalpicnum = 0;
    if (!isfloor && (picanm[globalpicnum] & 0xC0))
        globalpicnum += animateoffs(globalpicnum);
    if (tilesizx[globalpicnum] <= 0 || tilesizy[globalpicnum] <= 0)
        return 0;
    if (isfloor && (picanm[globalpicnum] & 0xC0))
        globalpicnum += animateoffs(globalpicnum);
    loadtile(globalpicnum);
    setgotpic(globalpicnum);
    globalbufplc = waloff[globalpicnum];
    globalshade = isfloor ? sec->floorshade : sec->ceilingshade;
    globalorientation = isfloor ? sec->floorstat : sec->ceilingstat;
    globalxpanning = (isfloor ? sec->floorxpanning : sec->ceilingxpanning) << 24;
    globalypanning = (isfloor ? sec->floorypanning : sec->ceilingypanning) << 24;
    if (globalorientation & 0x40) {
        walltype *w0 = &wall[sec->wallptr];

        globalx1 = w0->x;
        globaly1 = w0->y;
        globalx2 = wall[w0->point2].x - globalx1;
        globaly2 = wall[w0->point2].y - globaly1;
        globalx1 = (globalx1 - globalposx) << 12;
        globaly1 = (globaly1 - globalposy) << 12;
        i = imul(globaly2, globaly2) + imul(globalx2, globalx2);
        if (i != 0)
            i = tablesqrt(i) >> 10;
        globalzx = i != 0 ? (int)(0x100000LL / i) : 1024;
        globalx2 = imul(globalx2, globalzx);
        globaly2 = imul(globaly2, globalzx);
    }
    if (globalorientation & 8) {
        xshift = yshift = picsiz[globalpicnum] == 0x66 ? 3 : 2;
    } else {
        xshift = (unsigned char)(8 - (picsiz[globalpicnum] & 15));
        yshift = (unsigned char)(8 - (picsiz[globalpicnum] >> 4));
    }
    return 1;
}

/* ceilscan 0x46FEC */
static void ceilscan(int x1, int x2, short sectnum)
{
    int x, y1, y2, twall, bwall;

    if (!flatsetup(sectnum, 0))
        return;
    y1 = umost[x1] - 1;
    y2 = y1;
    for (x = x1; x <= x2; x++) {
        twall = umost[x] - 1;
        bwall = uwall[x] > dmost[x] ? dmost[x] : uwall[x];
        if (bwall - 1 <= twall) {
            while (y1 < y2 - 1) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            y1 = umost[x + 1] - 1;
            y2 = y1;
            continue;
        }
        if (y2 > twall) {
            while (y1 < twall) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            while (y1 > twall)
                lastx[y1--] = x;
        } else {
            while (y1 < y2 - 1) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            y1 = twall;
        }
        while (y2 > bwall) {
            y2--;
            hlinedriver(lastx[y2], x - 1, y2);
        }
        while (y2 < bwall)
            lastx[y2++] = x;
    }
    while (y1 < y2 - 1) {
        y1++;
        hlinedriver(lastx[y1], x2, y1);
    }
}

/* florscan 0x474A5 */
static void florscan(int x1, int x2, short sectnum)
{
    int x, y1, y2, twall, bwall;

    if (!flatsetup(sectnum, 1))
        return;
    y1 = dwall[x1] - 1 < umost[x1] - 1 ? umost[x1] - 1 : dwall[x1] - 1;
    y2 = y1;
    for (x = x1; x <= x2; x++) {
        twall = dwall[x] - 1 < umost[x] - 1 ? umost[x] - 1 : dwall[x] - 1;
        bwall = dmost[x];
        if (bwall - 1 <= twall) {
            while (y1 < y2 - 1) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            y1 = dwall[x + 1] - 1;
            y2 = y1;
            continue;
        }
        if (y2 > twall) {
            while (y1 < twall) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            while (y1 > twall)
                lastx[y1--] = x;
        } else {
            while (y1 < y2 - 1) {
                y1++;
                hlinedriver(lastx[y1], x - 1, y1);
            }
            y1 = twall;
        }
        while (y2 > bwall) {
            y2--;
            hlinedriver(lastx[y2], x - 1, y2);
        }
        while (y2 < bwall)
            lastx[y2++] = x;
    }
    while (y1 < y2 - 1) {
        y1++;
        hlinedriver(lastx[y1], x2, y1);
    }
}

/* 0x4FFEE. This beta's terrain floor: marches a 64x64 heightmap of 1024-unit
   cells from far (oldz) to near (newz) and fills the column from dmost up. */
static void heightfloor(int x, int oldz, int newz, short sectnum)
{
    sectortype *sec = &sector[sectnum];
    int dnear, dfar, half, t, xs, ys, xe, ye, dx, dy;
    int gx[2], dist[2], dinc[2], err[2], cell[2], step[2];
    int cnt, shade, maxd, ytop, ybot, h0, idx, prevd;
    const unsigned char *heights, *colors;
    unsigned char *dest;
    int stride = pitch();

    if (!(oldz >> 6 > 0 && newz >> 6 > 0 && umost[x] < dmost[x]))
        return;
    dnear = 0x8000000 / (oldz >> 6);
    dfar = 0x8000000 / (newz >> 6);
    if (xdimen != 320) {
        dnear = mulscale(viewingrange, dnear, 16);
        dfar = mulscale(viewingrange, dfar, 16);
    }
    dnear++;
    dfar--;
    if (!(dfar > dnear && dnear > 0 && dfar > 0))
        return;
    dest = frame + frameoffset + x + (dmost[x] - 1) * stride;
    half = x - (xdimen >> 1);
    t = imul(half, dnear) / (2 * xdimen);
    xs = ((imul(dnear >> 2, cosglobalang) - imul(t, singlobalang)) >> 12) + (globalposx << 6);
    ys = (globalposy << 6) + ((imul(singlobalang, dnear >> 2) + imul(cosglobalang, t)) >> 12);
    t = imul(dfar, half) / (2 * xdimen);
    dx = (imul(dfar >> 2, cosglobalang) - imul(t, singlobalang)) >> 12;
    xe = dx + (globalposx << 6);
    dy = (imul(singlobalang, dfar >> 2) + imul(cosglobalang, t)) >> 12;
    ye = dy + (globalposy << 6);
    step[0] = ksgn(dx);
    step[1] = ksgn(dy);
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    gx[0] = xs & ~1023;
    if (step[0] == 1)
        gx[0] += 1024;
    gx[1] = ys & ~1023;
    if (step[1] == 1)
        gx[1] += 1024;
    cell[0] = (xs >> 10) & 63;
    cell[1] = (ys >> 10) & 63;
    cnt = (imul(gx[0] - xs < 0 ? xs - gx[0] : gx[0] - xs, dy)
        - imul(dx, gx[1] - ys < 0 ? ys - gx[1] : gx[1] - ys)) >> 10;
    /* Unset when the ray never crosses that axis; the exe leaves stack garbage. */
    dinc[0] = dinc[1] = dist[0] = dist[1] = 0x7FFFFFFF;
    t = xe - (globalposx << 6);
    if (t < 0)
        t = -t;
    if (dfar >> 16 < t) {
        dinc[0] = (int)(((long long)dfar << 16) / t);
        if (xdimen != 320)
            dinc[0] = mulscale(xdimscale, dinc[0], 16);
        t = gx[0] - (globalposx << 6);
        dist[0] = mulscale(t < 0 ? -t : t, dinc[0], 10);
    }
    t = ye - (globalposy << 6);
    if (t < 0)
        t = -t;
    if (dfar >> 16 < t) {
        dinc[1] = (int)(((long long)dfar << 16) / t);
        if (xdimen != 320)
            dinc[1] = mulscale(xdimscale, dinc[1], 16);
        t = gx[1] - (globalposy << 6);
        dist[1] = mulscale(t < 0 ? -t : t, dinc[1], 10);
    }
    if (dinc[0] < 8)
        dinc[0] = 8;
    if (dinc[1] < 8)
        dinc[1] = 8;
    if (dist[0] < 8)
        dist[0] = 8;
    if (dist[1] < 8)
        dist[1] = 8;
    shade = (dnear >> (visibility + 2)) + sec->floorshade;
    if (shade < 0)
        shade = 0;
    if (shade >= numpalookups)
        shade = numpalookups - 1;
    maxd = dfar << 6;
    if (xdimen != 320)
        maxd = mulscale(xdimscale, maxd, 16);
    ytop = umost[x] - globalhoriz;
    ybot = dmost[x] - globalhoriz;
    err[0] = dy;
    err[1] = -dx;
    if (!waloff[sec->floorpicnum] || !waloff[sec->floorheinum])
        return;
    colors = waloff[sec->floorpicnum];
    heights = waloff[sec->floorheinum];
    h0 = sec->floorz - globalposz;
    idx = cnt >= 0;
    prevd = dist[idx];
    while (maxd > dist[idx]) {
        int h = heights[cell[1] | (cell[0] << 6)];
        int yfar = divscale(h0 - (h << 8), dist[idx], 15);
        unsigned char color;
        int n;

        if (yfar < 0) {
            for (;;) {
                int ynear = divscale(h0 - (h << 8), prevd, 15);

                if (ynear <= ybot) {
                    color = palookup[globalpal][(shade << 8) + colors[cell[1] | (cell[0] << 6)]];
                    if (ynear <= ytop)
                        goto fill_to_top;
                    n = ybot - ynear + 1;
                    ybot = ynear - 1;
                    while (n--) {
                        *dest = color;
                        dest -= stride;
                    }
                }
                cell[idx] = (cell[idx] + step[idx]) & 63;
                prevd = dist[idx];
                cnt += err[idx];
                dist[idx] += dinc[idx];
                idx = cnt >= 0;
                if (maxd <= dist[idx])
                    goto done;
                h = heights[cell[1] | (cell[0] << 6)];
            }
        }
        if (yfar <= ybot) {
            color = palookup[globalpal][(shade << 8) + colors[cell[1] | (cell[0] << 6)]];
            if (yfar <= ytop) {
            fill_to_top:
                if (ybot > ytop) {
                    n = ybot - ytop;
                    while (n--) {
                        *dest = color;
                        dest -= stride;
                    }
                    ybot = ytop - 1;
                }
                break;
            }
            n = ybot - yfar + 1;
            ybot = yfar - 1;
            while (n--) {
                *dest = color;
                dest -= stride;
            }
        }
        cell[idx] = (cell[idx] + step[idx]) & 63;
        prevd = dist[idx];
        dist[idx] += dinc[idx];
        cnt += err[idx];
        idx = cnt >= 0;
    }
done:
    dmost[x] = globalhoriz + ybot;
}

/* The per-wall texture setup shared by the wall passes of 0x45D09. */
static void wallsetup(short w, int anim_after_shift)
{
    globalorientation = wall[w].cstat;
    globalpicnum = wall[w].picnum;
    if ((unsigned)globalpicnum >= MAXTILES)
        globalpicnum = 0;
    globalxpanning = wall[w].xpanning;
    globalypanning = wall[w].ypanning;
    if (!anim_after_shift && (picanm[globalpicnum] & 0xC0))
        globalpicnum += animateoffs(globalpicnum);
    globalshade = wall[w].shade;
}

static void setshiftval(void)
{
    globalshiftval = (short)(picsiz[globalpicnum] >> 4);
    if ((1 << globalshiftval) != tilesizy[globalpicnum])
        globalshiftval++;
    globalshiftval = (short)(32 - globalshiftval);
}

static void closecolumns(int z)
{
    int x;

    for (x = xb1[z]; x <= xb2[z]; x++) {
        if (umost[x] <= dmost[x]) {
            umost[x] = wbot;
            dmost[x] = wtop;
            numhits--;
        }
    }
}

static void savesmost(int z, int flag, const int *src)
{
    int x;

    if (xb2[z] + smostcnt - xb1[z] + 1 >= 40960)
        return;
    smoststart[smostwallcnt] = (short)smostcnt;
    smostwall[smostwallcnt++] = (short)(z + flag);
    for (x = xb1[z]; x <= xb2[z]; x++)
        smost[smostcnt++] = (short)src[x];
}

/* 0x45D09. Draws the walls of one bunch and opens the sectors behind them. */
static void drawwalls(int bunch)
{
    int z = bunchfirst[bunch];
    short sectnum = thesector[z];
    sectortype *sec = &sector[sectnum];
    int x;

    globalpal = sec->floorpal;
    for (;;) {
        short w = thewall[z];
        short nextsectnum = wall[w].nextsector;

        if (nextsectnum == -1) {
            wallsetup(w, 0);
            setshiftval();
            globalyscale = wall[w].yrepeat << (globalshiftval - 19);
            globalzd = (globalypanning << 24) + (imul(globalyscale,
                globalposz - ((globalorientation & 4) ? sec->floorz : sec->ceilingz)) << 8);
            wallscan(xb1[z], xb2[z], uwall, dwall, swall, lwall);
            closecolumns(z);
            smostwall[smostwallcnt++] = (short)z;
        } else {
            sectortype *nsec = &sector[nextsectnum];

            if (wall[w].cstat & 0x10)
                maskwall[maskwallcnt++] = (short)z;
            if (!(sec->ceilingstat & 1) || !(nsec->ceilingstat & 1)) {
                if (nsec->ceilingz > sec->ceilingz) {
                    wallmost(dwall2, z, (sec->floorz >= nsec->ceilingz ? nsec->ceilingz : sec->floorz) - globalposz);
                    globalorientation = wall[w].cstat;
                    globalpicnum = wall[w].picnum;
                    if ((unsigned)globalpicnum >= MAXTILES)
                        globalpicnum = 0;
                    globalxpanning = wall[w].xpanning;
                    globalypanning = wall[w].ypanning;
                    setshiftval();
                    if (picanm[globalpicnum] & 0xC0)
                        globalpicnum += animateoffs(globalpicnum);
                    globalshade = wall[w].shade;
                    globalyscale = wall[w].yrepeat << (globalshiftval - 19);
                    globalzd = (globalypanning << 24) + (imul(globalyscale,
                        globalposz - ((globalorientation & 4) ? sec->ceilingz : nsec->ceilingz)) << 8);
                    wallscan(xb1[z], xb2[z], uwall, dwall2, swall, lwall);
                    for (x = xb1[z]; x <= xb2[z]; x++) {
                        if (umost[x] <= dmost[x] && dwall2[x] > umost[x]) {
                            umost[x] = dwall2[x];
                            if (umost[x] > dmost[x])
                                numhits--;
                        }
                    }
                    if (globalposz < nsec->ceilingz)
                        savesmost(z, 0x4000, umost);
                } else {
                    for (x = xb1[z]; x <= xb2[z]; x++) {
                        if (umost[x] <= dmost[x] && uwall[x] > umost[x]) {
                            umost[x] = uwall[x];
                            if (umost[x] > dmost[x])
                                numhits--;
                        }
                    }
                    if (nsec->ceilingz < sec->ceilingz)
                        savesmost(z, 0x4000, umost);
                }
            }
            if (!(sec->floorstat & 1) || !(nsec->floorstat & 1)) {
                if (nsec->floorz < sec->floorz) {
                    short tw = w;

                    wallmost(uwall2, z, (sec->ceilingz <= nsec->floorz ? nsec->floorz : sec->ceilingz) - globalposz);
                    if (wall[w].cstat & 2)
                        tw = wall[w].nextwall;
                    wallsetup(tw, 0);
                    setshiftval();
                    globalyscale = wall[w].yrepeat << (globalshiftval - 19);
                    globalzd = (globalypanning << 24) + (imul(globalyscale,
                        globalposz - ((globalorientation & 4) ? sec->ceilingz : nsec->floorz)) << 8);
                    wallscan(xb1[z], xb2[z], uwall2, dwall, swall, lwall);
                    for (x = xb1[z]; x <= xb2[z]; x++) {
                        if (umost[x] <= dmost[x] && uwall2[x] < dmost[x]) {
                            dmost[x] = uwall2[x];
                            if (umost[x] > dmost[x])
                                numhits--;
                        }
                    }
                    if (globalposz > nsec->floorz)
                        savesmost(z, 0x8000, dmost);
                } else {
                    for (x = xb1[z]; x <= xb2[z]; x++) {
                        if (umost[x] <= dmost[x] && dwall[x] < dmost[x]) {
                            dmost[x] = dwall[x];
                            if (umost[x] > dmost[x])
                                numhits--;
                        }
                    }
                    if (nsec->floorz > sec->floorz)
                        savesmost(z, 0x8000, dmost);
                }
            }
            if (wall[w].cstat & 0x20) {
                globalorientation = wall[w].cstat;
                globalpicnum = wall[w].overpicnum;
                if ((unsigned)globalpicnum >= MAXTILES)
                    globalpicnum = 0;
                globalxpanning = wall[w].xpanning;
                globalypanning = wall[w].ypanning;
                if (picanm[globalpicnum] & 0xC0)
                    globalpicnum += animateoffs(globalpicnum);
                globalshade = wall[w].shade;
                setshiftval();
                globalyscale = wall[w].yrepeat << (globalshiftval - 19);
                globalzd = (globalypanning << 24) + (imul(globalyscale,
                    globalposz - ((globalorientation & 4) ? sec->ceilingz : nsec->ceilingz)) << 8);
                wallscan(xb1[z], xb2[z], uwall, dwall, swall, lwall);
                closecolumns(z);
                smostwall[smostwallcnt++] = (short)z;
            }
        }
        if (numhits < 0)
            return;
        if (nextsectnum != -1 && !(gotsector[nextsectnum >> 3] & (1 << (nextsectnum & 7)))
            && !(wall[w].cstat & 0x20)) {
            for (x = xb1[z]; x <= xb2[z]; x++)
                if (umost[x] < dmost[x])
                    break;
            if (x <= xb2[z])
                scansector(nextsectnum);
        }
        z = p2[z];
        if (z == -1)
            return;
    }
}

/* The parallax sky pass of drawalls for the ceiling (isfloor 0) or floor. */
static void drawsky(int bunch, short sectnum, int isfloor)
{
    sectortype *sec = &sector[sectnum];
    int z, x, j = -1, run, basepic, sh, lshift, last;

    globalpal = isfloor ? sec->floorpal : sec->ceilingpal;
    globalpicnum = isfloor ? sec->floorpicnum : sec->ceilingpicnum;
    globalorientation = 0;
    if ((unsigned)globalpicnum >= MAXTILES)
        globalpicnum = 0;
    globalshade = isfloor ? sec->floorshade : sec->ceilingshade;
    globalxpanning = isfloor ? sec->floorxpanning : sec->ceilingxpanning;
    globalypanning = isfloor ? sec->floorypanning : sec->ceilingypanning;
    if (picanm[globalpicnum] & 0xC0)
        globalpicnum += animateoffs(globalpicnum);
    setshiftval();
    globalzd = (globalypanning << 24) + ((parallaxyoffs + (tilesizy[globalpicnum] >> 1)) << globalshiftval);
    globalyscale = 8 << (globalshiftval - 19);
    lshift = 11 - (picsiz[globalpicnum] & 15) - pskybits;
    for (z = bunchfirst[bunch]; z != -1; z = p2[z]) {
        short ns = wall[thewall[z]].nextsector;

        if (ns == -1 || !((isfloor ? sector[ns].floorstat : sector[ns].ceilingstat) & 1)
            || (wall[thewall[z]].cstat & 0x20)) {
            if (j == -1)
                j = xb1[z];
            for (x = xb1[z]; x <= xb2[z]; x++) {
                uwall2[x] = isfloor ? dwall[x] : umost[x];
                dwall2[x] = isfloor ? dmost[x] : uwall[x];
                if (parallaxtype == 0) {
                    skyswall[x] = viewingrange;
                    skylwall[x] = (((globalang
                        + (unsigned short)(((x - (xdimen >> 1)) << 9) / xdimen)) & 2047) >> lshift) << 6;
                } else if (parallaxtype == 1) {
                    skyswall[x] = viewingrange;
                    skylwall[x] = (((globalang + radarang[x]) & 2047) >> lshift) << 6;
                } else if (parallaxtype == 2) {
                    skyswall[x] = 0x40000000 / sintable()[(radarang[x] + 2560) & 2047];
                    if (xdimen != 320)
                        skyswall[x] = mulscale(viewingrange, skyswall[x], 16);
                    skylwall[x] = (((globalang + radarang[x]) & 2047) >> lshift) << 6;
                }
            }
        } else if (j >= 0) {
            sh = (picsiz[globalpicnum] & 15) + 6;
            basepic = globalpicnum;
            globalpicnum = (short)(basepic + pskyoff[skylwall[j] >> sh]);
            if (globalpicnum == basepic + pskyoff[skylwall[xb1[z] - 1] >> sh]) {
                wallscan(j, xb1[z] - 1, uwall2, dwall2, skyswall, skylwall);
            } else {
                run = j;
                for (; j < xb1[z]; j++) {
                    short p = (short)(basepic + pskyoff[skylwall[j] >> sh]);

                    if (globalpicnum != p) {
                        wallscan(run, j - 1, uwall2, dwall2, skyswall, skylwall);
                        run = j;
                        globalpicnum = p;
                    }
                }
                if (run < j)
                    wallscan(run, j - 1, uwall2, dwall2, skyswall, skylwall);
            }
            j = -1;
            globalpicnum = (short)basepic;
        }
    }
    if (j >= 0) {
        last = xb2[bunchlast[bunch]];
        sh = (picsiz[globalpicnum] & 15) + 6;
        basepic = globalpicnum;
        globalpicnum = (short)(basepic + pskyoff[skylwall[j] >> sh]);
        if (globalpicnum == basepic + pskyoff[skylwall[last] >> sh]) {
            wallscan(j, last, uwall2, dwall2, skyswall, skylwall);
        } else {
            run = j;
            for (; j <= last; j++) {
                short p = (short)(basepic + pskyoff[skylwall[j] >> sh]);

                if (globalpicnum != p) {
                    wallscan(run, j - 1, uwall2, dwall2, skyswall, skylwall);
                    run = j;
                    globalpicnum = p;
                }
            }
            /* The exe passes j here, one column past the bunch; wallscan clamps it. */
            if (run <= j)
                wallscan(run, j, uwall2, dwall2, skyswall, skylwall);
        }
        globalpicnum = (short)basepic;
    }
}

/* The lwall pass drawalls and drawmaskwall share. */
static void wallxcoords(int z, int xrep)
{
    int step = imul(xrep, -ry1[z]);
    int num = imul(mulscale(xdimen, rx1[z], 2) - mulscale(ry1[z], xb1[z] - (xdimen >> 1), 1), xrep);
    int dstep = (ry2[z] - ry1[z]) >> 1;
    int den = mulscale(dstep, xb1[z] - (xdimen >> 1), 1) + mulscale(xdimen, rx1[z] - rx2[z], 3);
    int x;

    for (x = xb1[z]; x <= xb2[z] + 1; x += 2) {
        if (den != 0)
            lwall[x] = (int)(((long long)num << 8) / den);
        num += step;
        den += dstep;
    }
    for (x = xb1[z] + 1; x <= xb2[z]; x += 2)
        lwall[x] = (lwall[x - 1] + lwall[x + 1]) >> 1;
    if (lwall[xb1[z]] < 0)
        lwall[xb1[z]] = 0;
}

/* 0x4493D. Ceilings, floors and sky of one bunch, plus the per-column wall
   arrays (lwall, swall, uwall, dwall) that drawwalls then uses. */
static void drawalls(int bunch)
{
    int z = bunchfirst[bunch];
    short sectnum = thesector[z];
    sectortype *sec = &sector[sectnum];
    int andwstat1 = -1, andwstat2 = -1;
    int iz = 0, iznext = 0, izinc, x;

    for (;;) {
        short w = thewall[z];
        short ns = wall[w].nextsector;
        int izs;

        iz = z == bunchfirst[bunch] ? 0x8000000 / yb1[z] : iznext;
        izs = iz;
        iznext = 0x8000000 / yb2[z];
        izinc = (iznext - iz) / (xb2[z] - xb1[z] + 1);
        if (xdimen != 320) {
            izs = mulscale(viewingrange, iz, 16);
            izinc = mulscale(viewingrange, izinc, 16);
        }
        if (sec->floorstat & 2) {
            int v = izs;

            for (x = xb1[z]; x <= xb2[z]; x++) {
                heightfloor(x, swall[x], v, sectnum);
                v += izinc;
            }
        }
        if (ns == -1 || sec->ceilingz < sector[ns].ceilingz || sec->floorz > sector[ns].floorz
            || (wall[w].cstat & 0x20)) {
            int xrep = wall[w].xrepeat;

            wallxcoords(z, xrep);
            if (xrep && (xrep << 9) <= lwall[xb2[z]])
                lwall[xb2[z]] = (xrep << 9) - 1;
            if (wall[w].cstat & 8)
                for (x = xb1[z]; x <= xb2[z]; x++)
                    lwall[x] = (xrep << 9) - 1 - lwall[x];
        }
        andwstat1 &= wallmost(uwall, z, sec->ceilingz - globalposz);
        andwstat2 &= wallmost(dwall, z, sec->floorz - globalposz);
        {
            int v = izs;

            for (x = xb1[z]; x <= xb2[z]; x++) {
                swall[x] = v;
                v += izinc;
            }
        }
        z = p2[z];
        if (z == -1)
            break;
    }
    if ((andwstat1 & 3) != 3) {
        if (sec->ceilingstat & 3) {
            if (sec->ceilingstat & 1)
                drawsky(bunch, sectnum, 0);
        } else {
            ceilscan(xb1[bunchfirst[bunch]], xb2[bunchlast[bunch]], sectnum);
        }
    }
    if ((andwstat2 & 12) != 12) {
        if (sec->floorstat & 3) {
            if (sec->floorstat & 1)
                drawsky(bunch, sectnum, 1);
        } else {
            florscan(xb1[bunchfirst[bunch]], xb2[bunchlast[bunch]], sectnum);
        }
    }
}

/* 0x53797 */
void updatesector(int x, int y, short *sectnum)
{
    int i;

    if (inside(x, y, *sectnum) == 1)
        return;
    if (*sectnum >= 0 && *sectnum < numsectors) {
        int w = sector[*sectnum].wallptr;
        int end = w + sector[*sectnum].wallnum - 1;

        for (; w <= end; w++) {
            short ns = wall[w].nextsector;

            if (ns >= 0 && inside(x, y, ns) == 1) {
                *sectnum = ns;
                return;
            }
        }
    }
    for (i = 0; i < numsectors; i++) {
        if (inside(x, y, (short)i) == 1) {
            *sectnum = (short)i;
            return;
        }
    }
    *sectnum = -1;
}

/* 0x43F18 */
void drawrooms(int daposx, int daposy, int daposz, short daang, int dahoriz, short dacursectnum)
{
    int i, j, closest;

    globalposx = daposx;
    globalposy = daposy;
    globalposz = daposz;
    globalhoriz = dahoriz;
    globalang = (short)(daang & 2047);
    globalcursectnum = dacursectnum;
    totalclocklock = totalclock;
    cosglobalang = sintable()[(globalang + 512) & 2047];
    singlobalang = sintable()[globalang];
    globalhorizraw = dahoriz;
    if (xdimen != 320)
        globalhoriz = mulscale(viewingrange, dahoriz - 100, 16) + 100;

    memset(gotsector, 0, (size_t)((numsectors + 31) >> 5) << 2);
    memcpy(umost, startumost, sizeof(int) * (size_t)xdimen);
    memcpy(dmost, startdmost, sizeof(int) * (size_t)xdimen);
    for (i = 0; i < xdimen; i++)
        swall[i] = 0x7FFFFFFF;
    updatesector(globalposx, globalposy, &globalcursectnum);
    numhits = (short)xdimen;
    numscans = 0;
    numbunches = 0;
    maskwallcnt = 0;
    smostwallcnt = 0;
    spritesortcnt = 0;
    smostcnt = 0;
    scansector(globalcursectnum);

    while (numbunches > 0 && numhits > 0) {
        closest = 0;
        memset(bunchgot, 0, (size_t)((numbunches + 3) >> 2) << 2);
        bunchgot[0] = 1;
        for (i = 1; i < numbunches; i++) {
            j = bunchfront(i, closest);
            if (j >= 0)
                bunchgot[i] = 1;
            if (j == i) {
                bunchgot[closest] = 1;
                closest = i;
            }
        }
        for (i = 0; i < numbunches; i++) {
            if (bunchgot[i])
                continue;
            j = bunchfront(i, closest);
            if (j >= 0)
                bunchgot[i] = 1;
            if (j == i) {
                bunchgot[closest] = 1;
                closest = i;
                i = 0;
            }
        }
        drawalls(closest);
        drawwalls(closest);
        numbunches--;
        bunchfirst[closest] = bunchfirst[numbunches];
        bunchlast[closest] = bunchlast[numbunches];
    }
}

/* mvlineasm 0x7BC24 (skips 255) and tvlineasm 0x7BC72 (blends through transluc). */
static void mvline(int vinc, int vplc, const unsigned char *pal, int cnt, int shift,
    const unsigned char *col, unsigned char *dest, int trans)
{
    int n = (cnt >> 8) + 1;

    shift &= 31;
    while (n-- > 0) {
        unsigned char t = col[(unsigned)vplc >> shift];

        if (t != 255)
            *dest = trans ? transluc[(*dest << 8) | pal[t]] : pal[t];
        vplc += vinc;
        dest += 320;
    }
}

/* 0x48494 (masked) and 0x49E44 via 0x48FF9 (translucent). The translucent
   column always takes the texture x modulo the width. */
static void maskwallscan(int x1, int x2, const int *uwal, const int *dwal, int trans)
{
    int x, sizx, sizy, xpow2, ypow2;

    sizx = tilesizx[globalpicnum];
    sizy = tilesizy[globalpicnum];
    if (sizx <= 0 || sizy <= 0)
        return;
    if (!trans && !((wbot >= uwal[x1] || wbot >= uwal[x2]) && (wtop <= dwal[x1] || wtop <= dwal[x2])))
        return;
    loadtile(globalpicnum);
    setgotpic(globalpicnum);
    if (globalorientation & 0x100) {
        globalyscale = -globalyscale;
        globalzd = -globalzd;
    }
    if (x1 < 0)
        x1 = 0;
    if (x2 >= xdimen)
        x2 = xdimen - 1;
    xpow2 = (1 << (picsiz[globalpicnum] & 15)) == sizx;
    ypow2 = (1 << (picsiz[globalpicnum] >> 4)) == sizy;
    for (x = x1; x <= x2; x++) {
        int y1, y2, scale, shade, vinc, xu;

        if (swall[x] <= 4)
            continue;
        y1 = uwal[x] < umost[x] ? umost[x] : uwal[x];
        y2 = trans ? (short)(dwal[x] - 1) : dwal[x] - 1;
        if (y2 >= dmost[x])
            y2 = trans ? (short)(dmost[x] - 1) : dmost[x] - 1;
        if (y2 < y1)
            continue;
        scale = (int)((65536LL << 16) / swall[x]);
        shade = globalshade + (scale >> (visibility + 2));
        if (shade < 0)
            shade = 0;
        if (shade >= numpalookups)
            shade = numpalookups - 1;
        vinc = imul(globalyscale, scale);
        xu = globalxpanning + (lwall[x] >> 6);
        if (trans || !xpow2)
            xu %= sizx;
        else
            xu &= sizx - 1;
        mvline(vinc, globalzd + imul(y1 - globalhoriz + 1, vinc), palookup[globalpal] + (shade << 8),
            (y2 - y1) << 8, globalshiftval,
            waloff[globalpicnum] + (ypow2 && !trans ? xu << (picsiz[globalpicnum] >> 4) : xu * sizy),
            frame + frameoffset + y1 * pitch() + x, trans && transluc);
    }
}

static void fillswall(int z)
{
    int iz = 0x8000000 / yb1[z];
    int izinc = (0x8000000 / yb2[z] - iz) / (xb2[z] - xb1[z] + 1);
    int x;

    if (xdimen != 320) {
        iz = mulscale(viewingrange, iz, 16);
        izinc = mulscale(viewingrange, izinc, 16);
    }
    for (x = xb1[z]; x <= xb2[z]; x++) {
        swall[x] = iz;
        iz += izinc;
    }
}

/* 0x4C322 */
static void drawmaskwall(short damaskwallcnt)
{
    int z = maskwall[damaskwallcnt];
    short w = thewall[z];
    short sectnum = thesector[z];
    sectortype *sec = &sector[sectnum], *nsec = &sector[wall[w].nextsector];
    int z1, z2, x, k;

    globalpal = sec->floorpal;
    z1 = sec->ceilingz > nsec->ceilingz ? sec->ceilingz : nsec->ceilingz;
    z2 = sec->floorz < nsec->floorz ? sec->floorz : nsec->floorz;
    if (z1 >= z2)
        return;
    wallxcoords(z, wall[w].xrepeat);
    if (wall[w].xrepeat && (wall[w].xrepeat << 9) <= lwall[xb2[z]])
        lwall[xb2[z]] = (wall[w].xrepeat << 9) - 1;
    if (wall[w].cstat & 8)
        for (x = xb1[z]; x <= xb2[z]; x++)
            lwall[x] = (wall[w].xrepeat << 9) - 1 - lwall[x];
    wallmost(uwall2, z, z1 - globalposz);
    wallmost(dwall2, z, z2 - globalposz);
    fillswall(z);
    globalorientation = wall[w].cstat;
    globalpicnum = wall[w].overpicnum;
    if ((unsigned)globalpicnum >= MAXTILES)
        globalpicnum = 0;
    globalxpanning = wall[w].xpanning;
    globalypanning = wall[w].ypanning;
    if (picanm[globalpicnum] & 0xC0)
        globalpicnum += animateoffs(globalpicnum);
    globalshade = wall[w].shade;
    setshiftval();
    globalyscale = wall[w].yrepeat << (globalshiftval - 19);
    globalzd = (globalypanning << 24)
        + (imul(globalyscale, globalposz - ((globalorientation & 4) ? z2 : z1)) << 8);
    for (x = xb1[z]; x <= xb2[z]; x++) {
        umost[x] = startumost[x];
        dmost[x] = startdmost[x];
    }
    for (k = 0; k < smostwallcnt; k++) {
        int i = smostwall[k] & 0x3FFF;
        int xa = xb1[i], xe = xb2[i];

        if (xa > xb2[z] || xe < xb1[z] || wallfront(i, z) != i)
            continue;
        if (xa < xb1[z])
            xa = xb1[z];
        if (xe > xb2[z])
            xe = xb2[z];
        switch (smostwall[k] & 0xC000) {
        case 0:
            for (x = xa; x <= xe; x++) {
                umost[x] = 1;
                dmost[x] = 0;
            }
            break;
        case 0x4000:
            for (x = xa; x <= xe; x++)
                if (smost[smoststart[k] - xb1[i] + x] > umost[x])
                    umost[x] = smost[smoststart[k] - xb1[i] + x];
            break;
        case 0x8000:
            for (x = xa; x <= xe; x++)
                if (smost[smoststart[k] - xb1[i] + x] < dmost[x])
                    dmost[x] = smost[smoststart[k] - xb1[i] + x];
            break;
        }
    }
    if (globalorientation & 0x80) {
        for (x = xb1[z]; umost[x] > dmost[x] && x <= xb2[z]; x++)
            ;
        maskwallscan(x, xb2[z], uwall2, dwall2, 1);
    } else {
        maskwallscan(xb1[z], xb2[z], uwall2, dwall2, 0);
    }
}

/* 0x447D6 */
static int spritewallfront(short s, short w)
{
    walltype *wal = &wall[w];
    int x1 = wal->x, y1 = wal->y;

    return imul(wall[wal->point2].x - x1, sprite[s].y - y1)
        >= imul(wall[wal->point2].y - y1, sprite[s].x - x1);
}

/* The smost pass all three sprite kinds share. A closed record (type 0) sets
   the bottom to wtop. Floor sprites take one off the saved ceiling rows. */
static void clipsmost(short spritenum, int depth, int x1, int x2, int *top, int *bot, int facesprite, int topbias)
{
    int k, x;

    for (k = 0; k < smostwallcnt; k++) {
        int i = smostwall[k] & 0x3FFF;
        int xa = xb1[i], xe = xb2[i];
        int front;

        if (xa > x2 || xe < x1 || !((yb1[i] << 8) < depth || (yb2[i] << 8) < depth))
            continue;
        if (facesprite) {
            front = !spritewallfront(spritenum, thewall[i])
                || ((yb1[i] << 8) < depth && (yb2[i] << 8) < depth);
        } else {
            walltype *wal = &wall[thewall[i]];
            int t1 = imul(wall[wal->point2].x - wal->x, sprite[spritenum].y - wal->y);
            int t2 = imul(sprite[spritenum].x - wal->x, wall[wal->point2].y - wal->y);

            front = t1 - t2 < 0 || (t1 == t2 && wal->nextsector == sprite[spritenum].sectnum);
        }
        if (!front)
            continue;
        if (xa < x1)
            xa = x1;
        if (xe > x2)
            xe = x2;
        switch (smostwall[k] & 0xC000) {
        case 0:
            for (x = xa; x <= xe; x++)
                bot[x] = wtop;
            break;
        case 0x4000:
            for (x = xa; x <= xe; x++) {
                int v = smost[smoststart[k] - xb1[i] + x] - topbias;

                if (v > top[x])
                    top[x] = v;
            }
            break;
        case 0x8000:
            for (x = xa; x <= xe; x++)
                if (smost[smoststart[k] - xb1[i] + x] < bot[x])
                    bot[x] = smost[smoststart[k] - xb1[i] + x];
            break;
        }
    }
}

static int sectorshade(short sectnum)
{
    return (sector[sectnum].ceilingstat & 1) ? sector[sectnum].ceilingshade : sector[sectnum].floorshade;
}

/* mhlineasm 0x7C234 / thlineasm 0x7C374, as 0x4F2A9 drives them for floor sprites. */
static const unsigned char *spanpal;

static void floorspritespan(int x1, int x2, int y)
{
    int d, half = xdimen >> 1, xlog, ylog, n, uinc, vinc;
    unsigned u, v;
    unsigned char *dest;
    const unsigned char *tex = waloff[globalpicnum];

    if (x2 < x1)
        return;
    d = mulscale(horizlookup[512 + y - globalhoriz], globalzd, 16);
    u = (unsigned)(int)(((long long)(imul(half, d) - globalypanning) * globalx1
        + (long long)(imul(d, x1 - half) - globalxpanning) * globalx2) >> 6);
    v = (unsigned)(int)(((long long)(imul(half, d) - globalypanning) * globaly1
        + (long long)(imul(d, x1 - half) - globalxpanning) * globaly2) >> 6);
    uinc = mulscale(d, globalx2, 6);
    vinc = mulscale(d, globaly2, 6);
    xlog = picsiz[globalpicnum] & 15;
    ylog = picsiz[globalpicnum] >> 4;
    dest = frame + frameoffset + y * pitch() + x1;
    for (n = x2 - x1 + 1; n > 0; n--) {
        unsigned idx = u >> ((32 - xlog) & 31);
        unsigned char t;

        if (ylog)
            idx = (idx << ylog) | (v >> (32 - ylog));
        t = tex[idx];
        if (t != 255)
            *dest = (globalorientation & 2) ? transluc[(*dest << 8) | spanpal[t]] : spanpal[t];
        dest++;
        u += (unsigned)uinc;
        v += (unsigned)vinc;
    }
}

static int cliptop(int v)
{
    return v < -1 ? -1 : v > 199 ? 199 : v;
}

static int clipbot(int v)
{
    return v < 0 ? 0 : v > 200 ? 200 : v;
}

/* 0x4F117. The ceilscan sweep over umost/dmost, clamped to rows -1..200. */
static void floorspritescan(int x1, int x2)
{
    int x, y1, y2, twall, bwall;

    y1 = cliptop(umost[x1]);
    y2 = clipbot(umost[x1]);
    for (x = x1; x < x2; x++) {
        twall = umost[x];
        bwall = dmost[x];
        if (twall >= bwall - 1) {
            while (y1 < y2 - 1) {
                floorspritespan(lastx[y1 + 1], x - 1, y1 + 1);
                y1++;
            }
            y1 = cliptop(umost[x + 1]);
            y2 = clipbot(umost[x + 1]);
            continue;
        }
        twall = cliptop(twall);
        bwall = clipbot(bwall);
        if (twall < y2) {
            while (y1 < twall) {
                floorspritespan(lastx[y1 + 1], x - 1, y1 + 1);
                y1++;
            }
            while (y1 > twall)
                lastx[y1--] = x;
        } else {
            while (y1 < y2 - 1) {
                floorspritespan(lastx[y1 + 1], x - 1, y1 + 1);
                y1++;
            }
            y1 = twall;
        }
        while (y2 > bwall) {
            y2--;
            floorspritespan(lastx[y2], x - 1, y2);
        }
        while (y2 < bwall)
            lastx[y2++] = x;
    }
    while (y1 < y2 - 1) {
        floorspritespan(lastx[y1 + 1], x2 - 1, y1 + 1);
        y1++;
    }
}

/* The floor-sprite half of 0x4CA84: rotate the four corners into view space,
   clip against the near, side, top and bottom planes, and fill umost/dmost. */
static void drawfloorsprite(int depth, short tilenum, short spritenum, int pal, int cstat, short sectnum)
{
    spritetype *spr = &sprite[spritenum];
    int px[8], py[8], pz[8], qx[8], qy[8], qz[8], sx[8], sy[8];
    int xs, ys, ang, c, s, xsiz = tilesizx[tilenum], ysiz = tilesizy[tilenum];
    int xoff = (signed char)(picanm[tilenum] >> 8), yoff = (signed char)(picanm[tilenum] >> 16);
    int ax, ay, bx, by, i, n, m, nn, z, a, b, len, v1, v2;
    int minx = 0x7FFFFFFF, maxx = (int)0x80000000;
    int imin = -1, imax = -1, j, x, x1, x2, shade;

    if (cstat & 4)
        xoff = -xoff;
    if (cstat & 8)
        yoff = -yoff;
    if ((cstat & 0x40) && (globalposz > spr->z) == !(cstat & 8))
        return;
    xs = spr->x - globalposx;
    ys = spr->y - globalposy;
    px[0] = dmulscale(xs, cosglobalang, ys, singlobalang, 14);
    ang = (spr->ang + 2048 - globalang) & 2047;
    py[0] = dmulscale(ys, cosglobalang, xs, -singlobalang, 14);
    c = sintable()[(ang + 512) & 2047];
    s = sintable()[ang];
    ax = spr->xrepeat * (xoff + (xsiz >> 1));
    ay = spr->yrepeat * (yoff + (ysiz >> 1));
    px[0] += dmulscale(ax, s, ay, c, 16);
    bx = ysiz * spr->yrepeat;
    by = spr->xrepeat * xsiz;
    py[0] += dmulscale(ay, s, ax, -c, 16);
    px[1] = px[0] - mulscale(by, s, 16);
    py[1] = py[0] + mulscale(by, c, 16);
    a = -mulscale(bx, c, 16);
    b = -mulscale(bx, s, 16);
    px[2] = a + px[1];
    py[2] = b + py[1];
    px[3] = a + px[0];
    py[3] = b + py[0];
    z = 16 * (spr->z - globalposz) / 320;
    pz[0] = z;
    if (z == 0)
        return;
    pz[1] = pz[2] = pz[3] = z;
    {
        int i0 = (cstat & 4) ? 1 : 0, i1 = (cstat & 4) ? 0 : 1, i3 = (cstat & 4) ? 2 : 3;
        int dx, dy;

        globalxpanning = py[i0] << 16;
        globalypanning = px[i0] << 16;
        globalzd = pz[i0] << 16;
        dy = py[i1] - py[i0];
        dx = px[i1] - px[i0];
        len = imul(dx, dx) + imul(dy, dy);
        if (!((dx < 0 ? -dx : dx) >> 23 < len && (dy < 0 ? -dy : dy) >> 23 < len))
            return;
        globalx1 = (int)(((long long)dx << 22) / len);
        globalx2 = (int)(((long long)dy << 22) / len);
        dx = px[i3] - px[i0];
        dy = py[i3] - py[i0];
        len = imul(dy, dy) + imul(dx, dx);
        if (!((dx < 0 ? -dx : dx) >> 23 < len && (dy < 0 ? -dy : dy) >> 23 < len))
            return;
        globaly1 = (int)(((long long)dx << 22) / len);
        globaly2 = (int)(((long long)dy << 22) / len);
    }
    if (pz[0] < 0) {
        int t = py[1];

        py[1] = py[3];
        py[3] = t;
        t = px[1];
        px[1] = px[3];
        px[3] = t;
    }
    /* Keep x + y >= 0. */
    n = 0;
    v2 = px[0] + py[0];
    for (i = 0; i < 4; i++) {
        int k = i == 3 ? 0 : i + 1;

        v1 = v2;
        v2 = px[k] + py[k];
        if (v1 >= 0) {
            qy[n] = py[i];
            qz[n] = pz[i];
            qx[n++] = px[i];
        }
        if ((v2 ^ v1) < 0) {
            int t = (int)(((long long)v1 << 16) / (v1 - v2));

            qy[n] = py[i] + mulscale(py[k] - py[i], t, 16);
            qz[n] = pz[i] + mulscale(pz[k] - pz[i], t, 16);
            qx[n++] = px[i] + mulscale(px[k] - px[i], t, 16);
        }
    }
    if (n <= 2)
        return;
    /* Keep y - x <= 0. */
    m = 0;
    v2 = qy[0] - qx[0];
    for (i = 0; i < n; i++) {
        int k = i + 1 == n ? 0 : i + 1;

        v1 = v2;
        v2 = qy[k] - qx[k];
        if (v1 <= 0) {
            py[m] = qy[i];
            px[m] = qx[i];
            pz[m++] = qz[i];
        }
        if ((v2 ^ v1) < 0) {
            int t = (int)(((long long)v1 << 16) / (v1 - v2));

            py[m] = qy[i] + mulscale(qy[k] - qy[i], t, 16);
            pz[m] = qz[i] + mulscale(qz[k] - qz[i], t, 16);
            px[m++] = qx[i] + mulscale(qx[k] - qx[i], t, 16);
        }
    }
    if (m <= 2)
        return;
    /* Keep below the top row. */
    nn = 0;
    v2 = 160 * pz[0] + (globalhorizraw - wtop) * px[0];
    for (i = 0; i < m; i++) {
        int k = i + 1 == m ? 0 : i + 1;

        v1 = v2;
        v2 = 160 * pz[k] + (globalhorizraw - wtop) * px[k];
        if (v1 >= 0) {
            qy[nn] = py[i];
            qx[nn] = px[i];
            qz[nn++] = pz[i];
        }
        if ((v2 ^ v1) < 0) {
            int t = (int)(((long long)v1 << 16) / (v1 - v2));

            qy[nn] = py[i] + mulscale(py[k] - py[i], t, 16);
            qz[nn] = pz[i] + mulscale(pz[k] - pz[i], t, 16);
            qx[nn++] = px[i] + mulscale(px[k] - px[i], t, 16);
        }
    }
    if (nn <= 2)
        return;
    /* Keep above the bottom row. */
    m = 0;
    v2 = 160 * qz[0] - (wbot - globalhorizraw) * qx[0];
    for (i = 0; i < nn; i++) {
        int k = i + 1 == nn ? 0 : i + 1;

        v1 = v2;
        v2 = 160 * qz[k] - (wbot - globalhorizraw) * qx[k];
        if (v1 <= 0) {
            py[m] = qy[i];
            px[m] = qx[i];
            pz[m++] = qz[i];
        }
        if ((v2 ^ v1) < 0) {
            int t = (int)(((long long)v1 << 16) / (v1 - v2));

            py[m] = qy[i] + mulscale(qy[k] - qy[i], t, 16);
            pz[m] = qz[i] + mulscale(qz[k] - qz[i], t, 16);
            px[m++] = qx[i] + mulscale(qx[k] - qx[i], t, 16);
        }
    }
    if (m <= 2)
        return;
    for (i = 0; i < m; i++) {
        sx[i] = (xdimen << 15) + (int)((long long)(xdimen << 15) * py[i] / px[i]);
        sy[i] = (globalhoriz << 16) + (int)((long long)(xdimen << 15) * pz[i] / px[i]);
        if (sx[i] < 0)
            sx[i] = 0;
        if ((xdimen << 16) < sx[i])
            sx[i] = xdimen << 16;
        if ((wtop << 16) > sy[i])
            sy[i] = wtop << 16;
        if ((wbot << 16) < sy[i])
            sy[i] = wbot << 16;
        if (sx[i] < minx) {
            minx = sx[i];
            imin = i;
        }
        if (sx[i] > maxx) {
            maxx = sx[i];
            imax = i;
        }
    }
    for (j = imin; j != imax; j = i) {
        int xa, xe;

        i = j + 1 == m ? 0 : j + 1;
        xa = (sx[j] + 0xFFFF) >> 16;
        xe = (sx[i] + 0xFFFF) >> 16;
        if (xe > xa) {
            int slope = (int)(((long long)(sy[i] - sy[j]) << 16) / (sx[i] - sx[j]));
            int yy = sy[j] + mulscale(slope, (xa << 16) - sx[j], 16);

            for (x = xa; x < xe; x++) {
                umost[x] = (yy >> 16) - 1;
                yy += slope;
            }
        }
    }
    while (j != imin) {
        int xa, xe;

        i = j + 1 == m ? 0 : j + 1;
        xa = (sx[j] + 0xFFFF) >> 16;
        xe = (sx[i] + 0xFFFF) >> 16;
        if (xa > xe) {
            int slope = (int)(((long long)(sy[i] - sy[j]) << 16) / (sx[i] - sx[j]));
            int yy = sy[i] + mulscale(slope, (xe << 16) - sx[i], 16);

            for (x = xe; x < xa; x++) {
                dmost[x] = yy >> 16;
                yy += slope;
            }
        }
        j = i;
    }
    x1 = (minx + 0xFFFF) >> 16;
    x2 = (maxx + 0xFFFF) >> 16;
    for (x = x1; x <= x2; x++) {
        if (startumost[x] - 1 > umost[x])
            umost[x] = startumost[x] - 1;
        if (dmost[x] > startdmost[x])
            dmost[x] = startdmost[x];
    }
    clipsmost(spritenum, depth, x1, x2, umost, dmost, 0, 1);
    globalorientation = cstat;
    globalpicnum = tilenum;
    loadtile(globalpicnum);
    setgotpic(globalpicnum);
    globalpal = pal;
    shade = (depth >> (visibility + 4)) + spr->shade + 6 + sectorshade(sectnum);
    if (shade < 0)
        shade = 0;
    if (numpalookups <= shade)
        shade = numpalookups - 1;
    globalshade = shade;
    spanpal = palookup[globalpal] + (shade << 8);
    floorspritescan(x1, x2);
}

/* 0x4CA84 */
static void drawsprite(int sxpos, int depth, short tilenum, short spritenum, int pal)
{
    spritetype *spr;
    int cstat, xoff, yoff, xsiz, ysiz, x, sectnum;

    if (picanm[tilenum] & 0xC0)
        tilenum += animateoffs(tilenum);
    if ((unsigned)tilenum >= MAXTILES || tilesizx[tilenum] <= 0 || tilesizy[tilenum] <= 0 || spritenum < 0)
        return;
    spr = &sprite[spritenum];
    cstat = spr->cstat;
    sectnum = spr->sectnum;
    if ((cstat & 2) && !transluc)
        cstat &= ~2;
    yoff = (signed char)(picanm[tilenum] >> 16);
    xoff = (signed char)(picanm[tilenum] >> 8);
    if (cstat & 4)
        xoff = -xoff;
    if (cstat & 8)
        yoff = -yoff;
    xsiz = tilesizx[tilenum];
    ysiz = tilesizy[tilenum];

    if ((cstat & 0x30) == 0x10) {
        int xv = spr->xrepeat * sintable()[(spr->ang + 4096) & 2047];
        int yv = sintable()[(spr->ang + 3584) & 2047] * spr->xrepeat;
        int x1 = spr->x - mulscale((xsiz >> 1) + xoff, xv, 16) - globalposx;
        int y1 = spr->y - mulscale((xsiz >> 1) + xoff, yv, 16) - globalposy;
        int yp1 = (imul(y1, cosglobalang) - imul(x1, singlobalang)) >> 14;
        int xp1 = (imul(x1, cosglobalang) + imul(y1, singlobalang)) >> 14;
        int x2 = mulscale(xsiz, xv, 16) + x1;
        int y2 = mulscale(xsiz, yv, 16) + y1;
        int yp2 = (imul(y2, cosglobalang) - imul(x2, singlobalang)) >> 14;
        int xp2 = (imul(y2, singlobalang) + imul(cosglobalang, x2)) >> 14;
        int swapped = 0, z = MAXWALLSB - 1, zt, zb, num, den, dinc, i;
        sectortype *sec = &sector[sectnum];

        if (imul(xp2, yp1) > imul(xp1, yp2)) {
            int t = yp1;

            yp1 = yp2;
            yp2 = t;
            t = xp1;
            xp1 = xp2;
            xp2 = t;
            swapped = 1;
        }
        if ((cstat & 0x40) && swapped == 1)
            return;
        if (xp1 <= 0 && xp2 <= 0)
            return;
        if (-xp1 > yp1) {
            if (-xp2 > yp2)
                return;
            xb1[z] = 0;
            den = yp1 + xp1 - xp2 - yp2;
            if (den == 0)
                return;
            yb1[z] = (int)((long long)(yp1 + xp1) * (xp2 - xp1) / den) + xp1;
        } else {
            if (yp1 > xp1 || xp1 == 0)
                return;
            xb1[z] = (int)((long long)(xdimen >> 1) * yp1 / xp1) + (xdimen >> 1);
            if (yp1 >= 0)
                xb1[z]++;
            if (xb1[z] >= xdimen)
                xb1[z] = xdimen - 1;
            yb1[z] = xp1;
        }
        if (yp2 > xp2) {
            if (yp1 > xp1)
                return;
            xb2[z] = xdimen - 1;
            den = yp2 - yp1 + xp1 - xp2;
            if (den == 0)
                return;
            yb2[z] = (int)((long long)(xp1 - yp1) * (xp2 - xp1) / den) + xp1;
        } else {
            if (-xp2 > yp2 || xp2 == 0)
                return;
            xb2[z] = (xdimen >> 1) + (int)((long long)(xdimen >> 1) * yp2 / xp2) - 1;
            if (yp2 >= 0)
                xb2[z] = (xdimen >> 1) + (int)((long long)(xdimen >> 1) * yp2 / xp2);
            if (xb2[z] >= xdimen)
                xb2[z] = xdimen - 1;
            yb2[z] = xp2;
        }
        if (yb1[z] <= 0 || yb2[z] <= 0 || xb1[z] > xb2[z])
            return;
        den = mulscale(xdimen, yp1 - yp2, 3);
        num = imul(mulscale(xdimen, yp1, 2) - mulscale(xp1, xb1[z] - (xdimen >> 1), 1), xsiz) >> 3;
        dinc = mulscale((xp2 - xp1) >> 1, xb1[z] - (xdimen >> 1), 1);
        for (i = xb1[z];; i += 2) {
            den += dinc;
            if (i > xb2[z] + 1)
                break;
            if (den != 0)
                lwall[i] = (int)(((long long)num << 8) / den);
            dinc = (xp2 - xp1) >> 1;
            num -= imul(xsiz, xp1) >> 3;
        }
        for (i = xb1[z] + 1; i <= xb2[z]; i += 2)
            lwall[i] = (lwall[i - 1] + lwall[i + 1]) >> 1;
        if (lwall[xb1[z]] < 0)
            lwall[xb1[z]] = 0;
        if ((xsiz << 6) <= lwall[xb2[z]])
            lwall[xb2[z]] = (xsiz << 6) - 1;
        if (((cstat & 4) != 0) != swapped)
            for (i = xb1[z]; i <= xb2[z]; i++)
                lwall[i] = (xsiz << 6) - 1 - lwall[i];
        rx1[z] = yp1;
        ry1[z] = xp1;
        rx2[z] = yp2;
        ry2[z] = xp2;
        zb = spr->z - 4 * spr->yrepeat * yoff;
        zt = zb - 4 * ysiz * spr->yrepeat;
        globalpal = pal;
        globalorientation = 0;
        globalpicnum = tilenum;
        globalxpanning = 0;
        globalypanning = 0;
        globalshade = spr->shade + 6 + sectorshade((short)sectnum);
        setshiftval();
        globalyscale = (int)((512LL << ((globalshiftval - 19) & 31)) / spr->yrepeat);
        globalzd = imul(globalyscale, globalposz - zt) << 8;
        if (cstat & 8) {
            globalzd = imul(-globalyscale, globalposz - zb) << 8;
            globalyscale = -globalyscale;
        }
        if (!(sec->ceilingstat & 1) && zt < sec->ceilingz)
            zt = sec->ceilingz;
        if (!(sec->floorstat & 1) && zb > sec->floorz)
            zb = sec->floorz;
        wallmost(uwall2, z, zt - globalposz);
        wallmost(dwall2, z, zb - globalposz);
        fillswall(z);
        for (x = xb1[z]; x <= xb2[z]; x++) {
            umost[x] = startumost[x];
            dmost[x] = startdmost[x];
        }
        clipsmost(spritenum, depth, xb1[z], xb2[z], umost, dmost, 0, 0);
        if (cstat & 2) {
            for (x = xb1[z]; umost[x] > dmost[x] && x <= xb2[z]; x++)
                ;
            maskwallscan(x, xb2[z], uwall2, dwall2, 1);
        } else {
            maskwallscan(xb1[z], xb2[z], uwall2, dwall2, 0);
        }
        return;
    }
    if ((cstat & 0x30) == 0x20) {
        drawfloorsprite(depth, tilenum, spritenum, pal, cstat, (short)sectnum);
        return;
    }
    if ((cstat & 0x30) == 0x30 || depth <= 1024)
        return;
    {
        int scale = (int)(0x800000000LL / depth);
        int xspan, yspan, xl, xr, top, bot, x1, x2, shade, vinc, ceil, flor, xinc, xpos, t;
        const unsigned char *pal_row;

        if (xdimen != 320)
            scale = mulscale(viewingrange, scale, 16);
        xspan = (int)(((long long)(xsiz * spr->xrepeat) * scale) >> 14);
        yspan = (int)(((long long)(ysiz * spr->yrepeat) * scale) >> 14);
        if (!(xsiz >> 11 < xspan && (yspan >> 1) > ysiz))
            return;
        xl = sxpos - (xspan >> 1);
        t = (int)(((long long)(xoff * spr->xrepeat) * scale) >> 14);
        xl = (cstat & 4) ? t + xl : xl - t;
        t = mulscale(scale, spr->z - globalposz, 16);
        bot = (globalhoriz << 8) + t - (int)(((long long)(yoff * spr->yrepeat) * scale) >> 14);
        top = bot - yspan;
        xr = xl + xspan;
        /* The exe sets both low bytes to 0xFF before this compare. */
        if ((top | 0xFF) >= (bot | 0xFF))
            return;
        x1 = (xl >> 8) + 1;
        if (x1 < 0)
            x1 = 0;
        x2 = xr >> 8;
        if (x2 >= xdimen)
            x2 = xdimen - 1;
        shade = (short)(sectorshade((short)sectnum) + (depth >> (visibility + 4)) + spr->shade + 6);
        if (shade < 0)
            shade = 0;
        if (shade >= numpalookups)
            shade = numpalookups - 1;
        pal_row = palookup[pal] + (shade << 8);
        loadtile(tilenum);
        setgotpic(tilenum);
        vinc = (int)(((long long)ysiz << 32) / yspan);
        ceil = (sector[sectnum].ceilingstat & 1) ? 0
            : globalhoriz + mulscale(sector[sectnum].ceilingz - globalposz, scale, 24) - 1;
        flor = (sector[sectnum].floorstat & 1) ? 0x7FFFFFFF
            : globalhoriz + mulscale(sector[sectnum].floorz - globalposz, scale, 24) + 1;
        if (top >> 8 > ceil)
            ceil = top >> 8;
        if (bot >> 8 < flor)
            flor = bot >> 8;
        if (cstat & 4) {
            xinc = -(int)(((long long)xsiz << 20) / xspan);
            xpos = mulscale(xinc, (x1 << 8) - xr, 8);
        } else {
            xinc = (int)(((long long)xsiz << 20) / xspan);
            xpos = mulscale(xinc, (x1 << 8) - xl, 8);
        }
        if (cstat & 8) {
            vinc = -vinc;
            top = bot;
        }
        for (x = x1; x <= x2; x++) {
            uwall2[x] = startumost[x] < ceil ? ceil : startumost[x];
            dwall2[x] = startdmost[x] > flor ? flor : startdmost[x];
        }
        clipsmost(spritenum, depth, x1, x2, uwall2, dwall2, 1, 0);
        for (x = x1; x <= x2; x++) {
            int y1 = uwall2[x], y2 = dwall2[x] - 1;

            if (y1 < y2)
                mvline(vinc, mulscale(vinc, (y1 << 8) + 256 - top, 8), pal_row, (y2 - y1) << 8, 24,
                    waloff[tilenum] + (xpos >> 12) * ysiz, frame + frameoffset + y1 * pitch() + x,
                    (cstat & 2) != 0);
            xpos += xinc;
        }
    }
}

/* 0x4BFDB */
void drawmasks(void)
{
    int gap, i, l;

    for (gap = spritesortcnt >> 1; gap > 0; gap >>= 1) {
        for (i = 0; i < spritesortcnt - gap; i++) {
            for (l = i; l >= 0 && spritesy[l] > spritesy[l + gap]; l -= gap) {
                int t;
                short st;
                unsigned char b;

                t = spritesx[l];
                spritesx[l] = spritesx[l + gap];
                spritesx[l + gap] = t;
                t = spritesy[l];
                spritesy[l] = spritesy[l + gap];
                spritesy[l + gap] = t;
                st = tspritepicnum[l];
                tspritepicnum[l] = tspritepicnum[l + gap];
                tspritepicnum[l + gap] = st;
                b = tspritepal[l];
                tspritepal[l] = tspritepal[l + gap];
                tspritepal[l + gap] = b;
                st = tspritelist[l];
                tspritelist[l] = tspritelist[l + gap];
                tspritelist[l + gap] = st;
            }
        }
    }
    while (spritesortcnt > 0 && maskwallcnt > 0) {
        int mw = maskwall[maskwallcnt - 1];

        if (spritewallfront(tspritelist[spritesortcnt - 1], thewall[mw])) {
            int first = -1, drawn = 0, k;

            for (i = spritesortcnt - 2; i >= 0; i--) {
                int xs = spritesx[i] >> 8;

                if (xs >= xb1[mw] && xs <= xb2[mw] && !spritewallfront(tspritelist[i], thewall[mw])) {
                    drawn++;
                    drawsprite(spritesx[i], spritesy[i], tspritepicnum[i], tspritelist[i], tspritepal[i]);
                    first = i;
                    tspritelist[i] = -1;
                }
            }
            if (first >= 0) {
                l = first;
                for (k = first; k < spritesortcnt; k++) {
                    if (tspritelist[k] < 0)
                        continue;
                    if (l < k) {
                        spritesx[l] = spritesx[k];
                        spritesy[l] = spritesy[k];
                        tspritepicnum[l] = tspritepicnum[k];
                        tspritelist[l] = tspritelist[k];
                        tspritepal[l] = tspritepal[k];
                    }
                    l++;
                }
                spritesortcnt -= drawn;
            }
            drawmaskwall((short)--maskwallcnt);
        } else {
            spritesortcnt--;
            drawsprite(spritesx[spritesortcnt], spritesy[spritesortcnt], tspritepicnum[spritesortcnt],
                tspritelist[spritesortcnt], tspritepal[spritesortcnt]);
        }
    }
    while (spritesortcnt > 0) {
        spritesortcnt--;
        drawsprite(spritesx[spritesortcnt], spritesy[spritesortcnt], tspritepicnum[spritesortcnt],
            tspritelist[spritesortcnt], tspritepal[spritesortcnt]);
    }
    while (maskwallcnt > 0)
        drawmaskwall((short)--maskwallcnt);
}

void draw_init(void)
{
    int i;

    initmath();
    for (i = 0; i <= MAXYDIM + 1; i++)
        lastx[i] = -1;
    for (i = 0; i < 1024; i++)
        if (i != 511)
            horizlookup[i] = 65536 / (i - 511);
}

const unsigned char *framebuffer(void)
{
    return frame;
}

int framebuffer_pitch(void)
{
    return pitch();
}
