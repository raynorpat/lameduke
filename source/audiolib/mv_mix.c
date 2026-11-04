/* The DMA side of multivoc: dsl.c's audio callback asks for frames, and
   each 256 come from one pass of the interrupt routine. */

#include "mv_mix.h"
#include "multivoc.h"

void MV_Mix(short *dest, unsigned frames)
{
    static const unsigned short *buf;
    static unsigned left;

    while (frames--) {
        if (!left) {
            buf = MV_ServiceVoc();
            left = 256;
        }
        /* the buffers are unsigned around 0x8000 */
        dest[0] = (short)(buf[0] ^ 0x8000);
        dest[1] = (short)(buf[1] ^ 0x8000);
        dest += 2;
        buf += 2;
        left--;
    }
}
