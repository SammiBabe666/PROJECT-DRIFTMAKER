/* DRIFTMAKER: the sound. Integer only (the BF524 has no FPU), no C library, nothing allocated in the audio.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#ifndef DM_DSP_H
#define DM_DSP_H

/* The sound's knobs (the OS keeps them in the sound; knob 4 is the core's mark). 0-32767. */
#define DM_K_CHAR    0     /* blue, CORE:   character, the source morph: hollow, organ, choir ah, choir oo, bowed,
                              gurdy, reed, bell */
#define DM_K_TONE    1     /* green, CORE:  tone, the layer voice: sub octave .. unison .. fifth .. twelfth */
#define DM_K_DETUNE  2     /* white, CORE:  detune, the unison spread and the pitch drift */
#define DM_K_FILTER  3     /* orange, CORE: filter, the resonant low-pass */
#define DM_K_DEPTH1  5     /* MOTION green: mod 1 depth (pitch wander, filter sweep) */
#define DM_K_DEPTH2  6     /* MOTION orange: mod 2 depth (swells, the timbre's drift, the grains' travel) */
#define DM_K_MIX     7     /* SPACE orange: the space's mix */

/* The sound's data-held values (0-99), kept in the sound's engine data (dm_glue). */
enum {
    DM_D_RATE1, DM_D_RATE2, DM_D_GRAIN, DM_D_FREEZE, DM_D_DENSITY, DM_D_DEGRADE, DM_D_REVERB, DM_D_DELAY,
    DM_D_FEEDBACK, DM_DATA_N
};
extern volatile unsigned char dm_data[DM_DATA_N];

struct dm_voice {
    unsigned ph[3];            /* the three oscillators' phases */
    int drift[3];              /* their random walks (octave x 2^16 units, before scaling) */
    int lp, bp;                /* the filter */
    unsigned lfo1, lfo2;       /* the two modulators' phases */
    int rnd, rnd_to;           /* mod 1's smoothed random part */
    unsigned seed;
    int amp;                   /* the swell's gain last block, Q15 */
    int last;                  /* the last 44.1 kHz sample, for the halfway point */
    int morph;                 /* the character's position, smoothed, 0..7*4096 */
    int cut;                   /* the cutoff's index x 256, smoothed */
    unsigned age;              /* blocks since its note */
    int index;
};

/* The visible state for the page (written by the audio, read by the display). */
struct dm_vis {
    volatile int m1, m2;       /* the newest voice's modulators, -32767..32767 */
    volatile int morph;        /* its character position 0..7*4096 */
    volatile int level;        /* the block's peak, 0..32767 */
    volatile int grains;       /* grains sounding */
    volatile unsigned blocks;  /* blocks rendered */
    volatile int bloom;        /* its age as 0..32767 */
};
extern struct dm_vis dm_vis;

void dm_tables_at(const short *waves);       /* where the waves are this block (a fast-memory lease) or 0 */
unsigned dm_waves_bytes(void);
void dm_scratch_at(void *s);                 /* the render's scratch this block (a lease) or 0 */
unsigned dm_scratch_bytes(void);
void dm_waves_copy(short *to);

void dm_voice_init(struct dm_voice *v, int index);
void dm_voice_note(struct dm_voice *v);
void dm_voice_render(struct dm_voice *v, short *out, const short *par, unsigned pitch);

/* The voices' sum, 64 samples at 44.1 kHz in place: degrade, grains and freeze, delay, reverb. */
struct dm_fx_mem;
unsigned dm_fx_bytes(void);
int dm_fx_init(void *mem);                   /* mem: dm_fx_bytes() zeroed bytes (the audio pool) */
void dm_fx_block(short *buf, const short *par);

#endif
