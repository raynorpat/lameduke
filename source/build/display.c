#include "display.h"
#include "engine.h"

#include <stdio.h>
#include <string.h>

#include "sdl_window.h"

int xdim = 320;
int ydim = 200;
unsigned char palette[768];

static int view_x1, view_y1, view_x2, view_y2;

void setview(int x1, int y1, int x2, int y2)
{
    view_x1 = x1;
    view_y1 = y1;
    view_x2 = x2;
    view_y2 = y2;
}

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

int loadlookup(void)
{
    FILE *fp = fopen("lookup.dat", "rb");
    if (!fp)
        return -1;
    fclose(fp);
    return 0;
}

int loadpalette(void)
{
    FILE *fp;
    unsigned char raw[768];
    int i;

    memset(palette, 0, sizeof palette);
    fp = fopen("PALETTE.DAT", "rb");
    if (!fp)
        return -1;
    if (fread(raw, 1, sizeof raw, fp) != sizeof raw) {
        fclose(fp);
        return -1;
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
