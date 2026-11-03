#include "dsl.h"

#include <SDL.h>

static SDL_AudioDeviceID dsl_device;

int DSL_Init(unsigned rate, int channels, int bits)
{
    SDL_AudioSpec want;

    if (dsl_device != 0)
        DSL_Shutdown();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return -1;
    SDL_zero(want);
    want.freq = (int)rate;
    want.format = bits >= 16 ? AUDIO_S16SYS : AUDIO_U8;
    want.channels = (Uint8)(channels < 1 ? 1 : channels);
    want.samples = 512;
    want.callback = NULL;
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
