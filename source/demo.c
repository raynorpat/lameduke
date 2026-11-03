#include "demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void demo_name(int index, char *name, int namelen)
{
    if (index < 0)
        index = 0;
    if (index > 9)
        index = 9;
    snprintf(name, (size_t)namelen, "mydemo_%d.dmo", index);
}

void demo_free(demo_t *demo)
{
    if (!demo)
        return;
    free(demo->forward);
    free(demo->strafe);
    free(demo->turn);
    free(demo->tics);
    free(demo->bits);
    memset(demo, 0, sizeof *demo);
}

int demo_load(int index, demo_t *out)
{
    char name[32];
    FILE *fp;
    unsigned char head[3];
    int count;
    int n;

    memset(out, 0, sizeof *out);
    demo_name(index, name, (int)sizeof name);
    fp = fopen(name, "rb");
    if (!fp)
        return -1;
    if (fread(head, 1, 3, fp) != 3 || fread(&count, 4, 1, fp) != 1 || count < 0 || count > DEMO_MAX) {
        fclose(fp);
        return -1;
    }
    out->volume = head[0];
    out->level = head[1];
    out->skill = head[2];
    out->count = count;
    if (count == 0) {
        fclose(fp);
        return 0;
    }
    out->forward = (unsigned char *)malloc((size_t)count);
    out->strafe = (unsigned char *)malloc((size_t)count);
    out->turn = (unsigned char *)malloc((size_t)count);
    out->tics = (unsigned char *)malloc((size_t)count);
    out->bits = (int *)malloc((size_t)count * sizeof(int));
    if (!out->forward || !out->strafe || !out->turn || !out->tics || !out->bits) {
        fclose(fp);
        demo_free(out);
        return -1;
    }
    n = (int)fread(out->forward, 1, (size_t)count, fp);
    n += (int)fread(out->strafe, 1, (size_t)count, fp);
    n += (int)fread(out->turn, 1, (size_t)count, fp);
    n += (int)fread(out->tics, 1, (size_t)count, fp);
    n += (int)fread(out->bits, 4, (size_t)count, fp) * 4;
    fclose(fp);
    if (n != count * 8) {
        demo_free(out);
        return -1;
    }
    /* Playback at 0x15EA2 clears two bits of the third byte. */
    for (n = 0; n < count; n++)
        out->bits[n] &= (int)0xFFE7FFFF;
    return 0;
}

int demo_save(int index, const demo_t *demo)
{
    char name[32];
    FILE *fp;
    unsigned char head[3];
    int count;

    if (!demo)
        return -1;
    count = demo->count;
    if (count < 0 || count > DEMO_MAX)
        return -1;
    demo_name(index, name, (int)sizeof name);
    fp = fopen(name, "wb");
    if (!fp)
        return -1;
    head[0] = demo->volume;
    head[1] = demo->level;
    head[2] = demo->skill;
    if (fwrite(head, 1, 3, fp) != 3 || fwrite(&count, 4, 1, fp) != 1
        || (count && fwrite(demo->forward, 1, (size_t)count, fp) != (size_t)count)
        || (count && fwrite(demo->strafe, 1, (size_t)count, fp) != (size_t)count)
        || (count && fwrite(demo->turn, 1, (size_t)count, fp) != (size_t)count)
        || (count && fwrite(demo->tics, 1, (size_t)count, fp) != (size_t)count)
        || (count && fwrite(demo->bits, 4, (size_t)count, fp) != (size_t)count)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}
