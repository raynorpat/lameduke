#include "tiles.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int artversion;

typedef struct {
    short sizx;
    short sizy;
    unsigned char *data;
} tile_t;

static tile_t tiles[MAXTILES];

const unsigned char *tile_data(int pic, int *sizx, int *sizy)
{
    if (pic < 0 || pic >= MAXTILES || !tiles[pic].data) {
        if (sizx)
            *sizx = 0;
        if (sizy)
            *sizy = 0;
        return NULL;
    }
    if (sizx)
        *sizx = tiles[pic].sizx;
    if (sizy)
        *sizy = tiles[pic].sizy;
    return tiles[pic].data;
}

static int load_one_art(const char *name)
{
    FILE *fp;
    int version;
    int numtiles;
    int start;
    int end;
    int count;
    int i;
    short *sizx;
    short *sizy;

    fp = plat_fopen(name, "rb");
    if (!fp)
        return -1;
    if (fread(&version, 4, 1, fp) != 1 || fread(&numtiles, 4, 1, fp) != 1
        || fread(&start, 4, 1, fp) != 1 || fread(&end, 4, 1, fp) != 1
        || version != 1) {
        fclose(fp);
        return -1;
    }
    (void)numtiles;
    count = end - start + 1;
    if (start < 0 || end >= MAXTILES || count <= 0) {
        fclose(fp);
        return -1;
    }
    sizx = (short *)malloc((size_t)count * sizeof(short));
    sizy = (short *)malloc((size_t)count * sizeof(short));
    if (!sizx || !sizy
        || fread(sizx, 2, (size_t)count, fp) != (size_t)count
        || fread(sizy, 2, (size_t)count, fp) != (size_t)count
        || fseek(fp, (long)count * 4L, SEEK_CUR) != 0) {
        free(sizx);
        free(sizy);
        fclose(fp);
        return -1;
    }
    for (i = 0; i < count; i++) {
        int pic = start + i;
        int bytes;
        unsigned char *data;

        if (sizx[i] <= 0 || sizy[i] <= 0) {
            tiles[pic].sizx = 0;
            tiles[pic].sizy = 0;
            continue;
        }
        bytes = (int)sizx[i] * (int)sizy[i];
        if (bytes <= 0 || bytes > 1024 * 1024) {
            free(sizx);
            free(sizy);
            fclose(fp);
            return -1;
        }
        data = (unsigned char *)malloc((size_t)bytes);
        if (!data || fread(data, 1, (size_t)bytes, fp) != (size_t)bytes) {
            free(data);
            free(sizx);
            free(sizy);
            fclose(fp);
            return -1;
        }
        free(tiles[pic].data);
        tiles[pic].data = data;
        tiles[pic].sizx = sizx[i];
        tiles[pic].sizy = sizy[i];
    }
    free(sizx);
    free(sizy);
    fclose(fp);
    artversion = 1;
    return 0;
}

void loadpics(const char *filename)
{
    int i;

    (void)filename;
    for (i = 0; i < 100; i++) {
        char name[32];

        snprintf(name, sizeof name, "tiles%03d.art", i);
        if (load_one_art(name) != 0)
            break;
    }
}
