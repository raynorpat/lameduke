#include "multivoc.h"

#include "blaster.h"
#include "dsl.h"
#include "pas16.h"
#include "pitch.h"
#include "sndsrc.h"

#include <stdlib.h>
#include <string.h>

/* The 144-byte voice record of 0x6A665. Its demand-feed hook (+0x7C) is
   never set by VOC playback, so it is left out. */
typedef struct voice {
    struct voice *next, *prev;
    const unsigned char *nextblock; /* +0x08 */
    const unsigned char *loopstart; /* +0x0C */
    unsigned loopcount;             /* +0x10 */
    unsigned remaining;             /* +0x14: samples after this 0x8000 chunk */
    unsigned pitchscale;            /* +0x18 */
    const unsigned char *sound;     /* +0x1C */
    unsigned length;                /* +0x20, 16.16 */
    unsigned rate;                  /* +0x24 */
    unsigned step;                  /* +0x28, 16.16 */
    unsigned position;              /* +0x2C, 16.16 */
    int playing;                    /* +0x30 */
    int inbuffer[16];               /* +0x34: playing when that buffer was mixed */
    int handle;                     /* +0x74 */
    int priority;                   /* +0x78 */
    int volume, left, right;        /* +0x80, +0x84, +0x88: levels 0-15 */
    unsigned long callbackval;      /* +0x8C */
} voice;

int MV_ErrorCode = 0;

static int mv_installed;                     /* dword_894BC */
static int mv_maxvolume;                     /* dword_894C4 */
static int mv_swapleftright;                 /* dword_894E0 */
static unsigned mv_mixrate;                  /* dword_244058 */
static int mv_numbuffers;                    /* dword_894D4 */
static int mv_mixpage, mv_playpage;          /* dword_894F8, dword_894FC */
static int mv_voicehandle;                   /* dword_89500 */
static void (*mv_callback)(unsigned long);   /* dword_89504 */
static voice *voices;                        /* dword_894E4 */
static voice *active_head, *active_tail;     /* dword_894E8, dword_894EC */
static voice *free_head, *free_tail;         /* dword_894F0, dword_894F4 */
static unsigned short mixbuffer[16][256 * 2]; /* dword_241C08: 256 stereo frames each */
static int bufferclean[16];                  /* dword_241BC8 */
static short voltable[16][256];              /* off_894B8 */
static unsigned char pantable[32 * 16 * 2];  /* byte_241C48: [angle][distance >> 4][left, right] */

const char *MV_ErrorString(int code)
{
    if (code < 0)
        code = MV_ErrorCode;
    switch (code) {
    case 0:
        return "Multivoc ok.";
    case 1:
        return "Selected sound card is not supported by Multivoc.";
    case 2:
        return "Multivoc not installed.";
    case 3:
        return "No free voices available to Multivoc.";
    case 4:
        return "Out of memory in Multivoc.";
    case 5:
        return "No voice with matching handle found.";
    case 6:
        return BLASTER_ErrorString(-1);
    case 7:
        return PAS_ErrorString(-1);
    case 8:
        return SNDSRC_ErrorString(-1);
    case 9:
        return "DPMI Error in Multivoc.";
    default:
        return "Unknown Multivoc error code.";
    }
}

/* 0x6C6C0 and 0x6C6E8, the linked-list helpers both voice lists use. */
static void addtail(voice *v, voice **head, voice **tail)
{
    v->next = NULL;
    v->prev = *tail;
    if (*tail)
        (*tail)->next = v;
    else
        *head = v;
    *tail = v;
}

static void unlink_voice(voice *v, voice **head, voice **tail)
{
    if (v->prev)
        v->prev->next = v->next;
    else
        *head = v->next;
    if (v->next)
        v->next->prev = v->prev;
    else
        *tail = v->prev;
    v->next = v->prev = NULL;
}

static int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

/* 0x699CE */
static voice *findvoice(int handle)
{
    voice *v;

    for (v = active_head; v; v = v->next)
        if (v->handle == handle)
            return v;
    return NULL;
}

/* 0x69A5E */
int MV_Kill(int handle)
{
    voice *v;
    unsigned long val;

    if (!mv_installed) {
        MV_ErrorCode = 2;
        return -1;
    }
    DSL_Lock();
    v = findvoice(handle);
    if (!v) {
        DSL_Unlock();
        MV_ErrorCode = 5;
        return -1;
    }
    val = v->callbackval;
    unlink_voice(v, &active_head, &active_tail);
    addtail(v, &free_head, &free_tail);
    DSL_Unlock();
    if (mv_callback)
        mv_callback(val);
    MV_ErrorCode = 0;
    return 0;
}

/* 0x69A26 */
int MV_KillAllVoices(void)
{
    if (!mv_installed) {
        MV_ErrorCode = 2;
        return -1;
    }
    while (active_head)
        MV_Kill(active_head->handle);
    return 0;
}

/* 0x69B57: with no free voice, the lowest priority one gives way to an
   equal or higher priority. Handles count up from 1, skipping live ones. */
static voice *allocvoice(int priority)
{
    voice *v;

    DSL_Lock();
    if (!free_head && active_head) {
        voice *lowest = active_head;

        for (v = active_head; v; v = v->next)
            if (v->priority < lowest->priority)
                lowest = v;
        if (priority >= lowest->priority)
            MV_Kill(lowest->handle);
    }
    v = free_head;
    if (v) {
        unlink_voice(v, &free_head, &free_tail);
        do {
            if (++mv_voicehandle < 1)
                mv_voicehandle = 1;
        } while (findvoice(mv_voicehandle));
        v->handle = mv_voicehandle;
    }
    DSL_Unlock();
    return v;
}

static void setchunk(voice *v, unsigned samples)
{
    unsigned n = samples >= 0x8000 ? 0x8000 : samples;

    v->length = n << 16;
    v->remaining = samples - n;
}

/* 0x6A461: the next 0x8000-sample chunk, else the next playable VOC block. */
static void nextblock(voice *v)
{
    const unsigned char *p;
    unsigned rate = 0, len = 0;
    int done = 0;

    if (v->remaining) {
        v->sound += 0x8000;
        v->position += 0x80000000u;
        setchunk(v, v->remaining);
        return;
    }
    p = v->nextblock;
    v->playing = 1;
    while (!done) {
        int type = p[0];
        const unsigned char *after;

        len = (unsigned)p[1] | (unsigned)p[2] << 8 | (unsigned)p[3] << 16;
        p += 4;
        after = p + len;
        switch (type) {
        case 0:
            v->playing = 0;
            done = 1;
            break;
        case 1:
            rate = 0xF424000u / (0x10000u - 256u * p[0]);
            len -= 2;
            done = p[1] == 0; /* a packed block is skipped */
            p += 2;
            if (!done)
                p += len;
            break;
        case 2:
            rate = v->rate;
            done = 1;
            break;
        case 6:
            v->loopstart = after;
            v->loopcount = (unsigned)p[0] | (unsigned)p[1] << 8;
            p = after;
            break;
        case 7:
            p += len;
            if (v->loopcount && v->loopstart) {
                unsigned n = v->loopcount;

                p = v->loopstart;
                if (n < 0xFFFF) {
                    v->loopcount = n - 1;
                    if (n == 1)
                        v->loopstart = NULL;
                }
            }
            break;
        case 9:
            rate = (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
            /* 8-bit mono PCM plays; the length keeps the 12 header bytes, as in the exe */
            if (p[4] == 8 && p[5] == 1 && p[6] == 0 && p[7] == 0) {
                done = 1;
                p += 12;
            } else {
                p += len;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 8:
            p = after;
            break;
        default:
            v->playing = 0;
            done = 1;
            break;
        }
    }
    if (!v->playing)
        return;
    v->nextblock = p + len;
    v->rate = rate;
    v->sound = p;
    v->step = (unsigned)(((unsigned long long)v->pitchscale * rate) / mv_mixrate);
    v->position = 0;
    setchunk(v, len);
}

static void setlevels(voice *v, int volume, int left, int right)
{
    v->volume = clamp255(volume) >> 4;
    if (mv_swapleftright) {
        v->left = clamp255(right) >> 4;
        v->right = clamp255(left) >> 4;
    } else {
        v->left = clamp255(left) >> 4;
        v->right = clamp255(right) >> 4;
    }
}

/* 0x6A665 */
static int playvoc(const unsigned char *ptr, int pitchoffset, int vol, int left, int right, int priority,
    unsigned long callbackval)
{
    voice *v;
    int i;

    if (!mv_installed) {
        MV_ErrorCode = 2;
        return -1;
    }
    v = allocvoice(priority);
    if (!v) {
        MV_ErrorCode = 3;
        return -1;
    }
    v->loopstart = NULL;
    v->loopcount = 0;
    v->nextblock = ptr + ((unsigned)ptr[20] | (unsigned)ptr[21] << 8);
    v->remaining = 0;
    v->pitchscale = (unsigned)PITCH_GetScale(pitchoffset);
    nextblock(v);
    for (i = 0; i < 16; i++)
        v->inbuffer[i] = 1;
    setlevels(v, vol, left, right);
    v->priority = priority;
    v->callbackval = callbackval;
    DSL_Lock();
    addtail(v, &active_head, &active_tail);
    DSL_Unlock();
    MV_ErrorCode = 0;
    return v->handle;
}

/* 0x6A870 and 0x69DDC share this: the pan table gives left and right for
   the angle and distance, and the volume falls off with distance. */
static void pan3d(int angle, int distance, int *vol, int *left, int *right)
{
    int idx;

    if (distance < 0) {
        distance = -distance;
        angle += 16;
    }
    idx = 32 * (angle & 31) + 2 * (clamp255(distance) >> 4);
    *left = pantable[idx];
    *right = pantable[idx + 1];
    *vol = 255 - distance < 0 ? 0 : 255 - distance;
}

/* 0x6A870 */
int MV_PlayVOC3D(const unsigned char *ptr, int pitchoffset, int angle, int distance, int priority,
    unsigned long callbackval)
{
    int vol, left, right;

    if (!mv_installed) {
        MV_ErrorCode = 2;
        return -1;
    }
    pan3d(angle, distance, &vol, &left, &right);
    return playvoc(ptr, pitchoffset, vol, left, right, priority, callbackval);
}

/* 0x69DDC, through 0x69C90 */
int MV_Pan3D(int handle, int angle, int distance)
{
    int vol, left, right;
    voice *v;

    pan3d(angle, distance, &vol, &left, &right);
    if (!mv_installed) {
        MV_ErrorCode = 2;
        return -1;
    }
    DSL_Lock();
    v = findvoice(handle);
    if (v)
        setlevels(v, vol, left, right);
    DSL_Unlock();
    if (!v) {
        MV_ErrorCode = 5;
        return -2;
    }
    MV_ErrorCode = 0;
    return 0;
}

/* 0x6967E: one voice into a 16-bit stereo buffer, unsigned around 0x8000. */
static void mixvoice(voice *v, unsigned short *dest)
{
    const short *lt = voltable[v->left], *rt = voltable[v->right];
    unsigned pos = v->position;
    int n;

    for (n = 256; n > 0; n--) {
        int s, l, r;

        if (pos >= v->length) {
            v->position = pos;
            nextblock(v);
            if (!v->playing)
                break;
            pos = v->position;
        }
        s = v->sound[pos >> 16];
        l = dest[0] + lt[s];
        r = dest[1] + rt[s];
        dest[0] = (unsigned short)(l < 0 ? 0 : l > 0xFFFF ? 0xFFFF : l);
        dest[1] = (unsigned short)(r < 0 ? 0 : r > 0xFFFF ? 0xFFFF : r);
        dest += 2;
        pos += v->step;
    }
    v->position = pos;
}

/* 0x697B6: a voice that had stopped when buffer page was mixed is done. */
static void retire(int page)
{
    voice *v, *next;

    for (v = active_head; v; v = next) {
        next = v->next;
        if (v->inbuffer[page])
            continue;
        unlink_voice(v, &active_head, &active_tail);
        addtail(v, &free_head, &free_tail);
        if (mv_callback)
            mv_callback(v->callbackval);
    }
}

/* 0x6983D, run once per buffer from the audio callback with the device
   locked; returns the 256 frames it mixed. */
const unsigned short *MV_ServiceVoc(void)
{
    voice *v;
    int i;

    retire(mv_playpage);
    mv_playpage = mv_mixpage;
    if (++mv_mixpage >= mv_numbuffers)
        mv_mixpage = 0;
    if (!bufferclean[mv_mixpage]) {
        for (i = 0; i < 256 * 2; i++)
            mixbuffer[mv_mixpage][i] = 0x8000;
        bufferclean[mv_mixpage] = 1;
    }
    for (v = active_head; v; v = v->next) {
        bufferclean[mv_mixpage] = 0;
        mixvoice(v, mixbuffer[mv_mixpage]);
        v->inbuffer[mv_mixpage] = v->playing;
    }
    return mixbuffer[mv_mixpage];
}

/* 0x6A906, the 16-bit table: level l of sample s is (s - 128) * l / 15 * gain / 256. */
static void calcvolume(int gain)
{
    int step = (gain << 16) / 15, scale = 0, l, s;

    for (l = 0; l < 16; l++) {
        for (s = 0; s < 256; s++)
            voltable[l][s] = (short)((scale * (s - 128)) >> 16);
        scale += step;
    }
}

/* 0x6AAD0 */
void MV_SetVolume(int volume)
{
    mv_maxvolume = clamp255(volume);
    DSL_Lock();
    calcvolume((mv_maxvolume << 8) / 255);
    DSL_Unlock();
}

/* 0x6AA13, written store for store: angles 0-8 face front, 16 behind,
   and each distance column d starts from 255 - 17d. */
static void calcpantable(void)
{
    unsigned char *t = pantable;
    int d2, top;

    for (d2 = 0, top = 3825; d2 != 32; d2 += 2, top -= 255) {
        int level = top / 15, b = d2, c = d2 + 512, acc = 0, a;

        for (a = 0; a <= 8; a++) {
            unsigned char dl = (unsigned char)(level - acc / 8), ah = (unsigned char)level;

            t[b] = dl;
            t[c] = dl;
            t[b + 0x200] = ah;
            t[c + 0x1E0] = ah;
            c -= 32;
            t[b + 1] = ah;
            b += 32;
            t[c + 0x21] = ah;
            t[b + 0x1E1] = dl;
            acc += level;
            t[c + 0x201] = dl;
        }
    }
}

/* 0x6AB12. Only the 16-bit stereo mixer is lifted: the mode main asks for. */
int MV_Init(int card, unsigned rate, int numvoices, int channels, int bits)
{
    int i;

    (void)card;
    if (channels != 2 || bits != 16) {
        MV_ErrorCode = 1;
        return -1;
    }
    voices = (voice *)calloc((size_t)numvoices, sizeof *voices);
    if (!voices) {
        MV_ErrorCode = 4;
        return -1;
    }
    for (i = 0; i < numvoices; i++)
        addtail(&voices[i], &free_head, &free_tail);
    /* The swap is for Sound Blaster Pro types 2 and 4; the port drives an SB16. */
    mv_swapleftright = 0;
    mv_mixrate = rate;
    mv_numbuffers = 4096 / (4 << 8);
    mv_maxvolume = 255;
    calcvolume(0x100);
    calcpantable();
    for (i = 0; i < mv_numbuffers; i++)
        bufferclean[i] = 0;
    mv_mixpage = mv_playpage = 0;
    mv_installed = 1;
    /* A missing audio device leaves the voices running silent, never mixed. */
    DSL_Init(rate, channels, bits);
    MV_ErrorCode = 0;
    return 0;
}

void MV_SetCallBack(void (*function)(unsigned long))
{
    mv_callback = function;
}
