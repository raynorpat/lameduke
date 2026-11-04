#include "dsl.h"
#include "mv_mix.h"

#include <SDL.h>
#include <math.h>

static SDL_AudioDeviceID dsl_device;
static int dsl_gain = 256; /* 8.8 */

static void dsl_callback(void *userdata, Uint8 *stream, int len)
{
    short *out = (short *)stream;
    int n = len / 4, i;

    (void)userdata;
    MV_Mix(out, (unsigned)n);
    for (i = 0; i < n * 2; i++)
        out[i] = (short)(out[i] * dsl_gain / 256);
}

int DSL_Init(unsigned rate, int channels, int bits)
{
    SDL_AudioSpec want;

    if (dsl_device != 0)
        DSL_Shutdown();
    if (channels != 2 || bits != 16)
        return -1;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return -1;
    SDL_zero(want);
    want.freq = (int)rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 256;
    want.callback = dsl_callback;
    dsl_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (dsl_device == 0)
        return -1;
    SDL_PauseAudioDevice(dsl_device, 0);
    return 0;
}

void DSL_Shutdown(void)
{
    if (dsl_device != 0) {
        SDL_CloseAudioDevice(dsl_device);
        dsl_device = 0;
    }
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

void DSL_SetVoiceLevel(int level)
{
    int g = (int)(256.0 * pow(10.0, -(31 - level) / 10.0) + 0.5);

    DSL_Lock();
    dsl_gain = g;
    DSL_Unlock();
}
