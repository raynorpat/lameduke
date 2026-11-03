/* Player input and weapons: processinput 0x31410 and the helpers it inlines
   or calls. Names of player fields follow global.h. */

#include "global.h"
#include "actors.h"
#include "sounds.h"
#include "gamedef.h"
#include "premap.h"
#include "sector.h"

#include "board.h"
#include "math.h"
#include "tiles.h"
#include "display.h"
#include "draw.h"
#include "engine.h"

#include "fx_man.h"

#include <stdlib.h>
#include <string.h>

#define APLAYER 283

static const unsigned char kickframes[6][20] = { /* byte_82364, indexed by b91 */
    { 0, 0, 1, 2, 3, 4, 4, 3, 2, 255 },
    { 0, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255 },
    { 0, 0, 1, 2, 3, 4, 3, 2, 255 },
    { 0, 0, 2, 3, 4, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 255 },
    { 0, 0, 2, 4, 6, 8, 7, 5, 4, 3, 1, 255 },
    { 0, 0, 1, 2, 3, 4, 3, 1, 0, 255 },
};

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

/* The help quotes at 0x1000 bytes into the strings, 32 bytes each. */
const char fta_quotes[13][32] = {
    "She's tougher than she looks!", "Go get the gun.", "The reactor must be destroyed!",
    "Enemy ALERT!!!", "The door is unlocked...", "The door is locked...", "Cola increases health.",
    "It's CRASHINGING!!!!!", "Shutting airlock bay doors.", "You found a secret room..!",
    "Pintched by the doors!", "ALL DOORS UNLOCKED!!!", "Falling to your bloody death."
};

/* 0x12630. With /q on, starts help quote q for the local player:
   w60[q] gets its display time and w7a remembers which quote it is. */
void quote(short q)
{
    playertype *p = &ps[myconnectindex];
    short *timer = &p->w60 + q;

    if (game_quotes != 1 || q < 0 || q >= 13 || *timer != 0)
        return;
    *timer = (short)(4 * strlen(fta_quotes[q]));
    p->w7a = q;
}

/* 0x25000. The nearest player by x+y distance. */
short findplayer(short i, int *dist)
{
    short p, best = 0;
    int d = 0x7FFFFFFF;

    for (p = connecthead; p >= 0; p = connectpoint2[p]) {
        int t = iabs(ps[p].posx - sprite[i].x) + iabs(ps[p].posy - sprite[i].y);

        if (t < d) {
            best = p;
            d = t;
        }
    }
    *dist = d;
    return best;
}

/* 0x35180 */
void addweapon(short p, short weapon, int force)
{
    unsigned char *have = NULL;

    switch (weapon) {
    case 191: have = &ps[p].b9d; break;
    case 389: have = &ps[p].b9e; break;
    case 563: have = &ps[p].ba0; break;
    case 721: have = &ps[p].b9f; break;
    case 1230: have = &ps[p].b9c; break;
    }
    if (have) {
        if (*have == 0 || force) {
            ps[p].ba1 = 5;
            ps[p].w5e = ps[p].w7e;
            ps[p].w7e = weapon;
        }
        *have = 1;
    }
    ps[p].w7c = 0;
}

/* 0x2F660. mode 1 steps a hitscan weapon sprite to what it hits (within 512)
   instead of firing; processinput inlines mode 0 once per weapon tile. */
void shoot(short i, short atwith, int mode)
{
    int sx, sy, sz, zvel;
    short sa, sect;

    if (mode) {
        short hitsect, hitwall, hitspr;
        int hx, hy, hz;
        spritetype *s = &sprite[i];

        hitscan(s->x, s->y, s->z, s->sectnum, sintable()[(s->ang + 512) & 2047], sintable()[s->ang & 2047], 0,
            &hitsect, &hitwall, &hitspr, &hx, &hy, &hz);
        if (ksqrt((s->y - hy) * (s->y - hy) + (s->x - hx) * (s->x - hx)) > 512)
            return;
        if (sector[s->sectnum].ceilingz + 2560 <= s->z || checkhitceiling(s->sectnum) == 0) {
            setsprite(i, hx, hy, hz);
            if (hitwall >= 0) {
                checkhitswitch(hitwall);
                checkhitwall(hitwall, hx, hy, hz, atwith);
            }
        }
        return;
    }
    if (sprite[i].picnum == APLAYER) {
        short p = sprite[i].yvel;

        sx = (syncavel[p] >> 4) + ps[p].posx;
        sy = (syncavel[p] >> 4) + ps[p].posy;
        sz = ps[p].posz + 3584;
        sa = (short)(ps[p].ang - 8);
        if (atwith == 563)
            sa = (short)(sa + ps[p].lookang);
        sect = ps[p].cursectnum;
        ps[p].bab = 1;
        if (atwith == 563)
            sz -= 1536;
        zvel = (atwith == 191 ? 31 : atwith == 563 ? 15 : 22) * (100 - ps[p].horiz);
        if (ps[p].w58 != 0)
            sprite[ps[p].w58].shade = 0;
    } else {
        int d;
        short p;

        sx = sprite[i].x;
        sy = sprite[i].y;
        sz = sprite[i].z - 0x2000;
        sa = sprite[i].ang;
        p = findplayer(i, &d);
        zvel = ((ps[p].posz - sprite[i].z) << 7)
            / (iabs(sprite[ps[p].i].x - sprite[i].x) + iabs(sprite[ps[p].i].y - sprite[i].y));
        sect = sprite[i].sectnum;
    }
    switch (atwith) {
    case 191: {
        short hitsect, hitwall, hitspr;
        int hx, hy, hz;

        sa = (short)(sa - 8);
        hitscan(sx, sy, sz, sect, sintable()[(sa + 512) & 2047], sintable()[sa & 2047], zvel << 6,
            &hitsect, &hitwall, &hitspr, &hx, &hy, &hz);
        spritesound(31, i);
        if (sector[hitsect].ceilingz + 1024 > hz)
            hz = sector[hitsect].ceilingz + 2048;
        if (sprite[i].picnum == APLAYER) {
            short k = EGS(hitsect, hx, hy, hz - 2816, 0, 1210, -16, 10, 10, sa, 64, zvel, i, 4);

            sprite[k].yvel = hitwall;
        } else {
            EGS(hitsect, hx, hy, hz - 2304, 0, 191, -16, 16, 16, sa, 64, zvel, i, 4);
        }
        break;
    }
    case 389:
        spritesound(1, i);
        spritesound(24, i);
        EGS(sect, sx + (sintable()[(sa + 512) & 2047] >> 6), sy + (sintable()[sa & 2047] >> 6), sz - 2048,
            1, 389, -127, 12, 12, sa, 0, zvel, i, 4);
        break;
    case 563:
        spritesound(38, i);
        EGS(sect, sx + (sintable()[(sa + 640) & 2047] >> 5), sy + (sintable()[(sa + 128) & 2047] >> 5),
            sz - 2304, 0x41, 563, -16, 16, 16, sa, 0, zvel, i, 4);
        break;
    case 1230:
        sx += sintable()[(sa + 544) & 2047] >> 7;
        sy += sintable()[(sa + 32) & 2047] >> 7;
        EGS(sect, sx, sy, sz, 0, 46, -16, 0, 0, sa, 32, 0, i, 4);
        /* The exe sets the shooter's own extra from the 1230 actor's strength. */
        if (actorscrptr[1230])
            sprite[i].extra = (short)script[actorscrptr[1230]];
        break;
    }
}

/* The second half of 0x31410: the kick, the trigger and the weapon frames. */
static void weapons(short snum)
{
    playertype *p = &ps[snum];
    unsigned char kick;

    if (p->ba3 && lockclock - p->b8b > 5) {
        p->b8b = (unsigned char)lockclock;
        p->ba3++;
        if (p->ba3 == 12 || (syncbits[snum] & 4)) {
            p->ba3 = 0;
            p->ba1 = 0x81;
        }
        return;
    }
    kick = p->ba1;
    if ((kick & 0x7F) > 2 && p->w7e == 191 && p->w7c < 5) {
        short hitsect, hitwall, hitspr, k;
        int hx, hy, hz, dist;
        short a = (short)(16 * (6 - (p->ba1 & 0x7F)) + p->ang - 16);

        hitscan(p->posx + (sintable()[(a + 512) & 2047] >> 8), p->posy + (sintable()[a & 2047] >> 8),
            p->posz - ((p->ba1 & 0x7F) << 10) + 5376, p->cursectnum,
            sintable()[(a + (unsigned short)(syncavel[snum] >> 3) + 512) & 2047],
            sintable()[(a + (unsigned short)(syncavel[snum] >> 3)) & 2047],
            (100 - p->horiz) << 11, &hitsect, &hitwall, &hitspr, &hx, &hy, &hz);
        a = (short)(a + 1024);
        k = EGS(hitsect, (sintable()[(a + 512) & 2047] >> 8) + hx, hy + (sintable()[a & 2047] >> 8),
            (sintable()[p->f3a & 2047] >> 8) + hz + 1024, 0, 214, -16, 12, 12, a, 0, 0, p->i, 5);
        dist = (ksqrt((ps[myconnectindex].posy - sprite[k].y) * (ps[myconnectindex].posy - sprite[k].y)
                   + (ps[myconnectindex].posx - sprite[k].x) * (ps[myconnectindex].posx - sprite[k].x)) >> 8) + 1;
        if (dist > 32)
            dist = 32;
        sprite[k].xrepeat = sprite[k].yrepeat = (unsigned char)dist;
        if (sector[sprite[k].sectnum].floorz - 768 < sprite[k].z)
            sprite[k].z -= 768;
    }
    if ((kick & 0x7F) == 0 || (kick & 0x7F) == 6) {
        if (p->w5e >= 0) {
            p->ba1 = 0x81;
            p->w5e = -1;
        }
    } else if (!(kick & 0x80)) {
        p->ba1--;
    } else {
        p->ba1++;
    }

    if ((syncbits[snum] & 4) && p->w7c == 0 && p->w5e == -1) {
        if ((p->ba1 & 0x7F) != 6)
            p->ba1 = 0x81;
        switch (p->w7e) {
        case 191:
            if (p->w4c > 0) {
                p->w7c = 1;
                p->w4c--;
            }
            break;
        case 389:
            if (p->w4e > 0) {
                p->w7c = 2;
                p->w4e--;
            }
            break;
        case 1230:
            p->b91 = (unsigned char)(p->w4e ? 5 : krand() % 5);
            p->w7c = 1;
            break;
        case 563:
        case 721:
        case 1140:
            p->w7c = 1;
            break;
        }
        if (p->w7c)
            p->b8b = (unsigned char)lockclock;
        return;
    }
    if (p->w7c == 0 || lockclock - p->b8b <= 7)
        return;
    p->b8b = (unsigned char)lockclock;
    switch (p->w7e) {
    case 191:
        if (p->w7c == 1)
            shoot(p->i, 191, 0);
        else if (p->w7c == 2)
            spawn(p->i, 1165);
        p->w7c++;
        if (p->w7c >= 5 && (p->w4c % 12 != 0 || p->w4c == 0))
            p->w7c = 0;
        if (p->w7c == 27)
            p->w7c = 0;
        break;
    case 389:
        if (++p->w7c >= 6)
            p->w7c = 0;
        if (p->w7c == 3)
            shoot(p->i, 389, 0);
        break;
    case 563:
        if (p->w52 > 0 && (syncbits[snum] & 4) && p->w7c == 10) {
            p->w7c = 11;
            p->w52--;
            shoot(p->i, 563, 0);
            p->horiz += 64;
        }
        if (p->w7c <= 10) {
            if (p->w7c != 0 && p->w7c < 10)
                p->w7c++;
        } else if (p->w7c >= 18) {
            p->w7c = 10;
        } else {
            p->w7c++;
        }
        break;
    case 721:
        if (++p->w7c == 11) {
            if (p->w50 > 0)
                p->w50--;
            EGS(p->cursectnum, (sintable()[(p->ang + 512) & 2047] >> 4) + p->posx,
                (sintable()[p->ang & 2047] >> 4) + p->posy, p->posz + 1024, 1, 721, -16, 16, 16,
                p->ang, 128, -348, p->i, 1);
            p->b90 = 1;
        }
        if (p->w7c > 19) {
            p->w7e = 1140;
            p->w7c = 0;
        }
        break;
    case 1140:
        if (++p->w7c == 2)
            p->b90 = 0;
        if (p->w7c == 10) {
            p->w7c = 0;
            if (p->w50)
                p->w7e = 721;
            else
                addweapon(snum, 191, 0);
        }
        break;
    case 1230: {
        unsigned char f;

        p->w7c++;
        f = kickframes[p->b91][p->w7c];
        if (f < 4) {
            if (f == 3 && p->w7c == 4 && p->w4e && (syncbits[snum] & 4)
                && (sector[p->cursectnum].lotag & 0x7FFF) != 2) {
                shoot(p->i, 1230, 0);
                spritesound(24, p->i);
                p->w4e--;
                p->w7c--;
                p->ba4++;
            }
        } else if (f == 4) {
            shoot(p->i, 1230, 0);
        } else if (f == 255) {
            p->w7c = 0;
        }
        break;
    }
    }
}

/* 0x31410 */
void processinput(short snum)
{
    playertype *p = &ps[snum];
    int tics = synctics, tics2, cz, ch, fz, fh, xvect = 0, yvect = 0, d;
    short lotag;
    int eye;

    if (p->w54 <= 0) {
        short s = p->cursectnum;

        if ((sector[s].lotag & 0x7FFF) == 2) {
            if (iabs(p->posz - sector[s].floorz) > 4608)
                p->posz += 348;
            updatesector(p->posx, p->posy, &p->cursectnum);
            clipmove(&p->posx, &p->posy, &p->posz, &p->cursectnum, 0, 0, 144, 1024, 1024, 0);
            setsprite(p->i, p->posx, p->posy, p->posz);
        }
        return;
    }
    if (p->w56 >= 0) {
        spritetype *c = &sprite[p->w56];

        p->posx = c->x;
        p->posy = c->y;
        p->posz = c->z;
        p->ang = c->ang;
        return;
    }
    {
        short *slot = &p->w60 + p->w7a; /* w60[w7a]: the timer of the current weapon slot */

        if (*slot > 1)
            (*slot)--;
    }
    if (syncbits[snum] & 0x40)
        p->lookang -= 192;
    else if (syncbits[snum] < 0)
        p->lookang += 192;
    p->lookang -= p->lookang / 3;
    if (p->w82 >= 0)
        goto domove;

    getzrange(p->posx, p->posy, p->posz, p->cursectnum, &cz, &ch, &fz, &fh, 128, 0);
    if (p->b94 == 0) {
        int dx = p->f1a - p->posx, dy = p->f1e - p->posy;

        p->f3a += ksqrt(dx * dx + dy * dy);
    }
    p->f1a = p->posx;
    p->f1e = p->posy;
    p->f22 = p->posz;
    p->f42 = p->ang;
    if (p->b95)
        p->b95--;

    lotag = sector[p->cursectnum].lotag & 0x7FFF;
    if (lotag == 2) {
        if (krand() >> 8 >= 239)
            spritesound(10, p->i);
        if (p->b93) {
            FX_SetVolume(sound_volume - 32);
            if (--p->w5c < 1) {
                p->b93 = 0;
                p->w5c = 1728;
            }
        } else if (p->w5c <= 0) {
            if (!debugmode && p->b96) {
                p->b96 -= 4;
                FX_SetVolume(sound_volume);
            } else if (p->w54 > 0) {
                p->w54--;
                FX_SetVolume(sound_volume);
            } else {
                p->w54 = 0;
                FX_SetVolume(sound_volume);
            }
        } else {
            p->w5c -= 32;
            FX_SetVolume(sound_volume);
        }
    } else {
        FX_SetVolume(sound_volume);
        if (p->b93 == 0)
            p->w5c = 1728;
    }

    lotag = sector[p->cursectnum].lotag & 0x7FFF;
    tics2 = tics * 2;
    if (lotag == 2) {
        if (syncfvel[snum] < 0)
            syncfvel[snum] >>= 2;
        p->w5a = 0;
        if (syncbits[snum] & 1) {
            if (p->f26 <= -2048)
                p->f26 = -2048;
            else
                p->f26 -= 348;
        } else if (syncbits[snum] & 2) {
            if (p->f26 >= 2048)
                p->f26 = 2048;
            else
                p->f26 += 348;
        } else {
            p->f3a += 32;
            p->f26 = ((sintable()[(2 * p->f3a) & 2047] >> 8) + p->f26) >> 1;
        }
        p->posz += p->f26;
        if (fz - 3840 < p->posz)
            p->posz += (fz - 3840 - p->posz) >> 1;
        if (cz + 1024 > p->posz) {
            p->posz = cz + 1024;
            p->f26 = 0;
        }
        goto doturn;
    }
    if (p->b94) {
        p->b8f = 0;
        p->w5a = 0;
        if (syncbits[snum] & 0x20)
            tics = tics2;
        if (p->b94 < 11) {
            if (p->b94 == 10)
                spritesound(19, p->i);
            p->posz -= ++p->b94 << 7;
        }
        if (syncbits[snum] & 1)
            p->posz -= 2048;
        if (syncbits[snum] & 2)
            p->posz += 3096;
        if (syncfvel[snum] > 0)
            p->horiz -= 16;
        if (syncfvel[snum] < 0)
            p->horiz += 16;
        if (fz - 4096 < p->posz)
            p->posz += (fz - 4096 - p->posz) >> 1;
        if (cz + 4096 > p->posz)
            p->posz += (cz + 4096 - p->posz) >> 1;
        goto doturn;
    }
    stopsound(19);
    if ((sector[p->cursectnum].lotag & 0x7FFF) == 1) {
        int s = sintable()[((p->f3a + 128) >> 1) & 2047];

        p->f3a += 128;
        eye = 34;
        p->posz += iabs(s) >> 6;
        if (p->b8f == 0 && (syncbits[snum] & 0x20))
            tics = tics2;
    } else {
        eye = 40;
        if (!(syncbits[snum] & 2) && (syncbits[snum] & 0x20))
            tics = tics2;
    }
    if (fz - (eye << 8) <= p->posz) {
        if (p->b8f == 0 && p->f26 > 3072) {
            spritesound(9, p->i);
            p->ba8 = (unsigned char)(p->f26 >> 10);
        }
        p->b8f = 1;
        if (eye == 40) {
            p->posz += (fz - (eye << 8) - p->posz) >> 1;
            if (p->f26 > 0)
                p->f26 -= 512;
            else
                p->f26 = 0;
        } else {
            p->posz += (fz - (eye << 8) - p->posz) >> 2;
            if (p->f26 > 0)
                p->f26 -= 64;
            else
                p->f26 = 0;
        }
        if ((syncbits[snum] & 1) && p->w5a == 0 && fz - cz > 17408) {
            p->w5a = 348;
            spritesound(8, p->i);
        }
        if ((syncbits[snum] & 2) && p->b8f)
            p->posz += eye == 40 ? 3072 : 2048;
    } else {
        p->b8f = 0;
        if (p->f26 < 6144)
            p->f26 += 320;
        p->f3a = 0;
    }
    if (p->w5a) {
        p->f3a = 1024;
        if (p->w5a >= 1024) {
            p->w5a = 0;
            p->f26 = 0;
        } else {
            p->b8f = 0;
            p->f26 -= sintable()[p->w5a] >> 5;
            p->w5a += 92;
        }
    }
    if (cz + 4096 > p->posz) {
        p->posz = cz + 4096;
        p->w5a = 0;
        p->f26 = 512;
    }
    p->posz += p->f26;

doturn:
    if (syncavel[snum])
        p->ang = (short)((tics * syncavel[snum] / 24 + p->ang) & 2047);
    if (iabs(cz - fz) < 7168) {
        lotag = sector[p->cursectnum].lotag & 0x7FFF;
        if (lotag > 2 && lotag != 23) {
            sector[p->cursectnum].ceilingz -= 1024;
            if (debugmode || p->b96 == 0) {
                if (p->w54 <= 0)
                    p->w54 = 0;
                else
                    p->w54 -= 5;
            } else {
                p->b96 -= 4;
            }
        }
    }
    if (syncfvel[snum] | syncsvel[snum]) {
        if (sintable()[p->f3a & 2047] >> 12 != 3) {
            p->ba5 = 0;
        } else if (p->ba5 == 0 && p->b8f) {
            lotag = sector[p->cursectnum].lotag & 0x7FFF;
            if (lotag == 1) {
                spritesound(28, p->i);
                p->ba5 = 1;
            } else if (lotag != 2) {
                spritesound(2, p->i);
                p->ba5 = 1;
            }
        }
        if (p->ba8)
            tics = 0;
        if (syncfvel[snum]) {
            xvect += (tics * syncfvel[snum] * sintable()[(p->ang + 2560) & 2047]) >> 3;
            yvect += (tics * syncfvel[snum] * sintable()[(p->ang + 2048) & 2047]) >> 3;
        }
        if (syncsvel[snum]) {
            xvect += (tics * syncsvel[snum] * sintable()[(p->ang + 2048) & 2047]) >> 3;
            yvect += (tics * syncsvel[snum] * sintable()[(p->ang + 1536) & 2047]) >> 3;
        }
        if (syncbits[snum] & 2) {
            xvect >>= 1;
            yvect >>= 1;
        }
        p->horiz += sintable()[(p->f3a + 1024) & 2047] >> 13;
        if (p->horiz > 50 && p->horiz < 150)
            p->horiz += 32 - p->horiz / 3;
    }

domove:
    updatesector(p->posx, p->posy, &p->cursectnum);
    clipmove(&p->posx, &p->posy, &p->posz, &p->cursectnum, xvect, yvect, 144, 1024, 1024, 0);
    setsprite(p->i, p->posx, p->posy, p->posz);
    if ((syncbits[snum] & 0x40000) || p->b94 || p->w7e == 563 || p->ba8) {
        p->horiz += 32 - p->horiz / 3;
        if (p->w7e == 563 || p->b94) {
            if (syncbits[snum] & 0x2000)
                p->horiz += 64;
            else if (syncbits[snum] & 0x4000)
                p->horiz -= 64;
        }
    } else if (syncbits[snum] & 0x2000) {
        p->horiz += 48;
    } else if (syncbits[snum] & 0x4000) {
        p->horiz -= 48;
    }
    if (p->ba8) {
        p->ba8--;
        p->horiz -= 13 * p->ba8;
    }
    if (p->horiz > 199)
        p->horiz = 199;
    else if (p->horiz < 0)
        p->horiz = 0;

    weapons(snum);
    (void)d;
}

/* 0x21DE0. Puts player p back at its start with full health. */
void resetplayer(short p)
{
    playertype *pl = &ps[p];

    pl->horiz = 100;
    pl->f3a = 0;
    pl->posx = pl->oposx;
    pl->posy = pl->oposy;
    pl->posz = pl->oposz;
    pl->ba4 = 0;
    pl->ang = pl->oang;
    pl->cursectnum = pl->ocursect;
    pl->b8f = 0;
    pl->lookang = (short)(256 - (krand() & 512));
    pl->w5a = 0;
    pl->f26 = 0;
    pl->b8b = 0;
    pl->w80 = -1;
    pl->w58 = -1;
    pl->w56 = -1;
    pl->ba8 = 0;
    pl->w54 = actorscrptr[APLAYER] ? (short)script[actorscrptr[APLAYER]] : 100;
    pl->w82 = -1;
    pl->b90 = 0;
    pl->w7c = 5;
    pl->ba1 = 6;
    pl->ba3 = 1;
    pl->f06 = 768;
}

unsigned char menucursor; /* byte_1BCA11 */
int menupage;             /* dword_1252F0: which menu or help screen b8a bit 0 opens */

/* 0x3E0D0. The local player's view window, w4a pixels in from each edge. */
void viewsize(short p)
{
    int b = ps[p].w4a < 0 ? 0 : ps[p].w4a;

    if (p == myconnectindex)
        setview(b, b, 319 - b, 199 - b);
}

static void resizeview(short p)
{
    if (setup_byte(0) == 7)
        setview(0, 0, 639, 479);
    else
        viewsize(p);
}

/* 0x29720. Player p's one-shot keys: view size and map zoom every tic,
   and once per press pause, menus, the map, weapon picks, inventory,
   and with /d the door cheat. */
void playerkeys(short p)
{
    playertype *pl = &ps[p];
    int bits = syncbits[p];
    short s, j;

    if ((bits & 0x10000) && pl->ba3 == 0)
        pl->ba3 = 1;
    if (pl->f02 == 2) {
        if ((bits & 0x80000) && pl->f06 < 4096)
            pl->f06 += pl->f06 >> 3;
        if ((bits & 0x100000) && pl->f06 > 48)
            pl->f06 -= pl->f06 >> 3;
    } else {
        if (bits & 0x80000) {
            if (pl->w4a > -4)
                pl->w4a -= 4;
            resizeview(p);
        }
        if (bits & 0x100000) {
            if (pl->w4a < 64)
                pl->w4a += 4;
            resizeview(p);
        }
    }
    if ((unsigned)bits < 0xFF) {
        pl->ba6 = 0;
        return;
    }
    if (pl->ba6)
        return;
    pl->ba6 = 1;
    if (bits & 0x200000)
        gamepaused = !gamepaused;
    if (gamepaused)
        return;
    if (bits < 0 && !(pl->gm & 2) && pl->w56 == -1) {
        menucursor = 0;
        menupage = 0;
        pl->gm |= 1;
        if (pl->f02 == 2) {
            pl->f02 = 3;
            if (p == myconnectindex)
                setgamemode();
        }
    }
    if (bits & 0x400000) {
        if (pl->f02 == 3) {
            pl->f02 = 2;
            /* pending: 0x53F42 switches the local player to the 640x350 map mode */
        } else {
            pl->f02 = 3;
            setgamemode();
            viewsize(p);
            /* pending: 0x14050 redraws the status bar */
        }
    }
    if (bits & 0x20000)
        pl->b92 ^= 1;
    if ((pl->ba1 & 0x7F) == 6) {
        if (bits & 0x100)
            addweapon(p, 1230, 1);
        if ((bits & 0x200) && (pl->b9d || debugmode))
            addweapon(p, 191, 1);
        if ((bits & 0x400) && (pl->b9e || debugmode))
            addweapon(p, 389, 1);
        if ((bits & 0x800) && (pl->b9f || debugmode))
            addweapon(p, 721, 1);
        if ((bits & 0x1000) && (pl->ba0 || debugmode))
            addweapon(p, 563, 1);
    }
    if ((bits & 0x1000000) && (pl->b99 || debugmode)) {
        if (debugmode)
            pl->b99 = 1;
        pl->b95 = pl->b95 == 1 ? 0 : 0x80;
    }
    if ((bits & 0x2000000) && (pl->b9a || debugmode)) {
        if (debugmode)
            pl->b9a = 1;
        pl->b96 = pl->b96 == 0 ? 32 : 0;
    }
    if ((bits & 0x4000000) && (pl->b97 || debugmode)) {
        if (debugmode)
            pl->b97 = 1;
        pl->b93 ^= 1;
    }
    if ((bits & 0x8000000) && (pl->b9b || debugmode)) {
        /* The holoduke: a stand-in player sprite that moveplayers keeps alive. */
        if (debugmode)
            pl->b9b = 1;
        if (pl->w58 != 0)
            pl->w58 = 0;
        else {
            pl->w58 = EGS(pl->cursectnum, pl->posx, pl->posy, pl->posz, 0, 283, -16, 48, 48, pl->ang, 0, 0, -1, 10);
            hittype[pl->w58][0] = p;
        }
    }
    if ((bits & 0x10000000) && (pl->b98 || debugmode)) {
        if (debugmode)
            pl->b98 = 1;
        pl->b94 = !pl->b94;
        spritesound(pl->b94 ? 18 : 27, pl->i);
    }
    if ((bits & 0x40000000) && pl->w80 == -1)
        pl->ba1 = (pl->ba1 & 0x80) ? 5 : 0x81;
    if (!debugmode) {
        if (bits & 0x1000000) {
            menupage = 20;
            pl->gm |= 1;
        }
        if (bits & 0x2000000) {
            menupage = 50;
            pl->gm |= 1;
        }
        if (bits & 0x4000000) {
            menupage = 10;
            pl->gm |= 1;
        }
        return;
    }
    if (!(bits & 0x800000))
        return;
    /* /d: unlock and operate every door-type sector, then flip the force fields. */
    quote(11);
    for (s = 0; s < numsectors; s++) {
        int lt = sector[s].lotag & 0x0FFF;

        if (lt < 15 || lt > 29 || lt == 24)
            continue;
        if (sector[s].lotag & 0xBFFF)
            sector[s].lotag &= ~0x4000;
        operatesectors(s, pl->i);
    }
    for (j = 0; j < numanimwalls; j++) {
        short w = animwall[j].wallnum;

        if (wall[w].overpicnum < 1184 || wall[w].overpicnum > 1186)
            continue;
        animwall[j].tag = 0;
        if (wall[w].cstat) {
            wall[w].cstat = 0;
            sector[wall[w].nextsector].floorpal = sector[wall[w].nextsector].ceilingpal = 0;
        } else {
            wall[w].cstat = 85;
            sector[wall[w].nextsector].floorpal = sector[wall[w].nextsector].ceilingpal = 2;
        }
    }
}
