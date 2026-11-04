/* The MIDI file player of 0x6B330-0x6C6C0. MUSIC_Init hands it the card's
   driver table; TaskMan calls MIDI_Service once per MIDI tick. */

#include "midi.h"
#include "task_man.h"

#include <stdlib.h>

typedef struct track {
    struct track *next, *prev;
    int delay;
    int active;
    int runningstatus;
    const unsigned char *start, *pos;
} track;

static const midifuncs *funcs;        /* dword_89570 */
static track *tracks;                 /* dword_89558 */
static track *track_head, *track_tail; /* dword_24409C, dword_2440A0 */
static int numactive;                 /* dword_2440A4 */
static int ticks;                     /* dword_2440AC */
static int division;                  /* dword_2440B0 */
static int loaded, playing, looping;  /* dword_89560, dword_8955C, dword_89564 */
static int totalvolume = 255;         /* dword_8956C */
static int channelvolume[16];         /* dword_24405C */

/* dword_6B590: data bytes after each status nibble. */
static const int eventlength[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 2, 0 };

void MIDI_SetMidiFuncs(const midifuncs *f)
{
    funcs = f;
}

/* 0x6B5D0 */
static int bigendian(const unsigned char *p, int n)
{
    int v = 0;

    while (n-- > 0)
        v = (v << 8) + *p++;
    return v;
}

/* 0x6B5F8 */
static int readdelay(track *t)
{
    int v = *t->pos++;

    if (v & 0x80) {
        int b;

        v &= 0x7F;
        do {
            b = *t->pos++;
            v = (v << 7) + (b & 0x7F);
        } while (b & 0x80);
    }
    return v;
}

static void rewindtracks(void)
{
    track *t;

    numactive = 0;
    ticks = 0;
    for (t = track_head; t; t = t->next) {
        t->pos = t->start;
        numactive++;
        t->delay = readdelay(t);
        t->active = 1;
        t->runningstatus = 0;
    }
}

/* 0x6B9EC */
static int sendcontrol(int channel, int type, int data)
{
    if (!funcs || !funcs->ControlChange)
        return -1;
    funcs->ControlChange(channel, type, data);
    return 0;
}

/* 0x6BA8C: with no card mixer the total volume scales every channel. */
static void setchannelvolume(int channel, int volume)
{
    channelvolume[channel] = volume;
    if (!funcs || !funcs->ControlChange)
        return;
    if (!funcs->SetVolume)
        volume = totalvolume * volume / 255;
    funcs->ControlChange(channel, 7, volume);
}

/* 0x6BB44 */
static void midireset(void)
{
    int c;

    for (c = 0; c < 16; c++)
        if (sendcontrol(c, 123, 0) != 0)
            return;
    for (c = 0; c < 16; c++) {
        sendcontrol(c, 121, 0);
        sendcontrol(c, 100, 0);
        sendcontrol(c, 101, 0);
        sendcontrol(c, 6, 2);
        sendcontrol(c, 38, 0);
        channelvolume[c] = 90;
    }
    for (c = 0; c < 16; c++)
        setchannelvolume(c, channelvolume[c]);
}

/* 0x6B6A0. A meta event's length is one byte, and a sysex is read as a
   status with no data, as in the exe. */
void MIDI_Service(void)
{
    track *t;
    int d1 = 0, d2 = 0;

    if (!playing)
        return;
    ticks++;
    t = track_head;
    while (t) {
        while (t->active && t->delay == 0) {
            int ev = *t->pos++, channel, cmd;

            if (ev == 0xFF) {
                int type = *t->pos++, len = *t->pos++;

                if (type == 0x2F) {
                    t->active = 0;
                    numactive--;
                } else if (type == 0x51) {
                    TS_SetTaskRate(60000000 / bigendian(t->pos, 3) * division / 60);
                }
                t->pos += len;
                t->delay = readdelay(t);
                continue;
            }
            if (ev & 0x80) {
                t->runningstatus = ev;
            } else {
                ev = t->runningstatus;
                t->pos--;
            }
            channel = ev & 15;
            cmd = ev >> 4;
            if (eventlength[cmd] > 0) {
                d1 = *t->pos++;
                if (eventlength[cmd] > 1)
                    d2 = *t->pos++;
            }
            switch (cmd) {
            case 8:
                if (funcs->NoteOff)
                    funcs->NoteOff(channel, d1, d2);
                break;
            case 9:
                if (funcs->NoteOn)
                    funcs->NoteOn(channel, d1, d2);
                break;
            case 10:
                if (funcs->PolyAftertouch)
                    funcs->PolyAftertouch(channel, d1, d2);
                break;
            case 11:
                if (d1 == 126)
                    t->pos++;
                if (d1 == 7)
                    setchannelvolume(channel, d2);
                else if (funcs->ControlChange)
                    funcs->ControlChange(channel, d1, d2);
                break;
            case 12:
                if (funcs->ProgramChange)
                    funcs->ProgramChange(channel, d1);
                break;
            case 13:
                if (funcs->ChannelAftertouch)
                    funcs->ChannelAftertouch(channel, d1);
                break;
            case 14:
                if (funcs->PitchBend)
                    funcs->PitchBend(channel, d1, d2);
                break;
            }
            t->delay = readdelay(t);
        }
        t->delay--;
        t = t->next;
        if (numactive == 0) {
            rewindtracks();
            if (!looping) {
                playing = 0;
                return;
            }
            t = track_head;
        }
    }
}

/* 0x6BCD0 */
void MIDI_StopSong(void)
{
    if (!loaded)
        return;
    TS_Terminate();
    playing = 0;
    loaded = 0;
    midireset();
    if (funcs->ReleasePatches)
        funcs->ReleasePatches();
    track_head = track_tail = NULL;
    free(tracks);
    tracks = NULL;
}

/* 0x6BD48 */
int MIDI_PlaySong(const unsigned char *song, int loop)
{
    const unsigned char *p;
    int headerlen, format, numtracks, i;

    if (loaded)
        MIDI_StopSong();
    looping = loop;
    if (!funcs)
        return 1;
    if (song[0] != 'M' || song[1] != 'T' || song[2] != 'h' || song[3] != 'd')
        return 2;
    headerlen = bigendian(song + 4, 4);
    format = bigendian(song + 8, 2);
    numtracks = bigendian(song + 10, 2);
    division = bigendian(song + 12, 2);
    if (format > 1)
        return 3;
    track_head = track_tail = NULL;
    p = song + 8 + headerlen;
    if (numtracks == 0)
        return 4;
    tracks = (track *)calloc((size_t)numtracks, sizeof *tracks);
    if (!tracks)
        return 6;
    for (i = 0; i < numtracks; i++) {
        track *t = &tracks[i];

        if (p[0] != 'M' || p[1] != 'T' || p[2] != 'r' || p[3] != 'k') {
            free(tracks);
            tracks = NULL;
            track_head = track_tail = NULL;
            return 5;
        }
        t->start = p + 8;
        p = t->start + bigendian(p + 4, 4);
        t->next = NULL;
        t->prev = track_tail;
        if (track_tail)
            track_tail->next = t;
        else
            track_head = t;
        track_tail = t;
    }
    if (funcs->GetVolume)
        totalvolume = funcs->GetVolume();
    rewindtracks();
    /* The patch pre-scan (0x6C3AC) runs only for drivers that load patches;
       AL_MIDI does not. */
    midireset();
    TS_ScheduleTask(MIDI_Service, 120 * division / 60);
    loaded = 1;
    playing = 1;
    return 0;
}

/* 0x6BBDC */
int MIDI_SetVolume(int volume)
{
    int c;

    if (!funcs)
        return 1;
    if (volume > 255)
        volume = 255;
    if (volume < 0)
        volume = 0;
    totalvolume = volume;
    if (funcs->SetVolume) {
        funcs->SetVolume(volume);
    } else {
        for (c = 0; c < 16; c++)
            setchannelvolume(c, channelvolume[c]);
    }
    return 0;
}

int MIDI_Init(void)
{
    return 0;
}
