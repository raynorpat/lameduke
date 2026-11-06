#include "sounds.h"
#include "global.h"

#include "board.h"
#include "cache1d.h"
#include "engine.h"
#include "dsl.h"
#include "fx_man.h"
#include "buildmath.h"
#include "music.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMSOUNDS 39

/* off_82838 */
static const char *const soundnames[NUMSOUNDS] = {
    "elev1.voc", "shooting.voc", "running.voc", "expl1.voc", "xoptor.voc", "glass.voc", "door1.voc",
    "knuckle.voc", "jump.voc", "land.voc", "scuba.voc", "waterfal.voc", "getweapn.voc", "bodyblop.voc",
    "bodybloa.voc", "bodyblor.voc", "onBoard.voc", "dripping.voc", "jetPakOn.voc", "jetPakI.voc",
    "fta_hm.voc", "fta_hf.voc", "fta_r.voc", "liteflck.voc", "biglite.voc", "switch.voc", "reactor.voc",
    "jetpakof.voc", "runningw.voc", "squish1.voc", "rebound.voc", "gun1.voc", "subway.voc", "clipin.voc",
    "clipout.voc", "elev3.voc", "elev2.voc", "door2.voc", "rpg.voc",
};

static unsigned char *sounds[NUMSOUNDS]; /* dword_B56D8 */

/* The 19-byte records at 0x125420. They also keep the start x and z,
   which nothing reads back. */
static struct {
    short owner; /* +0x0C */
    int handle;  /* +0x0E */
    unsigned char playing; /* +0x12: copies started and not yet finished */
} soundinfo[NUMSOUNDS];

unsigned char soundon = 1; /* byte_1257AB, from userdefs.tmp (1 without one) */
int taskman;

/* The parsers read a few bytes past a block or track end, as the exe does;
   this much zeroed slack after each file keeps those reads in the buffer. */
#define SLACK 16

static unsigned le(const unsigned char *p, int n)
{
    unsigned v = 0;

    while (n-- > 0)
        v = v << 8 | p[n];
    return v;
}

/* Every block multivoc's nextblock walks to lies inside the file. */
static int vocvalid(const unsigned char *p, long len)
{
    long at;

    if (len < 26 || memcmp(p, "Creative Voice File", 19) != 0)
        return 0;
    for (at = (long)le(p + 20, 2); at < len; ) {
        int type = p[at];
        long blen;

        if (type == 0 || type > 9)
            return 1; /* nextblock stops here */
        if (at + 4 > len)
            return 0;
        blen = (long)le(p + at + 1, 3);
        if (at + 4 + blen > len || ((type == 1 || type == 6) && blen < 2) || (type == 9 && blen < 12))
            return 0;
        at += 4 + blen;
    }
    return 1; /* the zeroed slack reads as the terminator */
}

/* The header and every MTrk chunk MIDI_PlaySong walks lie inside the file. */
static int midivalid(const unsigned char *p, long len)
{
    long at, n;

    if (len < 14 || memcmp(p, "MThd", 4) != 0)
        return 0;
    at = 8 + (long)(p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7]);
    for (n = p[10] << 8 | p[11]; n > 0; n--) {
        if (at < 0 || at + 8 > len || memcmp(p + at, "MTrk", 4) != 0)
            return 0;
        at += 8 + (long)((unsigned)p[at + 4] << 24 | p[at + 5] << 16 | p[at + 6] << 8 | p[at + 7]);
        if (at > len)
            return 0;
    }
    return 1;
}

/* 0x3BBD0. Runs after FX_SetupCard and before FX_Init. A file that does
   not open, or whose blocks run past its end, leaves its sound empty. */
void loadsounds(void)
{
    int i;

    puts("Reading sounds...");
    for (i = 0; i < NUMSOUNDS; i++) {
        FILE *fp = plat_fopen(soundnames[i], "rb");
        long len;

        if (!fp)
            continue;
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        sounds[i] = (unsigned char *)allocache((int)len + SLACK);
        if (sounds[i]) {
            memset(sounds[i] + len, 0, SLACK);
            if (fread(sounds[i], 1, (size_t)len, fp) != (size_t)len || !vocvalid(sounds[i], len))
                sounds[i] = NULL;
        }
        fclose(fp);
    }
}

/* Passed to FX_SetCallBack. 0x3C190: a voice finished. */
void sound_callback(unsigned long num)
{
    if (soundinfo[num].playing)
        soundinfo[num].playing--;
}

/* The listener the 3D sound calls measure from: the demo camera, or the
   viewed player looking where it faces plus lookang. */
static void listener(int *x, int *y, int *ang)
{
    if (cameraspr == -1) {
        *x = ps[screenpeek].posx;
        *y = ps[screenpeek].posy;
        *ang = ps[screenpeek].ang + ps[screenpeek].lookang;
    } else {
        *x = sprite[cameraspr].x;
        *y = sprite[cameraspr].y;
        *ang = sprite[cameraspr].ang;
    }
}

/* 0x3BCB0 and 0x3BE70: at most three copies of a sound play at once, and
   one 16384 away (512 for sound 17) is not started. Sound 32 plays at
   priority 255. */
void soundxyz(int num, short i, int x, int y, int z)
{
    int lx, ly, lang, dx, dy, ang, dist, handle;

    (void)z;
    if (!soundon || soundinfo[num].playing > 2)
        return;
    listener(&lx, &ly, &lang);
    dx = lx - x;
    dy = ly - y;
    ang = (lang + 2048 - getangle(dx, dy)) & 2047;
    dist = abs(dx) + abs(dy);
    if (num == 17)
        dist <<= 5;
    if (dist >> 6 > 255)
        return;
    /* The exe plays whatever is at address 0 for a sound that did not load. */
    if (!sounds[num])
        return;
    DSL_Lock();
    handle = FX_PlayVOC3D(sounds[num], 0, ang >> 4, dist >> 6, num == 32 ? 255 : 0, (unsigned long)num);
    if (handle > 0) {
        soundinfo[num].owner = i == -1 ? ps[0].i : i;
        soundinfo[num].handle = handle;
        soundinfo[num].playing++;
    }
    DSL_Unlock();
}

void spritesound(int num, short i)
{
    soundxyz(num, i, sprite[i].x, sprite[i].y, sprite[i].z);
}

/* 0x3C040 */
void stopsound(int num)
{
    DSL_Lock();
    FX_StopSound(soundinfo[num].handle);
    if (soundinfo[num].playing)
        soundinfo[num].playing--;
    DSL_Unlock();
}

/* 0x3C090, once a drawn frame: every playing sound follows its owner. */
void pansounds(void)
{
    int lx, ly, lang, num;

    listener(&lx, &ly, &lang);
    for (num = 0; num < NUMSOUNDS; num++) {
        spritetype *s;
        int dx, dy;

        if (!soundinfo[num].playing)
            continue;
        s = &sprite[soundinfo[num].owner];
        dx = lx - s->x;
        dy = ly - s->y;
        FX_Pan3D(soundinfo[num].handle, ((lang + 2048 - getangle(dx, dy)) & 2047) >> 4,
            (abs(dx) + abs(dy)) >> 6);
    }
}

/* 0x3BC40: read the whole song (into unk_B5810 in the exe) and loop it.
   A song that does not open or is cut short leaves the music as it was. */
void playmusic(const char *fn)
{
    static unsigned char *song;
    unsigned char *next;
    FILE *fp;
    long len;

    if (setup_byte(2) == 0)
        return;
    fp = plat_fopen(fn, "rb");
    if (!fp)
        return;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    next = (unsigned char *)calloc(1, (size_t)len + SLACK);
    if (!next || fread(next, 1, (size_t)len, fp) != (size_t)len || !midivalid(next, len)) {
        free(next);
        fclose(fp);
        return;
    }
    fclose(fp);
    /* TaskMan takes the timer only when a song is scheduled */
    if (MUSIC_PlaySong(next, 1) == MUSIC_Ok)
        taskman = 1;
    free(song);
    song = next;
}
