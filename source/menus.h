#ifndef MENUS_H
#define MENUS_H

/* 0x3CCF0: the menu page menupage names, drawn over the frame. */
void menus(void);

/* aItOnlyLooksPai and dword_82568, kept in userdefs.tmp. */
extern char scorenames[12][33];
extern int scores[12];
void drawscores(void);
int newscore(void);
int getscorename(int slot);

#endif
