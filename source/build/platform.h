#ifndef PLATFORM_H
#define PLATFORM_H

/* Selected by CMake. Kept so DOS-only calls have somewhere to land. */

#include <stdio.h>

/* fopen that matches the file name case-insensitively, as DOS did.
   D3D.EXE asks for lowercase names and the release ships them uppercase. */
FILE *plat_fopen(const char *name, const char *mode);

/* Moves from over to, replacing to (matched as plat_fopen does). 0 on success. */
int plat_rename(const char *from, const char *to);

/* For the editor's file menu: fn gets each entry of the current directory
   (isdir 1 for a directory); plat_chdir changes it. 0 on success. */
int plat_listdir(void (*fn)(const char *name, int isdir, void *ctx), void *ctx);
int plat_chdir(const char *dir);
int plat_getcwd(char *buf, int len);

#endif
