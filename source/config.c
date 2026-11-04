#include "game.h"
#include "actors.h"
#include "gamedef.h"
#include "global.h"
#include "menus.h"
#include "premap.h"
#include "sector.h"
#include "sounds.h"

#include "board.h"
#include "engine.h"
#include "network.h"
#include "platform.h"
#include "sdl_input.h"

#include <stdio.h>
#include <string.h>

/* The 21 bytes at 0x1257A8 that userdefs.tmp keeps. 17-20 are never read;
   they are kept as the file had them. */
static unsigned char userdefs[21];

int brightness;  /* byte_1257AA: F10 steps it 0-4 */
int musicon = 1; /* byte_1257AC: M pauses and resumes the song */

static void putword(unsigned char *p, int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static short getword(const unsigned char *p)
{
    return (short)(p[0] | p[1] << 8);
}

/* 0x1B520. Without the file the defaults stand: visibility 13, sound and
   music 255, mouse 128, sound and music on, brightness 1. */
void load_user_stats(void)
{
    FILE *fp = plat_fopen("userdefs.tmp", "rb");

    demomode = 0;
    if (!fp) {
        brightness = 1;
        return;
    }
    if (fread(userdefs, 1, sizeof userdefs, fp) == sizeof userdefs) {
        user_visibility = userdefs[0];
        game_quotes = userdefs[1];
        brightness = userdefs[2];
        soundon = userdefs[3];
        musicon = userdefs[4];
        debugmode = getword(userdefs + 5);
        sound_volume = getword(userdefs + 7);
        music_volume = getword(userdefs + 9);
        gamepaused = getword(userdefs + 11);
        cameraspr = getword(userdefs + 13);
        mousesens = getword(userdefs + 15);
    }
    fread(scorenames, 1, sizeof scorenames, fp);
    fread(scores, 4, 12, fp);
    fclose(fp);
}

/* 0x1B5D0, on the way out. */
void save_user_stats(void)
{
    FILE *fp = plat_fopen("userdefs.tmp", "wb");

    if (!fp)
        return;
    userdefs[0] = (unsigned char)user_visibility;
    userdefs[1] = (unsigned char)game_quotes;
    userdefs[2] = (unsigned char)brightness;
    userdefs[3] = soundon;
    userdefs[4] = (unsigned char)musicon;
    putword(userdefs + 5, debugmode);
    putword(userdefs + 7, sound_volume);
    putword(userdefs + 9, music_volume);
    putword(userdefs + 11, gamepaused);
    putword(userdefs + 13, cameraspr);
    putword(userdefs + 15, mousesens);
    fwrite(userdefs, 1, sizeof userdefs, fp);
    fwrite(scorenames, 1, sizeof scorenames, fp);
    fwrite(scores, 4, 12, fp);
    fclose(fp);
}

/* The exe keeps addresses where the port keeps indices: hittype t1 and t4
   of an actor point into script (0x976CC), and ps.musicptr into musicfn
   (0x8AC70). DOS/4GW in DOSBox loads the data 0x194000 above the linked
   address: the demo 1 trace has the trooper's action, script[401] here,
   at 0x22BD0C. */
#define EXE_DATA_DELTA 0x194000
#define EXE_SCRIPT (0x976CC + EXE_DATA_DELTA)
#define EXE_MUSICFN (0x8AC70 + EXE_DATA_DELTA)

/* Sprites whose t1 and t4 the CON code owns: the actors, with 265-269
   sharing 265's script as the premap sets them up. */
static int conactor(short pic)
{
    if (pic >= 265 && pic <= 269)
        pic = 265;
    return actorscrptr[pic] != 0;
}

static int toexe(int index)
{
    return index ? EXE_SCRIPT + 4 * (index - 1) : 0;
}

/* The saved order no longer matches the sprites once the board renumbers
   them, so a restored value is taken as a script address by its range. */
static int fromexe(int ptr)
{
    if (ptr < EXE_SCRIPT || ptr >= EXE_SCRIPT + 4 * scriptsize)
        return ptr;
    return (ptr - EXE_SCRIPT) / 4 + 1;
}

static void savename(char *name, size_t n, int slot)
{
    snprintf(name, n, "SAVE___%d.SAV", slot + 1);
}

static void boardname(char *name, size_t n, int slot)
{
    snprintf(name, n, "SAVE_%d%d%d.SAV", ps[0].volume + 1, ps[0].level + 1, slot + 1);
}

/* word_124C30: 256 six-byte entries, the wall and its tag. */
static void writeanimwalls(FILE *fp)
{
    unsigned char e[6];
    int i;

    for (i = 0; i < 256; i++) {
        putword(e, animwall[i].wallnum);
        memcpy(e + 2, &animwall[i].tag, 4);
        fwrite(e, 1, 6, fp);
    }
}

static void readanimwalls(FILE *fp)
{
    unsigned char e[6];
    int i;

    for (i = 0; i < 256; i++) {
        if (fread(e, 1, 6, fp) != 6)
            return;
        animwall[i].wallnum = getword(e);
        memcpy(&animwall[i].tag, e + 2, 4);
    }
}

/* 0x1B630: the players, hittype, the animated walls and the moving
   sector points, then the board through saveboard (0x49337). */
void saveplayer(int slot)
{
    char name[32];
    FILE *fp;
    int k;

    savename(name, sizeof name, slot);
    fp = plat_fopen(name, "wb");
    if (fp) {
        playertype p[MAXPLAYERS];
        int t[MAXSPRITES][5];

        memcpy(p, ps, sizeof p);
        for (k = connecthead; k >= 0; k = connectpoint2[k])
            p[k].musicptr = EXE_MUSICFN + ps[k].musicptr;
        memcpy(t, hittype, sizeof t);
        for (k = 0; k < MAXSPRITES; k++) {
            if (!conactor(sprite[k].picnum))
                continue;
            t[k][1] = toexe(t[k][1]);
            t[k][4] = toexe(t[k][4]);
        }
        fwrite(p, sizeof p, 1, fp);
        fwrite(t, sizeof t, 1, fp);
        writeanimwalls(fp);
        fwrite(msx, 4, 1024, fp);
        fwrite(msy, 4, 1024, fp);
        fclose(fp);
    }
    boardname(name, sizeof name, slot);
    saveboard(name, ps[0].posx, ps[0].posy, ps[0].posz, ps[0].ang, ps[0].cursectnum);
}

/* 0x1B700. The board's name comes from the episode and level playing now,
   before the slot is read. The sprites are renumbered as the board loads,
   but hittype keeps its saved order, as in the exe. */
void loadplayer(int slot)
{
    char name[32], board[32];
    FILE *fp;
    int k;

    boardname(board, sizeof board, slot);
    savename(name, sizeof name, slot);
    fp = plat_fopen(name, "rb");
    if (fp) {
        fread(ps, sizeof ps, 1, fp);
        fread(hittype, sizeof hittype, 1, fp);
        readanimwalls(fp);
        fread(msx, 4, 1024, fp);
        fread(msy, 4, 1024, fp);
        fclose(fp);
        for (k = connecthead; k >= 0; k = connectpoint2[k])
            ps[k].musicptr -= EXE_MUSICFN;
        for (k = 0; k < MAXSPRITES; k++) {
            hittype[k][1] = fromexe(hittype[k][1]);
            hittype[k][4] = fromexe(hittype[k][4]);
        }
    }
    if (loadboard(board) != 0)
        errorexit("Board not found.");
    ps[0].posx = posx;
    ps[0].posy = posy;
    ps[0].posz = posz;
    ps[0].ang = ang;
    ps[0].cursectnum = cursectnum;
    updatesector(ps[0].posx, ps[0].posy, &ps[0].cursectnum);
    ps[screenpeek].gm = 4;
}
