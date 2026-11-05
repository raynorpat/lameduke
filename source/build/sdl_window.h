#ifndef SDL_WINDOW_H
#define SDL_WINDOW_H

int platform_video(int width, int height);
const char *platform_video_error(void);
void platform_present(const void *pixels, int width, int height);
void platform_shutdown(void);
void platform_title(const char *text);
const char *platform_title_text(void);
/* Renderer 3: the frame about to be shown, as RGB rows top first in a
   malloc'd buffer of *w by *h; NULL when this frame is the 8-bit one. */
unsigned char *platform_capture(int *w, int *h);

/* The size renderer 3 draws at (duke3d's): the GL drawable's, or larger
   when supersampled. */
extern int xres, yres;

/* What the GL driver has (duke3d's build.h). */
struct glinfo_t {
    float maxanisotropy;
    char bgra, clamptoedge, texcompr, texnpot, vbo, glsl, fbo;
};
extern struct glinfo_t glinfo;

/* Renderer 3: the engine drew to the back buffer since the last swap. */
void _glframedrawn(void);

#endif
