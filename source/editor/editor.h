/* What build.c expects from the 1996 engine headers (engine.h, build.h,
   pragmas.h, display.h), supplied from this repo's 1994 engine. */
#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include <string.h>

#include "board.h"
#include "display.h"
#include "draw.h"
#include "engine.h"
#include "buildmath.h"
#include "platform.h"
#include "polymost.h"
#include "sdl_input.h"
#include "sdl_main.h"
#include "sdl_sys.h"
#include "sdl_window.h"
#include "tiles.h"
#include "videocfg.h"

int editor_is_override(const char *arg);
void editor_config_init(int argc, char **argv);
void editor_set3d(void);
void editor_cycle_renderer(void);
void editor_renderer_message(int color);
void editor_tile_begin(void);
int editor_tile_draw(int x, int y, int tile, int size);
#ifdef USE_OPENGL
void setpolymost2dview(void);
#endif

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

/* The 1996 engine's sine table was an array. */
#define sintable (sintable())

/* pragmas.h */
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
static inline int32_t klabs(int32_t a) { return a < 0 ? -a : a; }
static inline int32_t ksgn(int32_t a) { return (a > 0) - (a < 0); }
static inline int32_t scale(int32_t a, int32_t b, int32_t c) { return (int32_t)((int64_t)a * b / c); }
#define MULSCALE(n) static inline int32_t mulscale##n(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> n); }
#define DIVSCALE(n) static inline int32_t divscale##n(int32_t a, int32_t b) { return (int32_t)(((int64_t)a << n) / b); }
#define DMULSCALE(n) static inline int32_t dmulscale##n(int32_t a, int32_t b, int32_t c, int32_t d) \
    { return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> n); }
MULSCALE(4) MULSCALE(10) MULSCALE(11) MULSCALE(14) MULSCALE(30)
DIVSCALE(14) DIVSCALE(30)
DMULSCALE(4) DMULSCALE(32)
static inline void clearbuf(void *d, int32_t dwords, int32_t v)
{
    int32_t *p = (int32_t *)d;

    while (dwords-- > 0)
        *p++ = v;
}
static inline void clearbufbyte(void *d, int32_t bytes, int32_t v) { memset(d, v, (size_t)bytes); }

/* The frame build.c's tile screen writes into, and its row stride. */
#define frameplace ((intptr_t)framebuffer())
#define bytesperline (ydim16 ? 640 : framebuffer_pitch())

/* Every tile is loaded with its ART file here. */
#define loadtile(tilenum) ((void)0)
#define printext256_noupdate printext256

/* build.c functions used before they are defined (build.h had these) */
int drawtilescreen(int32_t pictopleft, int32_t picbox);

/* edglue.c: the 1996 platform calls, on this engine's SDL layer. */
void _idle(void);
void initkeys(void);
void uninitkeys(void);
void inittimer(void);
void uninittimer(void);
void getmousearea(int *w, int *h);
void editor_getmousevalues(short *mousx, short *mousy, short *bstatus);
#define getmousevalues editor_getmousevalues
int32_t editormousemove(int delta, int32_t dim, int window, int32_t *rem);
void drawline256(int32_t x1, int32_t y1, int32_t x2, int32_t y2, unsigned char col);
void clearview(int32_t col);
void fillscreen16(int32_t offset, int32_t color, int32_t blocksize);
void editor_quit(void);
/* nextpage, then the dev aids */
void editor_nextpage(void);
#define nextpage editor_nextpage
void timerhandler(void);

#endif
