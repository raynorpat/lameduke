#include "global.h"

int sound_volume;   /* word_1257AF */
int music_volume;   /* word_1257B1 */

playertype ps[MAXPLAYERS];
signed char syncfvel[MAXPLAYERS], syncsvel[MAXPLAYERS], syncavel[MAXPLAYERS];
int syncbits[MAXPLAYERS];
unsigned char synctics;
short connecthead, connectpoint2[MAXPLAYERS], numplayers = 1, screenpeek, myconnectindex;
int lockclock;
short gamepaused, cameraspr = -1;
int demomode; /* dword_125258: 1 recording, 2 playing back */
