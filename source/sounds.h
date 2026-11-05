#ifndef SOUNDS_H
#define SOUNDS_H

extern unsigned char soundon; /* byte_1257AB */

void loadsounds(void);
void sound_callback(unsigned long num);
/* 0x3BE70 and 0x3C040 */
void spritesound(int num, short i);
void stopsound(int num);
/* 0x3BC40 */
void playmusic(const char *fn);
/* 1 once a song has started: TaskMan (0x65E2C) then owns the timer. */
extern int taskman;
/* 0x3BCB0: spritesound at a point; i -1 means the player. */
void soundxyz(int num, short i, int x, int y, int z);
/* 0x3C090: re-pan the playing sounds after a frame is drawn. */
void pansounds(void);

#endif
