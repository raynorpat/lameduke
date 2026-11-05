#include "premap.h"
#include "game.h"

#include "global.h"
#include "actors.h"
#include "gamedef.h"
#include "sector.h"
#include "sounds.h"
#include "fx_man.h"

#include "board.h"
#include "display.h"
#include "draw.h"
#include "engine.h"
#include "math.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int game_volume;
int game_level;
int game_skill;
int game_level_set;
int game_quotes;
int game_debug;
int user_visibility = 13; /* byte_1257A8, from userdefs.tmp (13 without one) */

int check_file_id(void)
{
    FILE *fp = plat_fopen("FILE_ID.DIZ", "rb");
    if (!fp)
        return 0;
    fclose(fp);
    return 0;
}

/* 0x154F0 */
static void show_help(const char *argv0)
{
    puts("\n *  Here's a little command line help...");
    printf("    %s [flags...]\n", argv0 ? argv0 : "d3d");
    puts("\t/?        This help message.\n");
    puts("\t/l#       Go to level #.");
    puts("\t/v#       Go to volume #.");
    puts("\t/s#       Skill # (0-3)");
    puts("\t/q        Turn on help quotes.");
    puts("\t/d        Activate DEBUG MODEs.");
    puts("\t/r        Record a demo.");
    puts("\t/oDIR     Reroute game output to DIR.");
    puts("\t/e        Encode maps...");
}

void check_args(int argc, char **argv)
{
    int i;

    game_volume = 0;
    game_level = 0;
    game_skill = 0;
    game_level_set = 0;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        char flag;

        if (!arg || arg[0] != '/' || arg[1] == 0) {
            printf("Unknown command line paramater '%s'\n", arg ? arg : "");
            show_help(argv[0]);
            exit(0);
        }
        flag = arg[1];
        if (flag >= 'A' && flag <= 'Z')
            flag = (char)(flag - 'A' + 'a');
        switch (flag) {
        case '?':
            show_help(argv[0]);
            exit(0);
        case 'd':
            game_debug = 1;
            break;
        case 'e': {
            /* 0x15748 only prints; no map is written. It leaves the index at 10. */
            char name[] = "L?.MAP";
            int v;

            for (v = 0; v < 3; v++) {
                for (i = 0; i < 9; i++) {
                    name[0] = (char)('L' + v);
                    name[1] = (char)('1' + i);
                    printf("Encoding %s\n", name);
                }
            }
            break;
        }
        case 'l':
            game_level_set = 1;
            game_level = atoi(arg + 2) - 1;
            break;
        case 'o':
            /* The path goes to byte_8EE78, which nothing reads. */
            printf("I/O path rerouted to %s.\n", arg + 2);
            break;
        case 'q':
            game_quotes = 1;
            break;
        case 'r':
            demomode = 1;
            puts("Demorec mode on...");
            break;
        case 's':
            game_skill = atoi(arg + 2) - 1;
            break;
        case 'v':
            game_volume = atoi(arg + 2) - 1;
            break;
        default:
            printf("Unknown command line paramater '%s'\n", arg);
            show_help(argv[0]);
            exit(0);
        }
    }
}

/* 0x2F190. A lift (SE 17) placed with shade over 16 sets every other lift
   of its hitag to its own floor and ceiling. */
static void synclifts(void)
{
    short i, j;

    for (i = headspritestat[3]; i >= 0; i = nextspritestat[i])
        if (sprite[i].lotag == 17 && sprite[i].shade > 16)
            for (j = headspritestat[3]; j >= 0; j = nextspritestat[j])
                if (sprite[j].lotag == 17 && i != j && sprite[i].hitag == sprite[j].hitag) {
                    sector[sprite[j].sectnum].floorz = sector[sprite[i].sectnum].floorz;
                    sector[sprite[j].sectnum].ceilingz = sector[sprite[i].sectnum].ceilingz;
                }
}

/* 0x22210. Resets player 0 and the level state, spawns every map sprite
   in place, and collects the animated and switch walls. */
static void prelevel(void)
{
    playertype *pl = &ps[0];
    short i, nexti;
    int k;

    pl->ba2 = pl->baa = pl->ba9 = pl->bad = pl->bac = pl->baf = pl->ba5 = pl->bae = 0;
    if (pl->f02 == 2)
        setgamemode();
    pl->f02 = 3;
    pl->oposx = pl->posx;
    pl->oposy = pl->posy;
    pl->oposz = pl->posz;
    pl->oang = pl->ang;
    pl->ocursect = pl->cursectnum;
    /* dword_125300/1252FC/1252F8, the keyboard ISR turn and strafe ramps, are
       zeroed here; this port builds input without them. */
    cameraspr = -1;
    viewscreen = -1;
    gamepaused = 0;
    numanimwalls = 0;
    pl->w84 = -1;
    myconnectindex = screenpeek;
    parallaxtype = 0;
    visibility = user_visibility;
    animatecnt = 0;
    mspos = 0;
    randomseed = 17;
    resetplayer(0);
    for (k = 0; k < 13; k++)
        (&pl->w60)[k] = 0;
    memset(show2dwall, 0xFF, 512);
    memset(show2dsprite, debugmode ? 0xFF : 0, 512);
    for (k = 0; k < numsectors; k++) {
        sector[k].extra = 256;
        if (pl->w84 == -1 && (sector[k].ceilingstat & 1))
            pl->w84 = (short)k;
        if ((sector[k].lotag & 0x7FFF) == 0x7FFF)
            pl->ba9++;
        else if (sector[k].lotag == 2)
            sector[k].floorpal = sector[k].ceilingpal = 1;
    }
    for (i = headspritestat[0]; i >= 0; i = nexti) {
        nexti = nextspritestat[i];
        if (sprite[i].picnum == 310) {
            sector[sprite[i].sectnum].extra = sprite[i].lotag;
            deletesprite(i);
        }
    }
    /* The exe inlines the in-place path of spawn here. */
    for (i = 0; i < MAXSPRITES; i++)
        if (sprite[i].statnum < MAXSTATUS)
            spawn(-1, i);
    /* The exe precaches the tiles of each kind found; every tile is resident here. */
    for (k = 0; k < numwalls; k++) {
        short pic = wall[k].picnum, op = wall[k].overpicnum;

        if (numanimwalls >= 256)
            errorexit("Maxed out on anim walls\n");
        if (op >= 1184 && op <= 1186) {
            if (wall[k].shade <= 31)
                wall[k].cstat |= 1;
            else
                wall[k].cstat = 0;
            wall[k].shade = 0;
            if (wall[k].lotag)
                wall[wall[k].nextwall].lotag = wall[k].lotag;
            animwall[numanimwalls++].wallnum = (short)k;
            continue;
        }
        switch (pic) {
        case 395:
        case 396:
        case 405:
            animwall[numanimwalls].tag = 0;
            animwall[numanimwalls++].wallnum = (short)k;
            break;
        case 406:
            wall[k].picnum = 405;
            /* exe: passes the wall lotag where checkhitswitch takes a wall */
            if (wall[k].lotag >= 0 && wall[k].lotag < MAXWALLS)
                checkhitswitch(wall[k].lotag);
            animwall[numanimwalls].tag = 0;
            animwall[numanimwalls++].wallnum = (short)k;
            break;
        case 761:
        case 764:
        case 765:
        case 766:
        case 767:
            animwall[numanimwalls].wallnum = (short)k;
            animwall[numanimwalls++].tag = pic;
            break;
        case 570:
        case 819:
        case 823:
        case 860:
        case 864:
            animwall[numanimwalls++].wallnum = (short)k;
            break;
        }
    }
}

/* 0x248A0. Fresh inventory and ammo for player 0; /d starts well armed. */
void newgame(int volume, int level, int skill)
{
    playertype *pl = &ps[0];

    pl->level = (unsigned char)level;
    pl->volume = (unsigned char)volume;
    pl->skill = (unsigned char)skill;
    pl->b93 = pl->b97 = pl->b94 = pl->b98 = pl->b95 = pl->b99 = pl->b96 = pl->b9a = pl->b92 = 0;
    pl->b9c = pl->b9e = pl->b9f = pl->ba0 = 0;
    pl->w4a = -4;
    pl->b9d = 1;
    pl->f36 = 0;
    if (debugmode) {
        pl->w52 = 99;
        pl->w50 = 696;
        pl->w4c = 144;
        pl->w4e = 120;
    } else {
        pl->w52 = 5;
        pl->w50 = 2;
        pl->w4c = 24;
        pl->w4e = 9;
    }
    pl->w5e = -1;
    pl->w7e = 191;
    /* dword_125254, the demo record position, is reset with the demo task */
}

/* 0x249B0. Loads the board of ps[0], sets the level up, copies player 0
   to every slot and binds the player sprites; mode becomes ps.gm. */
int enterlevel(int mode)
{
    char name[] = "L1.MAP";
    short p, j;
    int pal = 0;

    levelscreen(mode);
    name[0] = (char)('L' + ps[0].volume);
    name[1] = (char)('1' + ps[0].level);
    puts("Entering...");
    printf("Episode %d\n", ps[0].volume + 1);
    printf("Level %d\n", ps[0].level + 1);
    FX_StopAllSounds();
    if (loadboard(name) != 0)
        errorexit("Board not found.");
    ps[0].posx = posx;
    ps[0].posy = posy;
    ps[0].posz = posz;
    ps[0].ang = ang;
    ps[0].cursectnum = cursectnum;
    prelevel();
    EGS(ps[0].cursectnum, ps[0].posx, ps[0].posy, ps[0].posz, 0, 283, 0, 0, 0, ps[0].ang, 0, 0, 0, 10);
    synclifts();
    for (p = 1; p < MAXPLAYERS; p++)
        ps[p] = ps[0];
    memset(syncfvel, 0, sizeof syncfvel);
    memset(syncsvel, 0, sizeof syncsvel);
    memset(syncavel, 0, sizeof syncavel);
    memset(syncbits, 0, sizeof syncbits);
    /* pending: 0x11120 holds every node at the start line (multiplayer task) */
    if (setup_byte(0) == 7)
        setview(0, 0, 639, 479);
    else
        viewsize(0);
    for (p = connecthead, j = headspritestat[10]; p >= 0; p = connectpoint2[p], j = nextspritestat[j]) {
        spritetype *s = &sprite[j];

        show2dsprite[j >> 3] |= (unsigned char)(1 << (j & 7));
        s->cstat = 1;
        s->shade = -16;
        s->owner = j;
        s->yvel = p;
        ps[p].ba7 = (unsigned char)pal;
        pal = pal ? pal + 1 : 3;
        s->xrepeat = s->yrepeat = 42;
        ps[p].i = j;
        if (numplayers > 1) {
            ps[p].posx = ps[p].oposx = s->x;
            ps[p].posy = ps[p].oposy = s->y;
            ps[p].posz = ps[p].oposz = s->z;
            ps[p].ang = ps[p].oang = s->ang;
            ps[p].cursectnum = ps[p].ocursect = s->sectnum;
        }
        updatesector(s->x, s->y, &ps[p].cursectnum);
    }
    ps[screenpeek].musicptr = 130 * (ps[screenpeek].volume + 1) + 13 * ps[screenpeek].level;
    playmusic(&musicfn[0][0][0] + ps[screenpeek].musicptr);
    /* pending: 0x17890 draws the status bar */
    totalclock = lockclock = 0;
    synctics = 0;
    ps[screenpeek].gm = (unsigned char)mode;
    /* the frames before the next step blend from the new board */
    updateinterpolations();
    return 0;
}
