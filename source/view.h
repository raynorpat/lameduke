#ifndef VIEW_H
#define VIEW_H

/* 0x14050: player p's 3D view (or the demo camera), with its sprite and
   wall frame animation. */
void displayrooms(short p);
void drawview(short p);

/* 0x12850. The small font: tile is the character plus 991, a space is 2 px,
   a newline drops 8 and returns to the left, lowercase sits 2 px lower.
   Nothing is drawn while the local player is in the 2D map. Returns the x
   the next glyph would use. */
int minitext(int x, int y, const char *s, int shade);

/* 0x12750. The big font: digits are char+101, lowercase char+26 (2 px
   lower), uppercase char+32. Other characters repeat the previous glyph.
   A space is 5 px and a newline drops 16. */
int gametext(int x, int y, const char *s, int shade);
/* 0x123D0: the pulsing shade; every call steps the pulse. */
int hudshade(void);
/* 0x12A40 */
extern char typebuf[160];
extern unsigned char typebuflen;
int strget(int x, int y, int shade, int maxlen);
int bigtext(int x, int y, const char *s, int shade);

/* 0x13800. That font, for a base-10 number. */
void numtext(int x, int y, int value, int shade);

/* word_12538E / dword_125248. 0x11120 arms the line; the HUD counts it down. */
extern short msgtime;
extern const char *msgtext;

#endif
