/* DRIFTMAKER: the voices. One note: three wave oscillators morphing across eight sources (hollow pipe, organ,
 * choir ah, choir oo, bowed string, hurdy-gurdy, regal reed, bell), a layer voice (sub octave, fifth or twelfth),
 * breath noise for the choirs and strings, a resonant low-pass, and movement everywhere: each oscillator wanders on
 * its own random walk, mod 1 (a sine with a smoothed random part) bends the pitch and sweeps the filter, mod 2 swells
 * the level and drifts the timbre, and the note blooms over its first twelve seconds.
 * Renders 128 samples at 88.2 kHz a block (the Opus engine's voice). Integer only, no allocation, no C library.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include "dm_dsp.h"
#include "dm_tables.h"

volatile unsigned char dm_data[DM_DATA_N];
struct dm_vis dm_vis;
volatile int dm_newest = -1;               /* the newest note's voice index (dm_glue) */

static const short *waves = DM_WAVE;       /* the waves: the lease's copy when it has one */
static short own_scratch[2][257];
static short (*mt)[257] = own_scratch;     /* the block's two morphed waves (a fast-memory lease when it has one) */

void dm_tables_at(const short *w) { waves = w ? w : DM_WAVE; }
void dm_scratch_at(void *s) { mt = s ? (short (*)[257])s : own_scratch; }
unsigned dm_scratch_bytes(void) { return sizeof own_scratch; }

/* one morphed wave for the block: a + (b - a) x mf, with a guard point for the interpolation */
static void morph_wave(short *t, const short *a, const short *b, int mf)
{
    const short m = (short)(mf >> 1);
    for (int i = 0; i < 256; ++i)
        t[i] = (short)(a[i] + (((short)((b[i] - a[i]) >> 1) * m) >> 13));
    t[256] = t[0];
}
unsigned dm_waves_bytes(void) { return sizeof DM_WAVE; }
void dm_waves_copy(short *to)
{
    for (unsigned i = 0; i < sizeof DM_WAVE / sizeof DM_WAVE[0]; ++i)
        to[i] = DM_WAVE[i];
}

#define POS_MAX   (7 * 4096 - 1)            /* the character's position: wave a = pos >> 12, morph = pos & 4095 */
#define BLOOM_BLOCKS 8192u                  /* the note's bloom: about 12 s */

static inline unsigned rnd(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s; }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* 2^(oct / 65536) as Q16 (oct: octaves x 65536, -4..+4 octaves). */
static unsigned ratio_q16(int oct)
{
    int whole = oct >> 16;                  /* floor */
    const unsigned frac = (unsigned)oct & 0xFFFF;
    const unsigned i = frac >> 8, f = frac & 0xFF;
    const unsigned a = DM_EXP2[i], b = DM_EXP2[i + 1];
    unsigned r = 65536u + a + (((b - a) * f) >> 8);
    return whole >= 0 ? r << whole : r >> -whole;
}

/* pitch x ratio, a phase step. */
static unsigned step_of(unsigned pitch, unsigned ratio)
{
    return (unsigned)(((unsigned long long)pitch * ratio) >> 16);
}

/* The band limit for a step (88.2 kHz units): below 450 Hz all 48 harmonics, below 1.35 kHz 16, else 6: the voice
 * computes at 44.1 kHz, so every harmonic stays under 22 kHz. */
static int mip_of(unsigned step)
{
    return step < 21913098u ? 0 : step < 65739295u ? 1 : 2;
}

/* A slow sine from a phase (2^32 a cycle), Q15, interpolated. */
static int sine(unsigned ph)
{
    const unsigned i = ph >> 24, f = (ph >> 16) & 0xFF;
    return DM_SINE[i] + (((DM_SINE[i + 1] - DM_SINE[i]) * (int)f) >> 8);
}

/* breath noise by source (Q15), the eight waves: the choirs most, the strings a little */
static const short NOISE[8] = { 400, 0, 2600, 3300, 1200, 900, 0, 0 };

void dm_voice_init(struct dm_voice *v, int index)
{
    for (int i = 0; i < 3; ++i) {
        v->ph[i] = 0;
        v->drift[i] = 0;
    }
    v->lp = v->bp = 0;
    v->lfo1 = (unsigned)index * 0x2AAAAAABu;
    v->lfo2 = (unsigned)index * 0x15555555u;
    v->rnd = v->rnd_to = 0;
    v->seed = 0x9E3779B9u * (unsigned)(index + 1) | 1u;   /* xorshift: never 0 */
    v->amp = 32767;
    v->last = 0;
    v->morph = -1;
    v->cut = -1;
    v->age = 0;
    v->index = index;
}

void dm_voice_note(struct dm_voice *v)
{
    /* a new note: phases spread so the three oscillators never start in step (no click: the OS's envelope opens
     * from zero); its bloom starts again; the modulators run on (the drone keeps moving across notes) */
    v->ph[0] = rnd(&v->seed) | 1u;
    v->ph[1] = rnd(&v->seed);
    v->ph[2] = rnd(&v->seed);
    v->seed |= 1u;
    v->age = 0;
}

void dm_voice_render(struct dm_voice *v, short *out, const short *par, unsigned pitch)
{
    const int c = clampi(par[DM_K_CHAR], 0, 32767), tone = clampi(par[DM_K_TONE], 0, 32767);
    const int det = clampi(par[DM_K_DETUNE], 0, 32767), filt = clampi(par[DM_K_FILTER], 0, 32767);
    const int d1 = clampi(par[DM_K_DEPTH1], 0, 32767), d2 = clampi(par[DM_K_DEPTH2], 0, 32767);

    /* ---- the modulators, once a block ---- */
    const unsigned old1 = v->lfo1;
    v->lfo1 += DM_RATE1[dm_data[DM_D_RATE1] % 100];
    v->lfo2 += DM_RATE2[dm_data[DM_D_RATE2] % 100];
    if ((old1 ^ v->lfo1) & 0x40000000u)    /* every quarter turn mod 1's random part takes a new aim */
        v->rnd_to = (int)(rnd(&v->seed) >> 16) - 32768;
    v->rnd += (v->rnd_to - v->rnd) >> 7;
    const int m1 = (sine(v->lfo1) * 22938 + v->rnd * 9830) >> 15;      /* 70 % sine, 30 % wander */
    const int m2 = sine(v->lfo2);
    if (v->age < BLOOM_BLOCKS)
        ++v->age;
    const int bloom = (int)(v->age * 4u) > 32767 ? 32767 : (int)(v->age * 4u);

    /* ---- the character: where between the eight sources ---- */
    const int pos0 = (int)(((long long)c * POS_MAX) >> 15);       /* where the knob sets it */
    int pos = pos0;
    pos += (((m2 * d2) >> 15) * 5734) >> 15;             /* mod 2: +-1.4 sources */
    pos += (((bloom * d2) >> 15) * 2048) >> 15;          /* the bloom: half a source onwards */
    pos = clampi(pos, 0, POS_MAX);
    v->morph = v->morph < 0 ? pos : v->morph + ((pos - v->morph) >> 3);
    const int wa = v->morph >> 12, mf = (v->morph & 4095) << 3; /* morph Q15 */
    /* the bell zone (5.5 .. 7 sources), from the knob alone: the modulators move the timbre, never this interval */
    const int bell = clampi((pos0 - 22528) * 32767 / 6143, 0, 32767);

    /* ---- pitches ---- */
    const int sp = (((det * det) >> 15) * 1365) >> 15;              /* unison spread: up to 25 cents */
    const int wob = (((m1 * d1) >> 15) * 983) >> 15;                /* mod 1: +-18 cents */
    const int dscale = (det >> 1) + 8192;                           /* drift: a quarter even at detune 0 */
    int o3oct, l3;                                                  /* the layer voice: interval and level Q15 */
    if (tone < 16384) {
        o3oct = -65536;
        l3 = ((16384 - tone) * 26214) >> 14;                        /* sub octave, up to 0.8 */
    } else if (tone < 24576) {
        const int x = (tone - 16384) << 2;                          /* 0..32767 */
        o3oct = 38336;                                              /* a fifth */
        l3 = (((x * (32767 - x)) >> 15) * 22938) >> 13;            /* a hump to 0.7 */
    } else {
        o3oct = 103872;                                             /* a twelfth (octave and fifth) */
        l3 = ((tone - 24576) * 19660) / 8191;                       /* up to 0.6 */
    }
    int oct[3];
    for (int i = 0; i < 3; ++i) {
        v->drift[i] += ((int)(rnd(&v->seed) >> 20) - 2048) * 2 - (v->drift[i] >> 10);
        oct[i] = wob + (((v->drift[i] >> 8) * dscale) >> 15);
    }
    oct[0] -= sp;
    oct[1] += sp + (((bell >> 3) * 95853) >> 12);                          /* the bell zone: osc 2 glides to 2.756 */
    oct[2] += o3oct + (sp >> 1);
    unsigned st[3];
    int mip[3];
    for (int i = 0; i < 3; ++i) {
        st[i] = step_of(pitch, ratio_q16(oct[i]));
        mip[i] = mip_of(st[i]);
        st[i] <<= 1;                                               /* the voice computes at 44.1 kHz */
    }
    /* two morphed waves this block: the unison pair's (at the band limit of the higher of the two) and the layer's */
    const int wb = wa < 7 ? wa + 1 : 7;
    const int m01 = mip[0] > mip[1] ? mip[0] : mip[1];
    morph_wave(mt[0], waves + ((wa * DM_MIPS + m01) << 8), waves + ((wb * DM_MIPS + m01) << 8), mf);
    const short *t3 = mt[0];
    if (mip[2] != m01 && l3 > 0) {
        morph_wave(mt[1], waves + ((wa * DM_MIPS + mip[2]) << 8), waves + ((wb * DM_MIPS + mip[2]) << 8), mf);
        t3 = mt[1];
    }

    /* ---- the filter: cutoff in semitones x 256 from 30 Hz ---- */
    int oct_note = 0;                                               /* the note's octave, for key tracking */
    for (unsigned s = pitch; s > 9737000u; s >>= 1)                 /* 9737000: about 200 Hz */
        ++oct_note;
    int cut = (filt * 105) >> 15;                                   /* 0..105 semitones (30 Hz .. 13 kHz), x 256 below */
    cut <<= 8;
    cut += ((((m1 * d1) >> 15) * 6144) >> 15);                      /* mod 1: +-24 semitones */
    cut += (((bloom * d2) >> 15) * 2048) >> 15;                     /* the bloom opens it by up to 8 */
    cut += oct_note * 6 * 256;                                      /* half key tracking */
    cut = clampi(cut, 0, 127 * 256);
    v->cut = v->cut < 0 ? cut : v->cut + ((cut - v->cut) >> 2);
    const int ci = v->cut >> 8, cf = v->cut & 255;
    const int f = DM_CUTOFF[ci] + (((DM_CUTOFF[ci < 127 ? ci + 1 : 127] - DM_CUTOFF[ci]) * cf) >> 8);
    const int q = 8000 + ((32767 - filt) >> 2);                     /* more resonance as it darkens */

    /* ---- levels ---- */
    const int pa = wa < 7 ? wa + 1 : 7;
    const int nz = (NOISE[wa] * (32767 - mf) + NOISE[pa] * mf) >> 15;
    const int gnorm = (19000 << 15) / (32768 + l3);                /* about -5 dB whatever the layer */
    int target = 32767 - ((((d2 * (16384 - (m2 >> 1))) >> 15) * 26214) >> 15);   /* mod 2's swells */
    target = clampi(target, 3000, 32767);
    int amp = v->amp;
    const int damp = (target - amp) >> 6;

    /* ---- the samples ---- */
    unsigned p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2];
    int lp = v->lp, bp = v->bp;
    unsigned seed = v->seed;
    const short *t01 = mt[0];
    const short l3s = (short)(l3 > 32767 ? 32767 : l3), nzs = (short)nz, gs = (short)(gnorm > 32767 ? 32767 : gnorm);
    /* 64 samples at 44.1 kHz, then up to the voice's 88.2 kHz by halfway points: the voice is dark (its filter stops
     * at 8 kHz), and every image the halfway points leave falls back onto its own frequency at the OS's 44.1 kHz */
    short y[64];
    for (int n = 0; n < 64; ++n) {
        int i, s, x;
        p0 += st[0]; i = (int)(p0 >> 24);
        s = t01[i] + (((short)(t01[i + 1] - t01[i]) * (short)((p0 >> 9) & 0x7FFF)) >> 15);
        p1 += st[1]; i = (int)(p1 >> 24);
        s += t01[i] + (((short)(t01[i + 1] - t01[i]) * (short)((p1 >> 9) & 0x7FFF)) >> 15);
        s >>= 1;
        p2 += st[2]; i = (int)(p2 >> 24);
        x = t3[i] + (((short)(t3[i + 1] - t3[i]) * (short)((p2 >> 9) & 0x7FFF)) >> 15);
        s += ((short)x * l3s) >> 15;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;          /* xorshift: breath */
        s += ((short)(seed >> 16) * nzs) >> 16;
        s = ((short)clampi(s, -32767, 32767) * gs) >> 15;
        /* Chamberlin state-variable low-pass */
        lp += (f * bp) >> 15;
        const int hp = s - lp - ((q * bp) >> 15);
        bp += (f * hp) >> 15;
        bp = clampi(bp, -50000, 50000);
        lp = clampi(lp, -50000, 50000);
        amp += damp;
        y[n] = (short)clampi((lp * amp) >> 15, -32767, 32767);
    }
    int prev = v->last;
    for (int n = 0; n < 64; ++n) {
        out[2 * n] = (short)((prev + y[n]) >> 1);
        out[2 * n + 1] = y[n];
        prev = y[n];
    }
    v->last = prev;
    v->ph[0] = p0; v->ph[1] = p1; v->ph[2] = p2;
    v->lp = lp; v->bp = bp;
    v->seed = seed;
    v->amp = amp;

    if (v->index == dm_newest) {
        dm_vis.m1 = m1;
        dm_vis.m2 = m2;
        dm_vis.morph = v->morph;
        dm_vis.bloom = bloom;
    }
}
