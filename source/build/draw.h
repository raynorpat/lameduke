#ifndef DRAW_H
#define DRAW_H

/* The DOS column drawers sit just before the audio library. This view
   uses the same sector, wall and sprite records they walk. */

extern int parallaxtype; /* byte_23DE9D */
extern int visibility;  /* byte_23DE9E */
extern unsigned char gotpic[];
extern short tspritelist[], tspritepicnum[];
extern unsigned char tspritepal[];
extern int spritesortcnt;
void drawrooms(int daposx, int daposy, int daposz, short daang, int dahoriz, short dacursectnum);
void drawmasks(void);
/* 0x23CE28, the engine clock */
extern int totalclock;
void rotatesprite(int sx, int sy, short picnum, signed char shade, unsigned char flags, unsigned char pal);
void rotatescalesprite(int sx, int sy, int z, short a, short picnum);
void draw2dscreen(int x, int y, short ang, int zoom, short gride);

/* The 2D screen (640 wide, ydim16 rows above the status bar) and the
   editor's search pick: with searchit 2, drawrooms/drawmasks record in
   searchsector/searchwall/searchstat (0 wall, 1 ceiling, 2 floor,
   3 sprite, 4 masked wall) what lies at (searchx, searchy). */
extern short editstatus, searchit;
extern int searchx, searchy;
extern short searchsector, searchwall, searchstat;
extern short linehighlight, pointhighlight, highlightcnt;
extern int ydim16, numframes, pageoffset;
void clear2dscreen(void);
void draw2dgrid(int x, int y, short ang, int zoom, short gride);
void drawline16(int x1, int y1, int x2, int y2, int col);
void printext16(int xpos, int ypos, short col, short backcol, const char *name, char fontsize);
void printext256(int xpos, int ypos, short col, short backcol, const char *name, char fontsize);
void plotpixel(int x, int y, char col);
void nextpage(void);
void showpage(void);

void draw_init(void);
const unsigned char *framebuffer(void);
int framebuffer_pitch(void);

#endif
