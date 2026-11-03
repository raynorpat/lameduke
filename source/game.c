#include "game.h"
#include "premap.h"
#include "menus.h"
#include "sounds.h"
#include "demo.h"

#include "cache1d.h"
#include "engine.h"
#include "display.h"
#include "board.h"
#include "draw.h"
#include "network.h"
#include "sdl_main.h"
#include "sdl_sys.h"
#include "sdl_window.h"
#include "sdl_input.h"

#include "fx_man.h"
#include "music.h"
#include "sndcards.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char banner[] =
    " DUKE DUKEM 3D Beta version 1.3.95 -- DO NOT DISTRIBUTE!!! That would be bad. ";

/* D3D.EXE 0x828F8. Digitized-sound menu index to the card number FX_ sees.
   Index 0 is "no digitized sound" and main does not call FX with it. */
static const int setup_fx_card[9] = { -1, 0, 1, 3, 4, 7, 9, 10, 12 };

static int fx_card_from_setup(void)
{
    int menu = setup_byte(1);
    if (menu <= 0 || menu > 8)
        return -1;
    return setup_fx_card[menu];
}

static int signed_byte(unsigned char v)
{
    return (int)(signed char)v;
}

static unsigned char store_byte(int v)
{
    if (v > 127)
        v = 127;
    if (v < -128)
        v = -128;
    return (unsigned char)v;
}

static int rec_grow(demo_t *demo, int *cap)
{
    int next;
    unsigned char *forward;
    unsigned char *strafe;
    unsigned char *turn;
    unsigned char *tics;
    int *bits;

    if (demo->count < *cap)
        return 0;
    if (*cap >= DEMO_MAX)
        return -1;
    next = *cap ? *cap * 2 : 256;
    if (next > DEMO_MAX)
        next = DEMO_MAX;
    forward = (unsigned char *)realloc(demo->forward, (size_t)next);
    if (!forward)
        return -1;
    demo->forward = forward;
    strafe = (unsigned char *)realloc(demo->strafe, (size_t)next);
    if (!strafe)
        return -1;
    demo->strafe = strafe;
    turn = (unsigned char *)realloc(demo->turn, (size_t)next);
    if (!turn)
        return -1;
    demo->turn = turn;
    tics = (unsigned char *)realloc(demo->tics, (size_t)next);
    if (!tics)
        return -1;
    demo->tics = tics;
    bits = (int *)realloc(demo->bits, (size_t)next * sizeof(int));
    if (!bits)
        return -1;
    demo->bits = bits;
    *cap = next;
    return 0;
}

static void write_frame(const char *path)
{
    FILE *fp;
    unsigned char hdr[54];
    int x, y;
    const unsigned char *src = framebuffer();
    int row = (xdim * 3 + 3) & ~3;

    fp = fopen(path, "wb");
    if (!fp)
        return;
    {
        int n = 0;
        int i;
        for (i = 0; i < xdim * ydim; i++)
            if (src[i])
                n++;
        printf("frame indices %d\n", n);
    }
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (unsigned char)xdim;
    hdr[19] = (unsigned char)(xdim >> 8);
    hdr[22] = (unsigned char)ydim;
    hdr[23] = (unsigned char)(ydim >> 8);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, 54, fp);
    for (y = ydim - 1; y >= 0; y--) {
        for (x = 0; x < xdim; x++) {
            int c = src[y * xdim + x] * 3;
            unsigned char rgb[3];
            rgb[0] = palette[c + 2];
            rgb[1] = palette[c + 1];
            rgb[2] = palette[c];
            fwrite(rgb, 1, 3, fp);
        }
        if (row > xdim * 3)
            fwrite("\0\0\0", 1, (size_t)(row - xdim * 3), fp);
    }
    fclose(fp);
}

static void present(void)
{
    const char *shot = getenv("LAMEDUKE_FRAME");

    draw_view();
    platform_present(framebuffer(), xdim, ydim);
    if (shot && shot[0]) {
        write_frame(shot);
        exit(0);
    }
}

/* sub_102A0 steps the clock by 6 when music is on, otherwise by 1, at 120 Hz.
   A demo whose tics are all multiples of 6 was recorded on the 6-step clock. */
static int demo_hz(const demo_t *demo)
{
    int i;

    for (i = 0; i < demo->count; i++) {
        if (demo->tics[i] % 6)
            return 120;
    }
    return 720;
}

static int play_demo(const demo_t *demo)
{
    int i;
    int hz = demo_hz(demo);

    for (i = 0; i < demo->count; i++) {
        int tics = demo->tics[i];
        uint32_t start;

        if (input_poll())
            return 1;
        player_move(signed_byte(demo->forward[i]), signed_byte(demo->strafe[i]),
            signed_byte(demo->turn[i]), tics, demo->bits[i]);
        if (tics == 0)
            continue;
        present();
        start = timer_ticks();
        while ((int)(timer_ticks() - start) < tics * 1000 / hz) {
            if (input_poll())
                return 1;
            SDL_Delay(1);
        }
    }
    return 0;
}

static int play_level(int record)
{
    demo_t rec;
    int cap = 0;
    uint32_t prev;

    memset(&rec, 0, sizeof rec);
    rec.volume = (unsigned char)game_volume;
    rec.level = (unsigned char)game_level;
    rec.skill = (unsigned char)game_skill;
    prev = timer_ticks();
    present();
    while (!input_poll()) {
        uint32_t now = timer_ticks();
        int elapsed = (int)(now - prev);
        int irqs;
        int steps;
        int per_irq;
        int fv, sv, av, look, mouse_turn;

        /* Music byte selects the 6-step ISR. The PIT itself stays at 120 Hz. */
        per_irq = setup_byte(2) != 0 ? 6 : 1;
        if (elapsed < 0)
            elapsed = 0;
        irqs = elapsed * 120 / 1000;
        if (irqs < 1) {
            SDL_Delay(1);
            continue;
        }
        if (irqs > 40)
            irqs = 40;
        prev += (uint32_t)(irqs * 1000 / 120);
        steps = irqs * per_irq;
        if (steps > 255)
            steps = 255;
        input_sample(steps, &fv, &sv, &av, &look, &mouse_turn);
        player_look(look);
        player_move(fv, sv, av, steps, 0);
        if (mouse_turn)
            ang = (short)((ang + mouse_turn) & 2047);
        if (record && rec_grow(&rec, &cap) == 0) {
            int n = rec.count;
            rec.forward[n] = store_byte(fv);
            rec.strafe[n] = store_byte(sv);
            rec.turn[n] = store_byte(av);
            rec.tics[n] = (unsigned char)steps;
            rec.bits[n] = 0;
            rec.count = n + 1;
        }
        present();
    }
    if (record && rec.count > 0) {
        int slot = (game_volume == 0 && game_level == 0) ? 0 : 1;
        if (demo_save(slot, &rec) == 0)
            printf("Wrote mydemo_%d.dmo\n", slot);
    }
    demo_free(&rec);
    return 1;
}

int main(int argc, char **argv)
{
    int card;
    fx_device device;

    if (platform_init() != 0)
        return 1;

    puts(banner);
    puts("Loading user stats...");
    load_user_stats();
    puts("Init BootLeg CacheMaster...");
    initcache();
    puts("Checking program integrety...");
    if (check_file_id() != 0) {
        puts("No good.");
        platform_quit();
        return 1;
    }
    check_args(argc, argv);
    if (compilecon() != 0) {
        errorexit("game.con");
    }

    puts("Init engine...");
    menus_init();
    initengine();
    timer_init();

    puts("Checking for multiplayer...");
    initmultiplayers(setup_byte(4), setup_byte(5));

    puts("Loading art...");
    loadpics("tiles000.art");
    puts("Init colorize/remaps...");
    loadlookup();
    loadpalette();

    card = fx_card_from_setup();
    if (setup_byte(1) != 0 && card >= 0) {
        puts("Init sound FX...");
        if (FX_SetupCard(card, &device) != 0)
            errorexit(FX_ErrorString(-1));
        loadsounds();
        if (FX_Init(card, device.MaxVoices, 2, 16, 11000) != 0)
            errorexit(FX_ErrorString(-1));
        FX_SetCallBack(sound_callback);
        FX_SetVolume(sound_volume);
    }
    if (setup_byte(2) != 0 && card >= 0) {
        puts("Init music...");
        if (MUSIC_Init(card, 0) != 0)
            errorexit(MUSIC_ErrorString(-1));
        MUSIC_SetVolume(music_volume);
    }

    setgamemode();
    printf("video %dx%d\n", xdim, ydim);

    if (game_level_set) {
        enterlevel(4);
        play_level(game_demorec);
    } else {
        int index = 1;
        while (!input_poll()) {
            demo_t demo;
            if (demo_load(index, &demo) != 0)
                errorexit("Demo not found.");
            game_volume = demo.volume;
            game_level = demo.level;
            game_skill = demo.skill;
            enterlevel(2);
            if (play_demo(&demo)) {
                demo_free(&demo);
                break;
            }
            demo_free(&demo);
            index++;
            if (index == 8)
                index = 0;
        }
    }

    platform_shutdown();
    platform_quit();
    return 0;
}
