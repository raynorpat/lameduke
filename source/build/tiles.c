#include "tiles.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

short tilesizx[MAXTILES];
short tilesizy[MAXTILES];
int picanm[MAXTILES];
unsigned char picsiz[MAXTILES];
unsigned char *waloff[MAXTILES];

/* The drawers read past a tile's last row when its height is not a power of
   two. In D3D.EXE that read lands in whatever the cache holds next. Here each
   ART file stays one block in file order, padded so the overrun stays inside. */
#define ART_PAD 65536

static int load_one_art(const char *name)
{
    FILE *fp;
    int head[4];
    int count;
    int i;
    long pixels = 0;
    unsigned char *block;

    fp = plat_fopen(name, "rb");
    if (!fp)
        return -1;
    if (fread(head, 4, 4, fp) != 4 || head[0] != 1) {
        fclose(fp);
        return -1;
    }
    count = head[3] - head[2] + 1;
    if (head[2] < 0 || head[3] >= MAXTILES || count <= 0
        || fread(&tilesizx[head[2]], 2, (size_t)count, fp) != (size_t)count
        || fread(&tilesizy[head[2]], 2, (size_t)count, fp) != (size_t)count
        || fread(&picanm[head[2]], 4, (size_t)count, fp) != (size_t)count) {
        fclose(fp);
        return -1;
    }
    for (i = head[2]; i <= head[3]; i++)
        pixels += (long)tilesizx[i] * tilesizy[i];
    block = (unsigned char *)calloc((size_t)pixels + ART_PAD, 1);
    if (!block || fread(block, 1, (size_t)pixels, fp) != (size_t)pixels) {
        free(block);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    for (i = head[2]; i <= head[3]; i++) {
        waloff[i] = tilesizx[i] > 0 && tilesizy[i] > 0 ? block : NULL;
        block += (long)tilesizx[i] * tilesizy[i];
    }
    return 0;
}

/* 0x4ABCB. picsiz holds floor(log2) of each size: y in the high nibble, x in the low. */
void loadpics(const char *filename)
{
    int i;

    (void)filename;
    for (i = 0; i < 1000; i++) {
        char name[32];

        snprintf(name, sizeof name, "tiles%03d.art", i);
        if (load_one_art(name) != 0)
            break;
    }
    for (i = 0; i < MAXTILES; i++) {
        int j, k;

        for (j = 15; j > 0; j--)
            if ((1 << j) <= tilesizx[i])
                break;
        for (k = 15; k > 0; k--)
            if ((1 << k) <= tilesizy[i])
                break;
        picsiz[i] = (unsigned char)(k * 16 + j);
    }
}
