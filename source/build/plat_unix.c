#include "platform.h"

#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* The existing file whose name matches name in any case, in path; else name. */
static const char *matchcase(const char *name, char path[PATH_MAX])
{
    const char *slash = strrchr(name, '/');
    size_t dirlen = slash ? (size_t)(slash - name) + 1 : 0;
    DIR *dir;
    struct dirent *de;

    if (dirlen >= PATH_MAX)
        return name;
    memcpy(path, name, dirlen);
    path[dirlen] = 0;
    dir = opendir(dirlen ? path : ".");
    if (!dir)
        return name;
    while ((de = readdir(dir)) != NULL) {
        if (strcasecmp(de->d_name, name + dirlen) == 0
            && dirlen + strlen(de->d_name) < PATH_MAX) {
            strcpy(path + dirlen, de->d_name);
            closedir(dir);
            return path;
        }
    }
    closedir(dir);
    return name;
}

FILE *plat_fopen(const char *name, const char *mode)
{
    char path[PATH_MAX];
    FILE *fp;

    if (mode[0] == 'r' && (fp = fopen(name, mode)) != NULL)
        return fp;
    /* A write reuses an existing file of any case instead of adding a twin. */
    return fopen(matchcase(name, path), mode);
}

int plat_rename(const char *from, const char *to)
{
    char path[PATH_MAX];

    return rename(from, matchcase(to, path));
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

int plat_getcwd(char *buf, int len)
{
    return getcwd(buf, (size_t)len) ? 0 : -1;
}
