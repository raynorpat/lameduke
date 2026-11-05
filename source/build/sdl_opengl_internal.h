/*
 * Renderer 3's draw paths and the OpenGL entry points past 1.1, loaded at
 *  run time so the binary still runs on a 1.1 driver. The path names and
 *  gl_choosepath need no OpenGL: the game's and editor's settings use them.
 */
#ifndef SDL_OPENGL_INTERNAL_H
#define SDL_OPENGL_INTERNAL_H

#include <stdint.h>

	/* GL 1.1 immediate mode, GL 1.5 vertex buffers, GL 2.1 GLSL and FBOs. */
enum { GLPATH_AUTO, GLPATH_IMMEDIATE, GLPATH_VBO, GLPATH_SHADER, GLPATH_COUNT };

	/* The setting's name of a path: "auto", "immediate", "vbo" or "shader". */
static inline const char *gl_pathname(int path)
{
	static const char *const names[GLPATH_COUNT] = { "auto", "immediate", "vbo", "shader" };
	return names[(path > 0 && path < GLPATH_COUNT) ? path : GLPATH_AUTO];
}

	/* A path from its name in any case; anything else is GLPATH_AUTO. */
static inline int gl_pathfromname(const char *s)
{
	int path, i;

	for(path=GLPATH_COUNT-1;path>GLPATH_AUTO;path--)
	{
		const char *n = gl_pathname(path);
		for(i=0;n[i] && (s[i]|0x20) == n[i];i++);
		if (!n[i] && !s[i]) return path;
	}
	return GLPATH_AUTO;
}

	/* The path to draw with: the best the driver has, no better than
	 * requested (GLPATH_AUTO: the best). Shader needs GL 2.1 with GLSL, FBOs
	 * and vertex buffers; vbo needs vertex buffers. */
int gl_choosepath(int requested, int glmajor, int glminor, int hasvbo, int hasglsl, int hasfbo);

	/* The path asked for (set before setgamemode_gl) and the one in use. */
extern int32_t gldrawpathrequest, gldrawpath;
	/* The size the shader path draws at (0: the window's), scaled up to the
	 * window; the other paths draw at the window's. */
extern int32_t glinternalwidth, glinternalheight;
	/* The size drawn at for the settings w by h: 0x0 (the window's) when
	 * either is 0, otherwise at least 320x200. 0: as set, 1: one of them was
	 * 0, 2: made bigger. */
int gl_internalsize(int32_t w, int32_t h, int32_t *ow, int32_t *oh);
	/* The shader path's supersampling level (videoopts.h SS_*, 0: off): it
	 * draws videoopts_sssize's size (videoopts_ssheight high at the
	 * drawable's aspect, clamped to the largest frame buffer) and shows it
	 * box-filtered down to the window. Not with an internal size, or when
	 * that size isn't larger than the window (then the window's size). */
extern int32_t glsupersample;
	/* Renderer 3's texture filter (videoopts.h TEXFILTER_*: nearest,
	 * bilinear, trilinear). The 1.1 and 1.5 paths filter their textures so;
	 * the shader path draws colour instead of palette indices when it isn't
	 * nearest. Set it with setvideotexfilter (display.h). */
extern int32_t gltexturefilter;
	/* Test hooks: the largest frame buffer, if less than GL's (0: GL's); and
	 * the times supersampling was clamped to it, with a message. */
extern int32_t gl_maxfbtest;
extern uint32_t gl_ssclamps;
	/* The supersampled present draws at most this many texels to a pixel
	 * per axis (polymost.c's GLSHDOWNTAPS, less one for a part texel at
	 * each end): larger sizes are clamped to it. */
#define GLSHDOWNMAXSCALE 14
	/* The shader path's contrast and saturation, applied to the palette it
	 * shows after the fade (EDuke32's: 1.0 each leaves it as it is). */
#define GL_CONTRAST_MIN 0.5
#define GL_CONTRAST_MAX 1.5
#define GL_SATURATION_MIN 0.0
#define GL_SATURATION_MAX 2.0
extern double glcontrast, glsaturation;
	/* Applies them to the colour c (r, g, b): saturation around its Rec.709
	 * luma, then contrast around half, clamped. 1, 1: c untouched. */
void gl_colorcorrect(unsigned char *c, double contrast, double saturation);
	/* Where a w by h frame shows in a dw by dh drawable: scaled by the
	 * largest factor that fits, pixels square, centred. r: x, y (the bars'
	 * width and height), width, height, in whole pixels. */
void gl_fitrect(int32_t w, int32_t h, int32_t dw, int32_t dh, int32_t *r);
	/* Test hook: 1 makes GLSL and FBOs unavailable, as if they failed; 2 makes
	 * the shader path's shader not compile, 3 its framebuffer incomplete; 4
	 * makes the run's first GL context fail, as on a machine without GL; 5
	 * makes supersampling's shader not compile; 6 texture filtering's (but
	 * its supersampled present's), 7 trilinear's alone, 8 filtering's
	 * supersampled present alone; 9 makes the framebuffer incomplete only
	 * at a supersampled size. */
extern int32_t gl_forcefail;
	/* Test hooks (polymost.c): RGB tile textures made, and times they were all
	 * made dirty (gltexinvalidateall); the shader path makes none. */
extern uint32_t gl_rgbtexloads, gl_rgbtexinvalidations;
	/* Test hooks (polymost.c): copies of the shader path's frame made for
	 * translucent draws, and nonzero to copy all of it before every one. */
extern uint32_t gl_transcopies;
extern int32_t gl_transcopyall;

#ifdef USE_OPENGL
#include "SDL_opengl.h"

	/* Loads the entry points below with the GL context current (NULL where
	 * the driver lacks them) and sets glinfo's vbo, glsl and fbo, then sets
	 * gldrawpath from gldrawpathrequest and prints it. exts is GL_EXTENSIONS. */
void gl_initdrawpath(const char *exts);

	/* Whole-word match of name in a GL_EXTENSIONS string. */
int gl_has_extension(const char *exts, const char *name);

	/* GL 1.5 or GL_ARB_vertex_buffer_object */
extern PFNGLGENBUFFERSPROC bglGenBuffers;
extern PFNGLDELETEBUFFERSPROC bglDeleteBuffers;
extern PFNGLBINDBUFFERPROC bglBindBuffer;
extern PFNGLBUFFERDATAPROC bglBufferData;
extern PFNGLBUFFERSUBDATAPROC bglBufferSubData;

	/* GL 1.3 or GL_ARB_multitexture */
extern PFNGLACTIVETEXTUREPROC bglActiveTexture;

	/* GL 2.0 */
extern PFNGLCREATESHADERPROC bglCreateShader;
extern PFNGLSHADERSOURCEPROC bglShaderSource;
extern PFNGLCOMPILESHADERPROC bglCompileShader;
extern PFNGLGETSHADERIVPROC bglGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC bglGetShaderInfoLog;
extern PFNGLDELETESHADERPROC bglDeleteShader;
extern PFNGLCREATEPROGRAMPROC bglCreateProgram;
extern PFNGLATTACHSHADERPROC bglAttachShader;
extern PFNGLBINDATTRIBLOCATIONPROC bglBindAttribLocation;
extern PFNGLLINKPROGRAMPROC bglLinkProgram;
extern PFNGLGETPROGRAMIVPROC bglGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC bglGetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC bglDeleteProgram;
extern PFNGLUSEPROGRAMPROC bglUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC bglGetUniformLocation;
extern PFNGLUNIFORM1IPROC bglUniform1i;
extern PFNGLUNIFORM1FPROC bglUniform1f;
extern PFNGLUNIFORM2FPROC bglUniform2f;
extern PFNGLUNIFORM4FPROC bglUniform4f;
extern PFNGLVERTEXATTRIBPOINTERPROC bglVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC bglEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC bglDisableVertexAttribArray;

	/* GL_ARB_framebuffer_object or GL_EXT_framebuffer_object */
extern PFNGLGENFRAMEBUFFERSPROC bglGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC bglDeleteFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC bglBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC bglFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC bglCheckFramebufferStatus;
extern PFNGLGENRENDERBUFFERSPROC bglGenRenderbuffers;
extern PFNGLDELETERENDERBUFFERSPROC bglDeleteRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC bglBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC bglRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC bglFramebufferRenderbuffer;
#endif

#endif
