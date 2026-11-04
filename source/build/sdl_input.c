#include "sdl_input.h"
#include "engine.h"

#include <SDL.h>

static int quit;
static int mouse_x;
static int mouse_y;
static int mouse_buttons;

/* byte_1BCA10: one byte per PC scancode, 0x80 and up for the E0-prefixed keys. */
unsigned char keystatus[256];

int mousesens = 128; /* word_1257B7, from userdefs.tmp (128 without one) */

/* PC scancode set 1 for an SDL scancode; 0 when the DOS keyboard has none. */
static int dos_scancode(SDL_Scancode s)
{
    static const unsigned char letters[26] = { 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24,
        0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C };

    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z)
        return letters[s - SDL_SCANCODE_A];
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0)
        return 0x02 + (s - SDL_SCANCODE_1);
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10)
        return 0x3B + (s - SDL_SCANCODE_F1);
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN: return 0x1C;
    case SDL_SCANCODE_LCTRL: return 0x1D;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2A;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_LALT: return 0x38;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_CAPSLOCK: return 0x3A;
    case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_KP_4: return 0x4B;
    case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_KP_6: return 0x4D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_KP_1: return 0x4F;
    case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: return 0x53;
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_KP_ENTER: return 0x9C;
    case SDL_SCANCODE_RCTRL: return 0x9D;
    case SDL_SCANCODE_KP_DIVIDE: return 0xB5;
    case SDL_SCANCODE_RALT: return 0xB8;
    case SDL_SCANCODE_HOME: return 0xC7;
    case SDL_SCANCODE_UP: return 0xC8;
    case SDL_SCANCODE_PAGEUP: return 0xC9;
    case SDL_SCANCODE_LEFT: return 0xCB;
    case SDL_SCANCODE_RIGHT: return 0xCD;
    case SDL_SCANCODE_END: return 0xCF;
    case SDL_SCANCODE_DOWN: return 0xD0;
    case SDL_SCANCODE_PAGEDOWN: return 0xD1;
    case SDL_SCANCODE_INSERT: return 0xD2;
    case SDL_SCANCODE_DELETE: return 0xD3;
    default: return 0;
    }
}

int input_poll(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            quit = 1;
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            int sc = dos_scancode(event.key.keysym.scancode);

            if (sc)
                keystatus[sc] = event.type == SDL_KEYDOWN;
        } else if (event.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
            mouse_x += event.motion.xrel;
            mouse_y += event.motion.yrel;
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            int b = event.button.button == SDL_BUTTON_LEFT ? 1 : event.button.button == SDL_BUTTON_RIGHT ? 2 : 0;

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                mouse_buttons |= b;
                if (b == 1)
                    SDL_SetRelativeMouseMode(SDL_TRUE);
            } else {
                mouse_buttons &= ~b;
            }
        }
    }
    return quit;
}

/* dword_125300, dword_1252FC, dword_1252F8 */
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

static int key(int setup_index)
{
    return keystatus[setup_byte(setup_index) & 0xFF];
}

/* One pass of the timer ISR 0x102A0. SETUP.DAT bytes 6..9 are forward,
   back, left and right, byte 11 the strafe key; the keypad and grey
   arrows always work. */
static void ramp_step(void)
{
    int left = keystatus[0x4B] | key(8) | keystatus[0xCB];
    int right = keystatus[0x4D] | key(9) | keystatus[0xCD];

    if (key(11)) {
        if (left)
            ramp_s = clamp_vel(ramp_s + 8);
        if (right)
            ramp_s = clamp_vel(ramp_s - 8);
    } else {
        if (left)
            ramp_t = clamp_vel(ramp_t - 16);
        if (right)
            ramp_t = clamp_vel(ramp_t + 16);
    }
    if (keystatus[0x48] | key(6) | keystatus[0xC8])
        ramp_f = clamp_vel(ramp_f + 8);
    if (keystatus[0x50] | key(7) | keystatus[0xD0])
        ramp_f = clamp_vel(ramp_f - 8);
    ramp_t = friction(ramp_t, 10);
    ramp_s = friction(ramp_s, 2);
    ramp_f = friction(ramp_f, 2);
}

/* The key half of getinput 0x30E60: fixed scancodes, not SETUP.DAT. */
static int syncbits_from_keys(void)
{
    static const unsigned char bit[32][2] = {
        { 0x1E, 0 }, { 0x2C, 0 }, { 0x1D, 0 }, { 0, 0 }, { 0x39, 0 }, { 0x2A, 0x36 }, { 0x52, 0 }, { 0x53, 0 },
        { 0x02, 0 }, { 0x03, 0 }, { 0x04, 0 }, { 0x05, 0 }, { 0x06, 0 }, { 0xC9, 0x47 }, { 0xD1, 0x4F }, { 0x07, 0 },
        { 0x2E, 0 }, { 0x17, 0 }, { 0x4C, 0 }, { 0x4E, 0 }, { 0x4A, 0x0C }, { 0x19, 0 }, { 0x0F, 0 }, { 0x16, 0 },
        { 0x3B, 0 }, { 0x3C, 0 }, { 0x3D, 0 }, { 0x3E, 0 }, { 0x3F, 0 }, { 0x40, 0 }, { 0x1C, 0x9C }, { 0x01, 0 },
    };
    unsigned bits = 0;
    int b;

    for (b = 0; b < 32; b++)
        if ((bit[b][0] && keystatus[bit[b][0]]) || (bit[b][1] && keystatus[bit[b][1]]))
            bits |= 1u << b;
    return (int)bits;
}

/* 0x53914: the motion since the last read, and the buttons held. */
void getmousevalues(int *dx, int *dy, int *buttons)
{
    *dx = mouse_x;
    *dy = mouse_y;
    *buttons = mouse_buttons;
    mouse_x = 0;
    mouse_y = 0;
}

/* getinput 0x30E60 with the menu open: the ramps stop. */
void input_clear(void)
{
    ramp_f = ramp_s = ramp_t = 0;
}

void input_sample(int steps, int *fvel, int *svel, int *avel, int *bits)
{
    const int sens = mousesens + 129;
    int i;

    for (i = 0; i < steps; i++)
        ramp_step();
    ramp_t = clamp_vel(((sens * mouse_x) >> 7) + ramp_t);
    ramp_f = clamp_vel(ramp_f - ((mouse_y * sens) >> 7));
    mouse_x = 0;
    mouse_y = 0;
    *fvel = ramp_f;
    *svel = ramp_s;
    *avel = ramp_t;
    *bits = syncbits_from_keys();
}
