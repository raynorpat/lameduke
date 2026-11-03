/* The game's view: displayrooms 0x14050 and the wall and sprite frame
   animation it does around drawrooms. */

#include "view.h"
#include "global.h"
#include "actors.h"
#include "gamedef.h"
#include "sector.h"

#include "board.h"
#include "draw.h"
#include "math.h"
#include "tiles.h"
#include "sounds.h"
#include "premap.h"

/* byte_82364. The knee's frame list, shared with the trigger in player.c. */
extern const unsigned char kickframes[6][20];
extern int totalclock; /* 0x23CE28, the engine clock. It does not advance yet. */

/* word_12538E / dword_125248, and the sine walk at word_125390. */
short msgtime;
const char *msgtext = "";
static short hudpulse;

#define GOTPIC(p) (gotpic[(p) >> 3] & (1 << ((p) & 7)))

/* 0x25550. Steps the animated walls drawn this frame. */
static void animatewalls(void)
{
    int k;

    /* pending: tile 49 plasma (0x3E200) and the tile 570 view (0x21A10) when drawn */
    for (k = 0; k < numanimwalls; k++) {
        short w = animwall[k].wallnum, pic = wall[w].picnum;

        if (pic == 570)
            continue;
        if (pic >= 761 && pic <= 763) {
            wall[w].picnum = (short)animwall[k].tag;
            continue;
        }
        if (pic >= 764 && pic <= 767) {
            if ((wrand() & 0xFF) < 16) {
                animwall[k].tag = pic;
                wall[w].picnum = 761;
            }
            continue;
        }
        if ((pic >= 819 && pic <= 826) || (pic >= 860 && pic <= 867)) {
            wall[w].picnum = pic == 822 ? 819 : pic == 826 ? 823 : pic == 867 ? 864 : (short)(pic + 1);
            continue;
        }
        if (wall[w].overpicnum >= 1184 && wall[w].overpicnum <= 1186 && (wall[w].cstat & 0xFE)) {
            short a = (short)animwall[k].tag;

            wall[w].xpanning += sintable()[(a + 512) & 2047] >> 12;
            wall[w].ypanning += sintable()[a & 2047] >> 12;
            animwall[k].tag += 128;
            if (++wall[w].overpicnum > 1186)
                wall[w].overpicnum = 1184;
            if (wrand() & 4)
                wall[w].cstat ^= 0x80;
        }
    }
    gotpic[49 >> 3] &= ~(1 << (49 & 7));
    gotpic[568 >> 3] &= 0xEB;
}

/* The mirror test displayrooms does for actor frames that come in turned
   pairs: frames past the middle flip the sprite (cstat 4). */
static int mirrored(spritetype *s, int f, int last, int span)
{
    if (f <= last) {
        s->cstat &= ~4;
        return f;
    }
    s->cstat |= 4;
    return span - f;
}

/* The per-frame tile choice after drawrooms, against view angle viewang. */
static void animatesprites(int viewang)
{
    int k;

    for (k = 0; k < spritesortcnt; k++) {
        short i = tspritelist[k];
        spritetype *s = &sprite[i];
        int *t = hittype[i], a = (s->ang + 3200 - viewang) & 2047, f;

        if ((s->xrepeat | s->yrepeat) == 0)
            continue;
        if (s->picnum == 389)
            tspritepal[k] = 0;
        if (actorscrptr[s->picnum] && t[4]) {
            /* The action's viewtype picks how many angles the frames have. */
            int vt = script[t[4] + 2];

            if (vt >= 0)
                s->cstat &= ~0x10;
            else {
                vt = -vt;
                s->cstat |= 0x10;
            }
            switch (vt) {
            case 2:
                f = (a >> 8) & 1;
                break;
            case 3:
                f = mirrored(s, a / 192 % 5, 2, 4);
                break;
            case 5:
                f = mirrored(s, a >> 8, 4, 8);
                break;
            case 7:
                f = mirrored(s, a / 170, 6, 12);
                break;
            case 8:
                f = a >> 8;
                break;
            default:
                f = 0;
            }
            tspritepicnum[k] += (short)((short)t[3] * vt) + (short)script[t[4]] + f;
            continue;
        }
        switch (s->picnum) {
        case 1165:
            tspritepicnum[k] = (short)((t[0] & 1) + 1165);
            t[0] = (t[0] + 1) & 3;
            s->cstat |= 0xC;
            if (t[0] > 1)
                s->cstat &= 0xFB;
            if (t[0] > 2)
                s->cstat &= 0xF3;
            break;
        case 1280: {
            int mood = t[0];

            tspritepicnum[k] = 1280;
            f = a >> 8;
            if (f > 4) {
                if (mood == 2)
                    mood = 1;
                else if (mood == 1)
                    mood = 2;
            }
            f = mirrored(s, f, 4, 8);
            tspritepicnum[k] += 5 * mood + f;
            break;
        }
        case 283:
            if (sector[s->sectnum].floorpal != 2)
                tspritepal[k] = s->owner < 0 ? 0 : ps[s->yvel].ba7;
            break;
        case 263:
        case 264:
            if (t[0] == 0)
                break;
            f = mirrored(s, a >> 8, 4, 8);
            if (s->zvel < 0)
                f += 5 * (t[1] & 1);
            tspritepicnum[k] = (short)(f + 247);
            break;
        case 930:
        case 931:
            f = mirrored(s, a >> 8, 4, 8);
            if (s->picnum == 931)
                f--;
            if (t[4] == 1)
                f += 5;
            tspritepicnum[k] = (short)(s->picnum + f);
            break;
        }
    }
}

/* 0x126C0. A HUD tile, drawn only in the 3D view, paletted by the floor
   under the local player or by that player's own palette. */
static void overlaypal(int x, int y, int pic, int shade, int flags)
{
    playertype *me = &ps[myconnectindex];
    int pal;

    if (me->f02 != 3)
        return;
    pal = sector[me->cursectnum].floorpal;
    if (pal == 0)
        pal = me->ba7;
    rotatesprite(x, y, (short)pic, (signed char)shade, (unsigned char)flags, (unsigned char)pal);
}

/* 0x2F280, eleven words. Frame ba3 (1..11) draws 681 plus the word before it. */
static const short raiseframes[11] = { 0, 1, 2, 2, 3, 3, 3, 2, 2, 1, 0 };

/* 0x2FD20. The weapon coming up at the start of a level. 1 when it drew. */
static int displayraise(short p, int shade)
{
    playertype *pl = &ps[p];
    int bob;

    if (pl->ba3 == 0 || pl->w54 <= 0)
        return 0;
    bob = pl->lookang < 0 ? -pl->lookang : pl->lookang;
    bob /= 9;
    /* ba3 runs 1..11. Past that the raise is over and weapons() has cleared it. */
    if (pl->ba3 > 11)
        return 1;
    overlaypal((syncavel[p] >> 4) + 160 - (pl->lookang >> 1), bob + 180 - (pl->horiz >> 4),
        raiseframes[pl->ba3 - 1] + 681, shade, 3);
    if (pl->ba3 == 5)
        spritesound(7, pl->i);
    return 1;
}

/* How far the knee pulls back, from the frame byte at 0x3003D. */
static int kneepull(int frame)
{
    switch (frame) {
    case 2: return 10;
    case 3: return 20;
    case 4: return 30;
    case 5:
    case 6: return 40;
    case 7: return 60;
    case 8: return 80;
    case 9: return 100;
    case 10: return 130;
    default: return 0;
    }
}

/* 0x2FE40. The weapon in front of the view. Tile numbers are the ones the
   executable compares: 191, 389, 563, 721, 1140, 1230. */
static void displayweapon(short p)
{
    playertype *pl = &ps[p];
    int shade, bob, gunz, yb, sway, xbase, weapon, frame, aim, hz;

    if (pl->w54 <= 0)
        return;
    visibility = user_visibility;
    bob = pl->lookang < 0 ? -pl->lookang : pl->lookang;
    bob /= 9;
    shade = sector[pl->cursectnum].floorshade;
    if (shade < 0)
        shade = 0;
    else if (shade > 24)
        shade = 24;
    if (displayraise(p, shade))
        return;
    gunz = pl->ba1 & 0x7F;
    if (gunz < 2)
        return;
    /* (gunz << 4) - 32 is a byte, then the bob sine comes off, and the low
       byte is what every y below subtracts (0x2FF48). */
    yb = (unsigned char)((gunz << 4) - 32);
    yb = (yb - (sintable()[pl->f3a & 2047] >> 12)) & 255;
    sway = (syncbits[p] & 0x20) ? syncavel[p] >> 3 : 0;
    xbase = sway + 12 + 16 * (6 - gunz);
    weapon = (unsigned short)(pl->w5e >= 0 ? pl->w5e : pl->w7e);
    frame = pl->w7c;
    aim = pl->lookang >> 1;
    hz = pl->horiz >> 4;

    if (weapon == 1230) {
        int pull = kneepull(kickframes[pl->b91][frame]);
        int x = xbase - pull;
        int pic;

        if (frame != 0) {
            pic = kickframes[pl->b91][frame] + 1230;
            overlaypal((syncavel[p] >> 4) + x + 180 - aim, bob + 280 - yb - hz, pic, shade, 3);
            if (kickframes[pl->b91][frame] == 2) {
                if (pl->w4e == 0 || (sector[pl->cursectnum].lotag & 0x7FFF) == 2)
                    pic = 1239;
                else
                    pic = 1240 + (pl->ba4 % 3);
                overlaypal((syncavel[p] >> 4) + x + 229 - aim, bob + 207 - yb - hz, pic, shade, 3);
            }
        } else {
            overlaypal((syncavel[p] >> 4) + x + 180 - aim, bob + 280 - yb - hz, 1231, shade, 3);
        }
        return;
    }
    if (weapon == 563) {
        /* 0x2F296, nine bytes, read at w7c - 11 (0x30398). */
        static const unsigned char recoil[9] = { 40, 30, 22, 16, 12, 16, 22, 30, 30 };
        int xb;

        if (frame > 11) {
            int s = shade;

            if (frame <= 13)
                s = frame - 12;
            overlaypal((syncavel[p] >> 5) + xbase - recoil[frame - 11] + 270, 2 * bob + 186 - yb - hz, 1252, s, 3);
        } else {
            int ang = frame << 7;

            xb = xbase + (sintable()[(1024 - ang) & 2047] >> 9);
            yb = (yb + (sintable()[(1536 - ang) & 2047] >> 10)) & 255;
            if (frame < 4)
                overlaypal((syncavel[p] >> 5) + xb + 150, 2 * bob + 224 - yb - hz, 1250, shade, 3);
            else if (frame < 7)
                overlaypal((syncavel[p] >> 5) + xb + 184, 2 * bob + 180 - yb - hz, 1251, shade, 3);
            else
                overlaypal((syncavel[p] >> 5) + xb + 260, 2 * bob + 196 - yb - hz, 1252, shade, 3);
        }
        return;
    }
    if (weapon == 1140) {
        /* 0x2F2C9, eleven bytes. w7c 0 is tile 1140 itself. */
        static const unsigned char frames1140[11] = { 0, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0 };
        int pic = 1140;

        xbase = -32;
        if (frame > 4)
            yb = (yb - 12 * (frame - 4)) & 255;
        if (frame > 0 && frame < 11)
            pic = frames1140[frame] + 1140;
        overlaypal((syncavel[p] >> 4) + xbase + 150 - aim, bob + 258 - yb - hz, pic, shade, 3);
        return;
    }
    if (weapon == 721) {
        /* 0x2F2B4, twenty-one bytes copied onto the stack. */
        static const unsigned char frames721[21] = {
            0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
        };
        int pic = 1143;

        if (frame != 0) {
            if (frame < 7)
                yb = (yb - 10 * frame) & 255;
            else if (frame < 12)
                yb = (yb + 20 * (frame - 10)) & 255;
            else if (frame < 20)
                yb = (yb - 9 * (frame - 14)) & 255;
            if (frame > 0 && frame < 21)
                pic = frames721[frame] + 1143;
        }
        overlaypal((syncavel[p] >> 4) + xbase + 190 - aim,
            bob + (frame == 0 ? 260 : 250) - yb - hz, pic, shade, 3);
        return;
    }
    if (weapon == 389) {
        int x = (syncavel[p] >> 4) + xbase + 160 - aim;
        int y = bob + 240 - yb - hz;

        if (frame == 0)
            overlaypal(x, y, 882, shade, 3);
        if (frame == 4)
            shade = 0;
        if (frame == 5)
            visibility++;
        if (frame != 0 && frame != 6)
            overlaypal(x, y, frame + 881, shade, 3);
        return;
    }
    if (weapon != 191 || frame >= 27)
        return;
    /* 0x2F2AA: five words added to 1150 while the pistol is at rest or firing. */
    if (frame < 5) {
        static const short pistol[5] = { 0, 1, 2, 0, 0 };
        int x = 195 + 16 * (6 - gunz);

        if (frame == 1)
            shade = 0;
        if (frame == 2) {
            shade = 4;
            x -= 3;
        }
        overlaypal((syncavel[p] >> 3) + x - aim, bob + 240 - yb - hz, pistol[frame] + 1150, shade, 3);
        return;
    }
    if (frame < 10) {
        overlaypal((syncavel[p] >> 3) + 190 - aim, bob + 230 - yb - hz, 1160, shade, 3);
        return;
    }
    if (frame < 15) {
        if (frame == 10)
            spritesound(34, pl->i);
        if (frame == 13)
            spritesound(33, pl->i);
        overlaypal((syncavel[p] >> 3) + 240 - 8 * frame - aim, bob + 130 + 16 * frame - yb - hz, 1162, shade, 3);
        overlaypal((syncavel[p] >> 3) + 220 - aim, bob + 220 - yb - hz, 1161, shade, 3);
        return;
    }
    if (frame < 20) {
        overlaypal((syncavel[p] >> 3) + 120 + 2 * frame - aim, bob + 430 - yb - hz - 8 * frame, 1163, shade, 3);
        overlaypal((syncavel[p] >> 3) + 220 - aim, bob + 220 - yb - hz, 1161, shade, 3);
        return;
    }
    if (frame < 23) {
        overlaypal((syncavel[p] >> 3) + 180 - aim, bob + 235 - yb - hz, 1164, shade, 3);
        overlaypal((syncavel[p] >> 3) + 220 - aim, bob + 210 - yb - hz, 1161, shade, 3);
        return;
    }
    if (frame < 25) {
        overlaypal((syncavel[p] >> 3) + 160 - aim, bob + 245 - yb - hz, 1164, shade, 3);
        overlaypal((syncavel[p] >> 3) + 220 - aim, bob + 220 - yb - hz, 1161, shade, 3);
        return;
    }
    overlaypal((syncavel[p] >> 3) + 190 - aim, bob + 235 - yb - hz, 1160, shade, 3);
}

/* 0x73169 with base 10: a minus sign, no leading zeros, "0" for zero. */
static void numstring(int value, char *buf)
{
    char rev[12];
    int n = 0, d = 0;
    unsigned int u = (unsigned int)value;

    if (value < 0) {
        buf[n++] = '-';
        u = 0u - u;
    }
    do {
        rev[d++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    while (d)
        buf[n++] = rev[--d];
    buf[n] = 0;
}

/* 0x12850. */
int minitext(int x, int y, const char *s, int shade)
{
    int left = x;
    const unsigned char *p = (const unsigned char *)s;

    if (ps[myconnectindex].f02 == 2)
        return 0;
    if (*p == 0)
        return x;
    for (;;) {
        unsigned char c = *p++;

        if (c == '\n') {
            x = left;
            y += 8;
        } else if (c == ' ') {
            x += 2;
        } else {
            int pic = c + 991;
            int dy = (c >= 'a' && c <= 'z') ? 2 : 0;

            rotatesprite(x, y + dy, (short)pic, (signed char)shade, 0, 0);
            x += tilesizx[pic] + 1;
        }
        if (*p == 0)
            return x;
    }
}

/* 0x12750. The big font. A digit is the character plus 101, a lowercase
   letter plus 26 and 2 px lower, an uppercase letter plus 32. Any other
   character repeats the previous glyph. A space is 5 px, a newline drops
   16 and returns to the left edge. Nothing is drawn while the local player
   is in the 2D map. Returns the x the next glyph would use. */
int gametext(int x, int y, const char *s, int shade)
{
    int left = x;
    const unsigned char *p = (const unsigned char *)s;
    int pic = -1;

    if (ps[myconnectindex].f02 == 2)
        return 0;
    while (*p) {
        unsigned char c = *p++;
        int dy = 2;

        if (c == '\n') {
            x = (short)left;
            y += 16;
            continue;
        }
        if (c == ' ') {
            x += 5;
            continue;
        }
        if (c >= '0' && c <= '9')
            pic = c + 101;
        if (c >= 'a' && c <= 'z')
            pic = c + 26;
        if (c >= 'A' && c <= 'Z') {
            dy = 0;
            pic = c + 32;
        }
        /* A glyph that is not a letter or a digit, and is not preceded by
           one, reads an uninitialised tile in the exe. Skip it. */
        if (pic < 0)
            continue;
        rotatesprite(x, y + dy, (short)pic, (signed char)shade, 0, 0);
        x += tilesizx[pic] + 1;
    }
    return x;
}

/* 0x13800. */
void numtext(int x, int y, int value, int shade)
{
    char buf[16];

    numstring(value, buf);
    minitext(x, y, buf, shade);
}

/* The status digits, 0x17890 and the same walk inlined at 0x138F0.
   The ones place sits on x and each earlier glyph is 6 px left.
   The tile is the character plus 0x536. */
static void drawdigits(int x, int y, int value)
{
    char buf[16];
    int i, n;

    numstring(value, buf);
    for (n = 0; buf[n]; n++)
        ;
    for (i = n; i > 0; i--) {
        rotatesprite(x, y, (short)((unsigned char)buf[i - 1] + 1334), 0, 0, 0);
        x -= 6;
    }
}

/* Ammo digits, the centred weapon icon, and one pip per four rounds. */
static void drawammo(int amount, int icon)
{
    int i;

    drawdigits(209, 186, amount);
    rotatesprite(160, 192, (short)icon, 0, 1, 0);
    for (i = 0; i < 25; i++)
        if (i < amount >> 2)
            rotatesprite(3 * i + 216, 186, 1392, 0, 0, 0);
}

/* 0x17890. Border, score, health and ammo when the view is inset, plus the
   inventory panel. A sector lotag of 15 lays tile 1136 over the view. */
static void displaystatus(short p)
{
    playertype *pl = &ps[p];
    int i;

    if (pl->f02 != 3)
        return;
    if ((sector[pl->cursectnum].lotag & 0x7FFF) == 15)
        overlaypal(160, 120, 1136, 0, 1);
    if (pl->w4a >= 0) {
        rotatesprite(0, 0, 1380, 0, 0, 0);
        rotatesprite(160, 189, 1381, 0, 1, 0);
        drawdigits(82, 6, pl->f36);
        drawdigits(119, 186, pl->w54);
        for (i = 0; i < 25; i++)
            if (i < pl->w54 >> 2)
                rotatesprite(3 * i + 30, 186, 1392, 0, 0, 0);
        switch (pl->w7e) {
        case 191:
            drawammo(pl->w4c, 1401);
            break;
        case 389:
        case 1230:
            drawammo(pl->w4e, 1400);
            break;
        case 563:
            drawammo(pl->w52, 1403);
            break;
        case 721:
        case 1140:
            drawammo(pl->w50, 1402);
            break;
        }
    }
    if (pl->b92) {
        rotatesprite(0, 28, 1398, 0, 0, 0);
        if (pl->b95)
            rotatesprite(39, 41, 1399, 0, 0, 0);
        if (pl->b96)
            rotatesprite(39, 66, 1399, 0, 0, 0);
        if (pl->b93)
            rotatesprite(39, 93, 1399, 0, 0, 0);
        /* w58 is -1 after resetplayer; any non-zero still draws the mark. */
        if (pl->w58)
            rotatesprite(39, 119, 1399, 0, 0, 0);
        if (pl->b94)
            rotatesprite(39, 145, 1399, 0, 0, 0);
        if (pl->b99)
            rotatesprite(13, 39, 717, 0, 0, 0);
        if (pl->b9a)
            rotatesprite(14, 64, 719, 0, 0, 0);
        if (pl->b97)
            rotatesprite(13, 90, 716, 0, 0, 0);
        if (pl->b9b)
            rotatesprite(13, 117, 718, 0, 0, 0);
        if (pl->b98)
            rotatesprite(13, 141, 713, 0, 0, 0);
    }
}

/* 0x12400, and the level frame's copy at 0x16E94. The countdown line, then
   the help quote armed by quote(). The timer moves even when the map hides
   the glyphs. An inset view lifts the quote by 23. */
static void displayquotes(short p)
{
    playertype *pl = &ps[p];
    int y;

    if (msgtime > 0) {
        msgtime--;
        minitext(8, 162, msgtext, 0);
    }
    if (pl->w7a < 0 || pl->w7a >= 13)
        return;
    if ((&pl->w60)[pl->w7a] > 0) {
        (&pl->w60)[pl->w7a]--;
        y = pl->w4a >= 0 ? 166 : 189;
        minitext(24, y, fta_quotes[pl->w7a], 0);
    }
}

/* 0x123D0. Shade from the sine table; the index is the counter's low 11 bits. */
static int hudshade(void)
{
    hudpulse = (short)(hudpulse + 0x80);
    return (sintable()[hudpulse & 0x07FF] >> 10) + 16;
}

/* 0x16761. The four corners, and a reticle that blinks with totalclock bit 4,
   while the player is looking through a monitor. */
static void displaymonitor(void)
{
    int shade = hudshade();

    overlaypal(10, 10, 275, shade, 2);
    overlaypal(274, 10, 276, shade, 2);
    overlaypal(10, 160, 277, shade, 2);
    overlaypal(274, 160, 278, shade, 2);
    if (totalclock & 0x10)
        overlaypal(18, 20, 279, shade, 2);
}

/* 0x14C21. After the masks, a drawn tile 572 reflects the view into the
   transparent pixels from row 99 down. Each clear pixel takes the pixel
   above the run, keeps its low 5 bits, and darkens it; past 64 pixels, or
   outside shades 96..128, the pixel is 96. The run carries into the next
   column, as the counter does in the exe. */
static void reflectfloor(void)
{
    unsigned char *fb;
    int col, run, src, pitch;

    if (!GOTPIC(572) || !waloff[572])
        return;
    /* The loop is the 320-wide frame. Setup mode 7 is 640 and is not mirrored. */
    pitch = framebuffer_pitch();
    if (pitch != 320)
        return;
    fb = (unsigned char *)framebuffer();
    run = 0;
    src = 0;
    for (col = 0; col < 320; col++) {
        int p = col + 99 * pitch;
        int row;

        for (row = 99; row < 200; row++) {
            if (fb[p] == 255) {
                int v;

                if (run == 0) {
                    run = 1;
                    src = p - pitch;
                }
                /* A run that starts at row 99 walks off the top of the frame.
                   The exe reads whatever precedes the buffer; those pixels are 96. */
                if (src < 0)
                    v = 96;
                else {
                    v = (fb[src] & 31) + 96 - (run >> 1);
                    if (run > 63 || (unsigned char)v < 96 || (unsigned char)v > 128)
                        v = 96;
                    src -= pitch;
                }
                run++;
                fb[p] = (unsigned char)v;
            } else {
                run = 0;
            }
            p += pitch;
        }
    }
}

/* 0x21380. Renders a 96x96 view from (x,y,z,ang,sect) into tile pic. The
   exe copies the window into the tile row by row, draws the tile back with
   0x56C60 and copies again, which leaves the tile holding the view in its
   own column order; the copy below does that in one pass. */
static void rendertile(int x, int y, int z, short a, short sect, short pic)
{
    int u, v, pitch = framebuffer_pitch();
    const unsigned char *src = framebuffer();

    if (!waloff[pic] || tilesizx[pic] != 96 || tilesizy[pic] != 96)
        return;
    setview(0, 0, 95, 95);
    drawrooms(x, y, z, a, 100, sect);
    animatesprites(a);
    drawmasks();
    for (u = 0; u < 96; u++)
        for (v = 0; v < 96; v++)
            waloff[pic][u * 96 + v] = src[v * pitch + u];
    gotpic[pic >> 3] |= (unsigned char)(1 << (pic & 7));
    viewsize(myconnectindex);
}

/* 0x25410. Every twelfth frame the viewscreen near the player (1408) shows
   what its camera, or the player's open monitor, sees. */
static void updateviewscreen(short p)
{
    short vs = viewscreen, o;
    int *t;

    if (vs == -1)
        return;
    t = hittype[vs];
    if (t[0] < 11) {
        t[0]++;
        return;
    }
    t[0] = 0;
    if (ps[p].w56 >= 0)
        sprite[vs].owner = ps[p].w56;
    o = sprite[vs].owner;
    if (o >= 0 && abs(sprite[ps[p].i].x - sprite[vs].x) + abs(sprite[ps[p].i].y - sprite[vs].y) < 2048)
        rendertile(sprite[o].x, sprite[o].y, sprite[o].z, sprite[o].ang, sprite[o].sectnum, 1408);
}

/* 0x14050. Draws player p's view, or the demo camera's. */
void displayrooms(short p)
{
    playertype *pl = &ps[p];

    if (pl->f02 == 2)
        return; /* pending: 0x5445E draws the 2D map */
    /* pending: resizing (sync bits 0x80000/0x100000/0x400000) refills the border with tile 1170 */
    animatewalls();
    updateviewscreen(p);
    if (cameraspr >= 0) {
        spritetype *c = &sprite[cameraspr];

        if (c->yvel < 0)
            c->yvel = 0;
        else if (c->yvel > 199)
            c->yvel = 199;
        drawrooms(c->x, c->y, c->z, c->ang, c->yvel, c->sectnum);
        animatesprites(c->ang);
    } else {
        drawrooms(pl->posx, pl->posy, pl->posz, (short)(pl->ang + pl->lookang), pl->horiz, pl->cursectnum);
        animatesprites(pl->ang + pl->lookang);
    }
    drawmasks();
    reflectfloor();
    /* Level play draws the weapon, the status bar and the quotes. A monitor
       (w56 >= 0) draws the corners instead: the level frame when gm has bit 2,
       the demo frame always (0x1A420 is the same draw). */
    if (pl->w56 >= 0) {
        if ((pl->gm & 4) || demomode == 2)
            displaymonitor();
    } else if (cameraspr < 0) {
        displayweapon(p);
        displaystatus(p);
        displayquotes(p);
    }
}
