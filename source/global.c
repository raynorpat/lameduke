#include "global.h"

int sound_volume = 255; /* word_1257AF, from userdefs.tmp (255 without one) */
int music_volume = 255; /* word_1257B1, from userdefs.tmp (255 without one) */

playertype ps[MAXPLAYERS];
signed char syncfvel[MAXPLAYERS], syncsvel[MAXPLAYERS], syncavel[MAXPLAYERS];
int syncbits[MAXPLAYERS];
unsigned char synctics;
short screenpeek, myconnectindex;
int lockclock;
short gamepaused, cameraspr = -1;
int demomode; /* dword_125258: 1 recording, 2 playing back */
int smoothratio = 65536;
int bposx[MAXSPRITES], bposy[MAXSPRITES], bposz[MAXSPRITES];
