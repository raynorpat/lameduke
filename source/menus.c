#include "menus.h"
#include "actors.h"
#include "game.h"
#include "global.h"
#include "premap.h"
#include "view.h"

#include "board.h"
#include "draw.h"
#include "display.h"
#include "videocfg.h"
#include "fx_man.h"
#include "music.h"
#include "sdl_input.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The 16-byte records 0x3C1C0 fills at 0x125708-0x1257A8 and 0x125410:
   where the items start, how many, the title tile and the item text. The
   record's last byte is never read; every page draws its text at shade 4. */
typedef struct {
    short x, y, count, tile;
    const char *text;
} menutype;

static const menutype mainmenu = { 100, 46, 8, 1129,
    "Begin a New Game\nRestore a Game\nInstructions\nGame Options\nOrdering Info\nDemo\nHigh Scores\nQuit" };
static const menutype episodemenu = { 100, 40, 5, 616,
    "Mr. Caliber\nMission Cockroach\nSuck Hole\nHard Landing\nBack" };
static const menutype skillmenu = { 100, 40, 5, 617, "Easy\nMedium\nHard\nHarder\nBack" };
static const menutype restoremenu = { 100, 40, 9, 613,
    "empty\nempty\nempty\nempty\nempty\nempty\nempty\nempty\nBack" };
static const menutype savemenu = { 100, 40, 9, 1137,
    "empty\nempty\nempty\nempty\nempty\nempty\nempty\nempty\nBack" };
static const menutype helpmenu = { 100, 40, 1, 615, "Back" };
static const menutype optionsmenu = { 80, 40, 6, 1128,
    "Sound Volume\nMusic Volume\nMouse Sensitivity\nJoystick\nVideo Options\nBack" };
static const menutype orderingmenu = { 100, 40, 1, 1127, "Back" };
static const menutype scoremenu = { 100, 0, 0, 1138, "" };
static const menutype ingamemenu = { 100, 40, 6, 1129,
    "Return to Game\nHow to Play Me\nSave a Game\nRestore a Game\nGame Options\nQuit to Main Menu" };

static short titlespin;                  /* word_125392 */
static short cursorx, cursory;           /* word_12539A, word_12539C */
static short click;                      /* word_125394: the buttons just released */
static int buttons;                      /* word_125398 */

/* 0x3C590: the items, one a line. The line under the cursor pulses and
   dims while a mouse button is held. */
static void drawmenutext(int x, int y, const char *s, int shade, int cy)
{
    int left = x, pic = -1;
    const unsigned char *p = (const unsigned char *)s;

    if (ps[0].f02 == 2)
        return;
    for (; *p; p++) {
        int sh;

        if (*p == '\n') {
            x = left;
            y += 16;
            continue;
        }
        if (*p == ' ') {
            x += 5;
            continue;
        }
        sh = shade;
        if (abs(y - cy + 8) < 8) {
            sh = hudshade();
            if (buttons)
                sh -= 4;
        }
        if (*p >= '0' && *p <= '9')
            pic = *p + 101;
        if (*p >= 'a' && *p <= 'z')
            pic = *p + 26;
        if (*p >= 'A' && *p <= 'Z')
            pic = *p + 32;
        /* any other character repeats the glyph before it, as in gametext */
        if (pic < 0)
            continue;
        rotatesprite(x, y, (short)pic, (signed char)sh, 0, sector[ps[screenpeek].cursectnum].floorpal);
        x += tilesizx[pic] + 1;
    }
}

/* 0x3C6F0: Q or a right click is -1, Esc -2, Enter, Space or a left click
   the item under the cursor counting from 1, else 0. Up and down step a
   line and wrap. */
static int menuinput(const menutype *m)
{
    int y = cursory;

    if (keystatus[0x10] || click == 2) {
        keystatus[0x10] = 0;
        return -1;
    }
    if (keystatus[0x01] || click == 2) {
        keystatus[0x01] = 0;
        return -2;
    }
    if (keystatus[0x1C] || keystatus[0x39] || click == 1) {
        keystatus[0x1C] = 0;
        keystatus[0x39] = 0;
        return (cursory - m->y) / 16 + 1;
    }
    if (keystatus[0x48] | keystatus[0xC8])
        y = cursory - 16;
    if (keystatus[0x50] | keystatus[0xD0])
        y += 16;
    if (keystatus[0x48] | keystatus[0xC8] | keystatus[0x50] | keystatus[0xD0]) {
        keystatus[0x48] = keystatus[0x50] = keystatus[0xC8] = keystatus[0xD0] = 0;
        cursorx = m->x;
        if (y < m->y + 8)
            y = m->y + 16 * m->count - 8;
        if (y > m->y + 16 * m->count - 8)
            y = m->y + 8;
    }
    cursory = (short)y;
    return 0;
}

/* 0x1A6F0: read the mouse, move the pointer by the sensitivity, and draw it
   in the 3D view. A click is the buttons, once they are let go. */
static void drawpointer(short pic)
{
    int shade = hudshade(), last = buttons, dx, dy, pal;

    getmousevalues(&dx, &dy, &buttons);
    if (buttons == 0 && last != 0)
        click = (short)last;
    if (buttons == 0 && last == 0)
        click = 0;
    if (mouse_present()) {
        cursorx = (short)(cursorx + (((mousesens + 129) * dx) >> 9));
        cursory = (short)(cursory + (((mousesens + 129) * dy) >> 9));
        if (cursorx < 0)
            cursorx = 0;
        if (cursorx > 319)
            cursorx = 319;
        if (cursory < 0)
            cursory = 0;
        if (cursory > 199)
            cursory = 199;
    }
    if (ps[screenpeek].f02 != 3)
        return;
    pal = sector[ps[screenpeek].cursectnum].floorpal;
    rotatesprite(cursorx, cursory, pic, (signed char)shade, 1, (unsigned char)(pal ? pal : ps[screenpeek].ba7));
}

/* The part every page shares: the title spins in, the items draw, and the
   keys and mouse pick one. */
static int runmenu(const menutype *m)
{
    /* a frame drawn between game loops leaves the spin where it is */
    if (viewtic) {
        if (titlespin >= 7844)
            titlespin = 0x2000;
        else
            titlespin = (short)(titlespin + 348);
    }
    rotatescalesprite(160 << 16, (m->y - tilesizy[m->tile] / 2 - 8) << 16, 8 * titlespin - 4096, titlespin,
        m->tile);
    drawmenutext(m->x, m->y, m->text, 4, cursory);
    return menuinput(m);
}

/* One of the Game Options bars at y: the left and right keys or a held
   button move it with the pointer, 0-255 across 32 segments. */
static void slider(int y, int *value, int video)
{
    int shade = 16, i;

    if (!video) {
        if (cursory > y - 2 && cursory < y + 14) {
            if (keystatus[0x4B] && viewtic) {
                cursorx = (short)(cursorx - 8);
                *value = cursorx >= 32 ? cursorx - 32 : 0;
            }
            if (keystatus[0x4D] && viewtic) {
                cursorx = (short)(cursorx + 8);
                *value = cursorx <= 288 ? cursorx - 32 : 255;
            }
            if (buttons) {
                *value = cursorx > 288 ? 255 : cursorx >= 32 ? cursorx - 32 : 0;
                shade = 0;
            }
        }
        for (i = 0; i < 32; i++) {
            if (i > *value >> 3)
                rotatesprite(8 * i + 32, y + 4, 679, (signed char)shade, 0, 0);
            else
                rotatesprite(8 * i + 32, y, 678, (signed char)shade, 0, 0);
        }
        return;
    }
    if (cursory > y - 2 && cursory < y + 14) {
        if (keystatus[0x4B] && viewtic) cursorx = (short)(cursorx - 4);
        if (keystatus[0x4D] && viewtic) cursorx = (short)(cursorx + 4);
        if (((keystatus[0x4B] || keystatus[0x4D]) && viewtic) || (buttons & 1)) {
            int x = cursorx - 176;
            *value = x < 0 ? 0 : x >= 128 ? 255 : x * 2;
        }
    }
    for (i = 0; i < 32; i++)
        rotatescalesprite((4 * i + 178) << 16, (y + 5) << 16, 32768, 0,
            (short)(i > *value >> 3 ? 679 : 678));
}

static void scoretext(int x, int y, const char *s, int shade);

/* Video settings apply at the next frame boundary, before the world is drawn. */
static videocfg_t video_pending;
static int video_pending_row = -1;
static int video_w[256], video_h[256], video_count, video_mode;
static int video_slider_key, video_slider_row;

void menus_applyvideo(void)
{
    int row = video_pending_row;
    if (row < 0) return;
    video_pending_row = -1;
    game_applyvideocfg(&video_pending);
    if (videocfg_apply_failed) {
        int i;
        for (i = 0; i < video_count; i++)
            if (video_w[i] == videocfg_active.width && video_h[i] == videocfg_active.height) video_mode = i;
        return;
    }
    switch (row) {
    case 0: videocfg.renderer = video_pending.renderer; break;
    case 1: videocfg.width = video_pending.width; videocfg.height = video_pending.height; break;
    case 2: videocfg.display = video_pending.display; break;
    case 3: videocfg.filter = video_pending.filter; break;
    case 4: videocfg.miplevel = video_pending.miplevel; break;
    case 5: videocfg.vsync = video_pending.vsync; break;
    case 6: videocfg.contrast = video_pending.contrast; break;
    case 7: videocfg.saturation = video_pending.saturation; break;
    case 8: videocfg.supersample = video_pending.supersample; break;
    }
    videocfg_write("duke.cfg", &videocfg);
}

static void video_addmode(int w, int h)
{
    int i;
    if (w < 320 || w > 7680 || h < 200 || h > 4320 || video_count >= 256) return;
    for (i = 0; i < video_count; i++)
        if (video_w[i] == w && video_h[i] == h) return;
    video_w[video_count] = w; video_h[video_count++] = h;
}

static void video_init(void)
{
    int w[256], h[256], dw, dh, n, i;
    n = getvideomodelist(w, h, 256, &dw, &dh);
    video_count = 0;
    for (i = 0; i < n; i++) video_addmode(w[i], h[i]);
    video_addmode(dw, dh);
    video_addmode(videocfg_active.width, videocfg_active.height);
    video_mode = 0;
    for (i = 0; i < video_count; i++)
        if (video_w[i] == videocfg_active.width && video_h[i] == videocfg_active.height) video_mode = i;
    cursorx = 160; cursory = 46;
    video_slider_key = 0; video_slider_row = -1;
}

static int video_dim(int row)
{
    if (row == 1) return videocfg_active.renderer < 2;
    if (row == 3 || row == 4) return videocfg_active.renderer != 3;
    if (row >= 6 && row <= 8) return videocfg_active.renderer != 3 || getvideoglpath() != 3;
    return 0;
}

static void videomenu(void)
{
    static const char *labels[] = { "Renderer", "Resolution", "Display", "Texture Filtering", "Mip Level", "VSync", "Contrast", "Saturation", "Supersample", "Back" };
    static const char *renderers[] = { "Classic", "Polymost Debug", "Polymost Software", "Polymost OpenGL" };
    static const char *displays[] = { "Windowed", "Fullscreen", "Borderless" };
    static const char *filters[] = { "Nearest", "Bilinear", "Trilinear" };
    static const char *samples[] = { "Off", "2K", "4K", "8K" };
    videocfg_t c = videocfg_active;
    char value[64];
    int row, i, dir = 0, enter, selected, v;
    if (viewtic) {
        if (titlespin >= 7844) titlespin = 0x2000;
        else titlespin = (short)(titlespin + 348);
    }
    rotatescalesprite(160 << 16, (40 - tilesizy[1128] / 2 - 8) << 16, 8 * titlespin - 4096, titlespin, 1128);
    selected = (cursory - 40) / 12;
    if (selected < 0) selected = 0;
    if (selected > 9) selected = 9;
    for (i = 0; i < 10; i++) {
        int y = 40 + 12 * i, shade = video_dim(i) ? 16 : 4;
        value[0] = 0;
        switch (i) {
        case 0: snprintf(value, sizeof value, "%s", renderers[c.renderer]); break;
        case 1: snprintf(value, sizeof value, "%dx%d", video_w[video_mode], video_h[video_mode]); break;
        case 2: snprintf(value, sizeof value, "%s", displays[c.display]); break;
        case 3: snprintf(value, sizeof value, "%s", filters[c.filter]); break;
        case 4: snprintf(value, sizeof value, "%d", c.miplevel); break;
        case 5: snprintf(value, sizeof value, "%s", c.vsync ? "On" : "Off"); break;
        case 6: snprintf(value, sizeof value, "%d", (int)(100 * c.contrast)); break;
        case 7: snprintf(value, sizeof value, "%d", (int)(100 * c.saturation)); break;
        case 8: snprintf(value, sizeof value, "%s", samples[c.supersample]); break;
        }
        if (i == selected && shade != 16) shade = 0;
        scoretext(8, y, labels[i], shade);
        if (i != selected || (i != 6 && i != 7) || video_dim(i))
            scoretext(178, y, value, shade);
    }
    if (videocfg_gl_failed) scoretext(8, 182, "OpenGL could not start", 16);
    if (keystatus[0x01] || keystatus[0x10] || click == 2) {
        keystatus[0x01] = keystatus[0x10] = 0; menupage = 30; return;
    }
    if (keystatus[0x48] || keystatus[0xC8] || keystatus[0x50] || keystatus[0xD0]) {
        int d = (keystatus[0x48] || keystatus[0xC8]) ? -1 : 1;
        keystatus[0x48] = keystatus[0xC8] = keystatus[0x50] = keystatus[0xD0] = 0;
        selected = (selected + d + 10) % 10;
        cursory = (short)(46 + 12 * selected);
        if (selected == 6) cursorx = (short)(176 + (c.contrast - .5) * 127.5);
        else if (selected == 7) cursorx = (short)(176 + c.saturation * 63.75);
    }
    row = selected;
    enter = keystatus[0x1c] || keystatus[0x39] || click == 1;
    keystatus[0x1c] = keystatus[0x39] = 0;
    if (keystatus[0x4b] || keystatus[0xcb]) dir = -1;
    if (keystatus[0x4d] || keystatus[0xcd]) dir = 1;
    if (row == 9 && enter) { menupage = 30; return; }
    if (video_slider_row != row) video_slider_key = 0;
    if (video_dim(row)) { video_slider_key = 0; keystatus[0x4b] = keystatus[0xcb] = keystatus[0x4d] = keystatus[0xcd] = 0; return; }
    if (row == 6 || row == 7) {
        if (dir && !viewtic) { video_slider_key = dir; video_slider_row = row; }
        if (viewtic && video_slider_key) {
            dir = video_slider_key; video_slider_key = 0;
            keystatus[dir < 0 ? 0x4b : 0x4d] = 1;
        }
        v = row == 6 ? (int)((c.contrast - .5) * 255) : (int)(c.saturation * 127.5);
        /* slider uses keypad arrows; normalize extended arrows for it. */
        if (keystatus[0xcb]) keystatus[0x4b] = 1;
        if (keystatus[0xcd]) keystatus[0x4d] = 1;
        slider(40 + 12 * row, &v, 1);
        if ((dir && viewtic) || (buttons & 1)) {
            if (row == 6) c.contrast = .5 + v / 255.0;
            else c.saturation = v / 127.5;
            video_pending = c; video_pending_row = row;
        }
    } else if (row == 1) {
        if (dir && video_count) video_mode = (video_mode + dir + video_count) % video_count;
        if (enter && video_count) { c.width = video_w[video_mode]; c.height = video_h[video_mode]; video_pending = c; video_pending_row = row; }
    } else if (dir || enter) {
        if (!dir) dir = 1;
        switch (row) {
        case 0:
            do { c.renderer = (c.renderer + dir + 4) % 4; }
            while (c.renderer == 1 || !rendereravailable(c.renderer) || (c.renderer == 3 && videocfg_gl_failed));
            break;
        case 2: c.display = (c.display + dir + 3) % 3; break;
        case 3: c.filter = (c.filter + dir + 3) % 3; break;
        case 4: c.miplevel = (c.miplevel + dir + 16) % 16; break;
        case 5: c.vsync = !c.vsync; break;
        case 8: do { c.supersample = (c.supersample + dir + 4) % 4; } while (!videosupersampleavailable(c.supersample)); break;
        }
        video_pending = c; video_pending_row = row;
    }
    if ((row != 6 && row != 7) || viewtic)
        keystatus[0x4b] = keystatus[0xcb] = keystatus[0x4d] = keystatus[0xcd] = 0;
}

/* The high score table (aItOnlyLooksPai, dword_82568), read and written with
   userdefs.tmp. */
char scorenames[12][33] = {
    "It only looks painful!", "It's fun to kill things.", "I like blood and guts.", "", "Call Toll Free.",
    "1-800-GAME123; that's a", "one-eight-hundred GAME 123.", "", " \"To be the best,", "  order the rest.\"",
    "", "",
};
int scores[12] = { 8675309, 131072, 40961, 666, 69, 4, 0, 0, 0, 0, 0, 0 };

/* 0x1A8B0's text: the small font, a space 2 px, lowercase 2 px lower. */
static void scoretext(int x, int y, const char *s, int shade)
{
    int left = x;

    if (ps[screenpeek].f02 == 2)
        return;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;

        if (c == '\n') {
            x = left;
            y += 8;
        } else if (c == ' ') {
            x += 2;
        } else {
            rotatesprite(x, y + (c >= 'a' && c <= 'z' ? 2 : 0), (short)(c + 991), (signed char)shade, 0, 0);
            x += tilesizx[c + 991] + 1;
        }
    }
}

/* 0x1A8B0: the table, the top quote pulsing. */
void drawscores(void)
{
    int shade = hudshade(), i;
    char buf[12];

    scoretext(30, 36, "#", 0);
    scoretext(50, 36, "Hero's Quote", 0);
    scoretext(270, 36, "Score", 0);
    for (i = 0; i < 12; i++) {
        snprintf(buf, sizeof buf, "%d", i);
        minitext(30, 11 * i + 50, buf, shade);
        scoretext(50, 11 * i + 50, scorenames[i], shade);
        snprintf(buf, sizeof buf, "%d", scores[i]);
        minitext(270, 11 * i + 50, buf, shade);
        shade = 0;
    }
}

/* 0x24E80: the row the score (ps.f36) earns, or -1. Rows 1-10 move down a
   place, and the row it lands on is overwritten rather than moved, as in
   the exe; its quote starts empty. */
int newscore(void)
{
    unsigned score = (unsigned)ps[screenpeek].f36;
    int slot = 0, k;

    hudshade();
    while (score <= (unsigned)scores[slot])
        if (++slot >= 12)
            return -1;
    for (k = 10; slot < k; k--) {
        scores[k + 1] = scores[k];
        memcpy(scorenames[k + 1], scorenames[k], 31);
    }
    scorenames[slot][0] = 0;
    scores[slot] = (int)score;
    return slot;
}

/* 0x24F80: type the quote on row slot; nonzero once Enter is pressed. */
int getscorename(int slot)
{
    return strget(50, 11 * slot + 50, hudshade(), 31) != 0;
}

/* 0x3CCF0, every frame while ps.gm has bit 0. menupage picks the page; bit
   0x80 marks it set up. Opened in play with no page asked for, it is the
   in-game menu. */
void menus(void)
{
    playertype *pl = &ps[screenpeek];
    int page, r;

    if (menupage == 0 && (pl->gm & 4))
        menupage = 100;
    if (menupage < 128) {
        cursorx = 160;
        menupage |= 0x80;
        cursory = 100;
        if ((menupage & 0x7f) == 31) video_init();
        click = 0;
        titlespin = 0;
    }
    page = menupage & 0x7F;
    switch (page) {
    case 0:
        switch (runmenu(&mainmenu)) {
        case -2:
        case -1:
        case 8:
            pl->gm = 0x80;
            break;
        case 1:
            menupage = 1;
            break;
        case 2:
            menupage = 10;
            break;
        case 3:
            menupage = 20;
            break;
        case 4:
            menupage = 30;
            break;
        case 5:
            menupage = 40;
            break;
        case 6:
            pl->gm = 6;
            break;
        case 7:
            menupage = 60;
            break;
        }
        break;
    case 1:
        r = runmenu(&episodemenu);
        if (r == -2 || r == -1 || r == 5) {
            menupage = 0;
        } else if (r >= 1 && r <= 4) {
            ps[0].volume = (unsigned char)(r - 1);
            menupage = 2;
        }
        break;
    case 2:
        r = runmenu(&skillmenu);
        if (r == -2 || r == -1 || r == 5) {
            menupage = 1;
        } else if (r >= 1 && r <= 4) {
            ps[0].skill = (unsigned char)(r - 1);
            newgame(ps[0].volume, 0, ps[0].skill);
            enterlevel(4);
        }
        break;
    case 10:
        r = runmenu(&restoremenu);
        if (r == -2 || r == -1 || r == 9)
            menupage = 0;
        else if (r >= 1 && r <= 8)
            loadplayer(r - 1);
        break;
    case 20:
        if (syncbits[0] & 1)
            gametext(10, 100, "Jump", 0);
        if (syncbits[0] & 2)
            gametext(10, 115, "Duck", 0);
        if (syncbits[0] & 4)
            gametext(10, 130, "Shoot", 0);
        if (syncbits[0] & 8)
            gametext(10, 145, "Open", 0);
        r = runmenu(&helpmenu);
        if (r == -2 || r == -1 || r == 1)
            menupage = 0;
        break;
    case 30:
        slider(optionsmenu.y, &sound_volume, 0);
        FX_SetVolume(sound_volume);
        slider(optionsmenu.y + 16, &music_volume, 0);
        MUSIC_SetVolume(music_volume);
        slider(optionsmenu.y + 32, &mousesens, 0);
        r = runmenu(&optionsmenu);
        if (r == -2 || r == -1 || r == 6)
            menupage = 0;
        else if (r == 5)
            menupage = 31;
        break;
    case 31:
        videomenu();
        break;
    case 40:
        r = runmenu(&orderingmenu);
        if (r == -2 || r == -1 || r == 1)
            menupage = 0;
        break;
    case 50:
        r = runmenu(&savemenu);
        if (r >= 1 && r <= 8)
            saveplayer(r - 1);
        if (r == -2 || r == -1 || (r >= 1 && r <= 9))
            menupage = 0;
        break;
    case 60:
        r = runmenu(&scoremenu);
        if (r == -2 || r == -1)
            menupage = 0;
        else
            drawscores();
        return;
    case 100:
        switch (runmenu(&ingamemenu)) {
        case -2:
        case 1:
            pl->gm = 4;
            break;
        case -1:
            pl->gm = 0x80;
            break;
        case 2:
            menupage = 20;
            break;
        case 3:
            menupage = 50;
            break;
        case 4:
            menupage = 10;
            break;
        case 5:
            menupage = 30;
            break;
        case 6:
            pl->gm = 2;
            break;
        }
        break;
    default:
        return;
    }
    drawpointer(614);
}
