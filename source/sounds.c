#include "sounds.h"
#include "global.h"

#include "board.h"
#include "cache1d.h"
#include "fx_man.h"
#include "math.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

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

/* 0x3BBD0. Runs after FX_SetupCard and before FX_Init. A file that does
   not open leaves its sound empty. */
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
        sounds[i] = (unsigned char *)allocache((int)len);
        if (sounds[i] && fread(sounds[i], 1, (size_t)len, fp) != (size_t)len)
            sounds[i] = NULL;
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
    handle = FX_PlayVOC3D(sounds[num], 0, ang >> 4, dist >> 6, num == 32 ? 255 : 0, (unsigned long)num);
    if (handle <= 0)
        return;
    soundinfo[num].owner = i == -1 ? ps[0].i : i;
    soundinfo[num].handle = handle;
    soundinfo[num].playing++;
}

void spritesound(int num, short i)
{
    soundxyz(num, i, sprite[i].x, sprite[i].y, sprite[i].z);
}

/* 0x3C040 */
void stopsound(int num)
{
    FX_StopSound(soundinfo[num].handle);
    if (soundinfo[num].playing)
        soundinfo[num].playing--;
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

/* 0x3BC40 */
void playmusic(const char *fn)
{
    (void)fn;
}
