#include "board.h"

#include "engine.h"
#include "math.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sectortype *sector;
walltype *wall;
spritetype *sprite;
int numsectors;
int numwalls;
int numsprites;

int posx;
int posy;
int posz;
int horiz;
short ang;
short cursectnum;

static void free_board(void)
{
    free(sector);
    free(wall);
    free(sprite);
    sector = NULL;
    wall = NULL;
    sprite = NULL;
    numsectors = 0;
    numwalls = 0;
    numsprites = 0;
}

int loadboard(const char *filename)
{
    FILE *fp;
    int version;
    int px, py, pz;
    short startang, startsect;
    short nsec, nwal, nspr, nextra;
    int i;

    free_board();
    fp = plat_fopen(filename, "rb");
    if (!fp)
        return -1;
    if (fread(&version, 4, 1, fp) != 1 || version != 5) {
        fclose(fp);
        return -1;
    }
    if (fread(&px, 4, 1, fp) != 1 || fread(&py, 4, 1, fp) != 1 || fread(&pz, 4, 1, fp) != 1
        || fread(&startang, 2, 1, fp) != 1 || fread(&startsect, 2, 1, fp) != 1
        || fread(&nsec, 2, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    if (nsec < 1 || nsec > 2048) {
        fclose(fp);
        return -1;
    }
    sector = (sectortype *)malloc((size_t)nsec * sizeof(sectortype));
    if (!sector || fread(sector, sizeof(sectortype), (size_t)nsec, fp) != (size_t)nsec) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(&nwal, 2, 1, fp) != 1 || nwal < 0 || nwal > 16384) {
        fclose(fp);
        free_board();
        return -1;
    }
    wall = (walltype *)malloc((size_t)nwal * sizeof(walltype));
    if (!wall || fread(wall, sizeof(walltype), (size_t)nwal, fp) != (size_t)nwal) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(&nspr, 2, 1, fp) != 1 || nspr < 0 || nspr > 4096) {
        fclose(fp);
        free_board();
        return -1;
    }
    sprite = (spritetype *)malloc(nspr ? (size_t)nspr * sizeof(spritetype) : 1);
    if (!sprite || (nspr && fread(sprite, sizeof(spritetype), (size_t)nspr, fp) != (size_t)nspr)) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fread(&nextra, 2, 1, fp) != 1 || nextra < 0) {
        fclose(fp);
        free_board();
        return -1;
    }
    if (fseek(fp, (long)nextra * 10L, SEEK_CUR) != 0) {
        fclose(fp);
        free_board();
        return -1;
    }
    i = fgetc(fp);
    fclose(fp);
    if (i != EOF) {
        free_board();
        return -1;
    }

    numsectors = nsec;
    numwalls = nwal;
    numsprites = nspr;
    posx = px;
    posy = py;
    posz = pz;
    ang = startang;
    cursectnum = startsect;
    horiz = ydim / 2;
    if (cursectnum >= 0 && cursectnum < numsectors)
        posz = sector[cursectnum].floorz - 9216;
    return 0;
}

int inside(int x, int y, short sect)
{
    int w;
    int first;
    int last;
    int count;

    if (sect < 0 || sect >= numsectors)
        return -1;
    first = sector[sect].wallptr;
    last = first + sector[sect].wallnum - 1;
    count = 0;
    for (w = first; w <= last; w++) {
        int x1;
        int y1;
        int p2;
        int x2;
        int y2;

        if (w < 0 || w >= numwalls)
            break;
        x1 = wall[w].x;
        y1 = wall[w].y;
        p2 = wall[w].point2;
        if (p2 < 0 || p2 >= numwalls)
            continue;
        x2 = wall[p2].x;
        y2 = wall[p2].y;
        if (x1 < x && x2 < x)
            continue;
        if (y1 > y2) {
            int t = x1;
            x1 = x2;
            x2 = t;
            t = y1;
            y1 = y2;
            y2 = t;
        }
        if (y1 <= y && y2 > y) {
            long long left = (long long)x1 * (y - y2) + (long long)(y1 - y) * x2;
            long long right = (long long)x * (y1 - y2);
            if (left <= right)
                count ^= 1;
        }
    }
    return count;
}

static int sector_at(int x, int y, short hint)
{
    int w;
    int i;

    if (inside(x, y, hint) == 1)
        return hint;
    if (hint >= 0 && hint < numsectors) {
        int first = sector[hint].wallptr;
        int last = first + sector[hint].wallnum;
        for (w = first; w < last; w++) {
            short next;
            if (w < 0 || w >= numwalls)
                break;
            next = wall[w].nextsector;
            if (next >= 0 && inside(x, y, next) == 1)
                return next;
        }
    }
    for (i = 0; i < numsectors; i++) {
        if (inside(x, y, (short)i) == 1)
            return i;
    }
    return -1;
}

static int can_stand(short from, short to)
{
    int step;
    int eye;
    int gap;

    if (to < 0 || to >= numsectors)
        return 0;
    step = sector[from].floorz - sector[to].floorz;
    if (step > 4096)
        return 0;
    gap = sector[to].floorz - sector[to].ceilingz;
    if (gap < 4096)
        return 0;
    eye = sector[to].floorz - 9216;
    if (eye < sector[to].ceilingz + 512)
        return 0;
    return 1;
}

static int try_step(int dx, int dy)
{
    int nx;
    int ny;
    int dest;

    nx = posx + dx;
    ny = posy + dy;
    dest = sector_at(nx, ny, cursectnum);
    if (dest < 0)
        return 0;
    if (dest != cursectnum && cursectnum >= 0 && !can_stand(cursectnum, (short)dest))
        return 0;
    posx = nx;
    posy = ny;
    cursectnum = (short)dest;
    posz = sector[cursectnum].floorz - 9216;
    return 1;
}

void player_move(int fvel, int svel, int avel, int tics, int bits)
{
    const short *trig;
    int co;
    int si;
    int dx;
    int dy;
    long long fs;
    long long ss;

    /* sub_31410 multiplies by the tic count once. A zero delta does not move. */
    if (tics <= 0)
        return;
    trig = sintable();
    if (!trig)
        return;
    /* Sector lotag 2 (high bit ignored) quarters a negative forward byte. */
    if (cursectnum >= 0 && cursectnum < numsectors
        && (sector[cursectnum].lotag & 0x7fff) == 2 && fvel < 0)
        fvel >>= 2;
    ang = (short)((ang + (avel * tics) / 24) & 2047);
    co = trig[(ang + 512) & 2047];
    si = trig[ang & 2047];
    fs = (long long)fvel * tics;
    ss = (long long)svel * tics;
    /* Vectors are (trig * vel * tics) >> 3, then clipmove shifts by 14.
       Positive strafe is screen-left: left arrow raises dword_1252FC. */
    dx = (int)((co * fs + si * ss) >> 17);
    dy = (int)((si * fs + (long long)trig[(ang + 1536) & 2047] * ss) >> 17);
    if (bits & 2) {
        dx >>= 1;
        dy >>= 1;
    }
    if (!try_step(dx, dy)) {
        try_step(dx, 0);
        try_step(0, dy);
    }
}

void player_look(int dh)
{
    horiz += dh;
    if (horiz < 8)
        horiz = 8;
    if (horiz > ydim - 8)
        horiz = ydim - 8;
}
