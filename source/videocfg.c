#include "videocfg.h"
#include "draw.h"
#include "display.h"
#include "sdl_window.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

videocfg_t videocfg, videocfg_active;
int videocfg_apply_failed, videocfg_gl_failed;
static const char *displays[] = { "windowed", "fullscreen", "borderless" };
static const char *filters[] = { "nearest", "bilinear", "trilinear" };
static const char *samples[] = { "off", "2k", "4k", "8k" };

void videocfg_defaults(videocfg_t *c)
{
    memset(c, 0, sizeof *c);
    c->width = 640;
    c->height = 480;
    c->vsync = 1;
    c->contrast = c->saturation = 1.0;
}

static int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

static int integer(const char *s, int fallback, int low, int high)
{
    char *end;
    long value;
    errno = 0;
    value = strtol(s, &end, 10);
    if (end == s || *end)
        return fallback;
    if (errno == ERANGE)
        return value < 0 ? low : high;
    return value < low ? low : value > high ? high : (int)value;
}

static double real(const char *s, double fallback, double low, double high)
{
    char *end;
    double value;
    errno = 0;
    value = strtod(s, &end);
    if (end == s || *end)
        return fallback;
    if (errno == ERANGE && !isfinite(value))
        return value < 0 ? low : high;
    if (!isfinite(value))
        return fallback;
    return value < low ? low : value > high ? high : value;
}

static char *trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

static int choice(const char *s, const char **names, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (!strcmp(s, names[i])) return i;
    return 0;
}

int videocfg_read(const char *path, videocfg_t *c)
{
    FILE *fp;
    char line[512];
    videocfg_defaults(c);
    fp = fopen(path, "r");
    if (!fp) return -1;
    while (fgets(line, sizeof line, fp)) {
        char *key, *value, *p;
        /* Ignore the entire overlong line, never parse its tail as a key. */
        if (!strchr(line, '\n') && !feof(fp)) {
            int ch;
            while ((ch = fgetc(fp)) != '\n' && ch != EOF) ;
            continue;
        }
        p = strchr(line, ';');
        if (p) *p = 0;
        p = strchr(line, '=');
        if (!p) continue;
        *p = 0;
        key = trim(line);
        value = trim(p + 1);
        if (!strcmp(key, "renderer")) {
            c->renderer = integer(value, 0, 0, 3);
            if (c->renderer == 1) c->renderer = 0;
        } else if (!strcmp(key, "width")) c->width = integer(value, 640, 320, 7680);
        else if (!strcmp(key, "height")) c->height = integer(value, 480, 200, 4320);
        else if (!strcmp(key, "display")) c->display = choice(value, displays, 3);
        else if (!strcmp(key, "vsync")) c->vsync = integer(value, 1, 0, 1);
        else if (!strcmp(key, "filter")) c->filter = choice(value, filters, 3);
        else if (!strcmp(key, "miplevel")) c->miplevel = integer(value, 0, 0, 15);
        else if (!strcmp(key, "contrast")) c->contrast = real(value, 1, 0.5, 1.5);
        else if (!strcmp(key, "saturation")) c->saturation = real(value, 1, 0, 2);
        else if (!strcmp(key, "supersample")) c->supersample = choice(value, samples, 4);
    }
    fclose(fp);
    return 0;
}

int videocfg_write(const char *path, const videocfg_t *c)
{
    FILE *fp = fopen(path, "w");
    int result;
    if (!fp) return -1;
    result = fprintf(fp, "renderer = %d\nwidth = %d\nheight = %d\ndisplay = %s\nvsync = %d\nfilter = %s\nmiplevel = %d\ncontrast = %.17g\nsaturation = %.17g\nsupersample = %s\n",
        c->renderer == 1 ? 0 : clamp(c->renderer, 0, 3), c->width, c->height,
        displays[clamp(c->display, 0, 2)], c->vsync != 0,
        filters[clamp(c->filter, 0, 2)], c->miplevel, c->contrast, c->saturation,
        samples[clamp(c->supersample, 0, 3)]);
    if (fclose(fp) != 0) return -1;
    return result < 0 ? -1 : 0;
}

void videocfg_apply(const videocfg_t *c)
{
    videocfg_t requested = *c;
    int previous = getrendermode();
    int renderer = clamp(requested.renderer, 0, 3);
    int resolution_failed = 0;
    int same_gl_mode = previous == 3 && renderer == 3
        && requested.width == renderwidth && requested.height == renderheight;
    videocfg_apply_failed = 0;
    if (requested.display != getvideodisplaymode())
        setvideodisplaymode(requested.display);
    setvideovsync(requested.vsync);
    if (!same_gl_mode && setrenderresolution(requested.width, requested.height) != 0) {
        videocfg_apply_failed = resolution_failed = 1;
        renderer = previous;
        printf("Video resolution unavailable; keeping the current renderer.\n");
        requested.width = renderwidth;
        requested.height = renderheight;
    }
    if (!resolution_failed && !rendereravailable(renderer)) {
        videocfg_apply_failed = 1;
        renderer = 0;
    } else if (!resolution_failed && renderer == 3 && !same_gl_mode) {
        videocfg_gl_failed = setgamemode_gl(renderwidth, renderheight) != 0;
        if (videocfg_gl_failed) {
            videocfg_apply_failed = 1;
            renderer = previous;
        }
    }
    requested.renderer = setrendermode(renderer);
    if (!resolution_failed && requested.renderer != clamp(c->renderer, 0, 3))
        printf("%s; %s\n", c->renderer >= 3 ? "OpenGL renderer unavailable"
            : "Polymost renderer unavailable in this build",
            requested.renderer ? "keeping the current renderer." : "using the classic renderer.");
    setvideotexfilter(requested.filter);
    setvideomiplevel(requested.miplevel);
    setvideocolor(requested.contrast, requested.saturation);
    if (!same_gl_mode || requested.supersample != videocfg_active.supersample)
        setvideosupersample(requested.supersample);
    videocfg_active = requested;
}
