#ifndef PLATFORM_H
#define PLATFORM_H

/* Selected by CMake. Kept so DOS-only calls have somewhere to land. */

#include <stdio.h>

/* fopen that matches the file name case-insensitively, as DOS did.
   D3D.EXE asks for lowercase names and the release ships them uppercase. */
FILE *plat_fopen(const char *name, const char *mode);

#endif
