#include "display.h"
#include "draw.h"
#include "engine.h"
#include "buildmath.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdl_window.h"

int xdim = 320;
int ydim = 200;
unsigned char palette[768];
static unsigned char vgapalette[768]; /* PALETTE.DAT's 6-bit colours */
static int curbrightness;             /* dword_82C12 */
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

    if (getrendermode() >= 2)
        mode = 0;
    if (mode == 7) {
        xdim = 640;
        ydim = 480;
    } else {
        xdim = 320;
        ydim = 200;
    }
    ydim16 = 0;
    pageoffset = 0;
    if (platform_video(getrendermode() == 2 ? renderwidth : xdim,
                       getrendermode() == 2 ? renderheight : ydim) != 0)
        errorexit(platform_video_error());
    setbrightness(curbrightness);
}

static const unsigned char ega[16][3] = {
    { 0, 0, 0 }, { 0, 0, 170 }, { 0, 170, 0 }, { 0, 170, 170 }, { 170, 0, 0 }, { 170, 0, 170 },
    { 170, 85, 0 }, { 170, 170, 170 }, { 85, 85, 85 }, { 85, 85, 255 }, { 85, 255, 85 }, { 85, 255, 255 },
    { 255, 85, 85 }, { 255, 85, 255 }, { 255, 255, 85 }, { 255, 255, 255 },
};

/* 0x53F42: the 640x350 16-colour mode the 2D map draws in, with the
   palette the BIOS sets for it. setgamemode puts the game palette back. */
void setmapmode(void)
{
    xdim = 640;
    ydim = 350;
    ydim16 = 350;
    pageoffset = 0;
    if (platform_video(xdim, ydim) != 0)
        errorexit(platform_video_error());
    memcpy(palette, ega, sizeof ega);
}

/* 0x53FF8: the 640x480 16-colour mode, 336 rows for the 2D view and the
   rest for the status bar. */
void qsetmode640480(void)
{
    xdim = 640;
    ydim = 480;
    ydim16 = 336;
    pageoffset = 92160;
    if (platform_video(xdim, ydim) != 0)
        errorexit(platform_video_error());
    memset(palette, 0, sizeof palette);
    memcpy(palette, ega, sizeof ega);
    /* the status bar memory in colour 8, the map area clear */
    memset((unsigned char *)framebuffer(), 8, 92160);
    memset((unsigned char *)framebuffer() + 92160, 0, 640 * 336);
}

/* 0x57A5A: the DAC gets each colour through row level of the brightness
   table, the last 1024 bytes of TABLES.DAT. */
void setbrightness(int level)
{
    const unsigned char *britable = tables_dat() + TABLES_DAT_SIZE - 1024;
    int i;

    curbrightness = level > 15 ? 15 : level;
    for (i = 0; i < 768; i++) {
        unsigned v = britable[64 * curbrightness + vgapalette[i]];

        palette[i] = (unsigned char)((v << 2) | (v >> 4));
    }
}

/* 0x4B390: the 320x200 screen to captNNNN.pcx, counting up from capt0000
   each run; a file that will not open takes its number back. */
void screencapture(void)
{
    /* unk_82C24 as the 320x200 path patches it */
    static const unsigned char header[128] = { 10, 5, 1, 8, 0, 0, 0, 0, 63, 1, 199, 0, 64, 1, 200, 0,
        0, 0, 0, 8, 8, 8, 16, 16, 16, 24, 24, 24, 32, 32, 32, 40, 40, 40, 48, 48, 48, 56, 56, 56, 64, 64, 64,
        72, 72, 72, 80, 80, 80, 88, 88, 88, 96, 96, 96, 104, 104, 104, 112, 112, 112, 120, 120, 120, 0, 1, 64,
        1 };
    static int capturecount; /* word_23DE58 */
    const unsigned char *src = framebuffer();
    char name[16];
    FILE *fp;
    int i;

    if (getrendermode() >= 2) /* The PCX writer is a 320x200 frame reader. */
        return;
    snprintf(name, sizeof name, "capt%04d.pcx", capturecount++ % 10000);
    fp = plat_fopen(name, "wb");
    if (!fp) {
        capturecount--;
        return;
    }
    fwrite(header, 1, sizeof header, fp);
    for (i = 0; i < 320 * 200;) {
        unsigned char c = src[i++];
        int run = 1;

        while (i < 320 * 200 && src[i] == c && run < 63 && i % 320 != 0) {
            run++;
            i++;
        }
        if (run > 1 || c >= 192)
            fputc(0xC0 | run, fp);
        fputc(c, fp);
    }
    fputc(12, fp);
    for (i = 0; i < 768; i++)
        fputc((palette[i] >> 2) * 4, fp);
    if (fclose(fp) != 0)
        capturecount--;
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
    for (i = 0; i < 768; i++)
        vgapalette[i] = (unsigned char)(raw[i] > 63 ? 63 : raw[i]);
    setbrightness(curbrightness);
    return 0;
}
