/* Win32 replacements for the DOS calls still named in D3D.EXE
   (int 10h, int 33h, dos_getvect). The live path is sdl_window.c. */

#include "platform.h"

/* NTFS already matches names case-insensitively. */
FILE *plat_fopen(const char *name, const char *mode)
{
    return fopen(name, mode);
}
