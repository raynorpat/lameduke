#ifndef GAME_H
#define GAME_H

/* Startup order in main at D3D.EXE 0x15B60. */

void load_user_stats(void);
int check_file_id(void);
void check_args(int argc, char **argv);
int compilecon(void);
void menus_init(void);
void loadsounds(void);
void sound_callback(unsigned long value);

extern int sound_volume;
extern int music_volume;

#endif
