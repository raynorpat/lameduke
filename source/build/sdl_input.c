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

static int ramp_f;
static int ramp_s;
static int ramp_t;

static int clamp_vel(int v)
{
    if (v > 127)
        return 127;
    if (v < -128)
        return -128;
    return v;
}

static int friction(int v, int drag)
{
    if (v < 0) {
        v += drag;
        if (v > 0)
            v = 0;
    } else if (v > 0) {
        v -= drag;
        if (v < 0)
            v = 0;
    }
    return v;
}

/* One pass of sub_102A0. Forward and strafe ±8 drag 2, turn ±16 drag 10. */
static void ramp_step(void)
{
    if (turn_left)
        ramp_t = clamp_vel(ramp_t - 16);
    if (turn_right)
        ramp_t = clamp_vel(ramp_t + 16);
    if (strafe_left)
        ramp_s = clamp_vel(ramp_s + 8);
    if (strafe_right)
        ramp_s = clamp_vel(ramp_s - 8);
    if (forward)
        ramp_f = clamp_vel(ramp_f + 8);
    if (back)
        ramp_f = clamp_vel(ramp_f - 8);
    ramp_t = friction(ramp_t, 10);
    ramp_s = friction(ramp_s, 2);
    ramp_f = friction(ramp_f, 2);
}

void input_sample(int steps, int *fvel, int *svel, int *avel, int *look, int *mouse_turn)
{
    int i;

    for (i = 0; i < steps; i++)
        ramp_step();
    *fvel = ramp_f;
    *svel = ramp_s;
    *avel = ramp_t;
    *look = mouse_y;
    *mouse_turn = mouse_x * 4;
    mouse_x = 0;
    mouse_y = 0;
}
