#ifndef SDL_OPENGL_H
#define SDL_OPENGL_H

/* The 1994 renderer writes a palettized framebuffer. This is where that
   image is uploaded. It is not Polymost. */

void gl_present(const void *pixels, int width, int height);

#endif
