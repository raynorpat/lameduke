#ifndef SOUNDS_H
#define SOUNDS_H

void loadsounds(void);
void sound_callback(unsigned long value);
/* 0x3BE70 and 0x3C040. Playback is lifted with the sound task. */
void spritesound(int num, short i);
void stopsound(int num);
/* 0x3BCB0: spritesound at a point; i -1 means the player. */
void soundxyz(int num, short i, int x, int y, int z);

#endif
