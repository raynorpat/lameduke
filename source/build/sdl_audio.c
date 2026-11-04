#include "sdl_audio.h"

#include <SDL.h>

static SDL_AudioDeviceID device;
static void (*filler)(short *out, int frames);

static void callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    filler((short *)stream, len / 4);
}

int platform_audio_open(int rate, void (*fill)(short *out, int frames))
{
    SDL_AudioSpec want;

    if (device != 0)
        return 0;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        return -1;
    SDL_zero(want);
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = callback;
    filler = fill;
    device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (device == 0)
        return -1;
    SDL_PauseAudioDevice(device, 0);
    return 0;
}

void platform_audio_close(void)
{
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

void platform_audio_lock(void)
{
    if (device != 0)
        SDL_LockAudioDevice(device);
}

void platform_audio_unlock(void)
{
    if (device != 0)
        SDL_UnlockAudioDevice(device);
}
