/*
 * Renderer 3's vertex submission: glBegin/glEnd-style calls that draw in
 *  immediate mode, or batch into vertex arrays or a vertex buffer and draw
 *  when glsub_flush is called. Anything that changes GL state, reads the
 *  framebuffer or swaps must draw the batch first, so batches draw in order
 *  with the state they were made under: a file that defines GLSUB_WRAP
 *  before including this (after SDL_opengl.h) gets its GL state calls
 *  wrapped to do that.
 */
#ifndef POLYMOST_OPENGL_SUBMIT_H
#define POLYMOST_OPENGL_SUBMIT_H

#ifdef USE_OPENGL
#include <stdint.h>
#include "SDL_opengl.h"

enum { GLSUB_IMMEDIATE, GLSUB_ARRAYS, GLSUB_VBO };

	/* How vertices are sent (set by glsub_init from gldrawpath). */
extern int32_t glsubmode;
	/* Test hook: GLSUB_IMMEDIATE or GLSUB_ARRAYS makes the immediate path
	 * send vertices that way (-1: its own choice). */
extern int32_t glsub_force;
	/* glDrawArrays calls made, for tests. */
extern uint32_t glsub_arraydraws;
	/* Test hook: nonzero draws each glsub_begin..glsub_end at its end, so a
	 * frame comes out the same unless a state change misses a flush. */
extern int32_t glsub_flusheach;

	/* With a new context current, after gl_initdrawpath. */
void glsub_init(void);
	/* Before the context goes: drops the vertex buffer. */
void glsub_uninit(void);

	/* prim: GL_TRIANGLE_FAN, GL_LINES or GL_POINTS. The texcoord and colour
	 * stay set for later vertices, as glTexCoord and glColor do. */
void glsub_begin(int prim);
void glsub_texcoord(float u, float v);
void glsub_color(float r, float g, float b, float a);
void glsub_vertex(float x, float y, float z);
void glsub_end(void);
	/* Draws what's batched. */
void glsub_flush(void);

	/* glBindTexture, glEnable and glDisable that, when batching, leave the
	 * batch open if the state doesn't change. */
void glsub_bindtexture(GLenum target, GLuint tex);
void glsub_enable(GLenum cap, int on);
	/* Draws the batch and forgets the state glsub_bindtexture and
	 * glsub_enable know, as when textures are deleted. */
void glsub_sync(void);

#ifdef GLSUB_WRAP
#define glBindTexture(a,b) glsub_bindtexture(a,b)
#define glEnable(a) glsub_enable(a,1)
#define glDisable(a) glsub_enable(a,0)
#define glDeleteTextures(a,b) (glsub_sync(),glDeleteTextures(a,b))
#define glBegin(a) (glsub_flush(),glBegin(a))
#define glAlphaFunc(a,b) (glsub_flush(),glAlphaFunc(a,b))
#define glBlendFunc(a,b) (glsub_flush(),glBlendFunc(a,b))
#define glClear(a) (glsub_flush(),glClear(a))
#define glColorMask(a,b,c,d) (glsub_flush(),glColorMask(a,b,c,d))
#define glCopyTexSubImage2D(a,b,c,d,e,f,g,h) (glsub_flush(),glCopyTexSubImage2D(a,b,c,d,e,f,g,h))
#define glDepthFunc(a) (glsub_flush(),glDepthFunc(a))
#define glDepthMask(a) (glsub_flush(),glDepthMask(a))
#define glDepthRange(a,b) (glsub_flush(),glDepthRange(a,b))
#define glFogf(a,b) (glsub_flush(),glFogf(a,b))
#define glFogfv(a,b) (glsub_flush(),glFogfv(a,b))
#define glFogi(a,b) (glsub_flush(),glFogi(a,b))
#define glLoadIdentity() (glsub_flush(),glLoadIdentity())
#define glLoadMatrixf(a) (glsub_flush(),glLoadMatrixf(a))
#define glMatrixMode(a) (glsub_flush(),glMatrixMode(a))
#define glOrtho(a,b,c,d,e,f) (glsub_flush(),glOrtho(a,b,c,d,e,f))
#define glPopMatrix() (glsub_flush(),glPopMatrix())
#define glPushMatrix() (glsub_flush(),glPushMatrix())
#define glReadPixels(a,b,c,d,e,f,g) (glsub_flush(),glReadPixels(a,b,c,d,e,f,g))
#define glScissor(a,b,c,d) (glsub_flush(),glScissor(a,b,c,d))
#define glTexImage2D(a,b,c,d,e,f,g,h,i) (glsub_flush(),glTexImage2D(a,b,c,d,e,f,g,h,i))
#define glTexParameterf(a,b,c) (glsub_flush(),glTexParameterf(a,b,c))
#define glTexParameteri(a,b,c) (glsub_flush(),glTexParameteri(a,b,c))
#define glTexSubImage2D(a,b,c,d,e,f,g,h,i) (glsub_flush(),glTexSubImage2D(a,b,c,d,e,f,g,h,i))
#define glViewport(a,b,c,d) (glsub_flush(),glViewport(a,b,c,d))
#endif
#endif

#endif
