#ifndef ENGINE_H
#define ENGINE_H

/* Public Build surface recovered from D3D.EXE (Watcom, 12/30/94).
   MAXTILES is 4096 here. Later duke3d builds use a larger table. */

#define MAXTILES 4096

void errorexit(const char *msg);

void initengine(void);
void loadpics(const char *filename);
void setview(int x1, int y1, int x2, int y2);
void setgamemode(void);

/* setup.dat, six bytes at the front of the file.
   0 graphics mode, 1 digitized-sound menu, 2 music on/off,
   3 mouse, 4 comm option, 5 comm speed. */
int setup_byte(int index);

int initmultiplayers(int comm, int rate);

extern int xdim;
extern int ydim;

#endif
