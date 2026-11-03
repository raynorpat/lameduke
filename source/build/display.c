#include "display.h"
#include "engine.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_window.h"

int xdim = 320;
int ydim = 200;
unsigned char palette[768];
unsigned char *palookup[256]; /* dword_1BCB10 */
int numpalookups;              /* word_23DE78 */
unsigned char *transluc;       /* dword_82C1F */
short pskyoff[256];            /* word_1BA410 */
short pskybits;                /* word_23DE7E */

void setgamemode(void)
{
    /* 0x49845. Mode 7 is 640x480 VESA. Modes 0-6 are 320x200 VGA variants
       (chained, mode-X, Tseng, Paradise, S3, two stereo modes). */
    int mode = setup_byte(0);

    if (mode == 7) {
        xdim = 640;
        ydim = 480;
    } else {
        xdim = 320;
        ydim = 200;
    }
    if (platform_video(xdim, ydim) != 0)
        errorexit(platform_video_error());
}

/* 0x13760. Each record is a palookup number and a 256-byte remap of palookup 0
   (makepalookup 0x57239 with no fog colour). */
int loadlookup(void)
{
    FILE *fp;
    unsigned char count;
    int i;

    loadpalette();
    pskyoff[0] = 0; /* 0x3E6E0 */
    pskybits = 3;
    fp = plat_fopen("lookup.dat", "rb");
    if (!fp)
        errorexit("Palette remaps not found.");
    if (fread(&count, 1, 1, fp) != 1)
        count = 0;
    for (i = 0; i < count; i++) {
        unsigned char num;
        unsigned char remap[256];
        int s, j;

        if (fread(&num, 1, 1, fp) != 1 || fread(remap, 1, 256, fp) != 256)
            break;
        if (!palookup[num])
            palookup[num] = (unsigned char *)malloc((size_t)numpalookups << 8);
        if (!palookup[num])
            errorexit("Out of memory!");
        for (s = 0; s < numpalookups; s++)
            for (j = 0; j < 256; j++)
                palookup[num][(s << 8) + j] = palookup[0][(s << 8) + remap[j]];
    }
    fclose(fp);
    return 0;
}

/* 0x49647. 768 VGA palette bytes, then numpalookups shade tables, then the
   translucency table stored as its upper triangle. */
int loadpalette(void)
{
    static int loaded; /* byte_23DE98 */
    FILE *fp;
    unsigned char raw[768];
    long len;
    int i, j;

    if (loaded)
        return 0;
    loaded = 1;
    fp = plat_fopen("palette.dat", "rb");
    if (!fp)
        return -1;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    numpalookups = (int)((len - 768) >> 7);
    if (numpalookups & 1) {
        numpalookups = (numpalookups - 255) >> 1;
        transluc = (unsigned char *)malloc(65536);
    } else {
        numpalookups >>= 1;
    }
    palookup[0] = (unsigned char *)malloc((size_t)numpalookups << 8);
    if (!palookup[0] || fread(raw, 1, 768, fp) != 768
        || fread(palookup[0], 1, (size_t)numpalookups << 8, fp) != (size_t)numpalookups << 8) {
        fclose(fp);
        errorexit("Out of memory!");
    }
    if (transluc) {
        for (i = 0; i < 255; i++) {
            if (fread(&transluc[257 * i + 1], 1, (size_t)(255 - i), fp) != (size_t)(255 - i))
                break;
            for (j = i + 1; j < 256; j++)
                transluc[(j << 8) + i] = transluc[(i << 8) + j];
        }
        for (i = 0; i < 256; i++)
            transluc[257 * i] = (unsigned char)i;
    }
    fclose(fp);
    for (i = 0; i < 768; i++) {
        unsigned int v = raw[i];
        if (v > 63)
            v = 63;
        palette[i] = (unsigned char)((v << 2) | (v >> 4));
    }
    return 0;
}
