/**************************************************************************************************
"POLYMOST" code written by Ken Silverman
Ken Silverman's official web site: http://www.advsys.net/ken

Motivation:
When 3D Realms released the Duke Nukem 3D source code, I thought somebody would do a OpenGL or
Direct3D port. Well, after a few months passed, I saw no sign of somebody working on a true
hardware-accelerated port of Build, just people saying it wasn't possible. Eventually, I realized
the only way this was going to happen was for me to do it myself. First, I needed to port Build to
Windows. I could have done it myself, but instead I thought I'd ask my Australian buddy, Jonathon
Fowler, if he would upgrade his Windows port to my favorite compiler (MSVC) - which he did. Once
that was done, I was ready to start the "POLYMOST" project.

About:
This source file is basically a complete rewrite of the entire rendering part of the Build engine.
There are small pieces in ENGINE.C to activate this code, and other minor hacks in other source
files, but most of it is in here. If you're looking for polymost-related code in the other source
files, you should find most of them by searching for either "polymost" or "rendmode". Speaking of
rendmode, there are now 4 rendering modes in Build:

	rendmode 0: The original code I wrote from 1993-1997
	rendmode 1: Solid-color rendering: my debug code before I did texture mapping
	rendmode 2: Software rendering before I started the OpenGL code (Note: this is just a quick
						hack to make testing easier - it's not optimized to my usual standards!)
	rendmode 3: The OpenGL code

The original Build engine did hidden surface removal by using a vertical span buffer on the tops
and bottoms of walls. This worked nice back in the day, but it it's not suitable for a polygon
engine. So I decided to write a brand new hidden surface removal algorithm - using the same idea
as the original Build - but one that worked with vectors instead of already rasterized data.

Brief history:
06/20/2000: I release Build Source code
04/01/2003: 3D Realms releases Duke Nukem 3D source code
10/04/2003: Jonathon Fowler gets his Windows port working in Visual C
10/04/2003: I start writing POLYMOST.BAS, a new hidden surface removal algorithm for Build that
					works on a polygon level instead of spans.
10/16/2003: Ported POLYMOST.BAS to C inside JonoF KenBuild's ENGINE.C; later this code was split
					out of ENGINE.C and put in this file, POLYMOST.C.
12/10/2003: Started OpenGL code for POLYMOST (rendmode 3)
12/23/2003: 1st public release
01/01/2004: 2nd public release: fixed stray lines, status bar, mirrors, sky, and lots of other bugs.

----------------------------------------------------------------------------------------------------

Todo list (in approximate chronological order):

High priority:
	*   BOTH: Do accurate software sorting/chopping for sprites: drawing in wrong order is bad :/
	*   BOTH: Fix hall of mirrors near "zenith". Call polymost_drawrooms twice?
	* OPENGL: drawmapview()

Low priority:
	* SOFT6D: Do back-face culling of sprites during up/down/tilt transformation (top of drawpoly)
	* SOFT6D: Fix depth shading: use saturation&LUT
	* SOFT6D: Optimize using hyperbolic mapping (similar to KUBE algo)
	* SOFT6D: Slab6-style voxel sprites. How to accelerate? :/
	* OPENGL: KENBUILD: Write flipping code for floor mirrors
	*   BOTH: KENBUILD: Parallaxing sky modes 1&2
	*   BOTH: Masked/1-way walls don't clip correctly to sectors of intersecting ceiling/floor slopes
	*   BOTH: Editart x-center is not working correctly with Duke's camera/turret sprites
	*   BOTH: Get rid of horizontal line above Duke full-screen status bar
	*   BOTH: Combine ceilings/floors into a single triangle strip (should lower poly count by 2x)
	*   BOTH: Optimize/clean up texture-map setup equations

**************************************************************************************************/


#include <stdint.h>
#include <math.h> //<-important!
typedef struct { float x, cy[2], fy[2]; int32_t n, p, tag, ctag, ftag; } vsptyp;
#define VSPMAX 4096 //<- careful!
static vsptyp vsp[VSPMAX];
static int32_t vcnt, gtag;

static double dxb1[MAXWALLSB], dxb2[MAXWALLSB];

#define SCISDIST 1.0 //1.0: Close plane clipping distance
#define PI 3.14159265358979323

static double gyxscale, gxyaspect, gviewxrange, ghalfx, grhalfxdown10, grhalfxdown10x, ghoriz;
static double gfocalx;
static double gcosang, gsinang, gcosang2, gsinang2;
static double gchang, gshang, gctang, gstang;
static float gtang = 0.0;
double guo, gux, guy; //Screen-based texture mapping parameters
double gvo, gvx, gvy;
double gdo, gdx, gdy;

static inline void ftol (float f, int32_t *a)
{
	*a = (int32_t)lrintf(f); //rounds to nearest, as the x87 fistp it replaces
}

static inline void dtol (double d, int32_t *a)
{
	*a = (int32_t)lrint(d);
}

/*
================
imod

Wrap a signed texture coordinate into 0..b-1, with b positive.
C's remainder alone leaves negative coordinates below zero.
================
*/
static inline int32_t imod (int32_t a, int32_t b)
{
	if (a >= 0) return(a%b);
	return(((a+1)%b)+b-1);
}

/* Backend implementations share the engine-private translation unit. */
#define METH_LAYERS 8
static int32_t pow2xsplit = 0;

/* Projected polygon shared by the software and OpenGL rasterizers. */
typedef struct {
	double *px, *py, *dd, *uu, *vv;
	int32_t n, method, dorot, behind, layers, tsizx, tsizy;
	char *walptr;
} polymost_polygon;

void drawpoly (double *dpx, double *dpy, int32_t n, int32_t method);
#include "polymost_soft.c"
#ifdef USE_OPENGL
#include "polymost_opengl.c"
#endif


	//(dpx,dpy) specifies an n-sided polygon. The polygon must be a convex clockwise loop.
	//    n must be <= 8 (assume clipping can double number of vertices)
	//method: 0:solid, 1:masked(255 is transparent), 2:transluscent #1, 3:transluscent #2
	//    +4 means it's a sprite, so wraparound isn't needed
	//    +8 (METH_LAYERS) draws the tile's fullbright texels again over it, unfogged (OpenGL only)


void drawpoly (double *dpx, double *dpy, int32_t n, int32_t method)
{
	double f, r, ox, oy, oz, ox2, oy2, oz2, dd[16], uu[16], vv[16], px[16], py[16];
	int32_t i, j, k, tsizx, tsizy;
	int32_t dorot;
	char *walptr;
	int32_t layers = method&METH_LAYERS, behind = 0;

	method &= ~METH_LAYERS;
	/* Cull by projected winding before backend setup. Triangles use a
	 * single cross product instead of summing the polygon area. */
	if (n == 3)
	{
		if ((dpx[0]-dpx[1])*(dpy[2]-dpy[1]) >= (dpx[2]-dpx[1])*(dpy[0]-dpy[1])) return; //for triangle
	}
	else
	{
		f = 0; //f is area of polygon / 2
		for(i=n-2,j=n-1,k=0;k<n;i=j,j=k,k++) f += (dpx[i]-dpx[k])*dpy[j];
		if (f <= 0) return;
	}

		//Load texture (globalpicnum)
	if ((uint32_t)globalpicnum >= MAXTILES) globalpicnum = 0;
	setgotpic(globalpicnum);
	tsizx = tilesizx[globalpicnum];
	tsizy = tilesizy[globalpicnum];
	if (!palookup[globalpal]) globalpal = 0;
	if (!waloff[globalpicnum])
	{
		loadtile(globalpicnum);
		if (!waloff[globalpicnum])
		{
			if (rendmode != 3) return;
			tsizx = tsizy = 1; method = 1; //Hack to update Z-buffer for invalid mirror textures
		}
	}
	walptr = (char *)waloff[globalpicnum];

	/* Rotate depth and texture numerators together to preserve perspective. */
	j = 0; dorot = ((gchang != 1.0) || (gctang != 1.0));
	if (dorot)
	{
		for(i=0;i<n;i++)
		{
			ox = dpx[i]-ghalfx;
			oy = dpy[i]-ghoriz;
			oz = gfocalx;

				//Up/down rotation
			ox2 = ox;
			oy2 = oy*gchang - oz*gshang;
			oz2 = oy*gshang + oz*gchang;

				//Tilt rotation
			ox = ox2*gctang - oy2*gstang;
			oy = ox2*gstang + oy2*gctang;
			oz = oz2;

			/* The software path rejects a polygon with a vertex behind the near
			 * threshold here; the GL path carries a flag for its later handling. */
			if (oz < SCISDIST) { if (rendmode != 3) return; behind = 1; } //annoying hack to avoid bugs in software rendering

			r = gfocalx / oz;

			dd[j] = (dpx[i]*gdx + dpy[i]*gdy + gdo)*r;
			uu[j] = (dpx[i]*gux + dpy[i]*guy + guo)*r;
			vv[j] = (dpx[i]*gvx + dpy[i]*gvy + gvo)*r;

			px[j] = ox*r + ghalfx;
			py[j] = oy*r + ghoriz;
			if ((!j) || (px[j] != px[j-1]) || (py[j] != py[j-1])) j++;
		}
	}
	/* Without pitch/roll, each backend evaluates the original mapping planes. */
	else
	{
		for(i=0;i<n;i++)
		{
			px[j] = dpx[i];
			py[j] = dpy[i];
			if ((!j) || (px[j] != px[j-1]) || (py[j] != py[j-1])) j++;
		}
	}
	/* Remove a repeated closing vertex as well as adjacent duplicates,
	 * so the backend does not receive a zero-length closing edge. */
	while ((j >= 3) && (px[j-1] == px[0]) && (py[j-1] == py[0])) j--;
	if (j < 3) return;
	n = j;

	polymost_polygon poly = {
		px, py, dd, uu, vv, n, method, dorot, behind, layers, tsizx, tsizy, walptr
	};
#ifdef USE_OPENGL
	if (rendmode == 3)
	{
		polymost_gl_drawpoly(&poly);
		return;
	}
#endif
	polymost_soft_drawpoly(&poly);
}


	//Init viewport boundary (must be 4 point convex loop):
	//      (px[0],py[0]).----.(px[1],py[1])
	//                  /      \
	//                /          \
	// (px[3],py[3]).--------------.(px[2],py[2])
	//
void initmosts (double *px, double *py, int32_t n)
{
	int32_t i, j, k, imin;

	vcnt = 1; //0 is dummy solid node

	if (n < 3) return;
	imin = (px[1] < px[0]);
	for(i=n-1;i>=2;i--) if (px[i] < px[imin]) imin = i;


	vsp[vcnt].x = px[imin];
	vsp[vcnt].cy[0] = vsp[vcnt].fy[0] = py[imin];
	vcnt++;
	i = imin+1; if (i >= n) i = 0;
	j = imin-1; if (j < 0) j = n-1;
	do
	{
		if (px[i] < px[j])
		{
			if ((vcnt > 1) && (px[i] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[i];
			vsp[vcnt].cy[0] = py[i];
			k = j+1; if (k >= n) k = 0;
				//(px[k],py[k])
				//(px[i],?)
				//(px[j],py[j])
			vsp[vcnt].fy[0] = (px[i]-px[k])*(py[j]-py[k])/(px[j]-px[k]) + py[k];
			vcnt++;
			i++; if (i >= n) i = 0;
		}
		else if (px[j] < px[i])
		{
			if ((vcnt > 1) && (px[j] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[j];
			vsp[vcnt].fy[0] = py[j];
			k = i-1; if (k < 0) k = n-1;
				//(px[k],py[k])
				//(px[j],?)
				//(px[i],py[i])
			vsp[vcnt].cy[0] = (px[j]-px[k])*(py[i]-py[k])/(px[i]-px[k]) + py[k];
			vcnt++;
			j--; if (j < 0) j = n-1;
		}
		else
		{
			if ((vcnt > 1) && (px[i] <= vsp[vcnt-1].x)) vcnt--;
			vsp[vcnt].x = px[i];
			vsp[vcnt].cy[0] = py[i];
			vsp[vcnt].fy[0] = py[j];
			vcnt++;
			i++; if (i >= n) i = 0; if (i == j) break;
			j--; if (j < 0) j = n-1;
		}
	} while (i != j);
	if (px[i] > vsp[vcnt-1].x)
	{
		vsp[vcnt].x = px[i];
		vsp[vcnt].cy[0] = vsp[vcnt].fy[0] = py[i];
		vcnt++;
	}


	for(i=0;i<vcnt;i++)
	{
		vsp[i].cy[1] = vsp[i+1].cy[0]; vsp[i].ctag = i;
		vsp[i].fy[1] = vsp[i+1].fy[0]; vsp[i].ftag = i;
		vsp[i].n = i+1; vsp[i].p = i-1;
	}
	vsp[vcnt-1].n = 0; vsp[0].p = vcnt-1;
	gtag = vcnt;

		//VSPMAX-1 is dummy empty node
	for(i=vcnt;i<VSPMAX;i++) { vsp[i].n = i+1; vsp[i].p = i-1; }
	vsp[VSPMAX-1].n = vcnt; vsp[vcnt].p = VSPMAX-1;
}

/*
================
vsdel

Unlink a visibility strip and return its node to the free list.
VSPMAX-1 is the free-list sentinel; zero anchors the active list.
================
*/
void vsdel (int32_t i)
{
	int32_t pi, ni;
		//Delete i
	pi = vsp[i].p;
	ni = vsp[i].n;
	vsp[ni].p = pi;
	vsp[pi].n = ni;

		//Add i to empty list
	vsp[i].n = vsp[VSPMAX-1].n;
	vsp[i].p = VSPMAX-1;
	vsp[vsp[VSPMAX-1].n].p = i;
	vsp[VSPMAX-1].n = i;
}

/*
================
vsinsaft

Copy strip i into a free node and link the copy immediately after it.
The caller supplies the new edge coordinates when it splits the strip.
================
*/
int32_t vsinsaft (int32_t i)
{
	int32_t r;
		//i = next element from empty list
	r = vsp[VSPMAX-1].n;
	vsp[vsp[r].n].p = VSPMAX-1;
	vsp[VSPMAX-1].n = vsp[r].n;

	vsp[r] = vsp[i]; //copy i to r

		//insert r after i
	vsp[r].p = i; vsp[r].n = vsp[i].n;
	vsp[vsp[i].n].p = r; vsp[i].n = r;

	return(r);
}

/*
================
testvisiblemost

Test whether any open visibility strip overlaps the screen x interval.
Portal traversal can stop when earlier geometry has covered that interval.
================
*/
int32_t testvisiblemost (float x0, float x1)
{
	int32_t i, newi;

	for(i=vsp[0].n;i;i=newi)
	{
		newi = vsp[i].n;
		if ((x0 < vsp[newi].x) && (vsp[i].x < x1) && (vsp[i].ctag >= 0)) return(1);
	}
	return(0);
}

static int32_t domostlayers = METH_LAYERS; //0 while drawing the parallaxed sky: it has no glow pass
/*
================
domost

Draw the exposed polygon between an edge and the current visibility strips.
Then move the ceiling or floor boundary inward to hide it from later draws.
================
*/
void domost (float x0, float y0, float x1, float y1)
{
	double dpx[4], dpy[4];
	float d, f, n, t, slop, dx, dx0, dx1, nx, nx0, ny0, nx1, ny1;
	float spx[4], spy[4], cy[2], cv[2];
	int32_t i, j, k, z, ni, vcnt, scnt, newi, dir, spt[4];

	if (x0 < x1)
	{
		dir = 1; //clip dmost (floor)
		y0 -= .01; y1 -= .01;
	}
	else
	{
		if (x0 == x1) return;
		f = x0; x0 = x1; x1 = f;
		f = y0; y0 = y1; y1 = f;
		dir = 0; //clip umost (ceiling)
		//y0 += .01; y1 += .01; //necessary?
	}

	slop = (y1-y0)/(x1-x0);
	for(i=vsp[0].n;i;i=newi)
	{
		newi = vsp[i].n; nx0 = vsp[i].x; nx1 = vsp[newi].x;
		if ((x0 >= nx1) || (nx0 >= x1) || (vsp[i].ctag <= 0)) continue;
		dx = nx1-nx0;
		cy[0] = vsp[i].cy[0]; cv[0] = vsp[i].cy[1]-cy[0];
		cy[1] = vsp[i].fy[0]; cv[1] = vsp[i].fy[1]-cy[1];

		scnt = 0;

			//Test if left edge requires split (x0,y0) (nx0,cy(0)),<dx,cv(0)>
		if ((x0 > nx0) && (x0 < nx1))
		{
			t = (x0-nx0)*cv[dir] - (y0-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0)))
				{ spx[scnt] = x0; spy[scnt] = y0; spt[scnt] = -1; scnt++; }
		}

			//Test for intersection on umost (j == 0) and dmost (j == 1)
		for(j=0;j<2;j++)
		{
			d = (y0-y1)*dx - (x0-x1)*cv[j];
			n = (y0-cy[j])*dx - (x0-nx0)*cv[j];
			if ((fabs(n) <= fabs(d)) && (d*n >= 0) && (d != 0))
			{
				t = n/d; nx = (x1-x0)*t + x0;
				if ((nx > nx0) && (nx < nx1))
				{
					spx[scnt] = nx; spy[scnt] = (y1-y0)*t + y0;
					spt[scnt] = j; scnt++;
				}
			}
		}

			//Nice hack to avoid full sort later :)
		if ((scnt >= 2) && (spx[scnt-1] < spx[scnt-2]))
		{
			f = spx[scnt-1]; spx[scnt-1] = spx[scnt-2]; spx[scnt-2] = f;
			f = spy[scnt-1]; spy[scnt-1] = spy[scnt-2]; spy[scnt-2] = f;
			j = spt[scnt-1]; spt[scnt-1] = spt[scnt-2]; spt[scnt-2] = j;
		}

			//Test if right edge requires split
		if ((x1 > nx0) && (x1 < nx1))
		{
			t = (x1-nx0)*cv[dir] - (y1-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0)))
				{ spx[scnt] = x1; spy[scnt] = y1; spt[scnt] = -1; scnt++; }
		}

		vsp[i].tag = vsp[newi].tag = -1;
		for(z=0;z<=scnt;z++,i=vcnt)
		{
			if (z < scnt)
			{
				vcnt = vsinsaft(i);
				t = (spx[z]-nx0)/dx;
				vsp[i].cy[1] = t*cv[0] + cy[0];
				vsp[i].fy[1] = t*cv[1] + cy[1];
				vsp[vcnt].x = spx[z];
				vsp[vcnt].cy[0] = vsp[i].cy[1];
				vsp[vcnt].fy[0] = vsp[i].fy[1];
				vsp[vcnt].tag = spt[z];
			}

			ni = vsp[i].n; if (!ni) continue; //this 'if' fixes many bugs!
			dx0 = vsp[i].x; if (x0 > dx0) continue;
			dx1 = vsp[ni].x; if (x1 < dx1) continue;
			ny0 = (dx0-x0)*slop + y0;
			ny1 = (dx1-x0)*slop + y0;

				//      dx0           dx1
				//       ³             ³
				//----------------------------
				//     t0+=0         t1+=0
				//   vsp[i].cy[0]  vsp[i].cy[1]
				//============================
				//     t0+=1         t1+=3
				//============================
				//   vsp[i].fy[0]    vsp[i].fy[1]
				//     t0+=2         t1+=6
				//
				//     ny0 ?         ny1 ?

			/* Both ends have three positions: above, inside or below the aperture.
			 * A base-three index selects the corresponding clipping case below. */
			k = 1+3;
			if ((vsp[i].tag == 0) || (ny0 <= vsp[i].cy[0]+.01)) k--;
			if ((vsp[i].tag == 1) || (ny0 >= vsp[i].fy[0]-.01)) k++;
			if ((vsp[ni].tag == 0) || (ny1 <= vsp[i].cy[1]+.01)) k -= 3;
			if ((vsp[ni].tag == 1) || (ny1 >= vsp[i].fy[1]-.01)) k += 3;

			if (!dir)
			{
				switch(k)
				{
					case 1: case 2:
						dpx[0] = dx0; dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1; dpy[1] = vsp[i].cy[1];
						dpx[2] = dx0; dpy[2] = ny0; drawpoly(dpx,dpy,3,domostlayers);
						vsp[i].cy[0] = ny0; vsp[i].ctag = gtag; break;
					case 3: case 6:
						dpx[0] = dx0; dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1; dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1; dpy[2] = ny1; drawpoly(dpx,dpy,3,domostlayers);
						vsp[i].cy[1] = ny1; vsp[i].ctag = gtag; break;
					case 4: case 5: case 7:
						dpx[0] = dx0; dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1; dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1; dpy[2] = ny1;
						dpx[3] = dx0; dpy[3] = ny0; drawpoly(dpx,dpy,4,domostlayers);
						vsp[i].cy[0] = ny0; vsp[i].cy[1] = ny1; vsp[i].ctag = gtag; break;
					case 8:
						dpx[0] = dx0; dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1; dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1; dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0; dpy[3] = vsp[i].fy[0]; drawpoly(dpx,dpy,4,domostlayers);
						vsp[i].ctag = vsp[i].ftag = -1; break;
					default: break;
				}
			}
			else
			{
				switch(k)
				{
					case 7: case 6:
						dpx[0] = dx0; dpy[0] = ny0;
						dpx[1] = dx1; dpy[1] = vsp[i].fy[1];
						dpx[2] = dx0; dpy[2] = vsp[i].fy[0]; drawpoly(dpx,dpy,3,domostlayers);
						vsp[i].fy[0] = ny0; vsp[i].ftag = gtag; break;
					case 5: case 2:
						dpx[0] = dx0; dpy[0] = vsp[i].fy[0];
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp[i].fy[1]; drawpoly(dpx,dpy,3,domostlayers);
						vsp[i].fy[1] = ny1; vsp[i].ftag = gtag; break;
					case 4: case 3: case 1:
						dpx[0] = dx0; dpy[0] = ny0;
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0; dpy[3] = vsp[i].fy[0]; drawpoly(dpx,dpy,4,domostlayers);
						vsp[i].fy[0] = ny0; vsp[i].fy[1] = ny1; vsp[i].ftag = gtag; break;
					case 0:
						dpx[0] = dx0; dpy[0] = vsp[i].cy[0];
						dpx[1] = dx1; dpy[1] = vsp[i].cy[1];
						dpx[2] = dx1; dpy[2] = vsp[i].fy[1];
						dpx[3] = dx0; dpy[3] = vsp[i].fy[0]; drawpoly(dpx,dpy,4,domostlayers);
						vsp[i].ctag = vsp[i].ftag = -1; break;
					default: break;
				}
			}
		}
	}

	gtag++;

		//Combine neighboring vertical strips with matching collinear top&bottom edges
		//This prevents x-splits from propagating through the entire scan
	i = vsp[0].n;
	while (i)
	{
		ni = vsp[i].n;
		if ((vsp[i].cy[0] >= vsp[i].fy[0]) && (vsp[i].cy[1] >= vsp[i].fy[1])) { vsp[i].ctag = vsp[i].ftag = -1; }
		if ((vsp[i].ctag == vsp[ni].ctag) && (vsp[i].ftag == vsp[ni].ftag))
			{ vsp[i].cy[1] = vsp[ni].cy[1]; vsp[i].fy[1] = vsp[ni].fy[1]; vsdel(ni); }
		else i = ni;
	}
}

static void polymost_scansector (int32_t sectnum);

/*
================
polymost_drawalls

Draw a wall bunch and its exposed ceiling/floor polygons through domost.
Visit the sector beyond a portal only while its screen interval is visible.
================
*/
static void polymost_drawalls (int32_t bunch)
{
	sectortype *sec, *nextsec;
	walltype *wal, *wal2, *nwal;
	double ox, oy, oz, ox2, oy2, px[3], py[3], dd[3], uu[3], vv[3];
	double fx, fy, x0, x1, y0, y1, cy0, cy1, fy0, fy1, xp0, yp0, xp1, yp1, ryp0, ryp1, nx0, ny0, nx1, ny1;
	double t, r, t0, t1, ocy0, ocy1, ofy0, ofy1, oxp0, oyp0, ft[4];
	double oguo, ogux, oguy;
	int32_t i, x, y, z, cz, fz, wallnum, sectnum, nextsectnum;

	sectnum = thesector[bunchfirst[bunch]]; sec = &sector[sectnum];

#ifdef USE_OPENGL
	polymost_gl_sectorfog(sectnum);
#endif

		//DRAW WALLS SECTION!
	for(z=bunchfirst[bunch];z>=0;z=p2[z])
	{
		wallnum = thewall[z]; wal = &wall[wallnum]; wal2 = &wall[wal->point2];
		nextsectnum = wal->nextsector; nextsec = &sector[nextsectnum];

			//Offset&Rotate 3D coordinates to screen 3D space
		x = wal->x-globalposx; y = wal->y-globalposy;
		xp0 = (double)y*gcosang  - (double)x*gsinang;
		yp0 = (double)x*gcosang2 + (double)y*gsinang2;
		x = wal2->x-globalposx; y = wal2->y-globalposy;
		xp1 = (double)y*gcosang  - (double)x*gsinang;
		yp1 = (double)x*gcosang2 + (double)y*gsinang2;

		oxp0 = xp0; oyp0 = yp0;

			//Clip to close parallel-screen plane
		if (yp0 < SCISDIST)
		{
			if (yp1 < SCISDIST) continue;
			t0 = (SCISDIST-yp0)/(yp1-yp0); xp0 = (xp1-xp0)*t0+xp0; yp0 = SCISDIST;
			nx0 = (wal2->x-wal->x)*t0+wal->x;
			ny0 = (wal2->y-wal->y)*t0+wal->y;
		}
		else { t0 = 0.f; nx0 = wal->x; ny0 = wal->y; }
		if (yp1 < SCISDIST)
		{
			t1 = (SCISDIST-oyp0)/(yp1-oyp0); xp1 = (xp1-oxp0)*t1+oxp0; yp1 = SCISDIST;
			nx1 = (wal2->x-wal->x)*t1+wal->x;
			ny1 = (wal2->y-wal->y)*t1+wal->y;
		}
		else { t1 = 1.f; nx1 = wal2->x; ny1 = wal2->y; }

		ryp0 = 1.f/yp0; ryp1 = 1.f/yp1;

			//Generate screen coordinates for front side of wall
		x0 = ghalfx*xp0*ryp0 + ghalfx;
		x1 = ghalfx*xp1*ryp1 + ghalfx;
		if (x1 <= x0) continue;

		ryp0 *= gyxscale; ryp1 *= gyxscale;

		getzsofslope(sectnum,nx0,ny0,&cz,&fz);
		cy0 = ((float)(cz-globalposz))*ryp0 + ghoriz;
		fy0 = ((float)(fz-globalposz))*ryp0 + ghoriz;
		getzsofslope(sectnum,nx1,ny1,&cz,&fz);
		cy1 = ((float)(cz-globalposz))*ryp1 + ghoriz;
		fy1 = ((float)(fz-globalposz))*ryp1 + ghoriz;


		globalpicnum = sec->floorpicnum; globalshade = sec->floorshade; globalpal = (int32_t)((unsigned char)sec->floorpal);
		globalorientation = sec->floorstat;
		if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,sectnum);
		if (!(globalorientation&1))
		{
				//(singlobalang/-16384*(sx-ghalfx) + 0*(sy-ghoriz) + (cosviewingrangeglobalang/16384)*ghalfx)*d + globalposx    = u*16
				//(cosglobalang/ 16384*(sx-ghalfx) + 0*(sy-ghoriz) + (sinviewingrangeglobalang/16384)*ghalfx)*d + globalposy    = v*16
				//(                  0*(sx-ghalfx) + 1*(sy-ghoriz) + (                             0)*ghalfx)*d + globalposz/16 = (sec->floorz/16)
			if (!(globalorientation&64))
				{ ft[0] = globalposx; ft[1] = globalposy; ft[2] = cosglobalang; ft[3] = singlobalang; }
			else
			{
					//relative alignment
				fx = (double)(wall[wall[sec->wallptr].point2].x-wall[sec->wallptr].x);
				fy = (double)(wall[wall[sec->wallptr].point2].y-wall[sec->wallptr].y);
				r = 1.0/sqrt(fx*fx+fy*fy); fx *= r; fy *= r;
				ft[2] = cosglobalang*fx + singlobalang*fy;
				ft[3] = singlobalang*fx - cosglobalang*fy;
				ft[0] = ((double)(globalposx-wall[sec->wallptr].x))*fx + ((double)(globalposy-wall[sec->wallptr].y))*fy;
				ft[1] = ((double)(globalposy-wall[sec->wallptr].y))*fx - ((double)(globalposx-wall[sec->wallptr].x))*fy;
				if (!(globalorientation&4)) globalorientation ^= 32; else globalorientation ^= 16;
			}
			gdx = 0;
			gdy = gxyaspect; if (!(globalorientation&2)) gdy /= (double)(sec->floorz-globalposz);
			gdo = -ghoriz*gdy;
			if (globalorientation&8) { ft[0] /= 8; ft[1] /= -8; ft[2] /= 2097152; ft[3] /= 2097152; }
									  else { ft[0] /= 16; ft[1] /= -16; ft[2] /= 4194304; ft[3] /= 4194304; }
			gux = (double)ft[3]*((double)viewingrange)/-65536.0;
			gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]-gux)*ghalfx;
			gvo -= (double)(ft[3]+gvx)*ghalfx;

				//Texture flipping
			if (globalorientation&4)
			{
				r = gux; gux = gvx; gvx = r;
				r = guy; guy = gvy; gvy = r;
				r = guo; guo = gvo; gvo = r;
			}
			if (globalorientation&16) { gux = -gux; guy = -guy; guo = -guo; }
			if (globalorientation&32) { gvx = -gvx; gvy = -gvy; gvo = -gvo; }

				//Texture panning
			fx = (float)sec->floorxpanning*((float)(1<<(picsiz[globalpicnum]&15)))/256.0;
			fy = (float)sec->floorypanning*((float)(1<<(picsiz[globalpicnum]>>4)))/256.0;
			if ((globalorientation&(2+64)) == (2+64)) //Hack for panning for slopes w/ relative alignment
			{
				r = (float)sec->floorheinum / 4096.0; r = 1.0/sqrt(r*r+1);
				if (!(globalorientation&4)) fy *= r; else fx *= r;
			}
			guy += gdy*fx; guo += gdo*fx;
			gvy += gdy*fy; gvo += gdo*fy;

			if (globalorientation&2) //slopes
			{
				px[0] = x0; py[0] = ryp0 + ghoriz;
				px[1] = x1; py[1] = ryp1 + ghoriz;

					//Pick some point guaranteed to be not collinear to the 1st two points
				ox = nx0 + (ny1-ny0);
				oy = ny0 + (nx0-nx1);
				ox2 = (double)(oy-globalposy)*gcosang  - (double)(ox-globalposx)*gsinang;
				oy2 = (double)(ox-globalposx)*gcosang2 + (double)(oy-globalposy)*gsinang2;
				oy2 = 1.0/oy2;
				px[2] = ghalfx*ox2*oy2 + ghalfx; oy2 *= gyxscale;
				py[2] = oy2 + ghoriz;

				for(i=0;i<3;i++)
				{
					dd[i] = px[i]*gdx + py[i]*gdy + gdo;
					uu[i] = px[i]*gux + py[i]*guy + guo;
					vv[i] = px[i]*gvx + py[i]*gvy + gvo;
				}

				py[0] = fy0;
				py[1] = fy1;
				py[2] = (getflorzofslope(sectnum,ox,oy)-globalposz)*oy2 + ghoriz;

				ox = py[1]-py[2]; oy = py[2]-py[0]; oz = py[0]-py[1];
				r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
				gdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				gux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
				gdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				guy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				gdo = dd[0] - px[0]*gdx - py[0]*gdy;
				guo = uu[0] - px[0]*gux - py[0]*guy;
				gvo = vv[0] - px[0]*gvx - py[0]*gvy;

				if (globalorientation&64) //Hack for relative alignment on slopes
				{
					r = (float)sec->floorheinum / 4096.0;
					r = sqrt(r*r+1);
					if (!(globalorientation&4)) { gvx *= r; gvy *= r; gvo *= r; }
												  else { gux *= r; guy *= r; guo *= r; }
				}
			}
			pow2xsplit = 0; domost(x0,fy0,x1,fy1); //flor
		}
		else if ((nextsectnum < 0) || (!(sector[nextsectnum].floorstat&1)))
		{
				//Parallaxing sky... hacked for Ken's mountain texture; paper-sky only :/
			domostlayers = 0;
#ifdef USE_OPENGL
	polymost_gl_beginsky(sec->visibility);
#endif
			if (1) //parallaxtype == 0)
			{
				dd[0] = (float)xdimen*.0000001; //Adjust sky depth based on screen size!
				t = (double)((1<<(picsiz[globalpicnum]&15))<<pskybits);
				vv[1] = dd[0]*((double)xdimscale*(double)viewingrange)/(65536.0*65536.0);
				vv[0] = dd[0]*((double)((tilesizy[globalpicnum]>>1)+parallaxyoffs)) - vv[1]*ghoriz;
				i = (1<<(picsiz[globalpicnum]>>4)); if (i != tilesizy[globalpicnum]) i += i;
				vv[0] += dd[0]*((double)sec->floorypanning)*((double)i)/256.0;


					//Hack to draw black rectangle below sky when looking down...
				gdx = 0; gdy = gxyaspect / 262144.0; gdo = -ghoriz*gdy;
				gux = 0; guy = 0; guo = 0;
				gvx = 0; gvy = (double)(tilesizy[globalpicnum]-1)*gdy; gvo = (double)(tilesizy[globalpicnum-1])*gdo;
				oy = (((double)tilesizy[globalpicnum])*dd[0]-vv[0])/vv[1];
				if ((oy > fy0) && (oy > fy1)) domost(x0,oy,x1,oy);
				else if ((oy > fy0) != (oy > fy1))
				{     //  fy0                      fy1
						//     \                    /
						//oy----------      oy----------
						//        \              /
						//         fy1        fy0
					ox = (oy-fy0)*(x1-x0)/(fy1-fy0) + x0;
					if (oy > fy0) { domost(x0,oy,ox,oy); domost(ox,oy,x1,fy1); }
							  else { domost(x0,fy0,ox,oy); domost(ox,oy,x1,oy); }
				} else domost(x0,fy0,x1,fy1);


				gdx = 0; gdy = 0; gdo = dd[0];
				gux = gdo*(t*((double)xdimscale)*((double)yxaspect)*((double)viewingrange))/(16384.0*65536.0*65536.0*5.0*1024.0);
				guy = 0; //guo calculated later
				gvx = 0; gvy = vv[1]; gvo = vv[0];

				i = globalpicnum; r = (fy1-fy0)/(x1-x0); //slope of line
				oy = ((double)viewingrange)/(ghalfx*256.0); oz = 1/oy;

				y = ((((int32_t)((x0-ghalfx)*oy))+globalang)>>(11-pskybits));
				fx = x0;
				do
				{
					globalpicnum = pskyoff[y&((1<<pskybits)-1)]+i;
					guo = gdo*(t*((double)(globalang-(y<<(11-pskybits))))/2048.0 + (double)sec->floorxpanning) - gux*ghalfx;
					y++;
					ox = fx; fx = ((double)((y<<(11-pskybits))-globalang))*oz+ghalfx;
					if (fx > x1) { fx = x1; i = -1; }

					pow2xsplit = 0; domost(ox,(ox-x0)*r+fy0,fx,(fx-x0)*r+fy0); //flor
				} while (i >= 0);

			}
#ifdef USE_OPENGL
	polymost_gl_endsky();
#endif
			domostlayers = METH_LAYERS;
		}

		globalpicnum = sec->ceilingpicnum; globalshade = sec->ceilingshade; globalpal = (int32_t)((unsigned char)sec->ceilingpal);
		globalorientation = sec->ceilingstat;
		if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,sectnum);
		if (!(globalorientation&1))
		{
			if (!(globalorientation&64))
				{ ft[0] = globalposx; ft[1] = globalposy; ft[2] = cosglobalang; ft[3] = singlobalang; }
			else
			{
					//relative alignment
				fx = (double)(wall[wall[sec->wallptr].point2].x-wall[sec->wallptr].x);
				fy = (double)(wall[wall[sec->wallptr].point2].y-wall[sec->wallptr].y);
				r = 1.0/sqrt(fx*fx+fy*fy); fx *= r; fy *= r;
				ft[2] = cosglobalang*fx + singlobalang*fy;
				ft[3] = singlobalang*fx - cosglobalang*fy;
				ft[0] = ((double)(globalposx-wall[sec->wallptr].x))*fx + ((double)(globalposy-wall[sec->wallptr].y))*fy;
				ft[1] = ((double)(globalposy-wall[sec->wallptr].y))*fx - ((double)(globalposx-wall[sec->wallptr].x))*fy;
				if (!(globalorientation&4)) globalorientation ^= 32; else globalorientation ^= 16;
			}
			gdx = 0;
			gdy = gxyaspect;
			if (!(globalorientation&2)) gdy /= (double)(sec->ceilingz-globalposz);
			gdo = -ghoriz*gdy;
			if (globalorientation&8) { ft[0] /= 8; ft[1] /= -8; ft[2] /= 2097152; ft[3] /= 2097152; }
									  else { ft[0] /= 16; ft[1] /= -16; ft[2] /= 4194304; ft[3] /= 4194304; }
			gux = (double)ft[3]*((double)viewingrange)/-65536.0;
			gvx = (double)ft[2]*((double)viewingrange)/-65536.0;
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]-gux)*ghalfx;
			gvo -= (double)(ft[3]+gvx)*ghalfx;

				//Texture flipping
			if (globalorientation&4)
			{
				r = gux; gux = gvx; gvx = r;
				r = guy; guy = gvy; gvy = r;
				r = guo; guo = gvo; gvo = r;
			}
			if (globalorientation&16) { gux = -gux; guy = -guy; guo = -guo; }
			if (globalorientation&32) { gvx = -gvx; gvy = -gvy; gvo = -gvo; }

				//Texture panning
			fx = (float)sec->ceilingxpanning*((float)(1<<(picsiz[globalpicnum]&15)))/256.0;
			fy = (float)sec->ceilingypanning*((float)(1<<(picsiz[globalpicnum]>>4)))/256.0;
			if ((globalorientation&(2+64)) == (2+64)) //Hack for panning for slopes w/ relative alignment
			{
				r = (float)sec->ceilingheinum / 4096.0; r = 1.0/sqrt(r*r+1);
				if (!(globalorientation&4)) fy *= r; else fx *= r;
			}
			guy += gdy*fx; guo += gdo*fx;
			gvy += gdy*fy; gvo += gdo*fy;

			if (globalorientation&2) //slopes
			{
				px[0] = x0; py[0] = ryp0 + ghoriz;
				px[1] = x1; py[1] = ryp1 + ghoriz;

					//Pick some point guaranteed to be not collinear to the 1st two points
				ox = nx0 + (ny1-ny0);
				oy = ny0 + (nx0-nx1);
				ox2 = (double)(oy-globalposy)*gcosang  - (double)(ox-globalposx)*gsinang ;
				oy2 = (double)(ox-globalposx)*gcosang2 + (double)(oy-globalposy)*gsinang2;
				oy2 = 1.0/oy2;
				px[2] = ghalfx*ox2*oy2 + ghalfx; oy2 *= gyxscale;
				py[2] = oy2 + ghoriz;

				for(i=0;i<3;i++)
				{
					dd[i] = px[i]*gdx + py[i]*gdy + gdo;
					uu[i] = px[i]*gux + py[i]*guy + guo;
					vv[i] = px[i]*gvx + py[i]*gvy + gvo;
				}

				py[0] = cy0;
				py[1] = cy1;
				py[2] = (getceilzofslope(sectnum,ox,oy)-globalposz)*oy2 + ghoriz;

				ox = py[1]-py[2]; oy = py[2]-py[0]; oz = py[0]-py[1];
				r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
				gdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				gux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
				gdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
				guy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
				gvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
				gdo = dd[0] - px[0]*gdx - py[0]*gdy;
				guo = uu[0] - px[0]*gux - py[0]*guy;
				gvo = vv[0] - px[0]*gvx - py[0]*gvy;

				if (globalorientation&64) //Hack for relative alignment on slopes
				{
					r = (float)sec->ceilingheinum / 4096.0;
					r = sqrt(r*r+1);
					if (!(globalorientation&4)) { gvx *= r; gvy *= r; gvo *= r; }
												  else { gux *= r; guy *= r; guo *= r; }
				}
			}
			pow2xsplit = 0; domost(x1,cy1,x0,cy0); //ceil
		}
		else if ((nextsectnum < 0) || (!(sector[nextsectnum].ceilingstat&1)))
		{
			domostlayers = 0;
#ifdef USE_OPENGL
	polymost_gl_beginsky(sec->visibility);
#endif
				//Parallaxing sky... hacked for Ken's mountain texture; paper-sky only :/
			if (1) //parallaxtype == 0)
			{
				dd[0] = (float)xdimen*.0000001; //Adjust sky depth based on screen size!
				t = (double)((1<<(picsiz[globalpicnum]&15))<<pskybits);
				vv[1] = dd[0]*((double)xdimscale*(double)viewingrange)/(65536.0*65536.0);
				vv[0] = dd[0]*((double)((tilesizy[globalpicnum]>>1)+parallaxyoffs)) - vv[1]*ghoriz;
				i = (1<<(picsiz[globalpicnum]>>4)); if (i != tilesizy[globalpicnum]) i += i;
				vv[0] += dd[0]*((double)sec->ceilingypanning)*((double)i)/256.0;

					//Hack to draw black rectangle below sky when looking down...
				gdx = 0; gdy = gxyaspect / -262144.0; gdo = -ghoriz*gdy;
				gux = 0; guy = 0; guo = 0;
				gvx = 0; gvy = 0; gvo = 0;
				oy = -vv[0]/vv[1];
				if ((oy < cy0) && (oy < cy1)) domost(x1,oy,x0,oy);
				else if ((oy < cy0) != (oy < cy1))
				{     //         cy1        cy0
						//        /             \
						//oy----------      oy---------
						//    /                    \
						//  cy0                     cy1
					ox = (oy-cy0)*(x1-x0)/(cy1-cy0) + x0;
					if (oy < cy0) { domost(ox,oy,x0,oy); domost(x1,cy1,ox,oy); }
								else { domost(ox,oy,x0,cy0); domost(x1,oy,ox,oy); }
				} else domost(x1,cy1,x0,cy0);


				gdx = 0; gdy = 0; gdo = dd[0];
				gux = gdo*(t*((double)xdimscale)*((double)yxaspect)*((double)viewingrange))/(16384.0*65536.0*65536.0*5.0*1024.0);
				guy = 0; //guo calculated later
				gvx = 0; gvy = vv[1]; gvo = vv[0];

				i = globalpicnum; r = (cy1-cy0)/(x1-x0); //slope of line
				oy = ((double)viewingrange)/(ghalfx*256.0); oz = 1/oy;

				y = ((((int32_t)((x0-ghalfx)*oy))+globalang)>>(11-pskybits));
				fx = x0;
				do
				{
					globalpicnum = pskyoff[y&((1<<pskybits)-1)]+i;
					guo = gdo*(t*((double)(globalang-(y<<(11-pskybits))))/2048.0 + (double)sec->ceilingxpanning) - gux*ghalfx;
					y++;
					ox = fx; fx = ((double)((y<<(11-pskybits))-globalang))*oz+ghalfx;
					if (fx > x1) { fx = x1; i = -1; }
					pow2xsplit = 0; domost(fx,(fx-x0)*r+cy0,ox,(ox-x0)*r+cy0); //ceil
				} while (i >= 0);
			}
#ifdef USE_OPENGL
	polymost_gl_endsky();
#endif
			domostlayers = METH_LAYERS;
		}

			//(x0,cy0) == (u=             0,v=0,d=)
			//(x1,cy0) == (u=wal->xrepeat*8,v=0)
			//(x0,fy0) == (u=             0,v=v)
			//             u = (gux*sx + guy*sy + guo) / (gdx*sx + gdy*sy + gdo)
			//             v = (gvx*sx + gvy*sy + gvo) / (gdx*sx + gdy*sy + gdo)
			//             0 = (gux*x0 + guy*cy0 + guo) / (gdx*x0 + gdy*cy0 + gdo)
			//wal->xrepeat*8 = (gux*x1 + guy*cy0 + guo) / (gdx*x1 + gdy*cy0 + gdo)
			//             0 = (gvx*x0 + gvy*cy0 + gvo) / (gdx*x0 + gdy*cy0 + gdo)
			//             v = (gvx*x0 + gvy*fy0 + gvo) / (gdx*x0 + gdy*fy0 + gdo)
			//sx = x0, u = t0*wal->xrepeat*8, d = yp0;
			//sx = x1, u = t1*wal->xrepeat*8, d = yp1;
			//d = gdx*sx + gdo
			//u = (gux*sx + guo) / (gdx*sx + gdo)
			//yp0 = gdx*x0 + gdo
			//yp1 = gdx*x1 + gdo
			//t0*wal->xrepeat*8 = (gux*x0 + guo) / (gdx*x0 + gdo)
			//t1*wal->xrepeat*8 = (gux*x1 + guo) / (gdx*x1 + gdo)
			//gdx*x0 + gdo = yp0
			//gdx*x1 + gdo = yp1
		gdx = (ryp0-ryp1)*gxyaspect / (x0-x1);
		gdy = 0;
		gdo = ryp0*gxyaspect - gdx*x0;

			//gux*x0 + guo = t0*wal->xrepeat*8*yp0
			//gux*x1 + guo = t1*wal->xrepeat*8*yp1
		gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)wal->xrepeat*8.f / (x0-x1);
		guo = t0*ryp0*gxyaspect*(float)wal->xrepeat*8.f - gux*x0;
		guo += (float)wal->xpanning*gdo;
		gux += (float)wal->xpanning*gdx;
		guy = 0;
			//Derivation for u:
			//   (gvx*x0 + gvy*cy0 + gvo) / (gdx*x0 + gdy*cy0 + gdo) = 0
			//   (gvx*x1 + gvy*cy1 + gvo) / (gdx*x1 + gdy*cy1 + gdo) = 0
			//   (gvx*x0 + gvy*fy0 + gvo) / (gdx*x0 + gdy*fy0 + gdo) = v
			//   (gvx*x1 + gvy*fy1 + gvo) / (gdx*x1 + gdy*fy1 + gdo) = v
			//   (gvx*x0 + gvy*cy0 + gvo*1) = 0
			//   (gvx*x1 + gvy*cy1 + gvo*1) = 0
			//   (gvx*x0 + gvy*fy0 + gvo*1) = t
		ogux = gux; oguy = guy; oguo = guo;

		if (nextsectnum >= 0)
		{
			getzsofslope(nextsectnum,nx0,ny0,&cz,&fz);
			ocy0 = ((float)(cz-globalposz))*ryp0 + ghoriz;
			ofy0 = ((float)(fz-globalposz))*ryp0 + ghoriz;
			getzsofslope(nextsectnum,nx1,ny1,&cz,&fz);
			ocy1 = ((float)(cz-globalposz))*ryp1 + ghoriz;
			ofy1 = ((float)(fz-globalposz))*ryp1 + ghoriz;

			if ((wal->cstat&48) == 16) maskwall[maskwallcnt++] = z;

			if (((cy0 < ocy0) || (cy1 < ocy1)) && (!((sec->ceilingstat&sector[nextsectnum].ceilingstat)&1)))
			{
				globalpicnum = wal->picnum; globalshade = wal->shade; globalpal = (int32_t)((unsigned char)wal->pal);
				if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

				if (!(wal->cstat&4)) i = sector[nextsectnum].ceilingz; else i = sec->ceilingz;
				t0 = ((float)(i-globalposz))*ryp0 + ghoriz;
				t1 = ((float)(i-globalposz))*ryp1 + ghoriz;
				t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.f);
				i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
				fy = (float)wal->ypanning * ((float)i) / 256.0;
				gvx = (t0-t1)*t;
				gvy = (x1-x0)*t;
				gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

				if (wal->cstat&8) //xflip
				{
					t = (float)(wal->xrepeat*8 + wal->xpanning*2);
					gux = gdx*t - gux;
					guy = gdy*t - guy;
					guo = gdo*t - guo;
				}
				if (wal->cstat&256) { gvx = -gvx; gvy = -gvy; gvo = -gvo; } //yflip

				pow2xsplit = 1; domost(x1,ocy1,x0,ocy0);

				if (wal->cstat&8) { gux = ogux; guy = oguy; guo = oguo; }
			}
			if (((ofy0 < fy0) || (ofy1 < fy1)) && (!((sec->floorstat&sector[nextsectnum].floorstat)&1)))
			{
				if (!(wal->cstat&2)) nwal = wal;
				else
				{
					nwal = &wall[wal->nextwall];
					guo += (float)(nwal->xpanning-wal->xpanning)*gdo;
					gux += (float)(nwal->xpanning-wal->xpanning)*gdx;
					guy += (float)(nwal->xpanning-wal->xpanning)*gdy;
				}
				globalpicnum = nwal->picnum; globalshade = nwal->shade; globalpal = (int32_t)((unsigned char)nwal->pal);
				if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

				if (!(nwal->cstat&4)) i = sector[nextsectnum].floorz; else i = sec->ceilingz;
				t0 = ((float)(i-globalposz))*ryp0 + ghoriz;
				t1 = ((float)(i-globalposz))*ryp1 + ghoriz;
				t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.f);
				i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
				fy = (float)nwal->ypanning * ((float)i) / 256.0;
				gvx = (t0-t1)*t;
				gvy = (x1-x0)*t;
				gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

				if (wal->cstat&8) //xflip
				{
					t = (float)(wal->xrepeat*8 + nwal->xpanning*2);
					gux = gdx*t - gux;
					guy = gdy*t - guy;
					guo = gdo*t - guo;
				}
				if (nwal->cstat&256) { gvx = -gvx; gvy = -gvy; gvo = -gvo; } //yflip

				pow2xsplit = 1; domost(x0,ofy0,x1,ofy1);

				if (wal->cstat&(2+8)) { guo = oguo; gux = ogux; guy = oguy; }
			}
		}

		if ((nextsectnum < 0) || (wal->cstat&32))   //White/1-way wall
		{
			if (nextsectnum < 0) globalpicnum = wal->picnum; else globalpicnum = wal->overpicnum;
			globalshade = wal->shade; globalpal = (int32_t)((unsigned char)wal->pal);
			if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,wallnum+16384);

			if (nextsectnum >= 0) { if (!(wal->cstat&4)) i = nextsec->ceilingz; else i = sec->ceilingz; }
								  else { if (!(wal->cstat&4)) i = sec->ceilingz;     else i = sec->floorz; }
			t0 = ((float)(i-globalposz))*ryp0 + ghoriz;
			t1 = ((float)(i-globalposz))*ryp1 + ghoriz;
			t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.f);
			i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
			fy = (float)wal->ypanning * ((float)i) / 256.0;
			gvx = (t0-t1)*t;
			gvy = (x1-x0)*t;
			gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

			if (wal->cstat&8) //xflip
			{
				t = (float)(wal->xrepeat*8 + wal->xpanning*2);
				gux = gdx*t - gux;
				guy = gdy*t - guy;
				guo = gdo*t - guo;
			}
			if (wal->cstat&256) { gvx = -gvx; gvy = -gvy; gvo = -gvo; } //yflip
			pow2xsplit = 1; domost(x0,-10000,x1,-10000);
		}

		if (nextsectnum >= 0)
			if ((!(gotsector[nextsectnum>>3]&pow2char[nextsectnum&7])) && (testvisiblemost(x0,x1)))
				polymost_scansector(nextsectnum);
	}
}

/*
================
polymost_bunchfront

Compare wall bunches only where their projected x intervals overlap.
Return -1 for no overlap; otherwise use the classic wall-front test.
================
*/
static int32_t polymost_bunchfront (int32_t b1, int32_t b2)
{
	double x1b1, x1b2, x2b1, x2b2;
	int32_t b1f, b2f, i;

	b1f = bunchfirst[b1]; x1b1 = dxb1[b1f]; x2b2 = dxb2[bunchlast[b2]]; if (x1b1 >= x2b2) return(-1);
	b2f = bunchfirst[b2]; x1b2 = dxb1[b2f]; x2b1 = dxb2[bunchlast[b1]]; if (x1b2 >= x2b1) return(-1);

	if (x1b1 >= x1b2)
	{
		for(i=b2f;dxb2[i]<=x1b1;i=p2[i]);
		return(wallfront(b1f,i));
	}
	for(i=b1f;dxb2[i]<=x1b2;i=p2[i]);
	return(wallfront(i,b2f));
}

/*
================
polymost_scansector

Project facing walls into linked bunches and collect the sector's sprites.
Portals close to the eye are queued before projection can discard their walls.
================
*/
static void polymost_scansector (int32_t sectnum)
{
	double d, xp1, yp1, xp2, yp2;
	walltype *wal, *wal2;
	spritetype *spr;
	int32_t z, zz, startwall, endwall, numscansbefore, scanfirst, bunchfrst, nextsectnum;
	int32_t xs, ys, x1, y1, x2, y2;

	if (sectnum < 0) return;
	if (automapping) show2dsector[sectnum>>3] |= pow2char[sectnum&7];

	sectorborder[0] = sectnum, sectorbordercnt = 1;
	do
	{
		sectnum = sectorborder[--sectorbordercnt];

		for(z=headspritesect[sectnum];z>=0;z=nextspritesect[z])
		{
			spr = &sprite[z];
			if ((((spr->cstat&0x8000) == 0) || (showinvisibility)) &&
				  (spr->xrepeat > 0) && (spr->yrepeat > 0) &&
				  (spritesortcnt < MAXSPRITESONSCREEN))
			{
				xs = spr->x-globalposx; ys = spr->y-globalposy;
				if ((spr->cstat&48) || (xs*gcosang+ys*gsinang > 0))
				{
					copybufbyte(spr,&tsprite[spritesortcnt],sizeof(spritetype));
					tsprite[spritesortcnt++].owner = z;
				}
			}
		}

		gotsector[sectnum>>3] |= pow2char[sectnum&7];

		bunchfrst = numbunches;
		numscansbefore = numscans;

		startwall = sector[sectnum].wallptr; endwall = sector[sectnum].wallnum+startwall;
		scanfirst = numscans;
		xp2 = 0; yp2 = 0;
		for(z=startwall,wal=&wall[z];z<endwall;z++,wal++)
		{
			wal2 = &wall[wal->point2];
			x1 = wal->x-globalposx; y1 = wal->y-globalposy;
			x2 = wal2->x-globalposx; y2 = wal2->y-globalposy;

			nextsectnum = wal->nextsector; //Scan close sectors
			if ((nextsectnum >= 0) && (!(wal->cstat&32)) && (!(gotsector[nextsectnum>>3]&pow2char[nextsectnum&7])))
			{
				d = (double)x1*(double)y2 - (double)x2*(double)y1; xp1 = (double)(x2-x1); yp1 = (double)(y2-y1);
				/* Squared distance to the wall line: no square root or projection divide. */
				if (d*d <= (xp1*xp1 + yp1*yp1)*(SCISDIST*SCISDIST*260.0))
					sectorborder[sectorbordercnt++] = nextsectnum;
			}

			if ((z == startwall) || (wall[z-1].point2 != z))
			{
				xp1 = ((double)y1*(double)cosglobalang             - (double)x1*(double)singlobalang            )/64.0;
				yp1 = ((double)x1*(double)cosviewingrangeglobalang + (double)y1*(double)sinviewingrangeglobalang)/64.0;
			}
			else { xp1 = xp2; yp1 = yp2; }
			xp2 = ((double)y2*(double)cosglobalang             - (double)x2*(double)singlobalang            )/64.0;
			yp2 = ((double)x2*(double)cosviewingrangeglobalang + (double)y2*(double)sinviewingrangeglobalang)/64.0;
			if ((yp1 >= SCISDIST) || (yp2 >= SCISDIST))
				if ((double)xp1*(double)yp2 < (double)xp2*(double)yp1) //if wall is facing you...
				{
					if (yp1 >= SCISDIST)
						  dxb1[numscans] = (double)xp1*ghalfx/(double)yp1 + ghalfx;
					else dxb1[numscans] = -1e32;

					if (yp2 >= SCISDIST)
						  dxb2[numscans] = (double)xp2*ghalfx/(double)yp2 + ghalfx;
					else dxb2[numscans] = 1e32;

					if (dxb1[numscans] < dxb2[numscans])
						{ thesector[numscans] = sectnum; thewall[numscans] = z; p2[numscans] = numscans+1; numscans++; }
				}

			if ((wall[z].point2 < z) && (scanfirst < numscans))
				{ p2[numscans-1] = scanfirst; scanfirst = numscans; }
		}

		for(z=numscansbefore;z<numscans;z++)
			if ((wall[thewall[z]].point2 != thewall[p2[z]]) || (dxb2[z] > dxb1[p2[z]]))
				{ bunchfirst[numbunches++] = p2[z]; p2[z] = -1; }

		for(z=bunchfrst;z<numbunches;z++)
		{
			for(zz=bunchfirst[z];p2[zz]>=0;zz=p2[zz]);
			bunchlast[z] = zz;
		}
	} while (sectorbordercnt > 0);
}

/*
================
polymost_drawrooms

Set up the camera and draw the opaque world in front-to-back bunch order.
Visibility strips retain the portions of the viewport that are still open.
================
*/
void polymost_drawrooms ()
{
	int32_t i, j, k, n, n2, closest;
	double ox, oy, oz, ox2, oy2, oz2, r, px[6], py[6], pz[6], px2[6], py2[6], pz2[6], sx[6], sy[6];

	if (!rendmode) return;

	begindrawing();
	frameoffset = frameplace + windowy1*bytesperline + windowx1;


#ifdef USE_OPENGL
	polymost_gl_beginrooms();
#endif

		//Polymost supports true look up/down :) Here, we convert horizon to angle.
		//gchang&gshang are cos&sin of this angle (respectively)
	gyxscale = ((double)xdimenscale)/131072.0;
	gxyaspect = ((double)xyaspect*(double)viewingrange)*(5.0/(65536.0*262144.0));
	gviewxrange = ((double)viewingrange)*((double)xdimen)/(32768.0*1024.0);
	gcosang = ((double)cosglobalang)/262144.0;
	gsinang = ((double)singlobalang)/262144.0;
    if (viewyawfraction) {
        gcosang = cos(renderangle * (3.14159265358979323846 / 1024.0)) / 16.0;
        gsinang = sin(renderangle * (3.14159265358979323846 / 1024.0)) / 16.0;
    }
	gcosang2 = gcosang*((double)viewingrange)/65536.0;
	gsinang2 = gsinang*((double)viewingrange)/65536.0;
	ghalfx = (double)halfxdimen; grhalfxdown10 = 1.0/(((double)ghalfx)*1024);
	/* Pitch and roll use the focal length, independent of viewport width. */
	gfocalx = ghalfx*65536.0/(double)viewingrange;
	ghoriz = viewhorizonfraction ? renderhorizon : (double)globalhoriz;

		//global cos/sin height angle
	r = (double)((ydimen>>1)-ghoriz);
	gshang = r/sqrt(r*r+gfocalx*gfocalx);
	gchang = sqrt(1.0-gshang*gshang);
	ghoriz = (double)(ydimen>>1);

	  //global cos/sin tilt angle
	gctang = cos(gtang);
	gstang = sin(gtang);
	if (fabs(gstang) < .001) //This hack avoids nasty precision bugs in domost()
		{ gstang = 0; if (gctang > 0) gctang = 1.0; else gctang = -1.0; }

	if (inpreparemirror)
		gstang = -gstang;

		//Generate viewport trapezoid (for handling screen up/down)
	px[0] = px[3] = 0-1; px[1] = px[2] = windowx2+1-windowx1+2;
	py[0] = py[1] = 0-1; py[2] = py[3] = windowy2+1-windowy1+2; n = 4;
	for(i=0;i<n;i++)
	{
		ox = px[i]-ghalfx; oy = py[i]-ghoriz; oz = gfocalx;

			//Tilt rotation (backwards)
		ox2 = ox*gctang + oy*gstang;
		oy2 = oy*gctang - ox*gstang;
		oz2 = oz;

			//Up/down rotation (backwards)
		px[i] = ox2;
		py[i] = oy2*gchang + oz2*gshang;
		pz[i] = oz2*gchang - oy2*gshang;
	}

		//Clip to SCISDIST plane
	n2 = 0;
	for(i=0;i<n;i++)
	{
		j = i+1; if (j >= n) j = 0;
		if (pz[i] >= SCISDIST) { px2[n2] = px[i]; py2[n2] = py[i]; pz2[n2] = pz[i]; n2++; }
		if ((pz[i] >= SCISDIST) != (pz[j] >= SCISDIST))
		{
			r = (SCISDIST-pz[i])/(pz[j]-pz[i]);
			px2[n2] = (px[j]-px[i])*r + px[i];
			py2[n2] = (py[j]-py[i])*r + py[i];
			pz2[n2] = SCISDIST; n2++;
		}
	}
	if (n2 < 3) { enddrawing(); return; }
	for(i=0;i<n2;i++)
	{
		r = gfocalx / pz2[i];
		sx[i] = px2[i]*r + ghalfx;
		sy[i] = py2[i]*r + ghoriz;
	}
	initmosts(sx,sy,n2);

	if (searchit == 2)
	{
		short hitsect, hitwall, hitsprite;
		int32_t vx, vy, vz, hitx, hity, hitz;

		ox2 = searchx-ghalfx; oy2 = searchy-ghoriz; oz2 = gfocalx;

			//Tilt rotation
		ox = ox2*gctang + oy2*gstang;
		oy = oy2*gctang - ox2*gstang;
		oz = oz2;

			//Up/down rotation
		ox2 = oz*gchang - oy*gshang;
		oy2 = ox;
		oz2 = oy*gchang + oz*gshang;

			//Standard Left/right rotation
		vx = ox2*((float)cosglobalang) - oy2*((float)singlobalang);
		vy = ox2*((float)singlobalang) + oy2*((float)cosglobalang);
		vz = oz2*16384.0;

		hitallsprites = 1;
		hitscan(globalposx,globalposy,globalposz,globalcursectnum, //Start position
			vx>>12,vy>>12,vz>>8,&hitsect,&hitwall,&hitsprite,&hitx,&hity,&hitz,0xffff0030);
		hitallsprites = 0;

		searchsector = hitsect;
		if (hitwall >= 0)
		{
			searchwall = hitwall; searchstat = 0;
			if (wall[hitwall].nextwall >= 0)
			{
				int32_t cz, fz;
				getzsofslope(wall[hitwall].nextsector,hitx,hity,&cz,&fz);
				if (hitz > fz)
				{
					if (wall[hitwall].cstat&2) //'2' bottoms of walls
						searchwall = wall[hitwall].nextwall;
				}
				else if ((hitz > cz) && (wall[hitwall].cstat&(16+32))) //masking or 1-way
					searchstat = 4;
			}
		}
		else if (hitsprite >= 0) { searchwall = hitsprite; searchstat = 3; }
		else
		{
			int32_t cz, fz;
			getzsofslope(hitsect,hitx,hity,&cz,&fz);
			if ((hitz<<1) < cz+fz) searchstat = 1; else searchstat = 2;
			//if (vz < 0) searchstat = 1; else searchstat = 2; //Won't work for slopes :/
		}
		searchit = 0;
	}

	numscans = numbunches = 0;

	if (globalcursectnum >= MAXSECTORS)
		globalcursectnum -= MAXSECTORS;
	else
	{
		i = globalcursectnum;
		updatesector(globalposx,globalposy,&globalcursectnum);
		if (globalcursectnum < 0) globalcursectnum = i;
	}

	polymost_scansector(globalcursectnum);

	if (inpreparemirror)
	{
		grhalfxdown10x = -grhalfxdown10;
		inpreparemirror = 0;
		polymost_drawalls(0);
		numbunches--;
		bunchfirst[0] = bunchfirst[numbunches];
		bunchlast[0] = bunchlast[numbunches];
	}
	else
		grhalfxdown10x = grhalfxdown10;

	while (numbunches > 0)
	{
		memset(tempbuf,0,numbunches+3); tempbuf[0] = 1;

		closest = 0;              //Almost works, but not quite :(
		for(i=1;i<numbunches;i++)
		{
			j = polymost_bunchfront(i,closest); if (j < 0) continue;
			tempbuf[i] = 1;
			if (!j) { tempbuf[closest] = 1; closest = i; }
		}
		for(i=0;i<numbunches;i++) //Double-check
		{
			if (tempbuf[i]) continue;
			j = polymost_bunchfront(i,closest); if (j < 0) continue;
			tempbuf[i] = 1;
			if (!j) { tempbuf[closest] = 1; closest = i; i = 0; }
		}

		polymost_drawalls(closest);

		numbunches--;
		bunchfirst[closest] = bunchfirst[numbunches];
		bunchlast[closest] = bunchlast[numbunches];
	}
#ifdef USE_OPENGL
	polymost_gl_endrooms();
#endif

	enddrawing();
}

/*
================
polymost_drawmaskwall

Project a portal's masked wall within the opening of its two sectors.
Clip the resulting polygon before handing it to the shared texture mapper.
================
*/
void polymost_drawmaskwall (int32_t damaskwallcnt)
{
	double dpx[8], dpy[8], dpx2[8], dpy2[8];
	float fx, fy, x0, x1, sx0, sy0, sx1, sy1, xp0, yp0, xp1, yp1, oxp0, oyp0, ryp0, ryp1;
	float f, r, t, t0, t1, nx0, ny0, nx1, ny1, py[4], csy[4], fsy[4];
	int32_t i, j, k, n, n2, x, z, sectnum, z1, z2, lx, rx, cz[4], fz[4], method;
	sectortype *sec, *nsec;
	walltype *wal, *wal2;

	z = maskwall[damaskwallcnt];
	wal = &wall[thewall[z]]; wal2 = &wall[wal->point2];
	sectnum = thesector[z]; sec = &sector[sectnum];
	nsec = &sector[wal->nextsector];
	z1 = max(nsec->ceilingz,sec->ceilingz);
	z2 = min(nsec->floorz,sec->floorz);

	globalpicnum = wal->overpicnum; if ((uint32_t)globalpicnum >= MAXTILES) globalpicnum = 0;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,(short)thewall[z]+16384);
	globalshade = (int32_t)wal->shade;
	globalpal = (int32_t)((unsigned char)wal->pal);
	globalorientation = (int32_t)wal->cstat;

	sx0 = (float)(wal->x-globalposx); sx1 = (float)(wal2->x-globalposx);
	sy0 = (float)(wal->y-globalposy); sy1 = (float)(wal2->y-globalposy);
	yp0 = sx0*gcosang2 + sy0*gsinang2;
	yp1 = sx1*gcosang2 + sy1*gsinang2;
	if ((yp0 < SCISDIST) && (yp1 < SCISDIST)) return;
	xp0 = sy0*gcosang - sx0*gsinang;
	xp1 = sy1*gcosang - sx1*gsinang;

		//Clip to close parallel-screen plane
	oxp0 = xp0; oyp0 = yp0;
	if (yp0 < SCISDIST) { t0 = (SCISDIST-yp0)/(yp1-yp0); xp0 = (xp1-xp0)*t0+xp0; yp0 = SCISDIST; }
						  else t0 = 0.f;
	if (yp1 < SCISDIST) { t1 = (SCISDIST-oyp0)/(yp1-oyp0); xp1 = (xp1-oxp0)*t1+oxp0; yp1 = SCISDIST; }
						else { t1 = 1.f; }

	getzsofslope(sectnum,(wal2->x-wal->x)*t0+wal->x,(wal2->y-wal->y)*t0+wal->y,&cz[0],&fz[0]);
	getzsofslope(wal->nextsector,(wal2->x-wal->x)*t0+wal->x,(wal2->y-wal->y)*t0+wal->y,&cz[1],&fz[1]);
	getzsofslope(sectnum,(wal2->x-wal->x)*t1+wal->x,(wal2->y-wal->y)*t1+wal->y,&cz[2],&fz[2]);
	getzsofslope(wal->nextsector,(wal2->x-wal->x)*t1+wal->x,(wal2->y-wal->y)*t1+wal->y,&cz[3],&fz[3]);

	ryp0 = 1.f/yp0; ryp1 = 1.f/yp1;

		//Generate screen coordinates for front side of wall
	x0 = ghalfx*xp0*ryp0 + ghalfx;
	x1 = ghalfx*xp1*ryp1 + ghalfx;
	if (x1 <= x0) return;

	ryp0 *= gyxscale; ryp1 *= gyxscale;

	gdx = (ryp0-ryp1)*gxyaspect / (x0-x1);
	gdy = 0;
	gdo = ryp0*gxyaspect - gdx*x0;

		//gux*x0 + guo = t0*wal->xrepeat*8*yp0
		//gux*x1 + guo = t1*wal->xrepeat*8*yp1
	gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)wal->xrepeat*8.f / (x0-x1);
	guo = t0*ryp0*gxyaspect*(float)wal->xrepeat*8.f - gux*x0;
	guo += (float)wal->xpanning*gdo;
	gux += (float)wal->xpanning*gdx;
	guy = 0;

	if (!(wal->cstat&4)) i = z1; else i = z2;
	i -= globalposz;
	t0 = ((float)i)*ryp0 + ghoriz;
	t1 = ((float)i)*ryp1 + ghoriz;
	t = ((gdx*x0 + gdo) * (float)wal->yrepeat) / ((x1-x0) * ryp0 * 2048.f);
	i = (1<<(picsiz[globalpicnum]>>4)); if (i < tilesizy[globalpicnum]) i <<= 1;
	fy = (float)wal->ypanning * ((float)i) / 256.0;
	gvx = (t0-t1)*t;
	gvy = (x1-x0)*t;
	gvo = -gvx*x0 - gvy*t0 + fy*gdo; gvx += fy*gdx; gvy += fy*gdy;

	if (wal->cstat&8) //xflip
	{
		t = (float)(wal->xrepeat*8 + wal->xpanning*2);
		gux = gdx*t - gux;
		guy = gdy*t - guy;
		guo = gdo*t - guo;
	}
	if (wal->cstat&256) { gvx = -gvx; gvy = -gvy; gvo = -gvo; } //yflip

	method = 1; pow2xsplit = 1;
	if (wal->cstat&128) { if (!(wal->cstat&512)) method = 2; else method = 3; }

#ifdef USE_OPENGL
	polymost_gl_beginmasked(sectnum);
#endif

	for(i=0;i<2;i++)
	{
		csy[i] = ((float)(cz[i]-globalposz))*ryp0 + ghoriz;
		fsy[i] = ((float)(fz[i]-globalposz))*ryp0 + ghoriz;
		csy[i+2] = ((float)(cz[i+2]-globalposz))*ryp1 + ghoriz;
		fsy[i+2] = ((float)(fz[i+2]-globalposz))*ryp1 + ghoriz;
	}

		//Clip 2 quadrilaterals
		//               /csy3
		//             /   |
		// csy0------/----csy2
		//   |     /xxxxxxx|
		//   |   /xxxxxxxxx|
		// csy1/xxxxxxxxxxx|
		//   |xxxxxxxxxxx/fsy3
		//   |xxxxxxxxx/   |
		//   |xxxxxxx/     |
		// fsy0----/------fsy2
		//   |   /
		// fsy1/

	dpx[0] = x0; dpy[0] = csy[1];
	dpx[1] = x1; dpy[1] = csy[3];
	dpx[2] = x1; dpy[2] = fsy[3];
	dpx[3] = x0; dpy[3] = fsy[1];
	n = 4;

		//Clip to (x0,csy[0])-(x1,csy[2])
	n2 = 0; t1 = -((dpx[0]-x0)*(csy[2]-csy[0]) - (dpy[0]-csy[0])*(x1-x0));
	for(i=0;i<n;i++)
	{
		j = i+1; if (j >= n) j = 0;

		t0 = t1; t1 = -((dpx[j]-x0)*(csy[2]-csy[0]) - (dpy[j]-csy[0])*(x1-x0));
		if (t0 >= 0) { dpx2[n2] = dpx[i]; dpy2[n2] = dpy[i]; n2++; }
		if ((t0 >= 0) != (t1 >= 0))
		{
			r = t0/(t0-t1);
			dpx2[n2] = (dpx[j]-dpx[i])*r + dpx[i];
			dpy2[n2] = (dpy[j]-dpy[i])*r + dpy[i];
			n2++;
		}
	}
	if (n2 < 3) return;

		//Clip to (x1,fsy[2])-(x0,fsy[0])
	n = 0; t1 = -((dpx2[0]-x1)*(fsy[0]-fsy[2]) - (dpy2[0]-fsy[2])*(x0-x1));
	for(i=0;i<n2;i++)
	{
		j = i+1; if (j >= n2) j = 0;

		t0 = t1; t1 = -((dpx2[j]-x1)*(fsy[0]-fsy[2]) - (dpy2[j]-fsy[2])*(x0-x1));
		if (t0 >= 0) { dpx[n] = dpx2[i]; dpy[n] = dpy2[i]; n++; }
		if ((t0 >= 0) != (t1 >= 0))
		{
			r = t0/(t0-t1);
			dpx[n] = (dpx2[j]-dpx2[i])*r + dpx2[i];
			dpy[n] = (dpy2[j]-dpy2[i])*r + dpy2[i];
			n++;
		}
	}
	if (n < 3) return;

	drawpoly(dpx,dpy,n,method|METH_LAYERS);
}

	//Of the depths at which the rays through the columns xl..xr (xp at depth yp)
	//   of a face sprite's picture cross the wall a..b (projected), the least
	//   (most 0) or the greatest (most 1); -1 if none crosses it in front of the
	//   eye. Both are at the picture's ends or at the wall's ends.
static float polymost_wallcross (float ax, float ay, float bx, float by, float yp, float xl, float xr, int32_t most)
{
	float dx = bx-ax, dy = by-ay, s[4], den, lam, u, d = -1.f;
	int32_t k;

	s[0] = xl; s[1] = xr; s[2] = s[3] = xl;
	if (ay > SCISDIST) s[2] = ax*yp/ay;
	if (by > SCISDIST) s[3] = bx*yp/by;
	for(k=0;k<4;k++)
	{
		if ((s[k] < xl) || (s[k] > xr)) continue;
		den = s[k]*dy - yp*dx; if (fabs(den) < 1e-6) continue;
		lam = (ax*dy - ay*dx)/den;
		u = (ax*yp - ay*s[k])/den;
		if ((u < -1e-4) || (u > 1.0001) || (lam <= 0.f)) continue;
		if ((d < 0.f) || ((lam*yp > d) == (most != 0))) d = lam*yp;
	}
	return(d);
}

	//Classic clips a face sprite only by the walls in front of it (drawsprite's
	//   spritewallfront test), so one lying against a wall it's in front of draws
	//   whole, its flat picture reaching through the wall. Polymost draws it all at
	//   its middle's depth, where the z-buffer cuts it on that wall. This is the
	//   depth (polymost's, as yp) it has to come to over its columns xl..xr (xp at
	//   yp) to stay in front of each such wall of its sector or the ones next to it
	//   (facing the eye, it in front, an end beyond it: classic doesn't clip by it),
	//   but no nearer than any wall drawn this frame that classic does clip it by
	//   (facing the eye, it behind, an end nearer; a red wall only with a step,
	//   up or down, as its sector's floor or ceiling edge hides what's past it) so
	//   that those still hide it; yp if it needn't or can't come nearer.
static int spritewallfront (spritetype *s, int32_t w);
static float polymost_spritenear (spritetype *tspr, float yp, float xl, float xr)
{
	float a[4], d, nearest = yp;
	int32_t i, w, endw, sect, pass;

	for(pass=0;pass<2;pass++)
		for(i=(pass?0:-1);i<(pass?numsectors:sector[tspr->sectnum].wallnum);i++)
		{
			if (pass)
			{
				if (nearest >= yp) return(yp); //nothing to come in front of
				sect = i;
				if (!(gotsector[sect>>3]&pow2char[sect&7])) continue;
			}
			else
			{
				sect = tspr->sectnum;
				if (i >= 0) { sect = wall[sector[sect].wallptr+i].nextsector; if (sect < 0) continue; }
			}
			for(w=sector[sect].wallptr,endw=w+sector[sect].wallnum;w<endw;w++)
			{
				if (spritewallfront(tspr,w) == pass) continue;
				if ((pass) && (wall[w].nextsector >= 0) &&
					 (sector[wall[w].nextsector].floorz == sector[sect].floorz) &&
					 (sector[wall[w].nextsector].ceilingz == sector[sect].ceilingz) &&
					 (!((sector[wall[w].nextsector].floorstat|sector[sect].floorstat|
						 sector[wall[w].nextsector].ceilingstat|sector[sect].ceilingstat)&2)))
					continue; //no step either way, flat: hides nothing
				a[0] = (float)(wall[w].x-globalposx); a[1] = (float)(wall[w].y-globalposy);
				if ((float)(wall[wall[w].point2].x-wall[w].x)*-a[1] < (float)(wall[wall[w].point2].y-wall[w].y)*-a[0])
					continue; //its back to the eye: not drawn
				a[2] = (float)(wall[wall[w].point2].x-globalposx); a[3] = (float)(wall[wall[w].point2].y-globalposy);
				d = a[1]*gcosang - a[0]*gsinang; a[1] = a[0]*gcosang2 + a[1]*gsinang2; a[0] = d;
				d = a[3]*gcosang - a[2]*gsinang; a[3] = a[2]*gcosang2 + a[3]*gsinang2; a[2] = d;
				if (!pass)
				{
					if ((a[1] >= yp) == (a[3] >= yp)) continue; //all nearer (classic clips) or all beyond
					d = polymost_wallcross(a[0],a[1],a[2],a[3],yp,xl,xr,0);
					if ((d > 0.f) && (d < nearest)) nearest = d;
				}
				else
				{
					if ((a[1] >= yp) && (a[3] >= yp)) continue; //all beyond: classic doesn't clip by it
					d = polymost_wallcross(a[0],a[1],a[2],a[3],yp,xl,xr,1)*1.02f; //behind it, past the masks' depth range
					if (d > nearest) nearest = d;
					if (nearest >= yp) return(yp);
				}
			}
		}
		//No nearer than a quarter of its depth, nor than 8 (128 map units): the
		//   GL near plane is at about 2*xdimen/ydimen
	return(max(nearest,max(yp*.25f,8.f)));
}

/*
================
polymost_drawsprite

Project face, wall and floor sprites through the shared polygon mapper.
Orientation and cstat choose the geometry, clipping and blend method.
================
*/
void polymost_drawsprite (int32_t snum)
{
	double px[6], py[6];
	float f, r, c, s, fx, fy, sx0, sy0, sx1, sy1, xp0, yp0, xp1, yp1, oxp0, oyp0, ryp0, ryp1, ft[4];
	float x0, y0, x1, y1, sc0, sf0, sc1, sf1, px2[6], py2[6], xv, yv, t0, t1, fogshades = 0.f;
	int32_t i, j, spritenum, xoff, yoff, method, npoints, nearer;
	spritetype *tspr;

	tspr = tspriteptr[snum];
	globalpicnum      = tspr->picnum;
	globalshade       = tspr->shade;
	globalpal         = tspr->pal;
	globalorientation = tspr->cstat;
	spritenum         = tspr->owner;
	if (picanm[globalpicnum]&192) globalpicnum += animateoffs(globalpicnum,spritenum+32768);

	xoff = (int32_t)((signed char)((picanm[globalpicnum]>>8)&255))+((int32_t)tspr->xoffset);
	yoff = (int32_t)((signed char)((picanm[globalpicnum]>>16)&255))+((int32_t)tspr->yoffset);

	method = 1+4;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) method = 2+4; else method = 3+4; }

#ifdef USE_OPENGL
	polymost_gl_beginmasked(tspr->sectnum);
#endif

	switch((globalorientation>>4)&3)
	{
		case 0: //Face sprite

				//Project 3D to 2D
			sx0 = (float)(tspr->x-globalposx);
			sy0 = (float)(tspr->y-globalposy);
			xp0 = sy0*gcosang  - sx0*gsinang;
			yp0 = sx0*gcosang2 + sy0*gsinang2;
			if (yp0 <= SCISDIST) return;
			ryp0 = 1/yp0;
			sx0 = ghalfx*xp0*ryp0 + ghalfx;
			sy0 = ((float)(tspr->z-globalposz))*gyxscale*ryp0 + ghoriz;

			f = ryp0*(float)xdimen/160.0;
			fx = ((float)tspr->xrepeat)*f;
			fy = ((float)tspr->yrepeat)*f*((float)yxaspect/65536.0);
			sx0 -= fx*(float)xoff; if (tilesizx[globalpicnum]&1) sx0 += fx*.5;
			sy0 -= fy*(float)yoff;
			fx *= ((float)tilesizx[globalpicnum]);
			fy *= ((float)tilesizy[globalpicnum]);

			px[0] = px[3] = sx0-fx*.5; px[1] = px[2] = sx0+fx*.5;
			if (!(globalorientation&128)) { py[0] = py[1] = sy0-fy; py[2] = py[3] = sy0; }
											 else { py[0] = py[1] = sy0-fy*.5; py[2] = py[3] = sy0+fy*.5; }

			gdx = gdy = guy = gvx = 0; gdo = ryp0*gviewxrange;
			if (!(globalorientation&4))
				  { gux = (float)tilesizx[globalpicnum]*gdo/(px[1]-px[0]+.002); guo = -gux*(px[0]-.001); }
			else { gux = (float)tilesizx[globalpicnum]*gdo/(px[0]-px[1]-.002); guo = -gux*(px[1]+.001); }
			if (!(globalorientation&8))
				  { gvy = (float)tilesizy[globalpicnum]*gdo/(py[3]-py[0]+.002); gvo = -gvy*(py[0]-.001); }
			else { gvy = (float)tilesizy[globalpicnum]*gdo/(py[0]-py[3]-.002); gvo = -gvy*(py[3]+.001); }

			//Clip sprites to ceilings/floors when no parallaxing and not sloped
			if (!(sector[tspr->sectnum].ceilingstat&3))
			{
				sy0 = ((float)(sector[tspr->sectnum].ceilingz-globalposz))*gyxscale*ryp0 + ghoriz;
				if (py[0] < sy0) py[0] = py[1] = sy0;
			}
			if (!(sector[tspr->sectnum].floorstat&3))
			{
				sy0 = ((float)(sector[tspr->sectnum].floorz-globalposz))*gyxscale*ryp0 + ghoriz;
				if (py[2] > sy0) py[2] = py[3] = sy0;
			}

				//In front of the walls behind it, as classic: nearer, the same picture
				//   (u, v and the depth scale together) and shade (its fog's shades per
				//   unit of depth scale back), not writing the depth it's moved to
			f = polymost_spritenear(tspr,yp0,(px[0]-ghalfx)*yp0/ghalfx,(px[1]-ghalfx)*yp0/ghalfx);
			nearer = (f < yp0);
			if (nearer)
			{
				f = yp0/f;
				gdo *= f; gux *= f; guo *= f; gvy *= f; gvo *= f;
				zbufwrite = 0;
#ifdef USE_OPENGL
				fogshades = polymost_gl_nearer_begin(f);
#endif
			}

			pow2xsplit = 0; drawpoly(px,py,4,method|METH_LAYERS);
			zbufwrite = 1;
#ifdef USE_OPENGL
	polymost_gl_nearer_end(nearer, fogshades);
#endif
			break;
		case 1: //Wall sprite
				//Project 3D to 2D
			if (globalorientation&4) xoff = -xoff;
			if (globalorientation&8) yoff = -yoff;

			xv = (float)tspr->xrepeat * (float)sintable[(tspr->ang     )&2047] / 65536.0;
			yv = (float)tspr->xrepeat * (float)sintable[(tspr->ang+1536)&2047] / 65536.0;
			f = (float)(tilesizx[globalpicnum]>>1) + (float)xoff;
			x0 = (float)(tspr->x-globalposx) - xv*f; x1 = xv*(float)tilesizx[globalpicnum] + x0;
			y0 = (float)(tspr->y-globalposy) - yv*f; y1 = yv*(float)tilesizx[globalpicnum] + y0;

			yp0 = x0*gcosang2 + y0*gsinang2;
			yp1 = x1*gcosang2 + y1*gsinang2;
			if ((yp0 <= SCISDIST) && (yp1 <= SCISDIST)) return;
			xp0 = y0*gcosang - x0*gsinang;
			xp1 = y1*gcosang - x1*gsinang;

				//Clip to close parallel-screen plane
			oxp0 = xp0; oyp0 = yp0;
			if (yp0 < SCISDIST) { t0 = (SCISDIST-yp0)/(yp1-yp0); xp0 = (xp1-xp0)*t0+xp0; yp0 = SCISDIST; }
								else { t0 = 0.f; }
			if (yp1 < SCISDIST) { t1 = (SCISDIST-oyp0)/(yp1-oyp0); xp1 = (xp1-oxp0)*t1+oxp0; yp1 = SCISDIST; }
								else { t1 = 1.f; }

			f = ((float)tspr->yrepeat) * (float)tilesizy[globalpicnum] * 4;

			ryp0 = 1.0/yp0;
			ryp1 = 1.0/yp1;
			sx0 = ghalfx*xp0*ryp0 + ghalfx;
			sx1 = ghalfx*xp1*ryp1 + ghalfx;
			ryp0 *= gyxscale;
			ryp1 *= gyxscale;

			tspr->z -= ((yoff*tspr->yrepeat)<<2);
			if (globalorientation&128)
			{
				tspr->z += ((tilesizy[globalpicnum]*tspr->yrepeat)<<1);
				if (tilesizy[globalpicnum]&1) tspr->z += (tspr->yrepeat<<1); //Odd yspans
			}

			sc0 = ((float)(tspr->z-globalposz-f))*ryp0 + ghoriz;
			sc1 = ((float)(tspr->z-globalposz-f))*ryp1 + ghoriz;
			sf0 = ((float)(tspr->z-globalposz))*ryp0 + ghoriz;
			sf1 = ((float)(tspr->z-globalposz))*ryp1 + ghoriz;

			gdx = (ryp0-ryp1)*gxyaspect / (sx0-sx1);
			gdy = 0;
			gdo = ryp0*gxyaspect - gdx*sx0;

				//Original equations:
				//(gux*sx0 + guo)/(gdx*sx1 + gdo) = tilesizx[globalpicnum]*t0
				//(gux*sx1 + guo)/(gdx*sx1 + gdo) = tilesizx[globalpicnum]*t1
				//
				// gvx*sx0 + gvy*sc0 + gvo = 0
				// gvy*sx1 + gvy*sc1 + gvo = 0
				//(gvx*sx0 + gvy*sf0 + gvo)/(gdx*sx0 + gdo) = tilesizy[globalpicnum]
				//(gvx*sx1 + gvy*sf1 + gvo)/(gdx*sx1 + gdo) = tilesizy[globalpicnum]

				//gux*sx0 + guo = t0*tilesizx[globalpicnum]*yp0
				//gux*sx1 + guo = t1*tilesizx[globalpicnum]*yp1
			if (globalorientation&4) { t0 = 1.f-t0; t1 = 1.f-t1; }
			gux = (t0*ryp0 - t1*ryp1)*gxyaspect*(float)tilesizx[globalpicnum] / (sx0-sx1);
			guy = 0;
			guo = t0*ryp0*gxyaspect*(float)tilesizx[globalpicnum] - gux*sx0;

				//gvx*sx0 + gvy*sc0 + gvo = 0
				//gvx*sx1 + gvy*sc1 + gvo = 0
				//gvx*sx0 + gvy*sf0 + gvo = tilesizy[globalpicnum]*(gdx*sx0 + gdo)
			f = ((float)tilesizy[globalpicnum])*(gdx*sx0 + gdo) / ((sx0-sx1)*(sc0-sf0));
			if (!(globalorientation&8))
			{
				gvx = (sc0-sc1)*f;
				gvy = (sx1-sx0)*f;
				gvo = -gvx*sx0 - gvy*sc0;
			}
			else
			{
				gvx = (sf1-sf0)*f;
				gvy = (sx0-sx1)*f;
				gvo = -gvx*sx0 - gvy*sf0;
			}

			//Clip sprites to ceilings/floors when no parallaxing
			if (!(sector[tspr->sectnum].ceilingstat&1))
			{
				f = ((float)tspr->yrepeat) * (float)tilesizy[globalpicnum] * 4;
				if (sector[tspr->sectnum].ceilingz > tspr->z-f)
				{
					sc0 = ((float)(sector[tspr->sectnum].ceilingz-globalposz))*ryp0 + ghoriz;
					sc1 = ((float)(sector[tspr->sectnum].ceilingz-globalposz))*ryp1 + ghoriz;
				}
			}
			if (!(sector[tspr->sectnum].floorstat&1))
			{
				if (sector[tspr->sectnum].floorz < tspr->z)
				{
					sf0 = ((float)(sector[tspr->sectnum].floorz-globalposz))*ryp0 + ghoriz;
					sf1 = ((float)(sector[tspr->sectnum].floorz-globalposz))*ryp1 + ghoriz;
				}
			}
			
			if (sx0 > sx1)
			{
				if (globalorientation&64) return; //1-sided sprite
				f = sx0; sx0 = sx1; sx1 = f;
				f = sc0; sc0 = sc1; sc1 = f;
				f = sf0; sf0 = sf1; sf1 = f;
			}

			px[0] = sx0; py[0] = sc0;
			px[1] = sx1; py[1] = sc1;
			px[2] = sx1; py[2] = sf1;
			px[3] = sx0; py[3] = sf0;
			pow2xsplit = 0; drawpoly(px,py,4,method|METH_LAYERS);
			break;
		case 2: //Floor sprite
			if ((globalorientation&64) != 0)
				if ((globalposz > tspr->z) == (!(globalorientation&8)))
					return;
			if ((globalorientation&4) > 0) xoff = -xoff;
			if ((globalorientation&8) > 0) yoff = -yoff;

			i = (tspr->ang&2047);
			c = sintable[(i+512)&2047]/65536.0;
			s = sintable[i]/65536.0;
			x0 = ((tilesizx[globalpicnum]>>1)-xoff)*tspr->xrepeat;
			y0 = ((tilesizy[globalpicnum]>>1)-yoff)*tspr->yrepeat;
			x1 = ((tilesizx[globalpicnum]>>1)+xoff)*tspr->xrepeat;
			y1 = ((tilesizy[globalpicnum]>>1)+yoff)*tspr->yrepeat;

				//Project 3D to 2D
			for(j=0;j<4;j++)
			{
				sx0 = (float)(tspr->x-globalposx);
				sy0 = (float)(tspr->y-globalposy);
				if ((j+0)&2) { sy0 -= s*y0; sx0 -= c*y0; } else { sy0 += s*y1; sx0 += c*y1; }
				if ((j+1)&2) { sx0 -= s*x0; sy0 += c*x0; } else { sx0 += s*x1; sy0 -= c*x1; }
				px[j] = sy0*gcosang  - sx0*gsinang;
				py[j] = sx0*gcosang2 + sy0*gsinang2;
			}

			if (tspr->z < globalposz) //if floor sprite is above you, reverse order of points
			{
				f = px[0]; px[0] = px[1]; px[1] = f;
				f = py[0]; py[0] = py[1]; py[1] = f;
				f = px[2]; px[2] = px[3]; px[3] = f;
				f = py[2]; py[2] = py[3]; py[3] = f;
			}

				//Clip to SCISDIST plane
			npoints = 0;
			for(i=0;i<4;i++)
			{
				j = ((i+1)&3);
				if (py[i] >= SCISDIST) { px2[npoints] = px[i]; py2[npoints] = py[i]; npoints++; }
				if ((py[i] >= SCISDIST) != (py[j] >= SCISDIST))
				{
					f = (SCISDIST-py[i])/(py[j]-py[i]);
					px2[npoints] = (px[j]-px[i])*f + px[i];
					py2[npoints] = (py[j]-py[i])*f + py[i]; npoints++;
				}
			}
			if (npoints < 3) return;

				//Project rotated 3D points to screen
			f = ((float)(tspr->z-globalposz))*gyxscale;
			for(j=0;j<npoints;j++)
			{
				ryp0 = 1/py2[j];
				px[j] = ghalfx*px2[j]*ryp0 + ghalfx;
				py[j] = f*ryp0 + ghoriz;
			}

				//gd? Copied from floor rendering code
			gdx = 0;
			gdy = gxyaspect / (double)(tspr->z-globalposz);
			gdo = -ghoriz*gdy;
				//copied&modified from relative alignment
			xv = (float)tspr->x + s*x1 + c*y1; fx = (double)-(x0+x1)*s;
			yv = (float)tspr->y + s*y1 - c*x1; fy = (double)+(x0+x1)*c;
			f = 1.0/sqrt(fx*fx+fy*fy); fx *= f; fy *= f;
			ft[2] = singlobalang*fy + cosglobalang*fx;
			ft[3] = singlobalang*fx - cosglobalang*fy;
			ft[0] = ((double)(globalposy-yv))*fy + ((double)(globalposx-xv))*fx;
			ft[1] = ((double)(globalposx-xv))*fy - ((double)(globalposy-yv))*fx;
			gux = (double)ft[3]*((double)viewingrange)/(-65536.0*262144.0);
			gvx = (double)ft[2]*((double)viewingrange)/(-65536.0*262144.0);
			guy = (double)ft[0]*gdy; gvy = (double)ft[1]*gdy;
			guo = (double)ft[0]*gdo; gvo = (double)ft[1]*gdo;
			guo += (double)(ft[2]/262144.0-gux)*ghalfx;
			gvo -= (double)(ft[3]/262144.0+gvx)*ghalfx;
			f = 4.0/(float)tspr->xrepeat; gux *= f; guy *= f; guo *= f;
			f =-4.0/(float)tspr->yrepeat; gvx *= f; gvy *= f; gvo *= f;
			if (globalorientation&4)
			{
				gux = ((float)tilesizx[globalpicnum])*gdx - gux;
				guy = ((float)tilesizx[globalpicnum])*gdy - guy;
				guo = ((float)tilesizx[globalpicnum])*gdo - guo;
			}

			pow2xsplit = 0; drawpoly(px,py,npoints,method|METH_LAYERS);
			break;

		//case 3: //Voxel sprite
		//   break;
	}
}

	//sx,sy       center of sprite; screen coods*65536
	//z           zoom*65536. > is zoomed in
	//a           angle (0 is default)
	//dastat&1    1:translucence
	//dastat&2    1:auto-scale mode (use 320*200 coordinates)
	//dastat&4    1:y-flip
	//dastat&8    1:don't clip to startumost/startdmost
	//dastat&16   1:force point passed to be top-left corner, 0:Editart center
	//dastat&32   1:reverse translucence
	//dastat&64   1:non-masked, 0:masked
	//dastat&128  1:draw all pages (permanent)
	//dastat&256  1:one-to-one pixel aspect (when dastat&2)
	//cx1,...     clip window (actual screen coords)
/*
================
polymost_dorotatesprite

Draw a screen-space tile with the shared polygon mapper.
Save and restore the world camera state so HUD draws do not alter the 3D view.
================
*/
void polymost_dorotatesprite (int32_t sx, int32_t sy, int32_t z, short a, short picnum,
	signed char dashade, unsigned char dapalnum, unsigned short dastat, int32_t cx1, int32_t cy1, int32_t cx2, int32_t cy2)
{
	int32_t i, n, nn, x, zz, xoff, yoff, xsiz, ysiz, method;
	int32_t rsxyaspect = xyaspect, rsyxaspect = yxaspect;
	int32_t ogpicnum, ogshade, ogpal, oxdimen, oydimen;
	intptr_t ofoffset;
	double ogchang, ogshang, ogctang, ogstang, oghalfx, oghoriz, fx, fy, x1, y1, x2, y2;
	double ogrhalfxdown10, ogrhalfxdown10x;
	double d, cosang, sinang, cosang2, sinang2, px[8], py[8], px2[8], py2[8];

	if (widescreen && (dastat&2) && (dastat&512)) {
		rsyxaspect = divscale16(ydim*320,xdim*200);
		rsxyaspect = divscale32(1,rsyxaspect);
	}

	ogpicnum = globalpicnum; globalpicnum = picnum;
	ogshade  = globalshade;  globalshade  = dashade;
	ogpal    = globalpal;    globalpal    = (int32_t)((unsigned char)dapalnum);
	oghalfx  = ghalfx;       ghalfx       = (double)(xdim>>1);
	ogrhalfxdown10 = grhalfxdown10;    grhalfxdown10 = 1.0/(((double)ghalfx)*1024);
	ogrhalfxdown10x = grhalfxdown10x;  grhalfxdown10x = grhalfxdown10;
	oghoriz  = ghoriz;       ghoriz       = (double)(ydim>>1);
	ofoffset = frameoffset;  frameoffset  = frameplace;
	oxdimen  = xdimen;       xdimen       = xdim;
	oydimen  = ydimen;       ydimen       = ydim;
	ogchang = gchang; gchang = 1.0;
	ogshang = gshang; gshang = 0.0;
	ogctang = gctang; gctang = 1.0;
	ogstang = gstang; gstang = 0.0;

#ifdef USE_OPENGL
	polymost_gl_beginhud();
#endif

	method = 0;
	if (!(dastat&64))
	{
		method = 1;
		if (dastat&1) { if (!(dastat&32)) method = 2; else method = 3; }
	}
	method |= 4; //Use OpenGL clamping - dorotatesprite never repeats

	xsiz = tilesizx[globalpicnum]; ysiz = tilesizy[globalpicnum];
	if (dastat&16) { xoff = 0; yoff = 0; }
	else
	{
		xoff = (int32_t)((signed char)((picanm[globalpicnum]>>8)&255))+(xsiz>>1);
		yoff = (int32_t)((signed char)((picanm[globalpicnum]>>16)&255))+(ysiz>>1);
	}
	if (dastat&4) yoff = ysiz-yoff;

	if (dastat&2)  //Auto window size scaling
	{
		if (!(dastat&8))
		{
			if (widescreen && !(dastat&512)) {
				if (dastat&1024) {
					/* Keep weapons at the 4:3 scale above HUD bars. */
					x = scale(oxdimen,ydim<<16,xdim*240);
					sx = ((cx1+cx2+2)<<15)+mulscale16(sx-(320<<15),x);
					x = mulscale16(x,yxaspect);
				} else {
					x = ydimenscale;
					sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),oydimen<<16,200*pixelaspect);
				}
			} else {
				x = scale(oxdimen,rsyxaspect,320);   //= scale(xdimen,yxaspect,320);
				sx = ((cx1+cx2+2)<<15)+scale(sx-(320<<15),oxdimen,320);
			}
			sy = ((cy1+cy2+2)<<15)+mulscale16(sy-(200<<15),x);
		}
		else
		{
				//If not clipping to startmosts, & auto-scaling on, as a
				//hard-coded bonus, scale to full screen instead
			if (widescreen && !(dastat&512)) {
				x = scale(ydim<<16,yxaspect,200*pixelaspect);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),ydim<<16,200*pixelaspect);
			} else {
				x = scale(xdim,rsyxaspect,320);
				sx = (xdim<<15)+32768+scale(sx-(320<<15),xdim,320);
			}
			sy = (ydim<<15)+32768+mulscale16(sy-(200<<15),x);
		}
		z = mulscale16(z,x);
	}

	d = (double)z/(65536.0*16384.0);
	cosang2 = cosang = (double)sintable[(a+512)&2047]*d;
	sinang2 = sinang = (double)sintable[a&2047]*d;
	if (((dastat&2) && !(dastat&256)) || (!(dastat&8))) //Don't aspect unscaled perms
		{ d = (double)rsxyaspect/65536.0; cosang2 *= d; sinang2 *= d; }
	else if (pixelaspect == 65536 && ((dastat&2) && (dastat&256))) //Skew 1:1 to 1.2:1 aspect
		{ d = 1.2; cosang2 *= d; sinang2 *= d; }
	px[0] = (double)sx/65536.0 - (double)xoff*cosang2+ (double)yoff*sinang2;
	py[0] = (double)sy/65536.0 - (double)xoff*sinang - (double)yoff*cosang;
	px[1] = px[0] + (double)xsiz*cosang2;
	py[1] = py[0] + (double)xsiz*sinang;
	px[3] = px[0] - (double)ysiz*sinang2;
	py[3] = py[0] + (double)ysiz*cosang;
	px[2] = px[1]+px[3]-px[0];
	py[2] = py[1]+py[3]-py[0];
	n = 4;

	gdx = 0; gdy = 0; gdo = 1.0;
		//px[0]*gux + py[0]*guy + guo = 0
		//px[1]*gux + py[1]*guy + guo = xsiz-.0001
		//px[3]*gux + py[3]*guy + guo = 0
	d = 1.0/(px[0]*(py[1]-py[3]) + px[1]*(py[3]-py[0]) + px[3]*(py[0]-py[1]));
	gux = (py[3]-py[0])*((double)xsiz-.0001)*d;
	guy = (px[0]-px[3])*((double)xsiz-.0001)*d;
	guo = 0 - px[0]*gux - py[0]*guy;

	if (!(dastat&4))
	{     //px[0]*gvx + py[0]*gvy + gvo = 0
			//px[1]*gvx + py[1]*gvy + gvo = 0
			//px[3]*gvx + py[3]*gvy + gvo = ysiz-.0001
		gvx = (py[0]-py[1])*((double)ysiz-.0001)*d;
		gvy = (px[1]-px[0])*((double)ysiz-.0001)*d;
		gvo = 0 - px[0]*gvx - py[0]*gvy;
	}
	else
	{     //px[0]*gvx + py[0]*gvy + gvo = ysiz-.0001
			//px[1]*gvx + py[1]*gvy + gvo = ysiz-.0001
			//px[3]*gvx + py[3]*gvy + gvo = 0
		gvx = (py[1]-py[0])*((double)ysiz-.0001)*d;
		gvy = (px[0]-px[1])*((double)ysiz-.0001)*d;
		gvo = (double)ysiz-.0001 - px[0]*gvx - py[0]*gvy;
	}

	cx2++; cy2++;
		//Clippoly4 (converted from long to double)
	nn = z = 0;
	do
	{
		zz = z+1; if (zz == n) zz = 0;
		x1 = px[z]; x2 = px[zz]-x1; if ((cx1 <= x1) && (x1 <= cx2)) { px2[nn] = x1; py2[nn] = py[z]; nn++; }
		if (x2 <= 0) fx = cx2; else fx = cx1;  d = fx-x1;
		if ((d < x2) != (d < 0)) { px2[nn] = fx; py2[nn] = (py[zz]-py[z])*d/x2 + py[z]; nn++; }
		if (x2 <= 0) fx = cx1; else fx = cx2;  d = fx-x1;
		if ((d < x2) != (d < 0)) { px2[nn] = fx; py2[nn] = (py[zz]-py[z])*d/x2 + py[z]; nn++; }
		z = zz;
	} while (z);
	if (nn >= 3)
	{
		n = z = 0;
		do
		{
			zz = z+1; if (zz == nn) zz = 0;
			y1 = py2[z]; y2 = py2[zz]-y1; if ((cy1 <= y1) && (y1 <= cy2)) { py[n] = y1; px[n] = px2[z]; n++; }
			if (y2 <= 0) fy = cy2; else fy = cy1;  d = fy-y1;
			if ((d < y2) != (d < 0)) { py[n] = fy; px[n] = (px2[zz]-px2[z])*d/y2 + px2[z]; n++; }
			if (y2 <= 0) fy = cy1; else fy = cy2;  d = fy-y1;
			if ((d < y2) != (d < 0)) { py[n] = fy; px[n] = (px2[zz]-px2[z])*d/y2 + px2[z]; n++; }
			z = zz;
		} while (z);
		pow2xsplit = 0; drawpoly(px,py,n,method);
	}

#ifdef USE_OPENGL
	polymost_gl_endhud();
#endif

	globalpicnum = ogpicnum;
	globalshade  = ogshade;
	globalpal    = ogpal;
	ghalfx       = oghalfx;
	grhalfxdown10 = ogrhalfxdown10;
	grhalfxdown10x = ogrhalfxdown10x;
	ghoriz       = oghoriz;
	frameoffset  = ofoffset;
	xdimen       = oxdimen;
	ydimen       = oydimen;
	gchang = ogchang;
	gshang = ogshang;
	gctang = ogctang;
	gstang = ogstang;
}


int32_t polymost_drawtilescreen (int32_t tilex, int32_t tiley, int32_t wallnum, int32_t dimen)
{
#ifdef USE_OPENGL
	return polymost_drawtilescreen_gl(tilex, tiley, wallnum, dimen);
#else
	return -1;
#endif
}

void polymost_precache(int32_t dapicnum, int32_t dapalnum, int32_t datype)
{
#ifdef USE_OPENGL
	polymost_precache_gl(dapicnum, dapalnum, datype);
#endif
}
