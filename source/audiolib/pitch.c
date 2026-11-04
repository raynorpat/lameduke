#include "pitch.h"

#include <math.h>

/* dword_241458: 2^(k/300) in 16.16, 25 steps of 4 cents per semitone. */
static unsigned pitchtable[300];
static int pitchinit; /* dword_8944C */

/* 0x66C70, building the table on first use as 0x66BF0 does. */
int PITCH_GetScale(int offset)
{
    int note, octave;
    unsigned scale;

    if (!pitchinit) {
        int k;

        for (k = 0; k < 300; k++)
            pitchtable[k] = (unsigned)(pow(2.0, k * (1.0 / 300.0)) * 65536.0);
        pitchinit = 1;
    }
    if (offset == 0)
        return (int)pitchtable[0];
    note = offset % 1200;
    if (note < 0)
        note += 1200;
    octave = (offset - note) / 1200;
    scale = pitchtable[25 * (note / 100) + (note % 100) / 4];
    return (int)(octave >= 0 ? scale << octave : scale >> -octave);
}
