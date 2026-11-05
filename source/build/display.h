#ifndef DISPLAY_H
#define DISPLAY_H

void setgamemode(void);
void setview(int x1, int y1, int x2, int y2);

/* 0x13760. Reads lookup.dat. Returns 0 on success. */
int loadlookup(void);

/* First 768 bytes of PALETTE.DAT, expanded from VGA 6-bit to 0..255. */
int loadpalette(void);
void setbrightness(int level);
void setmapmode(void);
void qsetmode640480(void);
void screencapture(void);
extern unsigned char palette[768];
extern unsigned char *palookup[256];
extern int numpalookups;
extern unsigned char *transluc;
extern short pskyoff[256];
extern short pskybits;

/* Renderer 3's window (duke3d's display.h names, in sdl_window.c). */
/* Makes the GL window xdim by ydim, or sizes the one there is: once made, it
   and its context stay for the run and renderers 0-2 are shown through it.
   0, or -1 with platform_video_error() set. */
int setgamemode_gl(int xdim, int ydim);
/* 0 windowed, 1 fullscreen, 2 borderless; now if there is a window. */
void setvideodisplaymode(int mode);
int getvideodisplaymode(void);
/* The main display's modes (w[i] by h[i], at most max) and its desktop size;
   returns how many. */
int getvideomodelist(int *w, int *h, int max, int *desktopw, int *desktoph);
void setvideovsync(int on);
/* 0 nearest, 1 bilinear, 2 trilinear; 1 if it is in use now. */
int setvideotexfilter(int filter);
/* 0 off, 1-3 drawing at 1440, 2160 or 4320 rows on the 2.1 path; 1 if in use. */
int setvideosupersample(int level);
int videosupersampleavailable(int level);
/* Drop this many mipmap levels from renderer 3's textures. */
void setvideomiplevel(int level);
/* The 2.1 path's contrast (0.5-1.5) and saturation (0-2). */
void setvideocolor(double contrast, double saturation);
/* The GLPATH_* (sdl_opengl_internal.h) in use; 0 without a GL window. */
int getvideoglpath(void);
/* Renderer 3: draws the last frame shown as the new frame's start. */
void glredrawlastframe(void);

#endif
