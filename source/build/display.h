#ifndef DISPLAY_H
#define DISPLAY_H

void setgamemode(void);
void setview(int x1, int y1, int x2, int y2);

/* 0x13760. Reads lookup.dat. Returns 0 on success. */
int loadlookup(void);

/* First 768 bytes of PALETTE.DAT, expanded from VGA 6-bit to 0..255. */
int loadpalette(void);
extern unsigned char palette[768];
extern unsigned char *palookup[256];
extern int numpalookups;
extern unsigned char *transluc;
extern short pskyoff[256];
extern short pskybits;

#endif
