#include "platform.h"

#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *plat_fopen(const char *name, const char *mode)
{
    char path[PATH_MAX];
    const char *slash = strrchr(name, '/');
    size_t dirlen = slash ? (size_t)(slash - name) + 1 : 0;
    DIR *dir;
    struct dirent *de;
    FILE *fp;

    if (mode[0] == 'r' && (fp = fopen(name, mode)) != NULL)
        return fp;
    if (dirlen >= sizeof path)
        return fopen(name, mode);
    memcpy(path, name, dirlen);
    path[dirlen] = 0;
    dir = opendir(dirlen ? path : ".");
    if (!dir)
        return fopen(name, mode);
    /* A write reuses an existing file of any case instead of adding a twin. */
    while ((de = readdir(dir)) != NULL) {
        if (strcasecmp(de->d_name, name + dirlen) == 0
            && dirlen + strlen(de->d_name) < sizeof path) {
            strcpy(path + dirlen, de->d_name);
            closedir(dir);
            return fopen(path, mode);
        }
    }
    closedir(dir);
    return fopen(name, mode);
}

int plat_listdir(void (*fn)(const char *name, int isdir, void *ctx), void *ctx)
{
    DIR *dir = opendir(".");
    struct dirent *de;

    if (!dir)
        return -1;
    while ((de = readdir(dir)) != NULL) {
        struct stat st;

        if (stat(de->d_name, &st) == 0)
            fn(de->d_name, S_ISDIR(st.st_mode) != 0, ctx);
    }
    closedir(dir);
    return 0;
}

int plat_chdir(const char *dir)
{
    return chdir(dir);
}
