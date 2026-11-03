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

/* 0x14050. Draws player p's view, or the demo camera's. */
void displayrooms(short p)
{
    playertype *pl = &ps[p];

    if (pl->f02 == 2)
        return; /* pending: 0x5445E draws the 2D map */
    /* pending: resizing (sync bits 0x80000/0x100000/0x400000) refills the border with tile 1170 */
    animatewalls();
    /* pending: 0x25410 renders the near viewscreen (1408) into its tile */
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
    /* pending: the tile 572 floor reflection over rows 99..199 */
}
