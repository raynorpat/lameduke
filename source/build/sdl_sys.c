#include "sdl_sys.h"

#include <SDL.h>

void timer_init(void)
{
    SDL_InitSubSystem(SDL_INIT_TIMER);
}

uint32_t timer_ticks(void)
{
    return SDL_GetTicks();
}

void timer_delay(uint32_t ms)
{
    SDL_Delay(ms);
}
