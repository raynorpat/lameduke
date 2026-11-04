#ifndef GAME_H
#define GAME_H

/* Startup order in main at D3D.EXE 0x15B60. */

void load_user_stats(void);
void save_user_stats(void);
extern int brightness, musicon;
/* 0x1B630 and 0x1B700: save and restore slot 0-7. */
void saveplayer(int slot);
void loadplayer(int slot);
int check_file_id(void);
void check_args(int argc, char **argv);
int compilecon(void);
void menus_init(void);
void loadsounds(void);
void sound_callback(unsigned long value);
/* 0x249B0's Entering screen, or the 0x19650 map and tally */
void levelscreen(int mode);

extern int sound_volume;
extern int music_volume;

#endif
