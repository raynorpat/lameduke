#include "gamedef.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int script[MAXSCRIPTSIZE];
int scriptsize;
int actorscrptr[MAXTILES];
char labels[MAXLABELS][64];
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
    if (labelcnt < MAXLABELS)
        copylabel(labels[labelcnt]);
    if (findlabel(labels[labelcnt], labelcnt) >= 0) {
        warning++;
        printf(warn, (long)line_number, labels[labelcnt]);
    } else if (labelcnt < MAXLABELS) {
        labelcode[labelcnt++] = scriptptr;
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
        copylabel(labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1]);
        for (i = 0; i < NUMCONSOUNDS; i++) {
            if (strcmp(consounds[i], labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1]) == 0) {
                emit(i);
                return 0;
            }
        }
        printf("  * ERROR!(L%ld) Sound '%s' not found.\n", (long)line_number,
            labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1]);
        error++;
        return 0;
    case K_STATE:
        if (parsing_actor < 0 && !parsing_state) {
            if (labelcnt < MAXLABELS)
                copylabel(labels[labelcnt]);
            parsing_state = 1;
            scriptptr--;
            if (labelcnt < MAXLABELS)
                labelcode[labelcnt++] = scriptptr;
            return 0;
        }
        copylabel(labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1]);
        i = findlabel(labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1], labelcnt);
        if (i >= 0)
            script[scriptptr] = labelcode[i];
        else {
            printf("  * ERROR!(L%ld) State '%s' not found.\n", (long)line_number,
                labels[labelcnt < MAXLABELS ? labelcnt : MAXLABELS - 1]);
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
        if (labelcnt < MAXLABELS)
            copylabel(labels[labelcnt]);
        i = findlabel(labels[labelcnt], labelcnt);
        if (i >= 0) {
            warning++;
            printf("  * WARNING.(L%ld) Dupiclate definition '%s' ignored.\n", (long)line_number,
                labels[labelcnt]);
        }
        transnum();
        if (i < 0 && labelcnt < MAXLABELS)
            labelcode[labelcnt++] = lastemitted();
        scriptptr -= 2;
        return 0;
    case K_COMMENT:
        scriptptr--;
        while (*textptr != '\n' && *textptr)
            textptr++;
        line_number++;
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
    scriptptr = 0;
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
    if (error || (warning && (puts("  * Press any key to ignore, or ESC to exit."), getchar() < 32)))
        exit(0);
    total_lines = (short)(total_lines + line_number);
    printf("    Total Lines:%ld.\n", (long)total_lines);
    printf("    Code Size:%ld bytes.\n", (long)scriptptr * 4);
    return 0;
}
