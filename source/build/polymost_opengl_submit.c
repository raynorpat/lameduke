/*
 * Renderer 3's vertex submission. See polymost_opengl_submit.h.
 */
#ifdef USE_OPENGL
#include <stddef.h>
#include <stdint.h>
#include "build.h"
#include "sdl_opengl_internal.h"
#include "polymost_opengl_submit.h"

int32_t glsubmode = GLSUB_IMMEDIATE;
int32_t glsub_force = -1;
uint32_t glsub_arraydraws = 0;
int32_t glsub_flusheach = 0;

typedef struct { float x, y, z, u, v, r, g, b, a; } glsubvert_t;

#define MAXBATCH 6144  /* vertices: a multiple of 3 and of 2. */

static glsubvert_t batch[MAXBATCH];
static int batchn = 0;  /* vertices in batch */
static GLenum batchprim = GL_TRIANGLES;  /* GL_TRIANGLES, GL_LINES or GL_POINTS */
static glsubvert_t cur = { 0, 0, 0, 0, 0, 1, 1, 1, 1 };  /* texcoord and colour */
static glsubvert_t fan0, fanlast;  /* a fan's first and last vertices */
static int fann;  /* vertices of the fan so far */
static GLenum prim;  /* of the glsub_begin being drawn */
static GLuint vbo = 0;

	//State glsub_bindtexture and glsub_enable have set (-1: not known).
static const GLenum caps[] = { GL_BLEND, GL_TEXTURE_2D, GL_FOG, GL_ALPHA_TEST, GL_DEPTH_TEST };
#define NUMCAPS ((int)(sizeof(caps)/sizeof(caps[0])))
static int capon[NUMCAPS];
static int64_t boundtex = -1;

void glsub_sync(void)
{
	int i;

	glsub_flush();
	for(i=0;i<NUMCAPS;i++) capon[i] = -1;
	boundtex = -1;
}

void glsub_bindtexture(GLenum target, GLuint tex)
{
	if ((glsubmode != GLSUB_IMMEDIATE) && (boundtex == (int64_t)tex)) return;
	glsub_flush();
	glBindTexture(target, tex);
	boundtex = tex;
}

void glsub_enable(GLenum cap, int on)
{
	int i;

	for(i=0;(i<NUMCAPS) && (caps[i] != cap);i++);
	if ((glsubmode != GLSUB_IMMEDIATE) && (i < NUMCAPS) && (capon[i] == on)) return;
	glsub_flush();
	if (on) glEnable(cap); else glDisable(cap);
	if (i < NUMCAPS) capon[i] = on;
}

void glsub_init(void)
{
	const char *base = (const char *)batch;

	static const glsubvert_t glstart = { 0, 0, 0, 0, 0, 1, 1, 1, 1 };

	batchn = 0;
	cur = glstart; //a new context's texcoord and colour
	vbo = 0;
		/* The shader path streams from client arrays too: on Apple's driver a
		 * buffer refilled per draw is slower than them (see glsub_flush). */
	if ((gldrawpath == GLPATH_VBO) && (glinfo.vbo)) bglGenBuffers(1, &vbo);
	if (vbo != 0) //none: client arrays
	{
		bglBindBuffer(GL_ARRAY_BUFFER, vbo); //left bound: the pointers are offsets into it
		base = NULL;
	}
	if (vbo != 0) glsubmode = GLSUB_VBO;
	else if ((glsub_force == GLSUB_IMMEDIATE) || (glsub_force == GLSUB_ARRAYS)) glsubmode = glsub_force;
	else glsubmode = GLSUB_ARRAYS;
	if (glsubmode != GLSUB_IMMEDIATE)
	{
			//Set once: immediate mode ignores the arrays, and nothing else uses them.
		glVertexPointer(3, GL_FLOAT, sizeof(glsubvert_t), (const void *)((uintptr_t)base + offsetof(glsubvert_t,x)));
		glTexCoordPointer(2, GL_FLOAT, sizeof(glsubvert_t), (const void *)((uintptr_t)base + offsetof(glsubvert_t,u)));
		glColorPointer(4, GL_FLOAT, sizeof(glsubvert_t), (const void *)((uintptr_t)base + offsetof(glsubvert_t,r)));
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
	}
	glsub_sync();
}

void glsub_uninit(void)
{
	glsub_sync();
	if (glsubmode != GLSUB_IMMEDIATE)
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
	if (vbo != 0) { bglBindBuffer(GL_ARRAY_BUFFER, 0); bglDeleteBuffers(1, &vbo); }
	vbo = 0;
	glsubmode = GLSUB_IMMEDIATE;
}

void glsub_flush(void)
{
	if (!batchn) return;
	if (glsubmode == GLSUB_VBO) //new storage each time: no wait for the last draw
		bglBufferData(GL_ARRAY_BUFFER, batchn*(GLsizeiptr)sizeof(glsubvert_t), batch, GL_STREAM_DRAW);
	glDrawArrays(batchprim, 0, batchn);
	glsub_arraydraws++;
	batchn = 0;
}

void glsub_begin(int p)
{
	GLenum bp;

	prim = (GLenum)p;
	if (glsubmode == GLSUB_IMMEDIATE) { glBegin(prim); return; }
	bp = (prim == GL_TRIANGLE_FAN) ? GL_TRIANGLES : prim;
	if (bp != batchprim) { glsub_flush(); batchprim = bp; }
	fann = 0;
}

void glsub_texcoord(float u, float v)
{
	if (glsubmode == GLSUB_IMMEDIATE) { glTexCoord2f(u,v); return; }
	cur.u = u; cur.v = v;
}

void glsub_color(float r, float g, float b, float a)
{
	if (glsubmode == GLSUB_IMMEDIATE) { glColor4f(r,g,b,a); return; }
	cur.r = r; cur.g = g; cur.b = b; cur.a = a;
}

void glsub_vertex(float x, float y, float z)
{
	if (glsubmode == GLSUB_IMMEDIATE) { glVertex3f(x,y,z); return; }
	cur.x = x; cur.y = y; cur.z = z;

	if (prim == GL_TRIANGLE_FAN) //as triangles: (first, last, this) from the third on
	{
		if (fann++ < 2) { if (fann == 1) fan0 = cur; else fanlast = cur; return; }
		if (batchn+3 > MAXBATCH) glsub_flush();
		batch[batchn++] = fan0;
		batch[batchn++] = fanlast;
		batch[batchn++] = cur;
		fanlast = cur;
		return;
	}
	if ((batchn+2 > MAXBATCH) && (!(batchn&1))) glsub_flush(); //keep a line's two ends together
	batch[batchn++] = cur;
}

void glsub_end(void)
{
	if (glsubmode == GLSUB_IMMEDIATE) glEnd();
	else if (glsub_flusheach) glsub_flush();
}
#endif
