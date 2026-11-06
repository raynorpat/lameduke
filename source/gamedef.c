#include "gamedef.h"
#include "global.h"
#include "actors.h"
#include "sounds.h"
#include "buildmath.h"

#include "platform.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int script[MAXSCRIPTSIZE];
int scriptsize;
int actorscrptr[MAXTILES];
char labels[MAXLABELS + 1][64]; /* the spare row holds a name being read when the table is full */
int labelcode[MAXLABELS];
int labelcnt;
char musicfn[5][10][13];

const char *const keyw[NUMKEYWORDS] = {
    "end", "actor", "addbatteryammo", "ifrnd", "enda", "ifcansee", "ifhitweapon", "action",
    "ifpdistl", "ifpdistg", "else", "strength", "break", "shoot", "addscore", "sound", "fall",
    "state", "ends", "define", "//", "nuke", "killit", "addweapon", "weapon", "addhealth", "ifdead",
    "ifsquished", "cactor", "{", "}", "spawn", "move", "ifwasweapon", "ifaction", "ifactioncount",
    "resetactioncount", "debris", "ifskill", "/*", "cstat", "ifmove", "resetplayer", "ifonwater",
    "ifinwater", "ifplayersweapon", "ifmovecount", "resetmovecount", "addinventory", "addammo",
    "hitradius", "ifp", "addrammo", "sink", "music", "include", "ifstrength", "blood", "guts",
    "ifspawnedby"
};

const char *const consounds[NUMCONSOUNDS] = {
    "elev1.voc", "shooting.voc", "running.voc", "expl1.voc", "xoptor.voc", "glass.voc",
    "door1.voc", "knuckle.voc", "jump.voc", "land.voc", "scuba.voc", "waterfal.voc",
    "getweapn.voc", "bodyblop.voc", "bodybloa.voc", "bodyblor.voc", "onBoard.voc",
    "dripping.voc", "jetPakOn.voc", "jetPakI.voc", "fta_hm.voc", "fta_hf.voc", "fta_r.voc",
    "liteflck.voc", "biglite.voc", "switch.voc", "reactor.voc", "jetpakof.voc", "runningw.voc",
    "squish1.voc", "rebound.voc", "gun1.voc", "subway.voc", "clipin.voc", "clipout.voc",
    "elev3.voc", "elev2.voc", "door2.voc"
};

enum {
    K_END = 0, K_ACTOR = 1, K_ENDA = 4, K_ACTION = 7, K_ELSE = 10, K_SOUND = 15, K_STATE = 17,
    K_ENDS = 18, K_DEFINE = 19, K_COMMENT = 20, K_LBRACE = 29, K_RBRACE = 30, K_MOVE = 32,
    K_BLOCKCOMMENT = 39, K_HITRADIUS = 50, K_IFP = 51, K_MUSIC = 54, K_INCLUDE = 55
};

static char *textptr;               /* 0x125230 */
static int scriptptr;               /* 0x12524C, as an index */
static short line_number;           /* 0x131DF6 */
static short total_lines;           /* 0x131DF8 */
static unsigned char warning, error; /* 0x1257BD, 0x1257BE */
static int parsing_actor = -1;      /* 0x125240; the exe uses 0 for none */
static unsigned char parsing_state; /* 0x131DFE */
static signed char num_braces;      /* 0x131DFD */
static unsigned char checking_ifelse; /* 0x131DFC */
static char tempbuf[256];           /* 0x124830 */

/* IsTable & 0xE0: letters and digits. */
static int isalnum_w(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int isdigit_w(char c)
{
    return c >= '0' && c <= '9';
}

/* 0x3E770 */
static int isaltok(char c)
{
    return isalnum_w(c) || c == '{' || c == '}' || c == '/' || c == '*' || c == '-' || c == '_' || c == '.';
}

/* 0x3E740. Counts the line when it meets a newline it does not consume. */
static int ispecial(char c)
{
    if (c == '\n') {
        line_number++;
        return 1;
    }
    return c == ' ' || c == '\r';
}

static void emit(int v)
{
    if (scriptptr >= 0 && scriptptr < MAXSCRIPTSIZE)
        script[scriptptr] = v;
    scriptptr++;
}

static int lastemitted(void)
{
    return scriptptr > 0 && scriptptr <= MAXSCRIPTSIZE ? script[scriptptr - 1] : 0;
}

/* Copies the next token into tempbuf without moving textptr.
   Returns its keyword index, -1 for a non-keyword, or -2 at the end of text. */
static int peekkeyword(void)
{
    char *p = textptr;
    int i;

    while (!isaltok(*p)) {
        if (*p == 0)
            return -2;
        p++;
    }
    for (i = 0; isaltok(p[i]) && i < (int)sizeof tempbuf - 1; i++)
        tempbuf[i] = p[i];
    tempbuf[i] = 0;
    for (i = 0; i < NUMKEYWORDS; i++)
        if (strcmp(tempbuf, keyw[i]) == 0)
            return i;
    return -1;
}

static int skiptotoken(void)
{
    while (!isaltok(*textptr)) {
        if (*textptr == '\n')
            line_number++;
        if (*textptr == 0)
            return 0;
        textptr++;
    }
    return 1;
}

static int copytoken(void)
{
    int i;

    for (i = 0; isaltok(textptr[i]) && i < (int)sizeof tempbuf - 1; i++)
        tempbuf[i] = textptr[i];
    tempbuf[i] = 0;
    return i;
}

/* 0x3EAE0 */
static void transnum(void)
{
    int len, i;

    if (!skiptotoken())
        return;
    len = copytoken();
    for (i = 0; i < labelcnt; i++) {
        if (strcmp(tempbuf, labels[i]) == 0) {
            emit(labelcode[i]);
            textptr += len;
            return;
        }
    }
    if (!isdigit_w(*textptr) && *textptr != '-') {
        printf("  * ERROR!(L%ld) Paramater '%s' is undefined.\n", (long)line_number, tempbuf);
        error++;
        textptr += len;
        return;
    }
    emit(atoi(textptr));
    textptr += len;
}

/* 0x3E920. Emits and returns the keyword index, or the value of a label. */
static int getkeyword(void)
{
    int len, i;

    if (!skiptotoken())
        return -1;
    len = copytoken();
    for (i = 0; i < NUMKEYWORDS; i++) {
        if (strcmp(tempbuf, keyw[i]) == 0) {
            emit(i);
            textptr += len;
            return i;
        }
    }
    for (i = 0; i < labelcnt; i++) {
        if (strcmp(tempbuf, labels[i]) == 0) {
            emit(labelcode[i]);
            textptr += len;
            return labelcode[i];
        }
    }
    textptr += len;
    printf("  * ERROR!(L%ld) Symbol '%s' is undefined.\n", (long)line_number, tempbuf);
    error++;
    return -1;
}

/* The name a definition takes: from the next letter or digit up to blank space. */
static void copylabel(char *dst)
{
    int i = 0;

    while (!isalnum_w(*textptr)) {
        if (*textptr == '\n')
            line_number++;
        textptr++;
        if (*textptr == 0) {
            dst[0] = 0;
            return;
        }
    }
    while (*textptr && !ispecial(*textptr) && i < 63)
        dst[i++] = *textptr++;
    dst[i] = 0;
}

static int findlabel(const char *name, int count)
{
    int i;

    for (i = 0; i < count; i++)
        if (strcmp(name, labels[i]) == 0)
            return i;
    return -1;
}

/* Keeps the name just read into labels[labelcnt] as a label for code. */
static void addlabel(int code)
{
    if (labelcnt < MAXLABELS) {
        labelcode[labelcnt++] = code;
        return;
    }
    printf("  * ERROR!(L%ld) More than %d labels.\n", (long)line_number, MAXLABELS);
    error++;
}

/* ORs the numbers that follow, up to the next keyword, into one word. */
static void emitflags(void)
{
    int m = 0;

    while (peekkeyword() == -1) {
        transnum();
        m |= lastemitted();
        scriptptr--;
    }
    emit(m);
}

/* Defines a label of `count` words (action 5, move 2), padding with zeros. */
static void definedata(int count, const char *warn)
{
    int j;

    scriptptr--;
    copylabel(labels[labelcnt]);
    if (findlabel(labels[labelcnt], labelcnt) >= 0) {
        warning++;
        printf(warn, (long)line_number, labels[labelcnt]);
    } else {
        addlabel(scriptptr);
    }
    for (j = 0; j < count; j++) {
        if (peekkeyword() != -1)
            break;
        transnum();
    }
    for (; j < count; j++)
        emit(0);
}

static int parsecommand(void);

static void musiclist(char table[10][13])
{
    int n = 0;

    for (;;) {
        int i;

        if (peekkeyword() != -1)
            break;
        while (!isaltok(*textptr) && *textptr) {
            if (*textptr == '\n')
                line_number++;
            textptr++;
        }
        for (i = 0; isaltok(textptr[i]) && i < 12; i++)
            table[n][i] = textptr[i];
        while (isaltok(textptr[i]))
            i++;
        textptr += i;
        table[n][i < 12 ? i : 12] = 0;
        if (n > 8)
            break;
        n++;
    }
    /* The exe then copies slot n over slots 9..n+1. */
    if (n < 9) {
        int k;

        for (k = 9; k > n; k--)
            memcpy(table[k], table[n], 13);
    }
}

/* 0x3ED60. Returns nonzero when compiling should stop. */
static int parsecommand(void)
{
    int tw, i, j, slot;

    if (warning + error > 12)
        return 1;
    /* a command writes at most a few words past scriptptr without emit's check */
    if (scriptptr >= MAXSCRIPTSIZE - 8) {
        printf("  * ERROR!(L%ld) Script is larger than %d words.\n", (long)line_number, MAXSCRIPTSIZE);
        error++;
        return 1;
    }
    if (textptr[0] == 0 || textptr[1] == 0)
        return 1;
    tw = getkeyword();
    switch (tw) {
    case K_END:
        script[scriptptr - 1] = (int)0x80000000;
        return 1;
    case K_ACTOR:
        if (parsing_state) {
            printf("  * ERROR!(L%ld) Found 'actor' within 'state'.\n", (long)line_number);
            error++;
        }
        if (parsing_actor >= 0) {
            printf("  * ERROR!(L%ld) Found 'actor' within 'actor'.\n", (long)line_number);
            error++;
        }
        num_braces = 0;
        scriptptr--;
        parsing_actor = scriptptr;
        transnum();
        scriptptr--;
        if ((unsigned)script[scriptptr] < MAXTILES)
            actorscrptr[script[scriptptr]] = parsing_actor;
        for (i = 0; i < 4; i++)
            script[parsing_actor + i] = 0;
        /* strength, action, move, then the OR of the move flags. */
        for (j = 0;;) {
            if (peekkeyword() != -1) {
                scriptptr += 4 - j;
                break;
            }
            transnum();
            script[parsing_actor + j] = lastemitted();
            j++;
            if (j == 3) {
                emitflags();
                break;
            }
        }
        checking_ifelse = 0;
        return 0;
    case K_ENDA:
        if (parsing_actor < 0) {
            printf("  * ERROR!(L%ld) Found 'aend' without defining 'actor'.\n", (long)line_number);
            error++;
            return 0;
        }
        if (num_braces > 0) {
            printf("  * ERROR!(L%ld) Found more '{' than '}' before 'aend'.\n", (long)line_number);
            error++;
        }
        if (num_braces < 0) {
            printf("  * ERROR!(L%ld) Found more '}' than '{' before 'aend'.\n", (long)line_number);
            error++;
            return 0;
        }
        parsing_actor = -1;
        return 0;
    case 2: case 11: case 13: case 14: case 23: case 24: case 25: case 28: case 31: case 37:
    case 40: case 48: case 49: case 52: case 57:
        transnum();
        return 0;
    case K_HITRADIUS:
        transnum();
        transnum();
        return 0;
    case K_ACTION:
        if (parsing_actor >= 0 || parsing_state) {
            transnum();
            return 0;
        }
        definedata(5, "  * WARNING.(L%ld) Dupiclate action '%s' ignored.\n");
        return 0;
    case K_MOVE:
        if (parsing_actor >= 0 || parsing_state) {
            transnum();
            emitflags();
            return 0;
        }
        definedata(2, "  * WARNING.(L%ld) Dupiclate move '%s' ignored.\n");
        return 0;
    case 3: case 8: case 9: case 33: case 34: case 35: case 38: case 41: case 45: case 46:
    case 56: case 59:
        transnum();
        /* fall through */
    case 5: case 6: case 26: case 27: case 43: case 44: case K_IFP:
        if (tw == K_IFP)
            emitflags();
        slot = scriptptr++;
        for (;;) {
            int k = peekkeyword();

            if (k != K_COMMENT && k != K_BLOCKCOMMENT)
                break;
            parsecommand();
        }
        parsecommand();
        if (slot < MAXSCRIPTSIZE)
            script[slot] = scriptptr;
        checking_ifelse++;
        return 0;
    case K_ELSE:
        if (checking_ifelse) {
            checking_ifelse--;
            slot = scriptptr++;
            parsecommand();
            if (slot < MAXSCRIPTSIZE)
                script[slot] = scriptptr;
        } else {
            scriptptr--;
            warning++;
            printf("  * WARNING.(L%ld) Found 'else' with no 'if', ignored.\n", (long)line_number);
        }
        return 0;
    case K_SOUND:
        copylabel(labels[labelcnt]);
        for (i = 0; i < NUMCONSOUNDS; i++) {
            if (strcmp(consounds[i], labels[labelcnt]) == 0) {
                emit(i);
                return 0;
            }
        }
        printf("  * ERROR!(L%ld) Sound '%s' not found.\n", (long)line_number,
            labels[labelcnt]);
        error++;
        return 0;
    case K_STATE:
        if (parsing_actor < 0 && !parsing_state) {
            copylabel(labels[labelcnt]);
            parsing_state = 1;
            scriptptr--;
            addlabel(scriptptr);
            return 0;
        }
        copylabel(labels[labelcnt]);
        i = findlabel(labels[labelcnt], labelcnt);
        if (i >= 0)
            script[scriptptr] = labelcode[i];
        else {
            printf("  * ERROR!(L%ld) State '%s' not found.\n", (long)line_number,
                labels[labelcnt]);
            error++;
        }
        scriptptr++;
        return 0;
    case K_ENDS:
        if (parsing_state) {
            parsing_state = 0;
        } else {
            printf("  * ERROR!(L%ld) Found 'ends' with no 'state'.\n", (long)line_number);
            error++;
        }
        return 0;
    case K_DEFINE:
        copylabel(labels[labelcnt]);
        i = findlabel(labels[labelcnt], labelcnt);
        if (i >= 0) {
            warning++;
            printf("  * WARNING.(L%ld) Dupiclate definition '%s' ignored.\n", (long)line_number,
                labels[labelcnt]);
        }
        transnum();
        if (i < 0)
            addlabel(lastemitted());
        scriptptr -= 2;
        return 0;
    case K_COMMENT:
        scriptptr--;
        /* stop on the newline; skiptotoken counts it */
        while (*textptr != '\n' && *textptr)
            textptr++;
        return 0;
    case K_BLOCKCOMMENT:
        scriptptr--;
        do {
            textptr++;
            if (*textptr == '\n')
                line_number++;
            if (*textptr == 0) {
                printf("  * ERROR!(L%ld) Found '/*' with no '*/'.\n", (long)line_number);
                error++;
                return 0;
            }
        } while (*textptr != '*' || textptr[1] != '/');
        textptr += 2;
        return 0;
    case K_LBRACE:
        num_braces++;
        while (parsecommand() == 0)
            ;
        return 0;
    case K_RBRACE:
        /* with no '{' open, returning 1 would end the whole compile */
        if (num_braces <= 0) {
            printf("  * ERROR!(L%ld) Found '}' with no '{'.\n", (long)line_number);
            error++;
            return 0;
        }
        num_braces--;
        return 1;
    case K_MUSIC:
        scriptptr--;
        transnum();
        i = lastemitted() - 1;
        scriptptr--;
        musiclist(musicfn[i >= 0 && i < 4 ? i + 1 : 0]);
        return 0;
    case K_INCLUDE: {
        char *saved = textptr, *buf;
        short savedline;
        unsigned char savedif;
        FILE *fp;
        long len;

        scriptptr--;
        skiptotoken();
        textptr += copytoken();
        printf("    Loading include file %s\n", tempbuf);
        fp = plat_fopen(tempbuf, "rb");
        len = 0;
        if (fp) {
            fseek(fp, 0, SEEK_END);
            len = ftell(fp);
            fseek(fp, 0, SEEK_SET);
        }
        savedline = line_number;
        savedif = checking_ifelse;
        checking_ifelse = 0;
        saved = textptr;
        buf = (char *)calloc((size_t)len + 2, 1);
        if (!buf) {
            puts("  * ERROR! Not enough memory.");
            exit(0);
        }
        if (fp) {
            len = (long)fread(buf, 1, (size_t)len, fp);
            fclose(fp);
        }
        buf[len] = 0;
        textptr = buf;
        while (parsecommand() == 0)
            ;
        textptr = saved;
        /* The include keeps counting from the including line, and that count is added. */
        total_lines = (short)(line_number + total_lines);
        line_number = savedline;
        checking_ifelse = savedif;
        free(buf);
        return 0;
    }
    default:
        return 0;
    }
}

/* 0x401E0. A missing game.con compiles as an empty script, as in the exe. */
int compilecon(void)
{
    FILE *fp = plat_fopen("game.con", "rb");
    char *text;
    long len = 0;

    puts("\nLoading script...");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }
    text = (char *)calloc((size_t)len + 2, 1);
    if (!text) {
        puts("  * ERROR! Not enough memory.");
        exit(0);
    }
    if (fp) {
        len = (long)fread(text, 1, (size_t)len, fp);
        fclose(fp);
    }
    /* The exe ends the text two bytes early. */
    text[len >= 2 ? len - 2 : 0] = 0;
    puts("    Compiling...");
    memset(actorscrptr, 0, sizeof actorscrptr);
    labelcnt = 0;
    scriptptr = 1; /* script[0] stays 0 so index 0 means none, as NULL does in the exe */
    warning = 0;
    error = 0;
    total_lines = 0;
    line_number = 1;
    textptr = text;
    while (parsecommand() == 0)
        ;
    scriptsize = scriptptr;
    printf("  * Found %ld warning(s) and %ld error(s).\n", (long)warning, (long)error);
    free(text);
    if (error)
        exit(0);
    /* The exe exits on any key below 32. Console input here comes a line at a
       time and may be absent, so only an explicit Esc exits. */
    if (warning) {
        puts("  * Press any key to ignore, or ESC to exit.");
        if (getchar() == 27)
            exit(0);
    }
    total_lines = (short)(total_lines + line_number);
    printf("    Total Lines:%ld.\n", (long)total_lines);
    printf("    Code Size:%ld bytes.\n", (long)(scriptptr - 1) * 4);
    return 0;
}

#define APLAYER 283

static int insptr;                  /* dword_125234, as an index */
static unsigned char killit;        /* byte_1257BF */
static short hitweapon = -1;        /* word_131DFA */
static short canseecache = -1;      /* word_131DF4 */

/* 0x403B0. The heading, out of angs evenly spaced ones, with the longest
   clear line from sprite i. */
static short furthestangle(short i, short angs)
{
    spritetype *s = &sprite[i];
    int a = 0, best = 0, greatest = INT_MIN;
    short hitsect, hitwall;
    int hx, hy, hz;

    do {
        int d;

        hitscan(s->x, s->y, s->z - 0x800, s->sectnum, sintable()[(a + 512) & 2047], sintable()[a & 2047], 0,
            &hitsect, &hitwall, &hitsect, &hx, &hy, &hz); /* the exe aliases hitsprite to hitsect */
        d = abs(hx - s->x) + abs(hy - s->y);
        if (d > greatest) {
            greatest = d;
            best = a;
        }
        a += 2048 / angs;
    } while ((short)a < 2048);
    return (short)best;
}

static short *strengthof(short i)
{
    return sprite[i].picnum == APLAYER ? &ps[sprite[i].yvel].w54 : &sprite[i].extra;
}

static int parse(short i, short p, int dist);

/* The tail every if shares. slot is the index of the jump slot; the true
   branch runs the one command after it, the false one jumps and runs a
   following else. */
static void doif(short i, short p, int dist, int cond, int slot)
{
    if (cond) {
        insptr = slot + 1;
        parse(i, p, dist);
        return;
    }
    insptr = script[slot];
    if (script[insptr] == K_ELSE) {
        insptr += 2;
        parse(i, p, dist);
    }
}

/* 0x408B0. Runs one command for sprite i with player p at distance dist;
   1 ends the enclosing block. execute (0x42260) inlines the same switch. */
static int parse(short i, short p, int dist)
{
    spritetype *s = &sprite[i];
    playertype *pl = &ps[p];
    int *t = hittype[i];
    int c = insptr, arg = script[c + 1], k;

    if (killit)
        return 1;
    switch (script[c]) {
    case 2: /* addbatteryammo */
        pl->w4e += (short)arg;
        insptr = c + 2;
        return 0;
    case 3: /* ifrnd */
        doif(i, p, dist, (krand() >> 8) >= 255 - arg, c + 2);
        return 0;
    case 4: /* enda */
    case 12: /* break */
    case 18: /* ends */
        return 1;
    case 5: /* ifcansee */
        if (canseecache == -1) {
            k = getincangle(s->ang, getangle(pl->posx - s->x, pl->posy - s->y), 0);
            canseecache = (short)(k <= -128 || k >= 128 ? 0
                : cansee(s->x, s->y, s->z - (s->yrepeat << 7), s->sectnum, pl->posx, pl->posy,
                      pl->posz - 4096, pl->cursectnum));
        }
        doif(i, p, dist, canseecache != 0, c + 1);
        return 0;
    case 6: /* ifhitweapon */
        hitweapon = ifhitweapon(i);
        doif(i, p, dist, hitweapon >= 0, c + 1);
        return 0;
    case 7: /* action */
        t[2] = t[3] = 0;
        t[4] = arg;
        insptr = c + 2;
        return 0;
    case 8: /* ifpdistl */
        doif(i, p, dist, dist < arg, c + 2);
        return 0;
    case 9: /* ifpdistg */
        doif(i, p, dist, dist > arg, c + 2);
        return 0;
    case K_ELSE:
        insptr = arg;
        return 0;
    case 11: /* strength */
        *strengthof(i) = (short)arg;
        insptr = c + 2;
        return 0;
    case 13: /* shoot */
        shoot(i, (short)arg, 0);
        insptr = c + 2;
        return 0;
    case 14: /* addscore */
        pl->f36 += arg;
        insptr = c + 2;
        return 0;
    case K_SOUND:
        spritesound((short)arg, i);
        insptr = c + 2;
        return 0;
    case 16: /* fall */
        insptr = c + 1;
        if (s->z >= sector[s->sectnum].floorz) {
            s->zvel = 0;
            s->z = sector[s->sectnum].floorz;
        } else {
            if (s->zvel < 2048)
                s->zvel += 256;
            s->z += s->zvel;
        }
        return 0;
    case K_STATE:
        insptr = arg;
        while (parse(i, p, dist) == 0)
            ;
        insptr = c + 2;
        return 0;
    case 21: /* nuke */
        insptr = c + 1;
        nuke(s->x, s->y, s->z, i);
        return 0;
    case 22: /* killit */
        killit = 1;
        insptr = c + 1;
        return 0;
    case 23: /* addweapon */
        insptr = c + 2;
        addweapon(p, (short)arg, 0);
        return 0;
    case 24: /* weapon */
        insptr = c + 2;
        return 0;
    case 25: /* addhealth */
        insptr = c + 2;
        if (debugmode || pl->b96 == 0)
            pl->w54 = pl->w54 > 0 ? (short)(pl->w54 + arg) : 0;
        else
            pl->b96 -= 4;
        return 0;
    case 26: /* ifdead */
        doif(i, p, dist, (s->picnum == APLAYER ? ps[s->yvel].w54 - 1 : s->extra) < 0, c + 1);
        return 0;
    case 27: /* ifsquished */
        doif(i, p, dist, ifsquished(i, p) >= 0, c + 1);
        return 0;
    case 28: /* cactor */
        s->picnum = (short)arg;
        insptr = c + 2;
        return 0;
    case K_LBRACE:
        insptr = c + 1;
        while (parse(i, p, dist) == 0)
            ;
        return 0;
    case K_RBRACE:
        insptr = c + 1;
        return 1;
    case 31: /* spawn */
        insptr = c + 2;
        spawn(i, (short)arg);
        return 0;
    case K_MOVE:
        t[0] = 0;
        t[1] = arg;
        s->hitag = (short)script[c + 2];
        insptr = c + 3;
        if (s->hitag & 4)
            s->ang = furthestangle(i, 8);
        return 0;
    case 33: /* ifwasweapon */
        doif(i, p, dist, hitweapon >= 0 && sprite[hitweapon].picnum == arg, c + 2);
        return 0;
    case 34: /* ifaction */
        doif(i, p, dist, t[4] == arg, c + 2);
        return 0;
    case 35: /* ifactioncount */
        doif(i, p, dist, t[2] >= arg, c + 2);
        return 0;
    case 36: /* resetactioncount */
        insptr = c + 1;
        t[2] = 0;
        return 0;
    case 37: /* debris */
        for (k = 0; k < arg; k++) {
            short zv = (short)(-(krand() & 511) - 512), xv = (short)((krand() & 128) + 64), a = (short)(krand() & 2047);

            EGS(s->sectnum, s->x, s->y, s->z, 0, (short)(krand() % 11 + 235), -8, 32, 32, a, xv, zv, i, 5);
        }
        insptr = c + 2;
        return 0;
    case 38: /* ifskill */
        doif(i, p, dist, pl->skill < arg, c + 2);
        return 0;
    case 40: /* cstat */
        s->cstat = (unsigned char)arg;
        insptr = c + 2;
        return 0;
    case 41: /* ifmove */
        doif(i, p, dist, t[1] == arg, c + 2);
        return 0;
    case 42: /* resetplayer */
        insptr = c + 1;
        resetplayer(p);
        return 0;
    case 43: /* ifonwater */
        doif(i, p, dist, (sector[s->sectnum].lotag & 0x7FFF) == 1
                && sector[s->sectnum].floorz - s->z < (s->yrepeat << 7), c + 1);
        return 0;
    case 44: /* ifinwater */
        doif(i, p, dist, (sector[s->sectnum].lotag & 0x7FFF) == 2, c + 1);
        return 0;
    case 45: /* ifplayersweapon */
        doif(i, p, dist, pl->w7e == arg, c + 2);
        return 0;
    case 46: /* ifmovecount */
        doif(i, p, dist, t[0] >= arg, c + 2);
        return 0;
    case 47: /* resetmovecount */
        insptr = c + 1;
        t[0] = 0;
        return 0;
    case 48: /* addinventory */
        if (arg == 0)
            pl->b99 = 1;
        else if (arg == 1)
            pl->b9a = 1;
        else if (arg == 2)
            pl->b97 = 1;
        else if (arg == 3)
            pl->b9b = 1;
        else if (arg == 4)
            pl->b98 = 1;
        insptr = c + 2;
        return 0;
    case 49: /* addammo */
        pl->w4c += (short)arg;
        insptr = c + 2;
        return 0;
    case K_HITRADIUS:
        hitradius(i, arg, script[c + 2]);
        insptr = c + 3;
        return 0;
    case K_IFP: {
        int vel = syncfvel[p] + syncsvel[p], run = syncbits[p] & 0x20, cond;

        cond = ((arg & 8) && pl->b8f && (syncbits[p] & 2))
            || ((arg & 0x10) && pl->w5a == 0 && pl->b8f == 0
                && sector[pl->cursectnum].floorz - pl->posz > 18432)
            || ((arg & 0x20) && pl->w5a > 348)
            || ((arg & 1) && (unsigned)vel < 8)
            || ((arg & 2) && vel >= 8 && !run)
            || ((arg & 4) && vel >= 8 && run)
            || ((arg & 0x40) && s->z - 12288 > pl->posz)
            || ((arg & 0x80) && vel <= -8 && !run)
            || ((arg & 0x100) && vel <= -8 && run)
            || ((arg & 0x200) && pl->lookang < -128)
            || ((arg & 0x400) && pl->lookang > 128);
        /* 0x800 excludes the three tests after it, as the exe's else-if does. */
        if (!cond && (arg & 0x800))
            cond = ((s->ang + 3200 - (cameraspr >= 0 ? sprite[cameraspr].ang : 0)) & 2047) >> 8 < 2;
        else if (!cond)
            cond = ((arg & 0x1000) && pl->bab) || ((arg & 0x2000) && pl->b8f) || ((arg & 0x4000) && pl->b94);
        doif(i, p, dist, cond, c + 2);
        return 0;
    }
    case 52: /* addrammo */
        pl->w52 += (short)arg;
        insptr = c + 2;
        return 0;
    case 53: /* sink */
        s->z += 4096;
        s->xvel >>= 1;
        insptr = c + 1;
        return 0;
    case 56: /* ifstrength */
        doif(i, p, dist, *strengthof(i) <= arg, c + 2);
        return 0;
    case 57: /* blood */
        for (k = 0; k < arg; k++) {
            short zv = (short)(-1024 - (krand() & 255)), a = (short)(s->ang + (krand() & 127));
            int yr = (krand() & 7) + 8, xr = (krand() & 7) + 8;

            EGS(s->sectnum, s->x, s->y, s->z - 0x2000, krand() & 2, 488, -16, xr, yr, a, 0, zv, i, 5);
        }
        insptr = c + 2;
        return 0;
    case 58: /* guts */
        insptr = c + 1;
        guts(i);
        return 0;
    case 59: /* ifspawnedby: the exe compares the owner's index with a tile number */
        doif(i, p, dist, s->owner >= 0 && s->owner == arg, c + 2);
        return 0;
    default:
        snprintf(tempbuf, sizeof tempbuf, "%d", s->picnum);
        errorexit(tempbuf);
        return 1;
    }
}

/* 0x42260. Steps sprite i's action, runs its actor code, then applies its
   move and its hitag move flags. */
void execute(short i, short p, int dist)
{
    spritetype *s = &sprite[i];
    int *t = hittype[i];
    int a = actorscrptr[s->picnum];
    short h;

    if (!a)
        return;
    insptr = a + 4;
    killit = 0;
    hitweapon = -1;
    canseecache = -1;
    if (t[4]) {
        const int *act = &script[t[4]];

        s->lotag += synctics;
        if (s->lotag > act[4]) {
            t[2]++;
            s->lotag = 0;
            t[3] += act[3];
        }
        if (abs(t[3]) >= abs(act[3] * act[1]))
            t[3] = 0;
    }
    while (parse(i, p, dist) == 0)
        ;
    if (killit) {
        deletesprite(i);
        return;
    }
    h = s->hitag;
    if (!t[1] || !h)
        return;
    t[0]++;
    if ((h & 0x200) && s->owner >= 0 && sprite[s->owner].statnum == 1) {
        s->x = sprite[s->owner].x;
        s->y = sprite[s->owner].y;
        s->z = sprite[s->owner].z;
    }
    if (h & 1)
        s->xvel >>= 1;
    if (h & 2)
        s->ang = (short)getangle(ps[p].posx - s->x, ps[p].posy - s->y);
    if (h & 8)
        s->xvel += (script[t[1]] - s->xvel) >> 1;
    if (h & 0x10)
        s->zvel += (16 * script[t[1] + 1] - s->zvel) >> 1;
    if (h & 0x20) {
        int fz = sector[s->sectnum].floorz;

        if (s->z + 0x2000 < fz)
            s->zvel += 16 * (short)script[t[1] + 1];
        else if (s->z + 0x2000 > fz)
            s->zvel -= 16 * (short)script[t[1] + 1];
    }
    if (h & 0x40)
        s->z = sector[s->sectnum].ceilingz + 2048;
    if (h & 0x400)
        s->z = sector[s->sectnum].floorz - 2048;
    if (h & 0x80)
        s->ang += getincangle(s->ang, getangle(ps[p].posx - s->x, ps[p].posy - s->y), 1);
    if ((s->zvel | s->xvel) && !ssp(i, 1) && (h & 0x100) && (sector[s->sectnum].lotag & 0x7FFF) != 1) {
        s->ang += 256;
        ssp(i, 1);
    }
}
