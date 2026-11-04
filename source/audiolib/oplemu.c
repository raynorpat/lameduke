#include "oplemu.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846

enum { ATTACK, DECAY, SUSTAIN, RELEASE };

typedef struct {
    int am, vib, egt, ksr, mult; /* 0x20 */
    int ksl, tl;                 /* 0x40 */
    int ar, dr;                  /* 0x60 */
    int sl, rr;                  /* 0x80 */
    int ws;                      /* 0xE0 */
    unsigned phase;              /* 19-bit counter; the top 10 bits index the wave */
    int env;                     /* attenuation, 0-511 in 0.1875 dB */
    int state;
    int out, prevout;            /* 13-bit signed */
} op_t;

typedef struct {
    op_t op[2];
    int fnum, block, key;
    int fb, cnt, outl, outr;
} ch_t;

static ch_t chans[18];
static int opl3, wavesel, nts, dam, dvb;
static unsigned egcnt, samplecnt;
static unsigned short logsin[256], expo[256];
static int tablesready;

static const int multx2[16] = { 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30 };
static const int kslrom[16] = { 0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64 };
static const int kslshift[4] = { 8, 1, 2, 0 };
/* Envelope steps for rates below 48 (one step every 2^(12 - rate/4) samples)
   and for 48-59 (every sample, doubling with each rate/4). */
static const int egslow[4][8] = {
    { 0, 1, 0, 1, 0, 1, 0, 1 }, { 0, 1, 0, 1, 1, 1, 0, 1 }, { 0, 1, 1, 1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 1, 1, 1, 1 },
};
static const int egfast[4][8] = {
    { 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 2, 1, 1, 1, 2 }, { 1, 2, 1, 2, 1, 2, 1, 2 }, { 1, 2, 2, 2, 1, 2, 2, 2 },
};

static void maketables(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        logsin[i] = (unsigned short)(-log(sin((i + 0.5) * PI / 512.0)) / log(2.0) * 256.0 + 0.5);
        expo[i] = (unsigned short)(pow(2.0, (255 - i) / 256.0) * 1024.0 + 0.5);
    }
    tablesready = 1;
}

void OPL_Reset(void)
{
    int c, o;

    if (!tablesready)
        maketables();
    memset(chans, 0, sizeof chans);
    for (c = 0; c < 18; c++) {
        chans[c].outl = chans[c].outr = 1;
        for (o = 0; o < 2; o++) {
            chans[c].op[o].env = 511;
            chans[c].op[o].state = RELEASE;
        }
    }
    opl3 = wavesel = nts = dam = dvb = 0;
    egcnt = samplecnt = 0;
}

/* Register offsets 0x00-0x15 to (channel, operator); -1 for the gaps. */
static op_t *slotop(int bank, int off)
{
    static const int chan[32] = { 0, 1, 2, 0, 1, 2, -1, -1, 3, 4, 5, 3, 4, 5, -1, -1,
        6, 7, 8, 6, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int c = chan[off & 31];

    if (c < 0)
        return NULL;
    return &chans[bank * 9 + c].op[(off & 7) >= 3 && (off & 7) < 6];
}

void OPL_Write(int reg, int val)
{
    int bank = (reg >> 8) & 1, r = reg & 0xFF;
    op_t *op;
    ch_t *ch;

    if (bank == 1 && r == 0x05) {
        opl3 = val & 1;
        return;
    }
    if (bank == 0) {
        if (r == 0x01) {
            wavesel = val & 0x20;
            return;
        }
        if (r == 0x08) {
            nts = (val >> 6) & 1;
            return;
        }
        if (r == 0xBD) {
            dam = (val >> 7) & 1;
            dvb = (val >> 6) & 1;
            return;
        }
    }
    if (r >= 0x20 && r < 0xA0) {
        op = slotop(bank, r & 0x1F);
        if (!op)
            return;
        switch (r & 0xE0) {
        case 0x20:
            op->am = (val >> 7) & 1;
            op->vib = (val >> 6) & 1;
            op->egt = (val >> 5) & 1;
            op->ksr = (val >> 4) & 1;
            op->mult = val & 15;
            break;
        case 0x40:
            op->ksl = (val >> 6) & 3;
            op->tl = val & 63;
            break;
        case 0x60:
            op->ar = (val >> 4) & 15;
            op->dr = val & 15;
            break;
        case 0x80:
            op->sl = (val >> 4) & 15;
            op->rr = val & 15;
            break;
        }
        return;
    }
    if (r >= 0xE0 && r < 0xF6) {
        op = slotop(bank, r & 0x1F);
        if (op)
            op->ws = val & 7;
        return;
    }
    if ((r & 0xF0) == 0xA0 || (r & 0xF0) == 0xB0 || (r & 0xF0) == 0xC0) {
        if ((r & 15) > 8)
            return;
        ch = &chans[bank * 9 + (r & 15)];
        if ((r & 0xF0) == 0xA0) {
            ch->fnum = (ch->fnum & 0x300) | val;
        } else if ((r & 0xF0) == 0xB0) {
            int key = (val >> 5) & 1, o;

            ch->fnum = (ch->fnum & 0xFF) | (val & 3) << 8;
            ch->block = (val >> 2) & 7;
            if (key && !ch->key) {
                for (o = 0; o < 2; o++) {
                    ch->op[o].phase = 0;
                    ch->op[o].state = ATTACK;
                }
            } else if (!key && ch->key) {
                ch->op[0].state = ch->op[1].state = RELEASE;
            }
            ch->key = key;
        } else {
            ch->fb = (val >> 1) & 7;
            ch->cnt = val & 1;
            ch->outl = (val >> 4) & 1;
            ch->outr = (val >> 5) & 1;
        }
    }
}

static int eginc(int rate)
{
    int hi = rate >> 2, lo = rate & 3;

    if (rate < 4)
        return 0;
    if (hi < 12) {
        int shift = 12 - hi;

        if (egcnt & ((1u << shift) - 1))
            return 0;
        return egslow[lo][(egcnt >> shift) & 7];
    }
    if (hi < 15)
        return egfast[lo][egcnt & 7] << (hi - 12);
    return 8;
}

static void envelope(op_t *op, const ch_t *ch)
{
    int reg, rate, ksv, inc;

    switch (op->state) {
    case ATTACK:
        reg = op->ar;
        break;
    case DECAY:
        reg = op->dr;
        break;
    case SUSTAIN:
        reg = op->egt ? 0 : op->rr;
        break;
    default:
        reg = op->rr;
        break;
    }
    if (reg == 0)
        return;
    ksv = (ch->block << 1) | ((ch->fnum >> (9 - nts)) & 1);
    if (!op->ksr)
        ksv >>= 2;
    rate = reg * 4 + ksv;
    if (rate > 63)
        rate = 63;
    if (op->state == ATTACK) {
        if (rate >= 60) {
            op->env = 0;
        } else {
            inc = eginc(rate);
            if (inc)
                op->env += (~op->env * inc) >> 3;
        }
        if (op->env <= 0) {
            op->env = 0;
            op->state = DECAY;
        }
        return;
    }
    op->env += eginc(rate);
    if (op->env > 511)
        op->env = 511;
    if (op->state == DECAY) {
        int sl = op->sl == 15 ? 0x1F0 : op->sl << 4;

        if (op->env >= sl)
            op->state = SUSTAIN;
    }
}

static int expout(int level)
{
    if ((level >> 8) >= 13)
        return 0;
    return (expo[level & 0xFF] << 1) >> (level >> 8);
}

/* One operator sample from its 10-bit phase plus modulation. */
static int wave(int ws, int phase, int att)
{
    int neg = 0, idx;

    phase &= 0x3FF;
    if (!opl3)
        ws = wavesel ? ws & 3 : 0;
    switch (ws & 3) {
    case 1:
        if (phase & 0x200)
            return 0;
        break;
    case 2:
        phase &= 0x1FF;
        break;
    case 3:
        if (phase & 0x100)
            return 0;
        phase &= 0xFF;
        break;
    }
    neg = phase & 0x200;
    idx = (phase & 0x100) ? ~phase & 0xFF : phase & 0xFF;
    idx = expout(logsin[idx] + (att << 3));
    return neg ? ~idx : idx;
}

static int opsample(op_t *op, const ch_t *ch, int mod, int trem, int vibpos)
{
    int fnum = ch->fnum, att, ksl;

    if (op->vib) {
        int range = (fnum >> 7) & 7;

        if (!(vibpos & 3))
            range = 0;
        else if (vibpos & 1)
            range >>= 1;
        range >>= dvb ? 0 : 1;
        if (vibpos & 4)
            range = -range;
        fnum += range;
    }
    op->phase += (unsigned)((((fnum << ch->block) >> 1) * multx2[op->mult]) >> 1);
    envelope(op, ch);
    ksl = (kslrom[ch->fnum >> 6] << 2) - ((8 - ch->block) << 5);
    if (ksl < 0)
        ksl = 0;
    att = op->env + (op->tl << 2) + (ksl >> kslshift[op->ksl]) + (op->am ? trem : 0);
    if (att > 511)
        att = 511;
    op->prevout = op->out;
    op->out = wave(op->ws, (int)((op->phase >> 9) & 0x3FF) + mod, att);
    return op->out;
}

void OPL_Generate(int *left, int *right)
{
    int c, l = 0, r = 0, trem, tpos, vibpos;

    egcnt++;
    samplecnt++;
    vibpos = (int)(samplecnt >> 10) & 7;
    tpos = (int)((samplecnt >> 6) % 210);
    trem = (tpos < 105 ? tpos : 209 - tpos) >> (dam ? 2 : 4);
    for (c = 0; c < 18; c++) {
        ch_t *ch = &chans[c];
        op_t *m = &ch->op[0], *k = &ch->op[1];
        int fbmod = ch->fb ? (m->out + m->prevout) >> (9 - ch->fb) : 0, mo, out;

        mo = opsample(m, ch, fbmod, trem, vibpos);
        out = opsample(k, ch, ch->cnt ? 0 : mo, trem, vibpos);
        if (ch->cnt)
            out += mo;
        if (!opl3 || ch->outl)
            l += out;
        if (!opl3 || ch->outr)
            r += out;
    }
    *left = l;
    *right = r;
}
