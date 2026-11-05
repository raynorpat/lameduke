#include "sdl_window.h"
#include "display.h"
#include "draw.h"
#include "engine.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_OPENGL
#include "sdl_opengl_internal.h"
#include "polymost_opengl_submit.h"
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static unsigned char *rgb;
static int rgb_bytes;
static int tex_w;
static int tex_h;
static const char *video_error;
static char title[96] = "Duke Nukem 3D";
static int displaymode; /* 0 windowed, 1 fullscreen, 2 borderless */
static int vsync = 1;

int xres, yres;
struct glinfo_t glinfo;

#ifdef USE_OPENGL
char nofog;
/* polymost.c's, compiled in draw.c */
void polymost_glreset(void);
int polymost_present_shader(const int *r, int dw, int dh, int dark);
void polymost_prepare_shader(void);
int polymost_supersample_failed(void);
int polymost_supersample_ready(void);
int polymost_filter_ready(int filter);
unsigned char *polymost_capture_shader(void);
void gltexapplyprops(void);
void gltexinvalidateall(void);
void setpolymost2dview(void);
extern int gltexfiltermode, gltexmiplevel, glshsupersampled;

/* Made at the first setgamemode_gl and kept, with its window, until
   shutdown (duke3d's rule); the 8-bit modes are shown through it from then. */
static SDL_GLContext glcontext;
static GLuint gl8tex;           /* an 8-bit frame, as shown through it */
static int gl8tex_w, gl8tex_h;  /* its texture's size */
static int framedrawn;          /* renderer 3 drew since the last swap */
static GLuint lastframe;        /* the last frame swapped, for glredrawlastframe */
static int lastframe_set, lastframe_w, lastframe_h;
static int gl_draw_w, gl_draw_h; /* the drawable */
static int gl_showrect[4];      /* where xres by yres shows in it */
static int gl_ss;               /* supersampled: xres by yres is larger */
#endif

static Uint32 display_flags(void)
{
    return displaymode == 1 ? SDL_WINDOW_FULLSCREEN : displaymode == 2 ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
}

static void destroy_texture(void)
{
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    free(rgb);
    rgb = NULL;
    rgb_bytes = 0;
    tex_w = 0;
    tex_h = 0;
}

/* The 8-bit frame through the palette, into rgb. */
static void expand(const unsigned char *src, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        int c = src[i] * 3;
        rgb[i * 3] = palette[c];
        rgb[i * 3 + 1] = palette[c + 1];
        rgb[i * 3 + 2] = palette[c + 2];
    }
}

#ifdef USE_OPENGL
/* The texture an 8-bit width by height frame is shown from (rounded up to
   powers of two without GL_ARB_texture_non_power_of_two). */
static int gl_alloc8(int width, int height)
{
    if (gl8tex && tex_w == width && tex_h == height)
        return 0;
    destroy_texture();
    if (gl8tex)
        glDeleteTextures(1, &gl8tex);
    gl8tex_w = width;
    gl8tex_h = height;
    if (!glinfo.texnpot) {
        for (gl8tex_w = 1; gl8tex_w < width; gl8tex_w <<= 1)
            ;
        for (gl8tex_h = 1; gl8tex_h < height; gl8tex_h <<= 1)
            ;
    }
    glGenTextures(1, &gl8tex);
    glBindTexture(GL_TEXTURE_2D, gl8tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, gl8tex_w, gl8tex_h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glsub_sync();
    rgb_bytes = width * height * 3;
    rgb = (unsigned char *)malloc((size_t)rgb_bytes);
    if (!rgb) {
        video_error = "out of memory";
        return -1;
    }
    tex_w = width;
    tex_h = height;
    return 0;
}

/* An 8-bit frame through the GL context, as the SDL renderer shows it: the
   palette applied, nearest pixel, 4:3 with black bars. Renderer 3's state is
   kept for its next frame. */
static void gl_present8(const unsigned char *src, int swap)
{
    int r[4];
    float u = (float)tex_w / (float)gl8tex_w, v = (float)tex_h / (float)gl8tex_h;

    framedrawn = 0;
    expand(src, tex_w * tex_h);
    SDL_GL_GetDrawableSize(window, &gl_draw_w, &gl_draw_h);
    gl_fitrect(getrendermode() == 2 && (!ydim16 || !editstatus) ? tex_w : 4,
               getrendermode() == 2 && (!ydim16 || !editstatus) ? tex_h : 3, gl_draw_w, gl_draw_h, r);
    glsub_flush();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    if (bglBindFramebuffer)
        bglBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (bglUseProgram)
        bglUseProgram(0);
    if (bglActiveTexture)
        bglActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glViewport(0, 0, gl_draw_w, gl_draw_h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(r[0], r[1], r[2], r[3]);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl8tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS); /* the frame's top row at the top */
    glTexCoord2f(0, v);
    glVertex2f(-1, -1);
    glTexCoord2f(u, v);
    glVertex2f(1, -1);
    glTexCoord2f(u, 0);
    glVertex2f(1, 1);
    glTexCoord2f(0, 0);
    glVertex2f(-1, 1);
    glEnd();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glPopAttrib();
    glsub_sync();
    if (swap)
        SDL_GL_SwapWindow(window);
}

/* The largest frame the shader path can draw, and at most GLSHDOWNMAXSCALE
   texels to a pixel for the supersampled present. */
static int gl_ssmaxfb(void)
{
    GLint t = 0, vp[2] = { 0, 0 };
    int m = gl_draw_w > gl_draw_h ? gl_draw_w : gl_draw_h;

    m *= GLSHDOWNMAXSCALE;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &t);
    if (t > 0 && t < m)
        m = t;
    t = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &t);
    if (t > 0 && t < m)
        m = t;
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, vp);
    if (vp[0] > 0 && vp[0] < m)
        m = vp[0];
    if (vp[1] > 0 && vp[1] < m)
        m = vp[1];
    return m;
}

/* duke3d's videoopts_sssize: level's height (1440, 2160, 4320) at the
   drawable's aspect, clamped to the largest frame. */
static void gl_sssize(int level, int *w, int *h)
{
    static const int heights[4] = { 0, 1440, 2160, 4320 };
    long long sw, sh = heights[level], maxfb = gl_ssmaxfb();

    sw = ((long long)gl_draw_w * sh + gl_draw_h / 2) / gl_draw_h;
    if (sw > maxfb || sh > maxfb) {
        if (sw >= sh) {
            sh = (sh * maxfb + sw / 2) / sw;
            sw = maxfb;
        } else {
            sw = (sw * maxfb + sh / 2) / sh;
            sh = maxfb;
        }
    }
    *w = (int)sw;
    *h = (int)sh;
}

static void gl_alloc_lastframe(void)
{
    GLint maxsize = 0;

    if (lastframe)
        glDeleteTextures(1, &lastframe);
    lastframe = 0;
    lastframe_set = 0;
    if (gl_ss) /* the shader path keeps its own frame */
        return;
    lastframe_w = xres;
    lastframe_h = yres;
    if (!glinfo.texnpot) {
        for (lastframe_w = 1; lastframe_w < xres; lastframe_w <<= 1)
            ;
        for (lastframe_h = 1; lastframe_h < yres; lastframe_h <<= 1)
            ;
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize);
    if (lastframe_w > maxsize || lastframe_h > maxsize)
        return;
    glGenTextures(1, &lastframe);
    glBindTexture(GL_TEXTURE_2D, lastframe);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lastframe_w, lastframe_h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glsub_sync();
}

/* xres by yres: the drawable's size, or supersampling's on the shader path. */
static void gl_resvars(void)
{
    SDL_GL_GetDrawableSize(window, &gl_draw_w, &gl_draw_h);
    xres = gl_draw_w;
    yres = gl_draw_h;
    gl_ss = 0;
    if (glsupersample > 0 && gldrawpath == GLPATH_SHADER && polymost_supersample_ready()) {
        int w, h;

        gl_sssize(glsupersample, &w, &h);
        if (h > gl_draw_h) {
            gl_ss = 1;
            xres = w;
            yres = h;
        }
    }
    glshsupersampled = gl_ss;
    gl_fitrect(xres, yres, gl_draw_w, gl_draw_h, gl_showrect);
    gl_alloc_lastframe();
}

/* After gl_resvars at a supersampled size: makes the shader path's frame now,
   and without supersampling if one that big can't be made. */
static void gl_prepare(void)
{
    if (!gl_ss)
        return;
    polymost_prepare_shader();
    if (polymost_supersample_failed()) {
        glsupersample = 0;
        gl_resvars();
    }
}

/* Shows renderer 3's frame: the shader path resolves its own, the others
   keep it for glredrawlastframe. */
static void gl_swap(void)
{
    if (!polymost_present_shader(gl_showrect, gl_draw_w, gl_draw_h, 0)) {
        glsub_flush();
        if (lastframe) {
            glBindTexture(GL_TEXTURE_2D, lastframe);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, xres, yres);
            glsub_sync();
            lastframe_set = 1;
        }
    }
    SDL_GL_SwapWindow(window);
    framedrawn = 0;
}

void _glframedrawn(void)
{
    framedrawn = 1;
}
#endif

/* Every mode was shown 4:3: full screen keeps that with black bars. */
static void fit_renderer(void)
{
    if (renderer && tex_w)
        SDL_RenderSetLogicalSize(renderer, displaymode ? tex_w : 0,
            displaymode ? (getrendermode() == 2 && (!ydim16 || !editstatus) ? tex_h : tex_w * 3 / 4) : 0);
}

/* Applies displaymode to the window. Fullscreen takes the display mode
   nearest the window's size. */
static void apply_displaymode(void)
{
    SDL_DisplayMode want, got;

    if (!window)
        return;
    if (displaymode == 1) {
        memset(&want, 0, sizeof want);
        SDL_GetWindowSize(window, &want.w, &want.h);
        if (SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(window), &want, &got))
            SDL_SetWindowDisplayMode(window, &got);
    }
    SDL_SetWindowFullscreen(window, 0);
    SDL_SetWindowFullscreen(window, display_flags());
    fit_renderer();
#ifdef USE_OPENGL
    if (glcontext) {
        gl_resvars();
        gl_prepare();
    }
#endif
}

int platform_video(int width, int height)
{
    int window_w;
    int window_h;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        video_error = SDL_GetError();
        return -1;
    }
#ifdef USE_OPENGL
    /* The GL window keeps its size; the 8-bit frame is scaled into it. */
    if (glcontext) {
        if (getrendermode() == 2 && (!ydim16 || !editstatus)) {
            SDL_SetWindowSize(window, width, height);
            gl_resvars();
        }
        video_error = NULL;
        return gl_alloc8(width, height);
    }
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window_w = getrendermode() == 2 ? width : width * 3;
    window_h = getrendermode() == 2 ? height : height * 3;
    /* 320x200 was stretched to 4:3 on a VGA monitor. */
    if (getrendermode() < 2 && width == 320 && height == 200)
        window_h = 720;
    if (window)
        SDL_SetWindowSize(window, window_w, window_h);
    else {
        window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_w, window_h, SDL_WINDOW_SHOWN | display_flags());
        if (!window) {
            video_error = SDL_GetError();
            return -1;
        }
    }
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!renderer)
            renderer = SDL_CreateRenderer(window, -1, 0);
        if (!renderer) {
            video_error = SDL_GetError();
            return -1;
        }
    }
    if (tex_w != width || tex_h != height) {
        destroy_texture();
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            video_error = SDL_GetError();
            return -1;
        }
        rgb_bytes = width * height * 3;
        rgb = (unsigned char *)malloc((size_t)rgb_bytes);
        if (!rgb) {
            video_error = "out of memory";
            return -1;
        }
        tex_w = width;
        tex_h = height;
        fit_renderer();
    }
    video_error = NULL;
    return 0;
}

const char *platform_video_error(void)
{
    return video_error ? video_error : "video init failed";
}

void platform_present(const void *pixels, int width, int height)
{
    if (getrendermode() == 2 && (!ydim16 || !editstatus)) {
        pixels = framebuffer();
        width = renderwidth;
        height = renderheight;
        if (tex_w != width || tex_h != height)
            platform_video(width, height);
    }
#ifdef USE_OPENGL
    if (glcontext && getrendermode() == 3 && framedrawn) {
        gl_swap();
        return;
    }
#endif
    if (!pixels || !rgb || width != tex_w || height != tex_h)
        return;
#ifdef USE_OPENGL
    if (glcontext) {
        gl_present8((const unsigned char *)pixels, 1);
        return;
    }
#endif
    if (!renderer || !texture)
        return;
    expand((const unsigned char *)pixels, width * height);
    SDL_UpdateTexture(texture, NULL, rgb, width * 3);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

unsigned char *platform_capture(int *w, int *h)
{
#ifdef USE_OPENGL
    unsigned char *buf;
    int y, row;

    if (!glcontext) {
        if (getrendermode() == 2 && (!ydim16 || !editstatus)) {
            *w = renderwidth; *h = renderheight;
            buf = (unsigned char *)malloc((size_t)*w * *h * 3);
            if (!buf) return NULL;
            for (y = 0; y < *w * *h; y++)
                memcpy(buf + y*3, palette + framebuffer()[y]*3, 3);
            return buf;
        }
        return NULL;
    }
    if (getrendermode() == 3 && framedrawn) {
        /* Resolve exactly as presentation does, including supersampling and
           colour correction, then read the window-sized back buffer. */
        polymost_present_shader(gl_showrect, gl_draw_w, gl_draw_h, 0);
        SDL_GL_GetDrawableSize(window, w, h);
    } else {
        /* Exercise the same presentation path as the window, including after
           a mid-run switch. Reading framebuffer() would hide a blank window. */
        if (getrendermode() == 2 && (!ydim16 || !editstatus) && (tex_w != renderwidth || tex_h != renderheight))
            platform_video(renderwidth, renderheight);
        if (getrendermode() < 2 && (tex_w != xdim || tex_h != ydim))
            platform_video(xdim, ydim);
        if (rgb)
            gl_present8(framebuffer(), 0);
        SDL_GL_GetDrawableSize(window, w, h);
    }
    row = *w * 3;
    buf = (unsigned char *)malloc((size_t)row * (size_t)(*h + 1));
    if (!buf)
        return NULL;
    glsub_flush();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, *w, *h, GL_RGB, GL_UNSIGNED_BYTE, buf);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    /* GL's rows are bottom-up; the spare row swaps them */
    for (y = 0; y < *h / 2; y++) {
        memcpy(buf + (size_t)row * *h, buf + (size_t)row * y, (size_t)row);
        memcpy(buf + (size_t)row * y, buf + (size_t)row * (*h - 1 - y), (size_t)row);
        memcpy(buf + (size_t)row * (*h - 1 - y), buf + (size_t)row * *h, (size_t)row);
    }
    return buf;
#else
    (void)w;
    (void)h;
    return NULL;
#endif
}

int setgamemode_gl(int width, int height)
{
#ifdef USE_OPENGL
    const char *exts;
    int w8 = tex_w, h8 = tex_h; /* the 8-bit frame's size, shown through GL from now */

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        video_error = SDL_GetError();
        return -1;
    }
    /* test hook: the context fails, as on a machine without GL */
    if (getenv("LAMEDUKE_GL_FORCEFAIL"))
        gl_forcefail = 4;
    if (glcontext) {
        SDL_SetWindowSize(window, width, height);
    } else {
        SDL_Window *win;
        SDL_GLContext ctx;

        /* The GL window is made before the 8-bit one goes, so a failure leaves
           that as it was. SDL2 can't add GL to a window with an SDL_Renderer. */
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN | display_flags());
        ctx = win && gl_forcefail != 4 ? SDL_GL_CreateContext(win) : NULL;
        if (!ctx) {
            video_error = gl_forcefail == 4 ? "no context (LAMEDUKE_GL_FORCEFAIL)" : SDL_GetError();
            if (win)
                SDL_DestroyWindow(win);
            return -1;
        }
        destroy_texture();
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
        }
        if (window)
            SDL_DestroyWindow(window);
        window = win;
        glcontext = ctx;
    }
    /* destroying an SDL_Renderer can leave no context current */
    SDL_GL_MakeCurrent(window, glcontext);
    SDL_GL_SetSwapInterval(vsync);

    /* the state JFBuild sets after every GL mode change */
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearColor(0, 0, 0, 0.5);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);
    glDisable(GL_DITHER);

    exts = (const char *)glGetString(GL_EXTENSIONS);
    if (!exts)
        exts = "";
    glinfo.maxanisotropy = 1.0;
    if (gl_has_extension(exts, "GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glinfo.maxanisotropy);
    glinfo.bgra = gl_has_extension(exts, "GL_EXT_bgra");
    glinfo.clamptoedge = gl_has_extension(exts, "GL_EXT_texture_edge_clamp")
        || gl_has_extension(exts, "GL_SGIS_texture_edge_clamp");
    glinfo.texcompr = gl_has_extension(exts, "GL_ARB_texture_compression");
    glinfo.texnpot = gl_has_extension(exts, "GL_ARB_texture_non_power_of_two");
    if (w8 && gl_alloc8(w8, h8) != 0)
        return -1;
    gl_initdrawpath(exts);
    glsub_init();
    gl_resvars();
    polymost_glreset();
    gl_prepare();
    video_error = NULL;
    return 0;
#else
    (void)width;
    (void)height;
    video_error = "this build has no OpenGL";
    return -1;
#endif
}

void glredrawlastframe(void)
{
#ifdef USE_OPENGL
    float u, v;

    if (!glcontext || getrendermode() != 3 || !lastframe_set)
        return;
    u = (float)xres / (float)lastframe_w;
    v = (float)yres / (float)lastframe_h;
    setpolymost2dview();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, lastframe);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, v);
    glVertex2i(0, 0);
    glTexCoord2f(u, v);
    glVertex2i(xres, 0);
    glTexCoord2f(u, 0);
    glVertex2i(xres, yres);
    glTexCoord2f(0, 0);
    glVertex2i(0, yres);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glsub_sync();
#endif
}

void setvideodisplaymode(int mode)
{
    displaymode = mode < 0 || mode > 2 ? 0 : mode;
    apply_displaymode();
}

int getvideodisplaymode(void)
{
    return displaymode;
}

int getvideomodelist(int *w, int *h, int max, int *desktopw, int *desktoph)
{
    SDL_DisplayMode mode;
    int i, n = 0, count;

    *desktopw = *desktoph = 0;
    if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
        *desktopw = mode.w;
        *desktoph = mode.h;
    }
    /* SDL lists a size once per refresh rate, highest first: one entry each */
    count = SDL_GetNumDisplayModes(0);
    for (i = 0; i < count && n < max; i++) {
        int j;

        if (SDL_GetDisplayMode(0, i, &mode) != 0)
            continue;
        for (j = 0; j < n && (w[j] != mode.w || h[j] != mode.h); j++)
            ;
        if (j == n) {
            w[n] = mode.w;
            h[n] = mode.h;
            n++;
        }
    }
    return n;
}

void setvideovsync(int on)
{
    vsync = on != 0;
#ifdef USE_OPENGL
    if (glcontext)
        SDL_GL_SetSwapInterval(vsync);
#endif
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (renderer)
        SDL_RenderSetVSync(renderer, vsync);
#endif
}

int setvideotexfilter(int filter)
{
#ifdef USE_OPENGL
    /* polymost's glfiltermodes: GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR */
    static const int modes[3] = { 0, 1, 5 };

    if (filter < 0 || filter > 2)
        filter = 0;
    gltexturefilter = filter;
    gltexfiltermode = modes[filter];
    if (!glcontext)
        return filter == 0;
    if (gldrawpath != GLPATH_SHADER)
        gltexapplyprops();
    return filter == 0 || gldrawpath != GLPATH_SHADER || polymost_filter_ready(filter);
#else
    return filter <= 0;
#endif
}

int setvideosupersample(int level)
{
#ifdef USE_OPENGL
    glsupersample = level < 0 || level > 3 ? 0 : level;
    if (glcontext) {
        gl_resvars();
        gl_prepare();
    }
    return gl_ss;
#else
    (void)level;
    return 0;
#endif
}

int videosupersampleavailable(int level)
{
#ifdef USE_OPENGL
    int w, h;

    if (level <= 0)
        return 1;
    if (level > 3 || !glcontext || gldrawpath != GLPATH_SHADER
        || !polymost_supersample_ready())
        return 0;
    gl_sssize(level, &w, &h);
    return h > gl_draw_h;
#else
    return level <= 0;
#endif
}

void setvideomiplevel(int level)
{
#ifdef USE_OPENGL
    gltexmiplevel = level < 0 ? 0 : level > 15 ? 15 : level;
    if (glcontext)
        gltexinvalidateall();
#else
    (void)level;
#endif
}

void setvideocolor(double contrast, double saturation)
{
#ifdef USE_OPENGL
    glcontrast = contrast < GL_CONTRAST_MIN ? GL_CONTRAST_MIN : contrast > GL_CONTRAST_MAX ? GL_CONTRAST_MAX : contrast;
    glsaturation = saturation < GL_SATURATION_MIN ? GL_SATURATION_MIN
        : saturation > GL_SATURATION_MAX ? GL_SATURATION_MAX : saturation;
#else
    (void)contrast;
    (void)saturation;
#endif
}

int getvideoglpath(void)
{
#ifdef USE_OPENGL
    return glcontext ? gldrawpath : 0;
#else
    return 0;
#endif
}

void platform_shutdown(void)
{
    destroy_texture();
#ifdef USE_OPENGL
    if (glcontext) {
        glFinish();
        SDL_GL_MakeCurrent(window, NULL);
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }
#endif
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* The window's title bar; kept for the window if it is not made yet. */
void platform_title(const char *text)
{
    snprintf(title, sizeof title, "%s", text);
    if (window)
        SDL_SetWindowTitle(window, title);
}

const char *platform_title_text(void)
{
    return window ? SDL_GetWindowTitle(window) : title;
}
