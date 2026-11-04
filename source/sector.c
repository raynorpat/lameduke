/* Doors, lifts, switches and breakable walls: 0x259E0 and 0x2B4B0..0x2C180. */

#include "sector.h"
#include "actors.h"
#include "global.h"
#include "sounds.h"

#include "board.h"

/* The animate list (0x121430..0x8EEC8, count dword_125250). */
short animatesect[MAXANIMATES];
int *animateptr[MAXANIMATES];
int animategoal[MAXANIMATES], animatevel[MAXANIMATES], animatecnt;

/* The anim walls prelevel collects (word_124C30, 6 bytes each; count word_1253A8). */
animwalltype animwall[MAXANIMWALLS];
short numanimwalls;

#define SE 175

/* The setanimate 0x259E0 inlines at every use. Returns the slot, or -1 when full. */
int setanimate(short sect, int *ptr, int goal, int vel)
{
    int j, k;

    if (animatecnt >= MAXANIMATES)
        return -1;
    j = animatecnt;
    for (k = 0; k < animatecnt; k++) {
        if (animateptr[k] == ptr) {
            j = k;
            break;
        }
    }
    animatesect[j] = sect;
    animateptr[j] = ptr;
    animategoal[j] = goal;
    animatevel[j] = vel;
    if (j == animatecnt)
        animatecnt++;
    return j;
}

int getanimationgoal(const int *ptr)
{
    int k;

    for (k = 0; k < animatecnt; k++)
        if (animateptr[k] == ptr)
            return k;
    return -1;
}

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

/* sprite[-1] in the exe reads zeroed memory before the sprite array. */
static short sprhitag(short i)
{
    return i >= 0 ? sprite[i].hitag : 0;
}

/* 0x25810, which 0x259E0 inlines for lotag 15: sets every SE 17 lift of
   liftspr's hitag moving in dir, or stops them (sound 35) when dir is 0. */
void movelift(short liftspr, int dir)
{
    short j;

    for (j = headspritestat[3]; j >= 0; j = nextspritestat[j]) {
        if (sprite[j].lotag == 17 && sprite[j].hitag == sprite[liftspr].hitag
            && (iabs(sector[sprite[liftspr].sectnum].floorz - hittype[liftspr][2]) > sprite[j].yvel
                || sector[sprite[j].sectnum].hitag == sector[sprite[liftspr].sectnum].hitag - dir))
            break;
    }
    if (j == -1)
        dir = 0;
    else
        spritesound(dir ? 0 : 35, liftspr);
    for (j = headspritestat[3]; j >= 0; j = nextspritestat[j]) {
        if (sprite[j].lotag == 17 && sprite[j].hitag == sprite[liftspr].hitag) {
            hittype[j][0] = dir;
            hittype[j][1] = dir;
        }
    }
}

static void stardoor(short sn, short ii)
{
    sectortype *sec = &sector[sn];
    int startwall = sec->wallptr, endwall = startwall + sec->wallnum - 1;
    int cx = 0, cy = 0, w, k, speed = sec->extra >> 4;
    int wa[2] = { -1, -1 };

    for (w = startwall; w <= endwall; w++) {
        cx += wall[w].x;
        cy += wall[w].y;
    }
    cx /= endwall - startwall + 1;
    cy /= endwall - startwall + 1;
    for (w = startwall; w <= endwall; w++) {
        if (cx == wall[w].x || cy == wall[w].y) {
            if (wa[0] == -1)
                wa[0] = w;
            else
                wa[1] = w;
        }
    }
    for (k = 0; k < 2; k++) {
        int wn = wa[k], prev, p2, dax, day;

        prev = wn - 1;
        if (prev < startwall)
            prev = endwall;
        p2 = wall[wn].point2;
        dax = ((wall[prev].x + wall[p2].x) >> 1) - wall[wn].x;
        day = ((wall[prev].y + wall[p2].y) >> 1) - wall[wn].y;
        if (cx == wall[wn].x && cy == wall[wn].y) {
            if (dax != 0) {
                int d = wall[wall[p2].point2].x - wall[p2].x;

                setanimate(sn, &wall[wn].x, d + wall[wn].x, speed);
                setanimate(sn, &wall[prev].x, d + wall[prev].x, speed);
                setanimate(sn, &wall[p2].x, d + wall[p2].x, speed);
                spritesound(6, ii);
            } else if (day != 0) {
                int d = wall[wall[p2].point2].y - wall[p2].y;

                setanimate(sn, &wall[wn].y, d + wall[wn].y, speed);
                setanimate(sn, &wall[prev].y, d + wall[prev].y, speed);
                setanimate(sn, &wall[p2].y, d + wall[p2].y, speed);
                spritesound(6, ii);
            }
        } else {
            if (dax != 0) {
                setanimate(sn, &wall[wn].x, cx, speed);
                setanimate(sn, &wall[prev].x, cx + dax, speed);
                setanimate(sn, &wall[p2].x, cx + dax, speed);
                spritesound(6, ii);
            } else if (day != 0) {
                setanimate(sn, &wall[wn].y, cy, speed);
                setanimate(sn, &wall[prev].y, cy + day, speed);
                setanimate(sn, &wall[p2].y, cy + day, speed);
                spritesound(6, ii);
            }
        }
    }
}

/* 0x259E0. ii is the sprite that set it off (used for sounds). */
void operatesectors(short sn, short ii)
{
    sectortype *sec = &sector[sn];
    int i, j, goal;

    switch ((unsigned short)((sec->lotag & 0x7FFF) - 9)) {
    case 0:
        stardoor(sn, ii);
        return;
    case 6: {
        short s, p;

        for (s = headspritesect[sn]; s >= 0 && !(sprite[s].picnum == SE && sprite[s].lotag == 17); s = nextspritesect[s])
            ;
        if (s < 0)
            return; /* the exe would read sprite[-1] */
        for (p = connecthead; p >= 0 && ps[p].cursectnum != sn; p = connectpoint2[p])
            ;
        if (p < 0) {
            movelift(s, sector[sn].floorz <= sprite[s].z ? 1 : -1);
        } else {
            if (syncbits[p] & 1)
                movelift(s, -1);
            if (syncbits[p] & 2)
                movelift(s, 1);
        }
        return;
    }
    case 7:
    case 8:
        for (;;) {
            if ((unsigned short)((sec->lotag & 0x7FFF) - 9) == 8) {
                i = nextsectorneighborz(sn, sec->floorz, 1, -1);
                if (i != -1) {
                    setanimate(sn, &sec->floorz, sector[i].floorz, sec->extra);
                    break;
                }
                sec->lotag ^= 1;
            }
            i = nextsectorneighborz(sn, sec->floorz, 1, 1);
            if (i != -1) {
                setanimate(sn, &sec->floorz, sector[i].floorz, sec->extra);
                break;
            }
            sec->lotag ^= 1;
        }
        spritesound(0, ii);
        return;
    case 9:
    case 10:
        for (;;) {
            if ((unsigned short)((sec->lotag & 0x7FFF) - 9) == 10) {
                i = nextsectorneighborz(sn, sec->floorz, 1, -1);
                if (i != -1) {
                    goal = sector[i].floorz;
                    setanimate(sn, &sec->floorz, goal, sec->extra);
                    setanimate(sn, &sec->ceilingz, goal + sec->ceilingz - sec->floorz, sec->extra);
                    break;
                }
                sec->lotag ^= 1;
            }
            i = nextsectorneighborz(sn, sec->floorz, 1, 1);
            if (i != -1) {
                goal = sector[i].floorz;
                setanimate(sn, &sec->floorz, goal, 256);
                setanimate(sn, &sec->ceilingz, goal + sec->ceilingz - sec->floorz, sec->extra);
                break;
            }
            sec->lotag ^= 1;
        }
        spritesound(0, ii);
        return;
    case 11:
        if (!(sec->lotag & 0x8000)) {
            goal = sector[nextsectorneighborz(sn, sec->ceilingz, -1, -1)].ceilingz;
        } else {
            for (i = headspritestat[3]; i >= 0; i = nextspritestat[i])
                if (sprite[i].sectnum == sn && sprite[i].lotag == 9)
                    break;
            goal = i == -1 ? sec->floorz : sprite[i].z;
        }
        sec->lotag ^= 0x8000;
        setanimate(sn, &sec->ceilingz, goal, sec->extra);
        spritesound(6, ii); /* plays even when the animate list is full */
        return;
    case 12:
        j = getanimationgoal(&sec->floorz);
        if (j >= 0) {
            /* The exe reads animategoal at the sector's index here. */
            if (animategoal[sn] == sec->ceilingz)
                animategoal[j] = sector[nextsectorneighborz(sn, animategoal[sn], 1, 1)].floorz;
            else
                animategoal[j] = sec->ceilingz;
            return;
        }
        if (sec->ceilingz == sec->floorz)
            goal = sector[nextsectorneighborz(sn, sec->ceilingz, 1, 1)].floorz;
        else
            goal = sec->ceilingz;
        sec->lotag ^= 0x8000;
        if (setanimate(sn, &sec->floorz, goal, sec->extra) >= 0)
            spritesound(6, ii);
        return;
    case 13: {
        int mid = (sec->floorz + sec->ceilingz) >> 1;

        j = getanimationgoal(&sec->ceilingz);
        if (j >= 0) {
            if (mid == animategoal[sn])
                animategoal[j] = sector[nextsectorneighborz(sn, sec->floorz, -1, -1)].ceilingz;
            else
                animategoal[j] = mid;
        } else {
            goal = mid == sec->ceilingz ? sector[nextsectorneighborz(sn, sec->floorz, -1, -1)].ceilingz : mid;
            if (setanimate(sn, &sec->ceilingz, goal, sec->extra) >= 0) {
                sec->lotag ^= 0x8000;
                spritesound(6, ii);
            }
        }
        j = getanimationgoal(&sec->floorz);
        if (j >= 0) {
            if (mid == animategoal[sn])
                animategoal[j] = sector[nextsectorneighborz(sn, sec->ceilingz, 1, 1)].floorz;
            else
                animategoal[j] = mid;
            return;
        }
        if (mid == sec->floorz)
            mid = sector[nextsectorneighborz(sn, sec->ceilingz, 1, 1)].floorz;
        if (setanimate(sn, &sec->floorz, mid, sec->extra) >= 0)
            spritesound(6, ii);
        return;
    }
    case 14: {
        short found = -1;

        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if (sprite[i].lotag == 11 && sprite[i].sectnum == sn && hittype[i][4] == 0) {
                found = (short)i;
                sec->lotag ^= 0x8000;
                hittype[i][4] = 1;
                hittype[i][3] = -hittype[i][3];
                break;
            }
        }
        if (found < 0)
            return;
        spritesound(6, ii);
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if (i != found && sprite[i].lotag == 11 && sprite[i].hitag == sprite[found].hitag && hittype[i][4] == 0) {
                sector[sprite[i].sectnum].lotag ^= 0x8000;
                hittype[i][4] = 1;
                hittype[i][3] = -hittype[i][3];
            }
        }
        return;
    }
    case 16: {
        short s;

        for (s = headspritestat[3]; s >= 0 && !(sprite[s].lotag == 15 && sprite[s].sectnum == sn); s = nextspritestat[s])
            ;
        if (s >= 0)
            spritesound(6, ii);
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if (sprite[i].hitag != sprhitag(s))
                continue;
            if (sprite[i].lotag == 18)
                hittype[i][0] ^= 1;
            if (sprite[i].lotag == 15) {
                sector[sprite[i].sectnum].lotag ^= 0x8000;
                sprite[i].ang += 1024;
                hittype[i][4] = (sector[sprite[i].sectnum].lotag & 0x8000) ? 1 : 2;
            }
        }
        return;
    }
    case 17:
        if (getanimationgoal(&sec->floorz) == -1) {
            short save = sec->lotag;

            sec->lotag = (short)((save & 0xFF00) | 22);
            operatesectors(sn, ii);
            sec->lotag = (short)((sec->lotag & 0xFF00) | 9);
            operatesectors(sn, ii);
            sec->lotag = (short)((sec->lotag & 0xFF00) | (save & 0xFF));
        }
        return;
    case 18:
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if ((sprite[i].lotag & 0xFF) == 20 && sprite[i].sectnum == sn) {
                sec->lotag ^= 0x8000;
                hittype[i][0] = !(sec->lotag & 0x8000) + 1;
                spritesound(6, ii);
                break;
            }
        }
        return;
    case 19: {
        short h;

        for (i = headspritesect[sn]; i >= 0; i = nextspritesect[i])
            if (sprite[i].statnum == 3 && (sprite[i].lotag & 0x7FFF) == 21)
                break;
        h = sprhitag((short)i);
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i])
            if ((sprite[i].lotag & 0x7FFF) == 21 && hittype[i][0] == 0 && sprite[i].hitag == h)
                hittype[i][0] = 1;
        return;
    }
    case 20:
        if (!(sec->lotag & 0x8000))
            goal = sector[nextsectorneighborz(sn, sec->ceilingz, -1, -1)].ceilingz;
        else
            goal = sector[nextsectorneighborz(sn, sec->ceilingz, 1, 1)].floorz;
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if (sprite[i].lotag == 22 && sprite[i].hitag == sec->hitag) {
                sprite[i].yvel = (short)((sec->lotag & 0x8000) ? sec->extra : -sec->extra);
                hittype[i][0] = sn;
                hittype[i][1] = 1;
            }
        }
        sec->lotag ^= 0x8000;
        setanimate(sn, &sec->ceilingz, goal, sec->extra);
        spritesound(6, ii);
        return;
    }
}

/* 0x28210, which 0x2B4B0 inlines: every activator (stat 8) of this lotag
   operates its sector; a 23 toggles the sector's 0x4000 bit instead. */
void activatebylotag(short lotag)
{
    short i;

    for (i = headspritestat[8]; i >= 0; i = nextspritestat[i]) {
        if (sprite[i].lotag != lotag)
            continue;
        if (sprite[i].picnum == 23)
            sector[sprite[i].sectnum].lotag ^= 0x4000;
        else
            operatesectors(sprite[i].sectnum, i);
    }
}

/* 0x2B4B0 */
void checkhitswitch(short w)
{
    short lotag = wall[w].lotag, pic = wall[w].picnum;
    int k;

    if (!((pic >= 395 && pic <= 396) || (pic >= 405 && pic <= 406)))
        return;
    for (k = 0; k < numanimwalls; k++) {
        short aw = animwall[k].wallnum;

        if (lotag != wall[aw].lotag)
            continue;
        switch (wall[aw].picnum - 395) {
        case 0: case 10: wall[aw].picnum++; break;
        case 1: case 11: wall[aw].picnum--; break;
        }
    }
    switch (wall[w].picnum - 395) {
    case 0:
    case 1: {
        short i;

        quote(4);
        soundxyz(25, -1, wall[w].x, wall[w].y, 0);
        activatebylotag(lotag);
        for (i = headspritestat[1]; i >= 0; i = nextspritestat[i])
            if (sprite[i].picnum == 51 && lotag == sprite[i].lotag && sprite[i].yvel == 0)
                sprite[i].yvel = 1;
        break;
    }
    case 10:
    case 11: {
        short i;

        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i])
            if (sprite[i].lotag == 12 && sprite[i].hitag == wall[w].lotag)
                hittype[i][0] = wall[w].picnum - 395 == 10 ? 2 : 1;
        soundxyz(25, -1, wall[w].x, wall[w].y, 0);
        activatebylotag(lotag);
        for (k = 0; k < numanimwalls; k++) {
            short aw = animwall[k].wallnum;

            if ((lotag == wall[aw].lotag || lotag == -1) && wall[aw].overpicnum >= 1184 && wall[aw].overpicnum <= 1186) {
                animwall[k].tag = 0;
                if (wall[aw].cstat) {
                    wall[aw].cstat = 0;
                    sector[wall[aw].nextsector].floorpal = 0;
                    sector[wall[aw].nextsector].ceilingpal = 0;
                } else {
                    wall[aw].cstat = 85;
                    sector[wall[aw].nextsector].floorpal = 2;
                    sector[wall[aw].nextsector].ceilingpal = 2;
                }
            }
        }
        break;
    }
    }
}

/* Light-switch textures: the lit tiles 35/36/26/212 go dark. */
static int darkpic(short pic)
{
    switch (pic) {
    case 35: return 664;
    case 36: return 665;
    case 26: return 670;
    case 212: return 671;
    }
    return pic;
}

/* 0x2C180. A shot ceiling light: darkens every sector sharing its hitag. */
int checkhitceiling(short sn)
{
    sectortype *sec = &sector[sn];
    int j, k, shade, best = -1, cshade;
    short pic = sec->ceilingpicnum;
    signed char fs, cs;

    /* The exe clears only al on a miss, and every caller tests all of eax:
       the rest still holds 37 * sn, or the picnum in its low word. */
    if (sec->hitag == 0)
        return (37 * sn) & ~0xFF;
    if (!(pic == 26 || pic == 35 || pic == 36 || pic == 212))
        return pic & 0xFF00;
    sec->ceilingpicnum = (short)darkpic(pic);
    for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
        if (sprite[j].hitag == sec->hitag && sprite[j].lotag == 3)
            hittype[j][4] = 1;
    cshade = sec->ceilingshade;
    for (j = 0; j < numsectors; j++) {
        if (j != sn && sector[j].hitag == sec->hitag && cshade < sector[j].ceilingshade) {
            cshade = sector[j].ceilingshade;
            best = j;
        }
    }
    /* best stays as the exe left it (uninitialised) when no brighter sector exists. */
    if (best < 0)
        best = sn;
    fs = sector[best].floorshade;
    cs = sector[best].ceilingshade;
    shade = 0;
    for (k = sector[best].wallptr; k < sector[best].wallptr + sector[best].wallnum; k++)
        if (shade < wall[k].shade)
            shade = wall[k].shade;
    for (j = 0; j < numsectors; j++) {
        if (j == best || sector[j].hitag != sec->hitag)
            continue;
        sector[j].floorshade = fs;
        sector[j].floorpal = 0;
        sector[j].ceilingshade = cs;
        sector[j].ceilingpal = 0;
        for (k = sector[j].wallptr; k < sector[j].wallptr + sector[j].wallnum; k++) {
            wall[k].shade = (signed char)shade;
            if (wall[k].nextwall >= 0)
                wall[wall[k].nextwall].shade = (signed char)shade;
        }
    }
    return 1;
}

/* 0x2BB10 */
void checkhitwall(short w, int x, int y, int z, short atwith)
{
    short sn, k;
    short over = wall[w].overpicnum, pic;

    if (over == 179 || over == 331 || over == 573) {
        wall[w].overpicnum = 573;
        sn = -1;
        updatesector(x, y, &sn);
        k = EGS(sn, x, y, z, 0, SE, 0, 0, 0, ps[0].ang, 0, 0, ps[myconnectindex].i, 3);
        sprite[k].lotag = 128;
        hittype[k][1] = 5;
        hittype[k][2] = w;
        spritesound(5, k);
        return;
    }
    if (over == 790) {
        if (atwith == 563) {
            wall[w].overpicnum = 573;
            wall[wall[w].nextwall].overpicnum = 573;
        } else {
            sn = -1;
            updatesector(x, y, &sn);
            k = EGS(sn, x, y, z, 0, SE, 0, 0, 0, 0, 0, 0, ps[myconnectindex].i, 3);
            sprite[k].lotag = 129;
            hittype[k][2] = w;
        }
        return;
    }
    pic = wall[w].picnum;
    if (pic == 26 || pic == 35 || pic == 36 || pic == 212) {
        int shade = 0, i;
        signed char fs, cs;
        short ns;

        if (wall[w].lotag == 0)
            return;
        wall[w].picnum = (short)darkpic(pic);
        ns = wall[w].nextsector;
        for (i = sector[ns].wallptr; i < sector[ns].wallptr + sector[ns].wallnum; i++)
            if (wall[i].shade > shade)
                shade = wall[i].shade;
        fs = sector[ns].floorshade;
        cs = sector[ns].ceilingshade;
        for (i = headspritestat[3]; i >= 0; i = nextspritestat[i]) {
            if (sprite[i].hitag == wall[w].lotag && sprite[i].lotag == 3) {
                hittype[i][3] = shade;
                hittype[i][4] = 1;
            }
        }
        for (i = 0; i < numsectors; i++) {
            short s, nx;

            if (i == ns || wall[w].lotag != sector[i].hitag)
                continue;
            sector[i].floorpal = 0;
            sector[i].ceilingpal = 0;
            for (s = headspritesect[i]; s >= 0; s = nx) {
                nx = nextspritesect[s];
                if (sprite[s].picnum == SE && (sprite[s].lotag == 8 || sprite[s].lotag == 9))
                    deletesprite(s);
            }
            sector[i].floorshade = fs;
            sector[i].ceilingshade = cs;
            for (k = sector[i].wallptr; k < sector[i].wallptr + sector[i].wallnum; k++) {
                wall[k].shade = (signed char)shade;
                if (wall[k].nextwall >= 0)
                    wall[wall[k].nextwall].shade = (signed char)shade;
            }
        }
        return;
    }
    if (pic >= 819 && pic <= 822) {
        wall[w].picnum = 1183;
        soundxyz(23, -1, wall[w].x, wall[w].y, sector[wall[w].nextsector].floorz);
    } else if (pic >= 823 && pic <= 826) {
        wall[w].picnum = 1182;
        soundxyz(23, -1, wall[w].x, wall[w].y, sector[wall[w].nextsector].floorz);
    } else if (pic >= 860 && pic <= 863) {
        wall[w].picnum = 1180;
        soundxyz(23, -1, wall[w].x, wall[w].y, sector[wall[w].nextsector].floorz);
    } else if (pic >= 864 && pic <= 867) {
        wall[w].picnum = 1181;
        soundxyz(23, -1, wall[w].x, wall[w].y, sector[wall[w].nextsector].floorz);
    }
}

/* 0x250A0. Steps every animation toward its goal by vel per tic, carrying
   standing players in the sector along; a finished entry is replaced by the
   last one. */
void doanimations(int tics)
{
    int j;
    short p;

    for (j = animatecnt - 1; j >= 0; j--) {
        int a = *animateptr[j], goal = animategoal[j], v = tics * animatevel[j];
        short dasect = animatesect[j];

        if (a < goal) {
            a += v;
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == dasect && ps[p].b8f)
                    ps[p].posz += v;
            if (a > goal) {
                stopsound(36);
                soundxyz(35, -1, wall[sector[dasect].wallptr].x, wall[sector[dasect].wallptr].y,
                    sector[dasect].floorz);
                a = goal;
            }
        }
        if (a > goal) {
            a -= v;
            for (p = connecthead; p >= 0; p = connectpoint2[p])
                if (ps[p].cursectnum == dasect && ps[p].b8f)
                    ps[p].posz -= v;
            if (a < goal) {
                stopsound(36);
                soundxyz(35, -1, wall[sector[dasect].wallptr].x, wall[sector[dasect].wallptr].y,
                    sector[dasect].floorz);
                a = goal;
            }
        }
        *animateptr[j] = a;
        if (a == goal) {
            animatecnt--;
            animateptr[j] = animateptr[animatecnt];
            animatevel[j] = animatevel[animatecnt];
            animategoal[j] = animategoal[animatecnt];
            animatesect[j] = animatesect[animatecnt];
        }
    }
}
