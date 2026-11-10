/* DRIFTMAKER: the space after the voices, once a block on their sum (64 samples, 44.1 kHz, mono, in place):
 *
 *   degrade  -> capture (1.49 s ring) -> grains / freeze -> delay -> reverb -> mix
 *
 *   degrade:  tape wow (a modulated short delay), soft saturation, sample-and-hold, bit loss, darkening, hiss
 *   grains:   four Hann-windowed grains read back from the ring at 0.5x, 1x or 2x, scattered by density and carried
 *             along the ring by mod 2; freeze stops the ring's writing so the grains keep the captured texture alive,
 *             after the keys are let go too
 *   delay:    up to 0.72 s, a dark low-pass and a soft limit in its loop
 *   reverb:   four all-pass diffusers into a four-line feedback delay network with damping (Hadamard mixing)
 *
 * Integer only. Its memory (228 KB) comes from the audio pool once, at the start.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include "dm_dsp.h"
#include "dm_tables.h"

#define GBUF   65536                       /* the capture ring: 1.49 s */
#define GMASK  (GBUF - 1)
#define DLINE  32768                       /* the delay: 0.74 s */
#define DMASK  (DLINE - 1)
#define RVLEN  4096
#define RVMASK (RVLEN - 1)
#define APLEN  512
#define APMASK (APLEN - 1)
#define WOWLEN 1024
#define WOWMASK (WOWLEN - 1)
#define NG     4

struct dm_fx_mem {
    short grain[GBUF];
    short dline[DLINE];
    short rv[4][RVLEN];
    short ap[4][APLEN];
    short wow[WOWLEN];
};

static struct dm_fx_mem *M;

static const unsigned short RV_LEN[4] = { 1777, 2129, 2617, 3109 };
static const unsigned short AP_LEN[4] = { 142, 107, 379, 277 };

struct grain {
    unsigned pos;                          /* Q16 index into the ring */
    unsigned rate;                         /* Q16 */
    unsigned w, wstep;                     /* window phase Q16 of 256 points */
    int on;
};

static struct {
    unsigned gw;                           /* the ring's write index */
    struct grain g[NG];
    int until;                             /* samples to the next grain */
    unsigned seed;
    unsigned dw, rw, aw, ww;               /* write indexes: delay, reverb, all-pass, wow */
    int dtime;                             /* the delay's time, smoothed (samples x 16) */
    int dlp, rlp[4], rlast;                /* loop low-passes; the reverb's last output */
    int hold, held, dark, env;             /* degrade: sample-and-hold count and value, darkening, level */
    unsigned wowph;
    int cloud_gain;
    int mix, send_rv, send_dl, fb, frz, gra; /* smoothed Q15 */
    int ready;
} S;

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
/* a x b / 32768 rounded toward zero (no branch): what feeds back decays to true silence, never to a stuck constant */
static inline int mulz(int a, int b)
{
    const int p = a * b;
    return (p + ((p >> 31) & 32767)) >> 15;
}
static inline unsigned rnd(void) { S.seed = S.seed * 1664525u + 1013904223u; return S.seed; }
static inline int q15_of99(int v) { return (clampi(v, 0, 99) * 32767 + 49) / 99; }

/* a soft limit: unity below 20000, then a cubic knee to 32767 */
static inline int soft(int x)
{
    if (x > 20000) {
        int e = x - 20000; if (e > 25000) e = 25000;
        return 20000 + e - ((e * e) >> 15) * e / 75000;
    }
    if (x < -20000) {
        int e = -20000 - x; if (e > 25000) e = 25000;
        return -(20000 + e - ((e * e) >> 15) * e / 75000);
    }
    return x;
}

unsigned dm_fx_bytes(void) { return sizeof(struct dm_fx_mem); }

int dm_fx_init(void *mem)
{
    M = (struct dm_fx_mem *)mem;
    S.seed = 0x2545F491u;
    S.dtime = DM_DELAY[50] << 4;
    S.until = 0;
    S.ready = M != 0;
    return S.ready;
}

static void spawn(int dn, int frozen, int travel)
{
    struct grain *g = 0;
    for (int i = 0; i < NG; ++i)
        if (!S.g[i].on) { g = &S.g[i]; break; }
    const int len = 8192 - dn * 6656 / 99;                     /* 186 ms .. 35 ms */
    S.until += len * 16 / (24 + dn * 40 / 99);                 /* overlap 1.5 .. 4 */
    if (!g)
        return;
    const unsigned r = rnd();
    const int p_up = dn > 50 ? (dn - 50) * 11468 / 49 : 0;    /* to 0.35: an octave up, shimmer */
    const int p_dn = dn < 40 ? (40 - dn) * 4915 / 40 : 0;     /* to 0.15: an octave down */
    const int pick = (int)((r >> 8) & 0x7FFF);
    g->rate = pick < p_up ? 131072u : pick < p_up + p_dn ? 32768u : 65536u;
    g->rate += (unsigned)((int)((r >> 2) & 1023) - 512);      /* +-0.8 %: the cloud never sits in tune */
    const int margin = (int)(((unsigned)len * g->rate) >> 16) + 256;
    const int span = frozen ? 40000 : 6000 + dn * 60;
    const int back = margin + (int)(rnd() % (unsigned)span) + travel;
    g->pos = (unsigned)((S.gw - (unsigned)back) & GMASK) << 16;
    g->w = 0;
    g->wstep = (256u << 16) / (unsigned)len;
    g->on = 1;
}

void dm_fx_block(short *buf, const short *par)
{
    if (!S.ready)
        return;
    struct dm_fx_mem *const m = M;
    const int dg = dm_data[DM_D_DEGRADE] % 100, dn = dm_data[DM_D_DENSITY] % 100;
    const int dl = dm_data[DM_D_DELAY] % 100, rv = dm_data[DM_D_REVERB] % 100;
    /* smoothed controls, a block's step */
    S.mix += (clampi(par[DM_K_MIX], 0, 32767) - S.mix) >> 3;
    S.send_rv += (q15_of99(rv) - S.send_rv) >> 3;
    S.send_dl += ((dl ? 19660 : 0) - S.send_dl) >> 3;
    S.fb += ((q15_of99(dm_data[DM_D_FEEDBACK]) * 30146 >> 15) - S.fb) >> 3;   /* to 0.92 */
    S.frz += (q15_of99(dm_data[DM_D_FREEZE]) - S.frz) >> 3;
    S.gra += (q15_of99(dm_data[DM_D_GRAIN]) - S.gra) >> 3;
    const int frozen = dm_data[DM_D_FREEZE] > 0;
    S.dtime += ((DM_DELAY[dl] << 4) - S.dtime) >> 5;
    const int dtime = S.dtime >> 4;
    const int decay = DM_DECAY[rv];
    S.cloud_gain = (32767 * 32) / (24 + dn * 40 / 99);       /* 2 / overlap, Q15 x 2 */
    const int d2 = clampi(par[DM_K_DEPTH2], 0, 32767);
    const int travel = ((((dm_vis.m2 * d2) >> 15) + 32768) * 9000) >> 16;   /* mod 2 carries the grains */
    int x[64];

    for (int n = 0; n < 64; ++n)
        x[n] = buf[n];

    /* ---- degrade: wow, drive, sample-and-hold, bit loss, darkening, hiss ---- */
    if (dg) {
        const int crush = dg * 9 / 99;                         /* low bits lost: 0..9 */
        const int holdn = dg > 50 ? 1 + (dg - 50) * 5 / 49 : 1;/* sample-and-hold: 1..6 */
        const int wowd = dg * 24 / 99;                         /* wow depth in samples */
        const int drive = (32767 + dg * 3 * 331) >> 2;         /* saturation drive Q13: 1..4 */
        const int darkc = 32767 - dg * 220;                    /* darkening one-pole */
        const int hiss = dg > 60 ? (dg - 60) * 12 : 0;
        unsigned ww = S.ww, wph = S.wowph;
        const unsigned winc = 3000000u + (unsigned)dg * 40000u;   /* ~0.7..1.6 Hz */
        int hold = S.hold, held = S.held, dark = S.dark, env = S.env;
        short *const wow = m->wow;
        for (int n = 0; n < 64; ++n) {
            wow[ww & WOWMASK] = (short)x[n];
            wph += winc;
            const int sw = 32 + (((int)DM_SINE[wph >> 24] + 32768) * wowd >> 16);
            int v = wow[(ww - (unsigned)sw) & WOWMASK];
            ++ww;
            v = soft((v * drive) >> 13);
            if (--hold <= 0) { hold = holdn; held = v; }
            v = held >= 0 ? (held >> crush) << crush : -(((-held) >> crush) << crush);   /* toward zero */
            dark += mulz(v - dark, darkc);
            v = dark;
            const int a = v >= 0 ? v : -v;
            env = a > env ? a : env - (env >> 10);
            if (hiss)                                          /* tape hiss under the sound, gone with it */
                v += mulz(((int)(rnd() >> 20) - 2048) * hiss >> 11, env > 8191 ? 32767 : env << 2);
            x[n] = v;
        }
        S.ww = ww; S.wowph = wph; S.hold = hold; S.held = held; S.dark = dark; S.env = env;
    }

    /* ---- capture, grains and freeze (a grain at a time over the block) ---- */
    int active = 0;
    if (S.gra > 64 || S.frz > 64 || frozen) {
        unsigned gw = S.gw;
        short *const ring = m->grain;
        if (!frozen)
            for (int n = 0; n < 64; ++n)
                ring[(gw + (unsigned)n) & GMASK] = (short)clampi(x[n], -32767, 32767);
        S.until -= 64;
        while (S.until <= 0)                                   /* new grains start at the block's start */
            spawn(dn, frozen, travel);
        int cloud[64];
        for (int n = 0; n < 64; ++n)
            cloud[n] = 0;
        for (int i = 0; i < NG; ++i) {
            struct grain *g = &S.g[i];
            if (!g->on)
                continue;
            ++active;
            unsigned pos = g->pos, w = g->w;
            const unsigned rate = g->rate, wstep = g->wstep;
            unsigned left = ((256u << 16) - w + wstep - 1) / wstep;    /* samples to its end */
            const int cnt = left < 64 ? (int)left : 64;
            for (int n = 0; n < cnt; ++n) {
                const unsigned ix = (pos >> 16) & GMASK;
                const int a = ring[ix], b = ring[(ix + 1) & GMASK];
                const int smp = a + (((short)((b - a) >> 1) * (short)((pos >> 1) & 0x7FFF)) >> 14);
                cloud[n] += ((short)clampi(smp, -32767, 32767) * DM_HANN[w >> 16]) >> 15;
                pos += rate;
                w += wstep;
            }
            g->pos = pos;
            g->w = w;
            if (cnt < 64 || w >= (256u << 16))
                g->on = 0;
        }
        const int wet = S.gra > S.frz ? S.gra : S.frz;
        const int dry = 32767 - ((S.frz * 29490) >> 15);
        const int cg = S.cloud_gain;
        for (int n = 0; n < 64; ++n) {
            const int c = clampi((cloud[n] * cg) >> 16, -32767, 32767);
            x[n] = clampi(mulz(x[n], dry) + mulz(c, wet), -32767, 32767);
        }
    }
    if (!frozen)
        S.gw += 64;

    /* ---- delay: dark repeats, soft-limited in the loop ---- */
    int dout[64];
    {
        short *const line = m->dline;
        unsigned dw = S.dw;
        int dlp = S.dlp;
        const int sd = S.send_dl, fb = S.fb;
        for (int n = 0; n < 64; ++n) {
            const int o = line[(dw - (unsigned)dtime) & DMASK];
            dlp += mulz(o - dlp, 13000);
            line[dw & DMASK] = (short)clampi(soft(mulz(x[n], sd) + mulz(dlp, fb)), -32767, 32767);
            ++dw;
            dout[n] = o;
        }
        S.dw = dw; S.dlp = dlp;
    }

    /* ---- reverb at half rate (22.05 kHz, it is dark anyway): diffusers, then a four-line network ---- */
    int rout[64];
    {
        unsigned aw = S.aw, rw = S.rw;
        int r0 = S.rlp[0], r1 = S.rlp[1], r2 = S.rlp[2], r3 = S.rlp[3];
        int last = S.rlast;
        const int sr = S.send_rv;
        for (int n = 0; n < 64; n += 2) {
            int in = (mulz(x[n] + x[n + 1], sr) + ((dout[n] + dout[n + 1]) >> 2)) >> 2;
            for (int k = 0; k < 4; ++k) {                      /* all-pass diffusers, g 0.6 */
                short *ap = m->ap[k];
                const int d = ap[(aw - AP_LEN[k]) & APMASK];
                const int w = clampi(in + mulz(d, 19660), -32767, 32767);
                ap[aw & APMASK] = (short)w;
                in = d - mulz(w, 19660);
            }
            ++aw;
            r0 += mulz(m->rv[0][(rw - RV_LEN[0]) & RVMASK] - r0, 17000);
            r1 += mulz(m->rv[1][(rw - RV_LEN[1]) & RVMASK] - r1, 17000);
            r2 += mulz(m->rv[2][(rw - RV_LEN[2]) & RVMASK] - r2, 17000);
            r3 += mulz(m->rv[3][(rw - RV_LEN[3]) & RVMASK] - r3, 17000);
            const int a = r0 + r1, b = r0 - r1, c = r2 + r3, d = r2 - r3;
            m->rv[0][rw & RVMASK] = (short)clampi(in + mulz((a + c) / 2, decay), -32767, 32767);
            m->rv[1][rw & RVMASK] = (short)clampi(in + mulz((b + d) / 2, decay), -32767, 32767);
            m->rv[2][rw & RVMASK] = (short)clampi(in + mulz((a - c) / 2, decay), -32767, 32767);
            m->rv[3][rw & RVMASK] = (short)clampi(in + mulz((b - d) / 2, decay), -32767, 32767);
            ++rw;
            const int o = (r0 + r1 + r2 + r3) / 2;
            rout[n] = (last + o) / 2;                          /* back to 44.1 kHz: halfway, then the point */
            rout[n + 1] = o;
            last = o;
        }
        S.aw = aw; S.rw = rw;
        S.rlp[0] = r0; S.rlp[1] = r1; S.rlp[2] = r2; S.rlp[3] = r3;
        S.rlast = last;
    }

    /* ---- the mix ---- */
    int peak = 0;
    const int mix = S.mix, dry = 32767 - (S.mix >> 1);
    for (int n = 0; n < 64; ++n) {
        const int wet = clampi(rout[n] + dout[n], -40000, 40000);
        const int o = clampi(soft(mulz(x[n], dry) + mulz(wet, mix)), -32767, 32767);
        buf[n] = (short)o;
        const int a = o >= 0 ? o : -o;
        peak = a > peak ? a : peak;
    }
    dm_vis.level = peak;
    dm_vis.grains = active;
    ++dm_vis.blocks;
}
