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
void draw2dscreen(int x, int y, short ang, int zoom, const unsigned char *spriteshown);

void draw_init(void);
const unsigned char *framebuffer(void);
int framebuffer_pitch(void);

#endif
