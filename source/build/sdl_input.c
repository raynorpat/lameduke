#include "sdl_input.h"

#include <SDL.h>

static int quit;
static int forward;
static int back;
static int turn_left;
static int turn_right;
static int strafe_left;
static int strafe_right;
static int mouse_x;
static int mouse_y;

static void key_dir(SDL_Scancode code, int down)
{
    int v = down ? 1 : 0;

    if (code == SDL_SCANCODE_UP || code == SDL_SCANCODE_W)
        forward = v;
    else if (code == SDL_SCANCODE_DOWN || code == SDL_SCANCODE_S)
        back = v;
    else if (code == SDL_SCANCODE_LEFT)
        turn_left = v;
    else if (code == SDL_SCANCODE_RIGHT)
        turn_right = v;
    else if (code == SDL_SCANCODE_A || code == SDL_SCANCODE_COMMA)
        strafe_left = v;
    else if (code == SDL_SCANCODE_D || code == SDL_SCANCODE_PERIOD)
        strafe_right = v;
}

int input_poll(void)
{
    SDL_Event event;

    mouse_x = 0;
    mouse_y = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            quit = 1;
        else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
            quit = 1;
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            key_dir(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
        else if (event.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
            mouse_x += event.motion.xrel;
            mouse_y += event.motion.yrel;
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    return quit;
}

void input_stick(int *fvel, int *svel, int *avel, int *look)
{
    int fv = 0;
    int sv = 0;
    int av = 0;

    if (forward)
        fv += 125;
    if (back)
        fv -= 125;
    if (strafe_right)
        sv += 125;
    if (strafe_left)
        sv -= 125;
    if (turn_right)
        av += 48;
    if (turn_left)
        av -= 48;
    av += mouse_x * 4;
    *fvel = fv;
    *svel = sv;
    *avel = av;
    *look = mouse_y;
}
