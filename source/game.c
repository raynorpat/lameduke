#include "game.h"
#include "premap.h"
#include "menus.h"
#include "sounds.h"
#include "demo.h"
#include "global.h"
#include "actors.h"
#include "view.h"

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
    static int presented;
    const char *shot = getenv("LAMEDUKE_FRAME");
    const char *at = getenv("LAMEDUKE_FRAME_AT");

    if (sintable() && palookup[0]) {
        playertype *pl = &ps[screenpeek];

        displayrooms(screenpeek);
        /* the level frame only; the attract loop never shows the prompt */
        if (demomode != 2 && (pl->gm & 0x10))
            typemessage();
        if (pl->gm & 1) {
            pl->gm &= ~0x10;
            menus();
        }
        /* 0x15FC3, 0x18DAD: a camera view carries the translucent logo. */
        if (cameraspr >= 0)
            rotatesprite(268, 156, 481, 0, 4, 0);
    }
    pansounds();
    platform_present(framebuffer(), xdim, ydim);
    presented++;
    /* Debug aid: dump the Nth frame (default 1), then exit. */
    if (shot && shot[0] && presented >= (at && atoi(at) > 0 ? atoi(at) : 1)) {
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

/* 0x14CD0's warp screen (/d, then W): Page Up and Down pick the episode,
   the arrows the level, and anything but Esc starts it. */
static void warp(void)
{
    char buf[12];

    while (!(keystatus[0x1C] | keystatus[0x9C] | keystatus[0x39] | keystatus[0x01])) {
        if (input_poll())
            break;
        drawview(myconnectindex);
        if (keystatus[0xC9]) {
            ps[0].volume++;
            keystatus[0xC9] = 0;
        }
        if (keystatus[0xD1]) {
            ps[0].volume--;
            keystatus[0xD1] = 0;
        }
        if (keystatus[0xC8] | keystatus[0x48]) {
            ps[0].level++;
            keystatus[0x48] = keystatus[0xC8] = 0;
        }
        if (keystatus[0x50] | keystatus[0xD0]) {
            ps[0].level--;
            keystatus[0x50] = keystatus[0xD0] = 0;
        }
        ps[0].volume &= 3;
        ps[0].level &= 15;
        if (ps[myconnectindex].f02 != 2) {
            bigtext(112, 50, "Warp Mode", 0);
            bigtext(100, 69, "Episode", 0);
            bigtext(100, 88, "Level", 0);
        }
        snprintf(buf, sizeof buf, "%d", ps[0].volume + 1);
        gametext(210, 67, buf, 0);
        snprintf(buf, sizeof buf, "%d", ps[0].level + 1);
        gametext(210, 86, buf, 0);
        platform_present(framebuffer(), xdim, ydim);
    }
    if (!keystatus[0x01])
        enterlevel(4);
    keystatus[0x01] = keystatus[0x39] = keystatus[0x9C] = keystatus[0x1C] = 0;
}

/* 0x14CD0, once a frame in the attract and level loops: F12 captures the
   screen, F10 steps the brightness, D toggles /d, M the song, S the
   effects, V the visibility, and K moves the view to the next player. */
static void functionkeys(void)
{
    if (keystatus[0x58]) {
        keystatus[0x58] = 0;
        screencapture();
    }
    if (keystatus[0x44]) {
        keystatus[0x44] = 0;
        if (++brightness > 4)
            brightness = 0;
        setbrightness(brightness);
    }
    if (keystatus[0x14]) {
        keystatus[0x14] = 0;
        ps[screenpeek].gm |= 0x10;
    }
    if (keystatus[0x20]) {
        keystatus[0x20] = 0;
        debugmode ^= 1;
    }
    if (setup_byte(2) != 0 && keystatus[0x32]) {
        keystatus[0x32] = 0;
        musicon ^= 1;
        if (musicon)
            MUSIC_Continue();
        else
            MUSIC_Pause();
    }
    if (setup_byte(1) != 0 && keystatus[0x1F]) {
        keystatus[0x1F] = 0;
        soundon ^= 1;
        if (!soundon)
            FX_StopAllSounds();
    }
    if (keystatus[0x2F]) {
        keystatus[0x2F] = 0;
        if (--user_visibility < 7)
            user_visibility = 16;
    }
    if (keystatus[0x25]) {
        keystatus[0x25] = 0;
        myconnectindex = connectpoint2[myconnectindex];
        if (myconnectindex == -1)
            myconnectindex = connecthead;
        /* pending: in the 2D map 0x53F42 redraws it; the 3D view clears the border with tile 1170 */
        if (ps[myconnectindex].f02 == 3)
            setgamemode();
        viewsize(myconnectindex);
    }
    if (keystatus[0x11] && debugmode && !(ps[screenpeek].gm & 0x10)) {
        keystatus[0x11] = 0;
        warp();
    }
}

/* How a demo or a level hands back to main. */
enum { PLAY_DONE, PLAY_CLOSED, PLAY_QUIT, PLAY_NEXTDEMO, PLAY_LEVEL, PLAY_ATTRACT };

/* A demo frame ends on gm: 0x80 quits, and once a new game or a restore
   sets bit 2's neighbour 4, bit 2 (the Demo item) still means the next
   demo (0x16007). */
static int demo_outcome(void)
{
    int gm = ps[screenpeek].gm;

    if (gm == 0x80)
        return PLAY_QUIT;
    if (gm & 4)
        return (gm & 2) ? PLAY_NEXTDEMO : PLAY_LEVEL;
    return PLAY_DONE;
}

static int play_demo(const demo_t *demo)
{
    int i;
    int hz = demo_hz(demo);
    int remainder = 0;
    uint32_t deadline = timer_ticks();
    const char *shot = getenv("LAMEDUKE_FRAME");
    const char *trace = getenv("LAMEDUKE_TRACE");

    for (i = 0; i < demo->count; i++) {
        int tics = demo->tics[i];

        if (input_poll())
            return PLAY_CLOSED;
        functionkeys();
        syncfvel[0] = (signed char)signed_byte(demo->forward[i]);
        syncsvel[0] = (signed char)signed_byte(demo->strafe[i]);
        syncavel[0] = (signed char)signed_byte(demo->turn[i]);
        syncbits[0] = demo->bits[i];
        synctics = (unsigned char)tics;
        domovethings();
        if (trace && trace[0])
            printf("sample %d %d %d %d %d %d\n", i, ps[0].posx, ps[0].posy, ps[0].posz, ps[0].ang,
                ps[0].cursectnum);
        if (tics == 0)
            continue;
        /* 0x15EDC: after a frame's samples the exe stops once it reaches count - 1,
           and zero-tic samples always run on into the next one. */
        if (i + 1 >= demo->count - 1)
            break;
        /* 0x15F51: in the attract loop Esc opens the main menu. */
        if (keystatus[0x01]) {
            keystatus[0x01] = 0;
            menupage = 0;
            ps[screenpeek].gm |= 1;
        }
        present();
        if (demo_outcome() != PLAY_DONE)
            return demo_outcome();
        if (shot && shot[0])
            continue;
        remainder += tics * 1000;
        deadline += (uint32_t)(remainder / hz);
        remainder %= hz;
        while ((int32_t)(deadline - timer_ticks()) > 0) {
            if (input_poll())
                return PLAY_CLOSED;
            SDL_Delay(1);
        }
    }
    return PLAY_DONE;
}

static int play_level(void)
{
    uint32_t prev;
    int clock_remainder = 0;

    prev = timer_ticks();
    present();
    while (!input_poll()) {
        uint32_t now = timer_ticks();
        int elapsed = (int)(now - prev);
        int irqs;
        int steps;
        int per_irq;
        int fv, sv, av, bits;

        /* Music byte selects the 6-step ISR. The PIT itself stays at 120 Hz. */
        per_irq = setup_byte(2) != 0 ? 6 : 1;
        if (elapsed < 0)
            elapsed = 0;
        prev = now;
        clock_remainder += elapsed * 120;
        irqs = clock_remainder / 1000;
        if (irqs < 1) {
            SDL_Delay(1);
            continue;
        }
        if (irqs > 40)
            irqs = 40;
        clock_remainder -= irqs * 1000;
        steps = irqs * per_irq;
        if (steps > 255)
            steps = 255;
        functionkeys();
        /* getinput 0x30E60 gives nothing while the menu is open. */
        if (ps[myconnectindex].gm & 1) {
            input_clear();
            fv = sv = av = bits = 0;
        } else {
            input_sample(steps, &fv, &sv, &av, &bits);
        }
        syncfvel[0] = (signed char)fv;
        syncsvel[0] = (signed char)sv;
        syncavel[0] = (signed char)av;
        syncbits[0] = bits;
        synctics = (unsigned char)steps;
        domovethings();
        /* 0x17411: an exit sets bit 3, and the next level starts. */
        if (ps[screenpeek].gm & 8) {
            ps[screenpeek].level++;
            enterlevel(4);
            continue;
        }
        present();
        /* 0x17482: Quit to Main Menu, then 0x16036: Quit */
        if (ps[screenpeek].gm & 2)
            return PLAY_ATTRACT;
        if (ps[screenpeek].gm & 0x80)
            return PLAY_QUIT;
    }
    return PLAY_CLOSED;
}

static void show(void)
{
    pansounds();
    platform_present(framebuffer(), xdim, ydim);
}

/* 0x1749E. Unless /d is on or the player left for the main menu, a score
   that makes the table asks for a quote, and the table stays up until Esc,
   Space or Enter. Then the demo and userdefs.tmp are written. */
static void quitgame(void)
{
    if (!debugmode && !(ps[screenpeek].gm & 2)) {
        int slot = newscore();

        if (slot >= 0) {
            int done = 0;

            while (!done) {
                if (input_poll())
                    break;
                drawview(screenpeek);
                drawscores();
                done = getscorename(slot);
                show();
            }
            memcpy(scorenames[slot], typebuf, 31);
        }
        do {
            if (input_poll())
                break;
            drawview(screenpeek);
            drawscores();
            show();
        } while (!keystatus[0x01] && !keystatus[0x39] && !keystatus[0x1C]);
    }
    demo_write();
    save_user_stats();
    errorexit("Game Over, or is it?");
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
        if (FX_Init(card, 4, 2, 16, 11000) != 0)
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
        newgame(game_volume, game_level, game_skill);
        enterlevel(4);
    }
    for (;;) {
        int r = PLAY_LEVEL;

        if (!game_level_set) {
            int index = 1;

            for (;;) {
                demo_t demo;

                if (demo_load(index, &demo) != 0)
                    errorexit("Demo not found.");
                /* 0x1B810 starts a new game from each demo's header. */
                newgame(demo.volume, demo.level, demo.skill);
                enterlevel(2);
                demomode = 2;
                r = play_demo(&demo);
                demo_free(&demo);
                if (r == PLAY_CLOSED || r == PLAY_QUIT || r == PLAY_LEVEL)
                    break;
                index++;
                if (index == 8)
                    index = 0;
            }
            /* 0x16026 */
            demomode = 0;
        }
        game_level_set = 0;
        if (r == PLAY_LEVEL)
            r = play_level();
        if (r == PLAY_ATTRACT)
            continue;
        if (r == PLAY_QUIT)
            quitgame();
        break;
    }
    /* the window was closed */
    demo_write();
    save_user_stats();
    platform_shutdown();
    platform_quit();
    return 0;
}
