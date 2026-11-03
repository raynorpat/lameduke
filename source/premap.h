#ifndef PREMAP_H
#define PREMAP_H

/* 0x249B0 loads L1.MAP style boards ("Entering...", "Board not found.").
   0x24E00 checks FILE_ID.DIZ. 0x15570 handles L?.MAP and the command line. */

int check_file_id(void);
void check_args(int argc, char **argv);
int enterlevel(int mode);
void newgame(int volume, int level, int skill);
extern int user_visibility;

/* 0-based. /v1 /l1 stores volume 0, level 0. /r sets game_demorec. */
extern int game_volume;
extern int game_level;
extern int game_skill;
extern int game_level_set;
extern int game_demorec;
/* /q (byte_1257A9) and /d (word_1257AD). Read by code lifted in later tasks. */
extern int game_quotes;
extern int game_debug;

#endif
