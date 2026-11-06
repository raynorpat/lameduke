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
#include "buildmath.h"
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char banner[] =
    " DUKE DUKEM 3D Beta version 1.3.95 -- DO NOT DISTRIBUTE!!! That would be bad. ";

/* D3D.EXE 0x828F8. Digitized-sound menu index to the card number FX_ sees.
   Index 0 is "no digitized sound" and main does not call FX with it. */
static const int setup_fx_card[9] = { -1, 0, 1, 3, 4, 7, 9, 10, 12 };

static void save_video_settings(void)
{
    if (videocfg_write("duke.cfg", &videocfg) != 0)
        puts("Could not write duke.cfg.");
}

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
    int x, y, w, h, row;
    const unsigned char *src = framebuffer();
    unsigned char *gl = platform_capture(&w, &h);

    if (!gl) {
        w = xdim;
        h = ydim;
    }
    row = (w * 3 + 3) & ~3;
    fp = fopen(path, "wb");
    if (!fp) {
        free(gl);
        return;
    }
    if (!gl) {
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
    hdr[18] = (unsigned char)w;
    hdr[19] = (unsigned char)(w >> 8);
    hdr[22] = (unsigned char)h;
    hdr[23] = (unsigned char)(h >> 8);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, 54, fp);
    for (y = h - 1; y >= 0; y--) {
        for (x = 0; x < w; x++) {
            const unsigned char *c = gl ? gl + (y * w + x) * 3 : palette + src[y * w + x] * 3;
            unsigned char bgr[3];
            bgr[0] = c[2];
            bgr[1] = c[1];
            bgr[2] = c[0];
            fwrite(bgr, 1, 3, fp);
        }
        if (row > w * 3)
            fwrite("\0\0\0", 1, (size_t)(row - w * 3), fp);
    }
    fclose(fp);
    free(gl);
}

/* Debug aid: LAMEDUKE_SWITCH=<frame>:<renderer>[,...] switches renderer
   after that many frames are shown, as a menu would. */
static void debug_switch(int frame)
{
    static const char *spec;
    static int read;
    const char *p;

    if (!read) {
        spec = getenv("LAMEDUKE_SWITCH");
        read = 1;
    }
    for (p = spec; p && *p; p = strchr(p, ',') ? strchr(p, ',') + 1 : NULL) {
        const char *c = strchr(p, ':');

        if (!c)
            break;
        if (atoi(p) == frame) {
            game_setrenderer(atoi(c + 1));
            printf("renderer switch %d: requested %d, using %d\n",
                   frame, atoi(c + 1), getrendermode());
        }
    }
}

static void present(void)
{
    static int presented;
    const char *shot = getenv("LAMEDUKE_FRAME");
    const char *at = getenv("LAMEDUKE_FRAME_AT");

    menus_applyvideo();
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
    /* Debug aid: dump the Nth frame (default 1), then exit. Before it is
       shown: renderer 3's is read from the back buffer. */
    if (shot && shot[0] && presented + 1 >= (at && atoi(at) > 0 ? atoi(at) : 1)) {
        write_frame(shot);
        exit(0);
    }
    platform_present(framebuffer(), xdim, ydim);
    presented++;
    debug_switch(presented);
}

/* totalclock's ticks in t/per seconds. main (0x15BE7) runs sub_102A0 at
   120 Hz, and it steps by 6 with music on. Once a song starts, TaskMan
   (0x65CCC) takes the timer and chains sub_102A0 at only 1193182/65536 Hz. */
static int clock_tics(int t, int per, int *rem)
{
    int den = taskman ? 65536 * per : per;
    int n;

    /* a second at most, so the product stays in range after a stall */
    if (t > per)
        t = per;
    *rem += t * (taskman ? 1193182 : 120);
    n = *rem / den;
    *rem %= den;
    return setup_byte(2) != 0 ? n * 6 : n;
}

/* 0x14CD0's warp screen (/d, then W): Page Up and Down pick the episode,
   the arrows the level, and anything but Esc starts it. */
static void warp(void)
{
    char buf[12];

    while (!(keystatus[0x1C] | keystatus[0x9C] | keystatus[0x39] | keystatus[0x01])) {
        if (input_poll())
            return;
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
        if (ps[myconnectindex].f02 == 3) {
            rotatesprite(0, 0, 1170, 0, 0, 0);
            platform_present(framebuffer(), xdim, ydim);
            rotatesprite(0, 0, 1170, 0, 0, 0);
            platform_present(framebuffer(), xdim, ydim);
            setgamemode();
        } else
            setmapmode();
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
    int i, first;
    int clock = 0, target = 0, remainder = 0;
    uint32_t prev = timer_ticks();
    const char *shot = getenv("LAMEDUKE_FRAME");
    const char *trace = getenv("LAMEDUKE_TRACE");
    const char *nosmooth = getenv("LAMEDUKE_NOSMOOTH");

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
        /* the interrupt's count; the recording kept pace with it */
        totalclock += tics;
        /* blend from the last frame drawn, past any zero-tic samples since */
        if (i == 0 || demo->tics[i - 1])
            updateinterpolations();
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
        /* 0x15E53: samples run while totalclock is ahead of the demo's clock.
           The frames drawn while waiting fall between this sample and the next. */
        target += tics;
        for (first = 1;; first = 0) {
            uint32_t now;

            smoothratio = 65536;
            if (!(shot && shot[0]) && !(nosmooth && nosmooth[0]) && !(ps[myconnectindex].gm & 1))
                smoothratio = clock >= target ? 65536 : 65536 - (target - clock) * 65536 / tics;
            viewtic = first;
            present();
            viewtic = 1;
            smoothratio = 65536;
            if (demo_outcome() != PLAY_DONE)
                return demo_outcome();
            if ((shot && shot[0]) || clock >= target)
                break;
            if (input_poll())
                return PLAY_CLOSED;
            timer_delay(1);
            now = timer_ticks();
            clock += clock_tics((int)(now - prev), 1000, &remainder);
            prev = now;
        }
    }
    return PLAY_DONE;
}

/* Duke3D's main loop: a 120 Hz clock, and a logic step each time it passes
   ototalclock + TICSPERFRAME, every step with synctics = TICSPERFRAME. */
#define TICRATE 120
/* Duke3D's is TICRATE/26 (4); the prototype's per-step counters were paced
   on a 486's 15 loops a second, so 8 */
#define TICSPERFRAME 8

/* the wall clock in TICRATE ticks; *rem keeps the ms left over */
static void tick(uint32_t *prev, int *clock, int *rem)
{
    uint32_t now = timer_ticks(), ms = now - *prev;

    /* a gap past ~5 hours (a sleep) would overflow the multiply; more than
       a second is dropped as a stall anyway */
    if (ms > 1000)
        ms = 1000;
    *rem += (int)ms * TICRATE;
    *clock += *rem / 1000;
    *rem %= 1000;
    *prev = now;
}

static int play_level(void)
{
    uint32_t prev;
    int clock = 0, ototalclock = 0, rem = 0;
    const char *shot = getenv("LAMEDUKE_FRAME");
    const char *speed = getenv("LAMEDUKE_SPEED");
    const char *trace = getenv("LAMEDUKE_TRACE");
    const char *nosmooth = getenv("LAMEDUKE_NOSMOOTH");
    int logic = 0, frames = 0;

    prev = timer_ticks();
    present();
    while (!input_poll()) {
        int ran = 0, entered = 0;
        int fv, sv, av, bits;

        input_scriptframe(frames + 1);
        frames++;
        tick(&prev, &clock, &rem);
        /* a frame dump runs one step per frame, so frame N is always the same */
        if (shot && shot[0])
            clock = ototalclock + TICSPERFRAME;
        /* after a stall the lost time is not made up */
        else if (clock - ototalclock > TICRATE)
            ototalclock = clock - TICSPERFRAME;
        while (clock >= ototalclock + TICSPERFRAME) {
            ototalclock += TICSPERFRAME;
            ran = 1;
            functionkeys();
            /* getinput 0x30E60 gives nothing while the menu is open. */
            if (ps[myconnectindex].gm & 1) {
                input_clear();
                fv = sv = av = bits = 0;
            } else {
                input_sample(TICSPERFRAME, &fv, &sv, &av, &bits);
            }
            syncfvel[0] = (signed char)fv;
            syncsvel[0] = (signed char)sv;
            syncavel[0] = (signed char)av;
            syncbits[0] = bits;
            synctics = TICSPERFRAME;
            totalclock += TICSPERFRAME;
            updateinterpolations();
            domovethings();
            if (trace && trace[0])
                printf("sample %d %d %d %d %d %d\n", logic, ps[0].posx, ps[0].posy, ps[0].posz, ps[0].ang,
                    ps[0].cursectnum);
            if (speed && speed[0]) {
                printf("logic %d %u %d %d\n", logic + 1, (unsigned)timer_ticks(), ps[screenpeek].posx,
                    ps[screenpeek].posy);
                fflush(stdout);
            }
            logic++;
            /* 0x17411: an exit sets bit 3, and the next level starts. */
            if (ps[screenpeek].gm & 8) {
                ps[screenpeek].level++;
                enterlevel(4);
                tick(&prev, &clock, &rem);
                ototalclock = clock;
                entered = 1;
                break;
            }
        }
        if (entered)
            continue;
        /* duke3d's smoothratio: the share of this step gone by when the frame is drawn */
        smoothratio = 65536;
        if (!(shot && shot[0]) && !(nosmooth && nosmooth[0]) && !gamepaused && !(ps[myconnectindex].gm & 1)) {
            int gone;

            tick(&prev, &clock, &rem);
            gone = (clock - ototalclock) * 1000 + rem;
            smoothratio = gone >= TICSPERFRAME * 1000 ? 65536 : gone * 65536 / (TICSPERFRAME * 1000);
        }
        viewtic = ran;
        present();
        viewtic = 1;
        smoothratio = 65536;
        if (speed && speed[0]) {
            printf("frame %d %u\n", frames, (unsigned)timer_ticks());
            fflush(stdout);
        }
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

/* One line of the 0x19650 tally: label, percent, "%, =", points. */
static void tallyline(int y, const char *label, int pct, int worth, int shade)
{
    char buf[16];

    minitext(60, y - 10, label, shade);
    snprintf(buf, sizeof buf, "%d", (short)pct);
    minitext(60, y, buf, shade);
    minitext(72, y, "%, =", shade);
    snprintf(buf, sizeof buf, "%d", worth * (short)pct);
    minitext(100, y, buf, shade);
    minitext(140, y, "points.", shade);
    ps[screenpeek].f36 += worth * (short)pct;
}

/* 0x249B0's screen. With /d or without mode bit 4, tile 482 says which level
   loads. Otherwise 0x19650 shows the episode map with the levels done, plus
   the tally after an exit, until Enter, Space, Esc or 2048 clock ticks. The
   exe adds the tally to the score on every frame it shows it, as here. */
void levelscreen(int mode)
{
    static const short marks[8][2] = { { 9, 60 }, { 61, 82 }, { 84, 57 }, { 85, 112 },
                                       { 300, 64 }, { 261, 83 }, { 223, 97 }, { 0, 0 } };
    playertype *pl = &ps[0];
    short start = (short)totalclock;
    int pct = 0, rem = 0, i;
    uint32_t prev = timer_ticks();

    if (debugmode || !(mode & 4)) {
        rotatesprite(0, 0, 482, 0, 0, 0);
        minitext(170, 50, "Entering...", 0);
        minitext(174, 62, "Episode", 0);
        minitext(174, 74, "Level", 0);
        numtext(232, 62, pl->volume + 1, 0);
        numtext(232, 74, pl->level + 1, 0);
        show();
        return;
    }
    while (!keystatus[0x9C] && !keystatus[0x1C] && !keystatus[0x39] && !keystatus[0x01]
           && start + 2048 > totalclock) {
        int shade = hudshade();
        uint32_t now;

        if (input_poll())
            return;
        now = timer_ticks();
        totalclock += clock_tics((int)(now - prev), 1000, &rem);
        prev = now;
        rotatesprite(0, 0, pl->volume < 2 ? 1132 : 1135, 0, 0, 0);
        bigtext(36, 160, "Press SPACE to continue", shade);
        /* word_82304 has only episode 1's points; the rest are zeros and other data */
        for (i = 0; i < pl->level && i < 8; i++)
            rotatesprite(pl->volume ? 0 : marks[i][0], pl->volume ? 0 : marks[i][1], 1133, (signed char)shade, 0, 0);
        if (i < 8)
            rotatesprite(pl->volume ? 0 : marks[i][0], pl->volume ? 0 : marks[i][1], 1134, (signed char)shade, 0, 0);
        if (ps[myconnectindex].gm == 8) {
            bigtext(32, 10, "BONUS", 0);
            /* a zero count keeps the last percentage */
            if (start + 240 < totalclock) {
                if (pl->ba9)
                    pct = 100 * pl->baa / pl->ba9;
                tallyline(60, "Secret rooms Found", pct, 10000, shade);
            }
            if (start + 480 < totalclock) {
                if (pl->bac)
                    pct = 100 * pl->bad / pl->bac;
                tallyline(90, "Bad guys squashed", pct, 1000, shade);
            }
            if (start + 720 < totalclock) {
                if (pl->bae)
                    pct = 100 * pl->baf / pl->bae;
                tallyline(120, "Bad guys killed", pct, 1000, shade);
            }
        }
        show();
        /* nextpage waited for the 70 Hz retrace */
        timer_delay(14);
    }
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
            if (done)
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
    videocfg_read("duke.cfg", &videocfg);
    videocfg_active = videocfg;
    atexit(save_video_settings);
    if (compilecon() != 0) {
        errorexit("game.con");
    }

    puts("Init engine...");
#ifdef USE_OPENGL
    {
        extern int polymosttexfullbright;
        polymosttexfullbright = 240;
    }
#endif
    initengine();
    timer_init();

    puts("Checking for multiplayer...");
    initmultiplayers(setup_byte(4), setup_byte(5));

    puts("Loading art...");
    loadpics("tiles000.art");
    puts("Init colorize/remaps...");
    initplasma();
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

    game_setrenderer(game_renderer_arg >= 0 ? game_renderer_arg : videocfg.renderer);
    setgamemode();
    printf("video %dx%d\n", xdim, ydim);

    if (game_level_set) {
        newgame(game_volume, game_level, game_skill);
        enterlevel(4);
    }
    for (;;) {
        int r = PLAY_LEVEL;

        if (!game_level_set) {
            const char *pick = getenv("LAMEDUKE_DEMO");
            int index = pick && pick[0] ? atoi(pick) & 7 : 1;

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
