/* convmap4 IN OUT: a version-4 board (L9.MAP, M8.MAP) to version 5, through
   the editor's writeboard. The v4 layout is in docs/map-format-v4.md. The
   tags split into lotag (low half) and hitag (high half); the fields v5
   added take the shipped maps' commonest values (pal and visibility 0,
   extra -1). A v4 field v5 cannot hold is reported, not dropped silently. */

#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

static const unsigned char *p;

static int s8(int o) { return (signed char)p[o]; }
static int u8(int o) { return p[o]; }
static int s16(int o) { return (short)(p[o] | p[o + 1] << 8); }
static int u16(int o) { return p[o] | p[o + 1] << 8; }
static int s32(int o) { return (int)((unsigned)p[o] | (unsigned)p[o + 1] << 8 | (unsigned)p[o + 2] << 16 | (unsigned)p[o + 3] << 24); }

int main(int argc, char **argv)
{
    FILE *fp;
    long len;
    unsigned char *buf;
    int ns, nw, np, i, o, lost = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: convmap4 IN OUT\n");
        return 2;
    }
    if (_stricmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "convmap4: OUT must not be IN\n");
        return 2;
    }
    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "convmap4: cannot open %s\n", argv[1]);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        fprintf(stderr, "convmap4: cannot read %s\n", argv[1]);
        return 1;
    }
    fclose(fp);
    p = buf;
    if (len < 26 || s32(0) != 4) {
        fprintf(stderr, "convmap4: %s is not a version-4 board\n", argv[1]);
        return 1;
    }
    ns = u16(20);
    nw = u16(22);
    np = u16(24);
    if (ns < 1 || ns > MAXSECTORS || nw > MAXWALLS || np > MAXSPRITES || 26 + 32L * ns + 32L * nw + 40L * np != len) {
        fprintf(stderr, "convmap4: %s: counts %d/%d/%d do not match its %ld bytes\n", argv[1], ns, nw, np, len);
        return 1;
    }
    initspritelists();
    for (i = 0, o = 26; i < ns; i++, o += 32) {
        sectortype *s = &sector[i];

        memset(s, 0, sizeof *s);
        s->wallptr = (short)s16(o);
        s->wallnum = (short)s16(o + 2);
        s->ceilingstat = (unsigned char)u8(o + 4);
        s->ceilingxpanning = (unsigned char)u8(o + 5);
        s->ceilingypanning = (unsigned char)u8(o + 6);
        s->ceilingshade = (signed char)s8(o + 7);
        s->ceilingz = s32(o + 8);
        s->ceilingpicnum = (short)s16(o + 12);
        s->ceilingheinum = (short)s16(o + 14);
        s->floorstat = (unsigned char)u8(o + 16);
        s->floorxpanning = (unsigned char)u8(o + 17);
        s->floorypanning = (unsigned char)u8(o + 18);
        s->floorshade = (signed char)s8(o + 19);
        s->floorz = s32(o + 20);
        s->floorpicnum = (short)s16(o + 24);
        s->floorheinum = (short)s16(o + 26);
        s->lotag = (short)s16(o + 28);
        s->hitag = (short)s16(o + 30);
        s->extra = -1;
    }
    for (i = 0; i < nw; i++, o += 32) {
        walltype *w = &wall[i];

        memset(w, 0, sizeof *w);
        w->x = s32(o);
        w->y = s32(o + 4);
        w->point2 = (short)s16(o + 8);
        w->cstat = (short)u8(o + 10);
        w->shade = (signed char)s8(o + 11);
        w->xrepeat = (unsigned char)u8(o + 12);
        w->yrepeat = (unsigned char)u8(o + 13);
        w->xpanning = (unsigned char)u8(o + 14);
        w->ypanning = (unsigned char)u8(o + 15);
        w->picnum = (short)s16(o + 16);
        w->overpicnum = (short)s16(o + 18);
        w->nextsector = (short)s16(o + 20);
        w->nextwall = (short)s16(o + 22);
        w->nextsector2 = (short)s16(o + 24);
        w->nextwall2 = (short)s16(o + 26);
        w->lotag = (short)s16(o + 28);
        w->hitag = (short)s16(o + 30);
        w->extra = -1;
    }
    for (i = 0; i < np; i++, o += 40) {
        spritetype *s = &sprite[i];

        memset(s, 0, sizeof *s);
        s->x = s32(o);
        s->y = s32(o + 4);
        s->z = s32(o + 8);
        s->cstat = (unsigned char)u8(o + 12);
        s->shade = (signed char)s8(o + 13);
        s->xrepeat = (unsigned char)u8(o + 14);
        s->yrepeat = (unsigned char)u8(o + 15);
        s->picnum = (short)s16(o + 16);
        s->ang = (short)s16(o + 18);
        s->xvel = (short)s16(o + 20);
        s->yvel = (short)s16(o + 22);
        s->zvel = (short)s16(o + 24);
        s->owner = (short)s16(o + 26);
        s->sectnum = (short)s16(o + 28);
        s->statnum = (short)s16(o + 30);
        s->lotag = (short)s16(o + 32);
        s->hitag = (short)s16(o + 34);
        s->extra = -1;
        if (s32(o + 36) != 0) {
            printf("sprite %d: extra pointer %08x is not kept (v5 has none)\n", i, (unsigned)s32(o + 36));
            lost++;
        }
        if (s->statnum < 0 || s->statnum >= MAXSTATUS) {
            printf("sprite %d: statnum %d cannot be saved; set to 0\n", i, s->statnum);
            s->statnum = 0;
            lost++;
        }
    }
    numsectors = ns;
    numwalls = nw;
    numsprites = np;
    /* kept as the v4 data has them, but said: the shipped N8 has M8's start */
    if (inside(s32(4), s32(8), (short)s16(18)) != 1)
        printf("note: the start (%d,%d) is not inside its sector %d; kept\n", s32(4), s32(8), s16(18));
    for (i = 0; i < np; i++)
        if (sprite[i].sectnum < 0 || sprite[i].sectnum >= ns)
            printf("note: sprite %d is in sector %d, which does not exist; kept\n", i, sprite[i].sectnum);
    boardhadextra = 0; /* v4 has no extras count */
    if (writeboard(argv[2], s32(4), s32(8), s32(12), (short)s16(16), (short)s16(18)) != 0) {
        fprintf(stderr, "convmap4: cannot write %s\n", argv[2]);
        return 1;
    }
    printf("%s: %d sectors, %d walls, %d sprites to version 5 in %s%s\n", argv[1], ns, nw, np, argv[2],
        lost ? " (see above)" : "");
    free(buf);
    return 0;
}
