/* The LameDuke side of build.c's bstub.h hooks. Every name shown comes
   from this repo: tiles from DEFS.CON's defines, sector and effector tags
   from the game code that acts on them (cited per entry). A tag with no
   entry shows as its number. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "bstub.h"
#include "editor.h"

void editinput(void);
void printmessage16(char name[82]);
void printmessage256(char name[82]);
extern char names[MAXTILES][17];

typedef struct {
    int tag;
    const char *name;
} tagname;

/* Sector lotags (the 0x8000 bit is the open/closed state). */
static const tagname sectorlotagname[] = {
    { 1, "water" },          /* gamedef.c ifonwater, player.c swimming */
    { 2, "underwater" },     /* gamedef.c ifinwater, premap.c palette 1 */
    { 9, "star door" },      /* sector.c operatesectors case 0: stardoor */
    { 15, "SE 17 lift" },    /* operatesectors case 6: movelift on its SE 17 */
    { 16, "floor down" },    /* case 7: floor to the next lower floor */
    { 17, "floor up" },      /* case 8: floor to the next higher floor */
    { 18, "elevator down" }, /* case 9: floor and ceiling down together */
    { 19, "elevator up" },   /* case 10: floor and ceiling up together */
    { 20, "ceiling door" },  /* case 11: ceiling up, back to SE 9 or the floor */
    { 21, "floor door" },    /* case 12: floor up to the ceiling and back */
    { 22, "split door" },    /* case 13: ceiling and floor meet in the middle */
    { 23, "swinging door" }, /* case 14: turns its SE 11s of one hitag */
    { 25, "sliding door" },  /* case 16: its SE 15, and SE 18 of the hitag */
    { 26, "split star door" }, /* case 17: runs lotag 22 then 9 */
    { 27, "SE 20 slider" },  /* case 18: starts its SE 20 */
    { 28, "SE 21 trigger" }, /* case 19: starts the SE 21s of its hitag */
    { 29, "SE 22 door" },    /* case 20: ceiling door that moves its SE 22s */
    { 32767, "secret" },     /* premap.c: counted for the bonus screen */
};

/* SECTOREFFECTOR (tile 175) lotags, from actors.c moveeffectors. */
static const tagname selotagname[] = {
    { 0, "rotate" },          /* case 0: turns about its SE 1 or 5 */
    { 3, "flicker on hit" },  /* case 3: random shade once set off */
    { 4, "flicker" },         /* case 4: random shade */
    { 5, "boss ship" },       /* case 5: hunts locators */
    { 6, "train engine" },    /* case 6, then movetrain */
    { 10, "door closer" },    /* case 10: reoperates lotag 23 after hitag tics */
    { 11, "swinging door" },  /* case 11 */
    { 12, "light switch" },   /* case 12: shade on and off */
    { 13, "blast ceiling" },  /* case 13 */
    { 14, "train car" },      /* case 14: movetrain */
    { 15, "sliding door" },   /* case 15 */
    { 17, "lift" },           /* case 17 */
    { 18, "pull" },           /* case 18: draws the player toward it */
    { 20, "sliding sector" }, /* case 20 */
    { 23, "flooding sector" }, /* case 23 */
    { 24, "conveyor" },       /* case 24 */
    { 26, "shaking sector" }, /* case 26 */
    { 27, "demo camera" },    /* case 27 */
    { 128, "breaking glass" }, /* case 128 */
    { 129, "glass frames" },  /* case 129: animates overpicnum 790 */
};

static const char *lookup(const tagname *t, size_t n, int tag)
{
    size_t i;

    for (i = 0; i < n; i++)
        if (t[i].tag == tag)
            return t[i].name;
    return NULL;
}

#define SECTORNAME(lo) lookup(sectorlotagname, sizeof sectorlotagname / sizeof *sectorlotagname, (lo) & 0x7FFF)
#define SENAME(lo) lookup(selotagname, sizeof selotagname / sizeof *selotagname, lo)

static char caption[96];

/* DEFS.CON's "define NAME number" lines, read as the game reads them. */
void ExtLoadNames(char tilenames[][17])
{
    FILE *fp = plat_fopen("defs.con", "rb");
    char word[3][64];

    if (!fp)
        return;
    while (fscanf(fp, "%63s", word[0]) == 1) {
        char *end;
        long v;
        int i;

        for (i = 0; word[0][i]; i++)
            word[0][i] = (char)tolower((unsigned char)word[0][i]);
        if (strcmp(word[0], "define") != 0 || fscanf(fp, "%63s %63s", word[1], word[2]) != 2)
            continue;
        v = strtol(word[2], &end, 10);
        if (*end == 0 && v >= 0 && v < MAXTILES && !tilenames[v][0])
            snprintf(tilenames[v], 17, "%s", word[1]);
    }
    fclose(fp);
}

void ExtInit(void)
{
    platform_init();
    initengine();
    loadlookup();
    /* For the names check: every table entry, then exit. */
    if (getenv("LAMEDUKE_NAMES")) {
        static char tilenames[MAXTILES][17];
        size_t i;

        ExtLoadNames(tilenames);
        for (i = 0; i < sizeof sectorlotagname / sizeof *sectorlotagname; i++)
            printf("sector %d %s\n", sectorlotagname[i].tag, sectorlotagname[i].name);
        for (i = 0; i < sizeof selotagname / sizeof *selotagname; i++)
            printf("se %d %s\n", selotagname[i].tag, selotagname[i].name);
        for (i = 0; i < MAXTILES; i++)
            if (tilenames[i][0])
                printf("tile %d %s\n", (int)i, tilenames[i]);
        exit(0);
    }
}

void ExtUnInit(void) {}
void ExtLoadMap(const char *mapname) { (void)mapname; }
void ExtSaveMap(const char *mapname) { (void)mapname; }

const char *ExtGetSectorCaption(short sectnum)
{
    sectortype *s = &sector[sectnum];
    const char *n = SECTORNAME(s->lotag);

    if ((s->lotag | s->hitag) == 0)
        caption[0] = 0;
    else
        snprintf(caption, sizeof caption, "%hu,%hu%s%s", (unsigned short)s->lotag, (unsigned short)s->hitag,
            n ? " " : "", n ? n : "");
    return caption;
}

const char *ExtGetWallCaption(short wallnum)
{
    walltype *w = &wall[wallnum];

    if ((w->lotag | w->hitag) == 0)
        caption[0] = 0;
    else
        snprintf(caption, sizeof caption, "%hu,%hu", (unsigned short)w->lotag, (unsigned short)w->hitag);
    return caption;
}

const char *ExtGetSpriteCaption(short spritenum)
{
    spritetype *s = &sprite[spritenum];
    const char *n = SENAME(s->lotag);

    if (s->picnum == 175)
        snprintf(caption, sizeof caption, "SE %hu%s%s,%hu", (unsigned short)s->lotag, n ? " " : "", n ? n : "",
            (unsigned short)s->hitag);
    else if (names[s->picnum][0] && (s->lotag | s->hitag) == 0)
        snprintf(caption, sizeof caption, "%s", names[s->picnum]);
    else if (names[s->picnum][0])
        snprintf(caption, sizeof caption, "%s %hu,%hu", names[s->picnum], (unsigned short)s->lotag,
            (unsigned short)s->hitag);
    else if ((s->lotag | s->hitag) == 0)
        caption[0] = 0;
    else
        snprintf(caption, sizeof caption, "%hu,%hu", (unsigned short)s->lotag, (unsigned short)s->hitag);
    return caption;
}

static void say(void)
{
    if (ydim16)
        printmessage16(caption);
    else
        printmessage256(caption);
}

/* F5/F6: what the thing under the cursor is. */
void ExtShowSectorData(short sectnum)
{
    const char *n = SECTORNAME(sector[sectnum].lotag);

    snprintf(caption, sizeof caption, "Sector %d: lotag %hu %s", sectnum, (unsigned short)sector[sectnum].lotag,
        n ? n : "");
    say();
}

void ExtShowWallData(short wallnum)
{
    snprintf(caption, sizeof caption, "Wall %d: lotag %hu hitag %hu", wallnum, (unsigned short)wall[wallnum].lotag,
        (unsigned short)wall[wallnum].hitag);
    say();
}

void ExtShowSpriteData(short spritenum)
{
    const char *n = sprite[spritenum].picnum == 175 ? SENAME(sprite[spritenum].lotag) : NULL;

    snprintf(caption, sizeof caption, "Sprite %d: %s%s%s", spritenum,
        names[sprite[spritenum].picnum][0] ? names[sprite[spritenum].picnum] : "tile", n ? " " : "", n ? n : "");
    say();
}

void ExtEditSectorData(short sectnum) { (void)sectnum; }
void ExtEditWallData(short wallnum) { (void)wallnum; }
void ExtEditSpriteData(short spritenum) { (void)spritenum; }
void ExtPreCheckKeys(void) {}
void ExtAnalyzeSprites(void) {}

/* Once a frame in both modes; build.c's 3D keys are editinput. */
void ExtCheckKeys(void)
{
    if (!ydim16)
        editinput();
}
