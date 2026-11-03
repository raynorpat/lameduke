#include "sdl_main.h"

#include <SDL.h>

int platform_init(void)
{
    return SDL_Init(0);
}

void platform_quit(void)
{
    SDL_Quit();
}
