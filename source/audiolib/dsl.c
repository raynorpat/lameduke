#include "dsl.h"
#include "mv_mix.h"
#include "oplemu.h"
#include "task_man.h"

#include <SDL.h>
#include <math.h>

/* One SDL device stands in for the SB16: its DAC plays multivoc's buffers
   at the mix rate, and its OPL3 plays at the chip's own rate. Both are
   resampled to the device rate here and summed as the card's mixer does. */

#define DEVICE_RATE 44100

static SDL_AudioDeviceID dsl_device;
static unsigned fxrate;
static int fxon, musicon;
static int voicegain = 256, fmgain = 256; /* 8.8, from the card's mixer levels */

/* the DAC stream: the last two mix-rate frames and the position between them */
static short fxprev[2], fxnext[2];
static unsigned fxfrac;
/* the OPL stream: chip samples owed for the next output frame */
static unsigned oplfrac;

static int clamp16(int v)
{
    return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
}

static void dsl_callback(void *userdata, Uint8 *stream, int len)
{
    short *out = (short *)stream;
    int n = len / 4;

    (void)userdata;
    while (n-- > 0) {
        int l = 0, r = 0;

        if (fxon) {
            fxfrac += fxrate;
            while (fxfrac >= DEVICE_RATE) {
                fxfrac -= DEVICE_RATE;
                fxprev[0] = fxnext[0];
                fxprev[1] = fxnext[1];
                MV_Mix(fxnext, 1);
            }
            l = fxprev[0] + (int)(((long long)(fxnext[0] - fxprev[0]) * fxfrac) / DEVICE_RATE);
            r = fxprev[1] + (int)(((long long)(fxnext[1] - fxprev[1]) * fxfrac) / DEVICE_RATE);
            l = l * voicegain / 256;
            r = r * voicegain / 256;
        }
        if (musicon) {
            int ol = 0, or_ = 0, count = 0;

            TS_Advance(DEVICE_RATE);
            oplfrac += OPL_RATE;
            while (oplfrac >= DEVICE_RATE) {
                int a, b;

                oplfrac -= DEVICE_RATE;
                OPL_Generate(&a, &b);
                ol += a;
                or_ += b;
                count++;
            }
            if (count) {
                l += ol / count * fmgain / 256;
                r += or_ / count * fmgain / 256;
            }
        }
        out[0] = (short)clamp16(l);
        out[1] = (short)clamp16(r);
        out += 2;
    }
}

static int opendevice(void)
{
    SDL_AudioSpec want;

    if (dsl_device != 0)
        return 0;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return -1;
    SDL_zero(want);
    want.freq = DEVICE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = dsl_callback;
    dsl_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (dsl_device == 0)
        return -1;
    SDL_PauseAudioDevice(dsl_device, 0);
    return 0;
}

int DSL_Init(unsigned rate, int channels, int bits)
{
    if (channels != 2 || bits != 16)
        return -1;
    if (opendevice() != 0)
        return -1;
    DSL_Lock();
    fxrate = rate;
    fxon = 1;
    DSL_Unlock();
    return 0;
}

int DSL_InitMusic(void)
{
    if (opendevice() != 0)
        return -1;
    DSL_Lock();
    musicon = 1;
    DSL_Unlock();
    return 0;
}

void DSL_Shutdown(void)
{
    if (dsl_device != 0) {
        SDL_CloseAudioDevice(dsl_device);
        dsl_device = 0;
    }
    fxon = musicon = 0;
}

void DSL_Lock(void)
{
    if (dsl_device != 0)
        SDL_LockAudioDevice(dsl_device);
}

void DSL_Unlock(void)
{
    if (dsl_device != 0)
        SDL_UnlockAudioDevice(dsl_device);
}

static int levelgain(int level)
{
    return (int)(256.0 * pow(10.0, -(31 - level) / 10.0) + 0.5);
}

void DSL_SetVoiceLevel(int level)
{
    int g = levelgain(level);

    DSL_Lock();
    voicegain = g;
    DSL_Unlock();
}

void DSL_SetFMLevel(int level)
{
    int g = levelgain(level);

    DSL_Lock();
    fmgain = g;
    DSL_Unlock();
}
