#include "draw.h"

#include "board.h"
#include "display.h"
#include "math.h"
#include "tiles.h"

#include <stdlib.h>
#include <string.h>

static unsigned char frame[640 * 480];
static int umost[640];
static int dmost[640];
static int clip_u[24][640];
static int clip_d[24][640];
static unsigned char portal_col[640];
static int portal_u[640];
static int portal_d[640];
static unsigned char sectstack[2048];
static int draw_depth;
static int frame_co;
static int frame_si;
static int focal;

static int sample_tile(int pic, int u, int v)
{
    int sizx;
    int sizy;
    const unsigned char *data;

    data = tile_data(pic, &sizx, &sizy);
    if (!data || sizx <= 0 || sizy <= 0)
        return pic & 255;
    if (u < 0)
        u = sizx - 1 - ((-u - 1) % sizx);
    if (v < 0)
        v = sizy - 1 - ((-v - 1) % sizy);
    u %= sizx;
    v %= sizy;
    return data[u * sizy + v];
}

static void putpix(int x, int y, int color)
{
    if ((unsigned)x >= (unsigned)xdim || (unsigned)y >= (unsigned)ydim)
        return;
    frame[y * xdim + x] = (unsigned char)(color & 255);
}

static int project_x(int vx, int vy)
{
    return xdim / 2 + (int)((long long)vx * focal / vy);
}

/* sub_564CE at 320 wide: screen y = horiz + (zdiff << 19) / vy >> 16. */
static int project_y(int wz, int vy)
{
    return horiz + (int)((long long)(wz - posz) * 8 / vy);
}

static int wall_len(int x1, int y1, int x2, int y2)
{
    double dx = (double)(x2 - x1);
    double dy = (double)(y2 - y1);
    double n = dx * dx + dy * dy;
    int v = 1;

    if (n < 1.0)
        return 1;
    /* Integer square root, good enough for a texture coordinate. */
    while ((double)v * (double)v <= n && v < 1000000)
        v <<= 1;
    v >>= 1;
    {
        int bit = v >> 1;
        int acc = v;
        while (bit) {
            int next = acc + bit;
            if ((double)next * (double)next <= n)
                acc = next;
            bit >>= 1;
        }
        return acc < 1 ? 1 : acc;
    }
}

static void paint_floor(int x, int y0, int y1, int pic, int wz, int sky)
{
    int y;

    if (y0 < 0)
        y0 = 0;
    if (y1 >= ydim)
        y1 = ydim - 1;
    if (sky) {
        for (y = y0; y <= y1; y++)
            putpix(x, y, sample_tile(pic, (x + ang) & 127, y & 127));
        return;
    }
    for (y = y0; y <= y1; y++) {
        int denom = y - horiz;
        int vf;
        int vr;
        int wx;
        int wy;

        if (denom == 0)
            continue;
        vf = (int)((long long)(wz - posz) * focal / denom);
        if (vf < 1)
            vf = 1;
        vr = (int)((long long)(x - xdim / 2) * vf / focal);
        wx = posx + (int)(((long long)vf * frame_co - (long long)vr * frame_si) >> 14);
        wy = posy + (int)(((long long)vr * frame_co + (long long)vf * frame_si) >> 14);
        putpix(x, y, sample_tile(pic, wx >> 4, wy >> 4));
    }
}

/* Wall texel = repeat * world_delta >> 11, from the global shift in sub_45D09. */
static void paint_wall(int x, int y0, int y1, int pic, int u, int masked, int vy, int zref, int rep)
{
    int y;

    if (y0 < 0)
        y0 = 0;
    if (y1 >= ydim)
        y1 = ydim - 1;
    if (y0 > y1 || vy == 0)
        return;
    if (rep < 1)
        rep = 8;
    for (y = y0; y <= y1; y++) {
        int z = posz + (int)((long long)(y - horiz) * vy / 8);
        int v = (int)(((long long)(z - zref) * rep) >> 11);
        int color = sample_tile(pic, u, v);

        if (masked && color == 255)
            continue;
        putpix(x, y, color);
    }
}

static void clip_near(int *x1, int *y1, int x2, int y2)
{
    int dy;

    if (*y1 >= 8)
        return;
    dy = y2 - *y1;
    if (dy == 0) {
        *y1 = 8;
        return;
    }
    *x1 += (int)((long long)(x2 - *x1) * (8 - *y1) / dy);
    *y1 = 8;
}

static void draw_sector(short sect);

static void draw_wall(short sect, int w)
{
    walltype *wl;
    int x1, y1, x2, y2;
    int vx1, vy1, vx2, vy2;
    int sx1, sx2, xa, xb;
    int yt1, yt2, yb1, yb2;
    int x;
    int len;
    int masked;
    short next;
    int portal = 0;

    if (w < 0 || w >= numwalls)
        return;
    wl = &wall[w];
    if (wl->point2 < 0 || wl->point2 >= numwalls)
        return;
    x1 = wl->x;
    y1 = wl->y;
    x2 = wall[wl->point2].x;
    y2 = wall[wl->point2].y;
    /* sub_43F18 stores cos in dword_23C3A0 and sin in dword_23C39C.
       sub_46BEC: lateral = (dy*cos - dx*sin) >> 14, forward = (dx*cos + dy*sin) >> 14.
       Visible when forward2*lateral1 <= forward1*lateral2 and not fully behind. */
    {
        long long dx1 = (long long)x1 - posx;
        long long dy1 = (long long)y1 - posy;
        long long dx2 = (long long)x2 - posx;
        long long dy2 = (long long)y2 - posy;

        vx1 = (int)((dy1 * frame_co - dx1 * frame_si) >> 14);
        vy1 = (int)((dx1 * frame_co + dy1 * frame_si) >> 14);
        vx2 = (int)((dy2 * frame_co - dx2 * frame_si) >> 14);
        vy2 = (int)((dx2 * frame_co + dy2 * frame_si) >> 14);
        if ((long long)vy2 * vx1 > (long long)vy1 * vx2)
            return;
        if (vy1 <= 0 && vy2 <= 0)
            return;
    }
    if (vy1 < 8 && vy2 < 8)
        return;
    if (vy1 < 8)
        clip_near(&vx1, &vy1, vx2, vy2);
    if (vy2 < 8)
        clip_near(&vx2, &vy2, vx1, vy1);
    if (vy1 < 8 || vy2 < 8)
        return;
    sx1 = project_x(vx1, vy1);
    sx2 = project_x(vx2, vy2);
    if (sx1 >= sx2)
        return;
    if (sx2 < 0 || sx1 >= xdim)
        return;
    yt1 = project_y(sector[sect].ceilingz, vy1);
    yt2 = project_y(sector[sect].ceilingz, vy2);
    yb1 = project_y(sector[sect].floorz, vy1);
    yb2 = project_y(sector[sect].floorz, vy2);
    len = wall_len(x1, y1, x2, y2);
    masked = (wl->cstat & 0x10) != 0;
    next = wl->nextsector;
    xa = sx1;
    xb = sx2;
    if (xa < 0)
        xa = 0;
    if (xb > xdim)
        xb = xdim;

    memset(portal_col, 0, (size_t)xdim);
    for (x = xa; x < xb; x++) {
        int tden = sx2 - sx1;
        int t = x - sx1;
        int yt, yb;
        int u;
        int top, bot;

        if (tden < 1)
            tden = 1;
        if (t < 0)
            t = 0;
        yt = yt1 + (int)((long long)(yt2 - yt1) * t / tden);
        yb = yb1 + (int)((long long)(yb2 - yb1) * t / tden);
        u = wl->xpanning + (int)(((long long)len * t / tden) * (wl->repeat ? wl->repeat : 8) >> 11);
        top = umost[x];
        bot = dmost[x];
        if (top > bot)
            continue;
        if (yt < top)
            yt = top;
        if (yb > bot)
            yb = bot;
        {
            int vy = vy1 + (int)((long long)(vy2 - vy1) * t / tden);
            int rep = wl->repeat ? wl->repeat : 8;

            if (vy < 8)
                vy = 8;
            if (top <= yt - 1)
                paint_floor(x, top, yt - 1, sector[sect].ceilingpicnum, sector[sect].ceilingz,
                    sector[sect].ceilingstat & 1);
            if (yb + 1 <= bot)
                paint_floor(x, yb + 1, bot, sector[sect].floorpicnum, sector[sect].floorz, 0);
            if (next < 0 || next >= numsectors) {
                if (yt <= yb)
                    paint_wall(x, yt, yb, wl->picnum, u, 0, vy, sector[sect].ceilingz, rep);
            } else {
                int nyt, nyb;

                nyt = project_y(sector[next].ceilingz, vy);
                nyb = project_y(sector[next].floorz, vy);
                if (nyt < yt)
                    nyt = yt;
                if (nyb > yb)
                    nyb = yb;
                if (yt <= nyt - 1)
                    paint_wall(x, yt, nyt - 1, wl->picnum, u, 0, vy, sector[sect].ceilingz, rep);
                if (nyb + 1 <= yb)
                    paint_wall(x, nyb + 1, yb, wl->overpicnum ? wl->overpicnum : wl->picnum, u, 0, vy,
                        sector[sect].floorz, rep);
                if (masked && nyt <= nyb)
                    paint_wall(x, nyt, nyb, wl->picnum, u, 1, vy, sector[sect].ceilingz, rep);
                if (nyt <= nyb) {
                    portal_col[x] = 1;
                    portal_u[x] = nyt;
                    portal_d[x] = nyb;
                    portal = 1;
                }
            }
        }
    }
    if (portal && next >= 0 && next < numsectors && draw_depth < 24) {
        int i;

        memcpy(clip_u[draw_depth], umost, (size_t)xdim * sizeof(int));
        memcpy(clip_d[draw_depth], dmost, (size_t)xdim * sizeof(int));
        for (i = 0; i < xdim; i++) {
            if (portal_col[i]) {
                umost[i] = portal_u[i];
                dmost[i] = portal_d[i];
            } else {
                umost[i] = 1;
                dmost[i] = 0;
            }
        }
        draw_depth++;
        draw_sector(next);
        draw_depth--;
        memcpy(umost, clip_u[draw_depth], (size_t)xdim * sizeof(int));
        memcpy(dmost, clip_d[draw_depth], (size_t)xdim * sizeof(int));
    }
}

static void draw_sprites(short sect)
{
    int i;

    for (i = 0; i < numsprites; i++) {
        int vx, vy;
        int sx, sy0, sy1;
        int sizx, sizy;
        int scale;
        int x0, x1, y, x;
        const unsigned char *data;

        if (sprite[i].sectnum != sect || sprite[i].picnum <= 0)
            continue;
        data = tile_data(sprite[i].picnum, &sizx, &sizy);
        if (!data || sizx <= 0 || sizy <= 0)
            continue;
        vx = (int)(((long long)(sprite[i].y - posy) * frame_co - (long long)(sprite[i].x - posx) * frame_si) >> 14);
        vy = (int)(((long long)(sprite[i].x - posx) * frame_co + (long long)(sprite[i].y - posy) * frame_si) >> 14);
        if (vy < 16)
            continue;
        sx = project_x(vx, vy);
        scale = (int)((long long)sizy * focal / vy);
        if (scale < 2)
            continue;
        if (scale > ydim * 2)
            scale = ydim * 2;
        sy1 = project_y(sprite[i].z, vy);
        sy0 = sy1 - scale;
        x0 = sx - (int)((long long)sizx * scale / (sizy < 1 ? 1 : sizy) / 2);
        x1 = sx + (int)((long long)sizx * scale / (sizy < 1 ? 1 : sizy) / 2);
        for (x = x0; x <= x1; x++) {
            int u;
            if ((unsigned)x >= (unsigned)xdim)
                continue;
            u = (int)((long long)(x - x0) * sizx / (x1 - x0 + 1));
            if (u < 0 || u >= sizx)
                continue;
            for (y = sy0; y <= sy1; y++) {
                int v;
                int color;
                if (y < umost[x] || y > dmost[x])
                    continue;
                v = (int)((long long)(y - sy0) * sizy / (sy1 - sy0 + 1));
                if (v < 0 || v >= sizy)
                    continue;
                color = data[u * sizy + v];
                if (color == 255)
                    continue;
                putpix(x, y, color);
            }
        }
    }
}

static void draw_sector(short sect)
{
    int w;
    int first;
    int last;

    if (sect < 0 || sect >= numsectors || sectstack[sect])
        return;
    sectstack[sect] = 1;
    first = sector[sect].wallptr;
    last = first + sector[sect].wallnum;
    for (w = first; w < last && w < numwalls; w++)
        draw_wall(sect, w);
    draw_sprites(sect);
    sectstack[sect] = 0;
}

void draw_init(void)
{
    memset(frame, 0, sizeof frame);
}

void draw_view(void)
{
    const short *trig;
    int x;

    if (xdim < 1 || ydim < 1 || xdim > 640 || ydim > 480)
        return;
    trig = sintable();
    if (!trig || !sector)
        return;
    frame_co = trig[(ang + 512) & 2047];
    frame_si = trig[ang & 2047];
    focal = xdim / 2;
    if (focal < 1)
        focal = 1;
    memset(frame, 0, (size_t)xdim * (size_t)ydim);
    memset(sectstack, 0, sizeof sectstack);
    draw_depth = 0;
    for (x = 0; x < xdim; x++) {
        umost[x] = 0;
        dmost[x] = (short)(ydim - 1);
    }
    if (cursectnum < 0 || cursectnum >= numsectors || inside(posx, posy, cursectnum) != 1) {
        int i;
        for (i = 0; i < numsectors; i++) {
            if (inside(posx, posy, (short)i) == 1) {
                cursectnum = (short)i;
                break;
            }
        }
    }
    draw_sector(cursectnum);
}

const unsigned char *framebuffer(void)
{
    return frame;
}

int framebuffer_pitch(void)
{
    return xdim;
}
