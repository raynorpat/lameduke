/* The 1996 platform calls build.c makes, on this engine's SDL layer, and
   the dev aids the check scripts drive it with:
   LAMEDUKE_FRAME=file and LAMEDUKE_FRAME_AT=n write the nth shown frame
   (a PPM) and exit; LAMEDUKE_KEYS=step:scancode,... presses each key at
   that _idle step (nextpage and every waiting loop take one); build.c
   releases what it handles, step:~scancode releases a held key, step:@x/y
   puts the cursor at (x,y), step:px/y centres the view on map (x,y) and
   step:bN sets the mouse buttons (1 left). */

#include <stdio.h>
#include <stdlib.h>

#include "editor.h"

static uint32_t lasttick;
static int scriptbuttons; /* LAMEDUKE_KEYS step:b1 / step:b0 */
static int tickrem;
static int shown;

static void write_frame(const char *name)
{
    FILE *fp = fopen(name, "wb");
    const unsigned char *f = framebuffer();
    int pitch = ydim16 ? 640 : framebuffer_pitch(), x, y;

    if (!fp)
        return;
    fprintf(fp, "P6 %d %d 255\n", xdim, ydim);
    for (y = 0; y < ydim; y++)
        for (x = 0; x < xdim; x++) {
            /* the 640x480 2D mode shows the map from 92160 and the status bar from 0 */
            int row = ydim == 480 && ydim16 ? (y < 336 ? 144 + y : y - 336) : y;

            fwrite(&palette[3 * f[row * pitch + x]], 3, 1, fp);
        }
    fclose(fp);
}

static void scriptkeys(void)
{
    static int steps;
    const char *k = getenv("LAMEDUKE_KEYS");

    steps++;
    while (k && *k) {
        int f = atoi(k), sc;
        const char *c = strchr(k, ':');

        if (!c)
            break;
        if (f == steps && c[1] == 'b') {
            scriptbuttons = atoi(c + 2);
        } else if (f == steps && (c[1] == '@' || c[1] == 'p')) {
            /* @x/y puts the cursor there, px/y the view's centre */
            const char *slash = strchr(c, '/');
            int x = atoi(c + 2), y = slash ? atoi(slash + 1) : 0;

            if (c[1] == '@')
                searchx = x, searchy = y;
            else
                posx = x, posy = y;
        } else if (f == steps) {
            sc = (int)strtol(c + 1 + (c[1] == '~'), NULL, 16) & 0xFF;
            keystatus[sc] = c[1] != '~';
        }
        k = strchr(c, ',');
        if (k)
            k++;
    }
}

#undef nextpage
static uint32_t lastshow;
static int justshown;

/* A frame reached the screen: the frame dump counts it. */
static void countframe(void)
{
    const char *at = getenv("LAMEDUKE_FRAME_AT"), *name = getenv("LAMEDUKE_FRAME");

    lastshow = timer_ticks();
    shown++;
    if (name && shown >= (at && atoi(at) > 0 ? atoi(at) : 1)) {
        printf("title: %s\n", platform_title_text());
        write_frame(name);
        exit(0);
    }
}

void editor_nextpage(void)
{
    nextpage();
    countframe();
    justshown = 1;
    _idle();
}

/* The 120 Hz timer interrupt: totalclock and build.c's key ramp. */
void _idle(void)
{
    uint32_t now;

    if (input_poll())
        editor_quit();
    scriptkeys();
    /* 2D prompts wait here without nextpage; DOS showed video memory as it
       was written, so show the frame (every step under a script) */
    if (ydim16 && !justshown
        && (getenv("LAMEDUKE_KEYS") || getenv("LAMEDUKE_FRAME") || timer_ticks() - lastshow >= 15)) {
        showpage();
        countframe();
    }
    justshown = 0;
    now = timer_ticks();
    tickrem += (int)(now - lasttick) * 120;
    lasttick = now;
    while (tickrem >= 1000) {
        tickrem -= 1000;
        timerhandler();
    }
    if (getenv("LAMEDUKE_KEYS") || getenv("LAMEDUKE_FRAME"))
        return;
    timer_delay(1);
}

void initkeys(void)
{
}

void uninitkeys(void)
{
}

void inittimer(void)
{
    timer_init();
    lasttick = timer_ticks();
}

void uninittimer(void)
{
}

void getmousearea(int *w, int *h)
{
    *w = xdim;
    *h = ydim;
}

#undef getmousevalues
void editor_getmousevalues(short *mousx, short *mousy, short *bstatus)
{
    int x, y, b;

    getmousevalues(&x, &y, &b);
    *mousx = (short)x;
    *mousy = (short)y;
    *bstatus = (short)(b | scriptbuttons);
}

/* Mouse counts to screen pixels, keeping what is under a pixel. */
int32_t editormousemove(int delta, int32_t dim, int window, int32_t *rem)
{
    int32_t v = delta * dim + *rem;

    *rem = v % window;
    return v / window;
}

/* A line on the 3D screen in 20.12 coordinates, as the 1996 engine took them. */
void drawline256(int32_t x1, int32_t y1, int32_t x2, int32_t y2, unsigned char col)
{
    int32_t dx = x2 - x1, dy = y2 - y1, n = max(klabs(dx), klabs(dy)) >> 12, i;

    col = palookup[0][col];
    for (i = 0; i <= n; i++) {
        int32_t x = n ? x1 + (int32_t)((int64_t)dx * i / n) : x1, y = n ? y1 + (int32_t)((int64_t)dy * i / n) : y1;

        plotpixel(x >> 12, y >> 12, (char)col);
    }
}

void clearview(int32_t col)
{
    memset((unsigned char *)framebuffer(), col, (size_t)(framebuffer_pitch() * ydim));
}

/* With pageoffset 0 the offset counts eight-pixel groups into the status
   bar at the frame's start; otherwise it is a pixel offset. */
void fillscreen16(int32_t offset, int32_t color, int32_t blocksize)
{
    int32_t p = pageoffset ? offset : offset << 3;

    if (p < 0)
        p = 0;
    if (p + blocksize > 640 * 480)
        blocksize = 640 * 480 - p;
    if (blocksize > 0)
        memset((unsigned char *)framebuffer() + p, color, (size_t)blocksize);
}

void editor_quit(void)
{
    platform_quit();
    exit(0);
}
