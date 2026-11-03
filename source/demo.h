#ifndef DEMO_H
#define DEMO_H

/* mydemo_N.dmo, written by 0x1B920 and read by 0x1B810.
   3 bytes (volume, level, skill) + int32 count + four byte streams + one int32 stream.
   The second underscore in "mydemo__.dmo" is the digit. */

#define DEMO_MAX 16384

typedef struct {
    unsigned char volume;
    unsigned char level;
    unsigned char skill;
    int count;
    unsigned char *forward;
    unsigned char *strafe;
    unsigned char *turn;
    unsigned char *tics;
    int *bits;
} demo_t;

int demo_load(int index, demo_t *out);
void demo_free(demo_t *demo);
int demo_save(int index, const demo_t *demo);

#endif
