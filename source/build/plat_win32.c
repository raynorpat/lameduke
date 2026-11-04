/* Win32 replacements for the DOS calls still named in D3D.EXE
   (int 10h, int 33h, dos_getvect). The live path is sdl_window.c. */

#include "platform.h"

#include <direct.h>
#include <windows.h>

/* NTFS already matches names case-insensitively. */
FILE *plat_fopen(const char *name, const char *mode)
{
    return fopen(name, mode);
}

int plat_listdir(void (*fn)(const char *name, int isdir, void *ctx), void *ctx)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("*", &fd);

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    do
        fn(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, ctx);
    while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
}

int plat_chdir(const char *dir)
{
    return _chdir(dir);
}
