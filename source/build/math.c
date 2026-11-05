#include "buildmath.h"

#include "platform.h"

#include <stdio.h>
#include <string.h>

static unsigned char tables[TABLES_DAT_SIZE];
static int tables_ok;

int loadtables(void)
{
    FILE *fp;
    size_t n;

    if (tables_ok)
        return 0;
    fp = plat_fopen("tables.dat", "rb");
    if (!fp)
        return -1;
    n = fread(tables, 1, sizeof tables, fp);
    fclose(fp);
    if (n != sizeof tables) {
        memset(tables, 0, sizeof tables);
        return -1;
    }
    tables_ok = 1;
    return 0;
}

const unsigned char *tables_dat(void)
{
    return tables;
}

const short *sintable(void)
{
    if (!tables_ok)
        return NULL;
    return (const short *)tables;
}

int sqrttable[2048]; /* dword_180E90, filled by 0x49518 */
int randomseed; /* 0x23CE1C */

/* 0x4BBA8. Newton's iteration from 128. */
static int newton_sqrt(int n)
{
    int x = 128, old;

    if (n < 0)
        return -1;
    do {
        old = x;
        x = (x + n / x) >> 1;
    } while (old - x > 1 || x - old > 1);
    return x;
}

void initmath(void)
{
    int i;

    for (i = 0; i < 2048; i++)
        sqrttable[i] = newton_sqrt(i << 20);
}

/* The msqrt both 0x4BB6C and the drawers use, before its final >> 10. */
int tablesqrt(int v)
{
    int b, sh;

    if (v < 0)
        v = -v;
    for (b = 31; b > 0 && !((unsigned)v >> b); b--)
        ;
    if (b >= 11) {
        sh = (b - 9) & 0xFE;
        return sqrttable[(unsigned)v >> sh] << (sh >> 1);
    }
    return sqrttable[v];
}

/* 0x4BB6C */
int ksqrt(int v)
{
    return v ? (int)((unsigned)tablesqrt(v) >> 10) : 0;
}

/* 0x55647 */
int krand(void)
{
    randomseed = (unsigned short)(21 * randomseed + 1);
    return randomseed;
}

/* 0x4BA86. The arctangent comes from the 320-entry table in tables.dat. */
int getangle(int xvect, int yvect)
{
    const short *radarang = (const short *)(tables + 8192);
    int v, neg;

    if ((xvect | yvect) == 0)
        return 0;
    if (xvect == 0)
        return 512 + ((yvect < 0) << 10);
    if (yvect == 0)
        return (xvect < 0) << 10;
    if (xvect == yvect)
        return 256 + ((yvect < 0) << 10);
    if (xvect == -yvect)
        return 768 + ((yvect < 0) << 10);
    if ((xvect < 0 ? -xvect : xvect) <= (yvect < 0 ? -yvect : yvect)) {
        v = radarang[160 - 160 * xvect / yvect] + 512;
        neg = yvect < 0;
    } else {
        v = radarang[160 + 160 * yvect / xvect];
        neg = xvect < 0;
    }
    return (v + (neg << 10) + 2048) & 2047;
}

/* Watcom's rand(), which the game also calls. Nothing seeds it, so it starts at 1. */
int wrand(void)
{
    static unsigned int seed = 1;

    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) & 0x7FFF);
}
