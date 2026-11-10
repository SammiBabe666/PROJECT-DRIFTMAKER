/* DRIFTMAKER: an atmospheric drone engine for Opus 2 on the original OP-1. The engine glue: voices, the block, the
 * controls, the sound's data, the presets in the OP-1's browser.
 *
 * T1 has four pages of four values, one per encoder; push an encoder to open its page:
 *
 *   blue push   CORE     character   tone        detune      filter       (knobs 0-3: the OS turns them)
 *   green push  MOTION   rate 1      depth 1     rate 2      depth 2
 *   white push  TEXTURE  grain       freeze      density     degrade
 *   orange push SPACE    reverb      delay       feedback    mix
 *
 * shift + any turn: a fine step (1 of 99 a detent instead of 2). T2 and T4 stay the OS's (envelope, LFO).
 * Knobs 0-3 and 5-7 are kept with the sound (and reached by the OS's LFO: 0-3); the nine others in its engine data.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include "opus_plugin.h"
#include "op1fw.h"
#include "dm_dsp.h"
#include "dm_page.h"
#include "dm_presets.h"

static struct opl_self *me;
static int mark;                           /* the engine's mark (knob 4 of its sounds), from opus_engine_add */
extern volatile int dm_newest;
static volatile short heard[8];            /* the newest render's knobs (the LFO in), for the block */
static struct opus_fast_lease *wave_lease, *scratch_lease;

volatile int dm_group;                     /* the T1 page shown: 0 core, 1 motion, 2 texture, 3 space */
volatile int dm_touch = -1, dm_touch_age;  /* the value last turned (group * 4 + encoder) and frames since */
volatile signed char dm_notes[6] = { -1, -1, -1, -1, -1, -1 };   /* each voice's note (0-11) while held */
volatile int dm_preset_now = -1;           /* the preset whose name the current sound has, or -1 */

/* the compiler may call these for a struct copy or a cleared array; there is no C library */
#ifndef DM_HOST                            /* the computer's builds have their C library's */
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n)
{
    unsigned char *a = d; const unsigned char *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memset(void *d, int c, __SIZE_TYPE__ n)
{
    unsigned char *a = d;
    while (n--) *a++ = (unsigned char)c;
    return d;
}
#endif

static int k99(int v) { return ((v < 0 ? 0 : v) * 99 + 16383) / 32767; }
static int from99(int v) { v = v < 0 ? 0 : v > 99 ? 99 : v; return (v * 32767 + 98) / 99; }

/* ---------------------------------------------------------------- the page's values */

/* What each encoder turns on each page: a knob (0-7) or a data value (DATA + n). */
#define DATA 16
const signed char DM_PAGE_PARAM[4][4] = {
    { DM_K_CHAR, DM_K_TONE, DM_K_DETUNE, DM_K_FILTER },
    { DATA + DM_D_RATE1, DM_K_DEPTH1, DATA + DM_D_RATE2, DM_K_DEPTH2 },
    { DATA + DM_D_GRAIN, DATA + DM_D_FREEZE, DATA + DM_D_DENSITY, DATA + DM_D_DEGRADE },
    { DATA + DM_D_REVERB, DATA + DM_D_DELAY, DATA + DM_D_FEEDBACK, DM_K_MIX },
};

/* A value as 0-99 for the page: k the sound's knobs as heard. */
int dm_param99(int group, int enc, const short *k)
{
    const int p = DM_PAGE_PARAM[group & 3][enc & 3];
    return p >= DATA ? dm_data[p - DATA] : k99(k[p]);
}

/* ---------------------------------------------------------------- the sound's data */

/* The engine data: "dm", version 1, then the nine values. */
#define DATA_BYTES (3 + DM_DATA_N)

static void data_save(void)                /* key thread */
{
    unsigned char b[DATA_BYTES];
    b[0] = 'd'; b[1] = 'm'; b[2] = 1;
    for (int i = 0; i < DM_DATA_N; ++i)
        b[3 + i] = dm_data[i];
    opus_engine_data_set(b, DATA_BYTES);
}

static int same_name(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

/* Which nine values the current sound has: its data when it has some; a preset's when it carries that preset's name
 * and no data yet (a preset just chosen from the browser); a new sound's otherwise. The sequencer thread, every 16
 * blocks (23 ms): opus_engine_data and opus_engine_sound_name are any thread's. */
static void resolve(void)
{
    static unsigned n;
    if (n++ & 15)
        return;
    unsigned char b[DATA_BYTES];
    const unsigned char *src = DM_DEFAULT_DATA;
    int preset = -1;
    char name[24];
    if (opus_engine_sound_name(name, sizeof name) > 0)
        for (int i = 0; i < DM_PRESETS; ++i)
            if (same_name(name, DM_PRESET[i].name)) { preset = i; break; }
    if (opus_engine_data(b, DATA_BYTES) == DATA_BYTES && b[0] == 'd' && b[1] == 'm' && b[2] == 1)
        src = b + 3;
    else if (preset >= 0)
        src = DM_PRESET[preset].d;
    for (int i = 0; i < DM_DATA_N; ++i)
        if (dm_data[i] != (src[i] > 99 ? 99 : src[i]))
            dm_data[i] = src[i] > 99 ? 99 : src[i];
    dm_preset_now = preset;
}

/* ---------------------------------------------------------------- the voices */

static void v_init(void *voice, int index)
{
    dm_voice_init((struct dm_voice *)voice, index);
    if (index >= 0 && index < 6)
        dm_notes[index] = -1;
}

static void v_note_on(void *voice, const short *knobs)
{
    struct dm_voice *const v = (struct dm_voice *)voice;
    (void)knobs;
    dm_voice_note(v);
    dm_newest = v->index;
    if (v->index >= 0 && v->index < 6)
        dm_notes[v->index] = 12;           /* known at its first render (its pitch) */
}

static void v_release(void *voice, const short *knobs)
{
    struct dm_voice *const v = (struct dm_voice *)voice;
    (void)knobs;                           /* the OS's envelope lets it go */
    if (v->index >= 0 && v->index < 6)
        dm_notes[v->index] = -1;
}

/* the note (0-11) of a phase step: 88.2 kHz, step = Hz x 2^32 / 88200 */
static int note_of(unsigned step)
{
    static const unsigned C[13] = {        /* a quarter tone below C4 .. C5 */
        12377374u, 13113371u, 13893133u, 14719262u, 15594514u, 16521812u, 17504251u, 18545107u, 19647857u,
        20816179u, 22053974u, 23365371u, 24754749u };
    if (!step) return 0;
    while (step < C[0]) step <<= 1;
    while (step >= C[12]) step >>= 1;
    int i = 0;
    while (i < 11 && step >= C[i + 1]) ++i;
    return i;
}

static void v_render(void *voice, short *out, const short *par, const unsigned *pitch)
{
    struct dm_voice *const v = (struct dm_voice *)voice;
    for (int k = 0; k < 8; ++k)
        heard[k] = par[k];
    if (wave_lease)
        dm_tables_at((const short *)opus_fast_use(wave_lease));
    if (scratch_lease)
        dm_scratch_at(opus_fast_use(scratch_lease));
    if (v->index >= 0 && v->index < 6 && dm_notes[v->index] == 12)
        dm_notes[v->index] = (signed char)note_of(pitch[0]);
    dm_voice_render(v, out, par, pitch[0]);
}

static void e_block(short *buf)
{
    short par[8];
    for (int k = 0; k < 8; ++k)
        par[k] = heard[k];
    dm_fx_block(buf, par);
}

/* ---------------------------------------------------------------- the controls */

#define KEY_BLUE_PUSH   0x3B
#define KEY_ORANGE_PUSH 0x3E

static int e_turn(int encoder, int detents, int page)
{
    if (page != 0 || encoder < 0 || encoder > 3)
        return 0;
    const int shift = fw_shift_held();
    const int g = dm_group & 3;
    if (g == 0 && !shift) {                /* CORE: the OS's own turn of knobs 0-3 */
        dm_touch = encoder;
        dm_touch_age = 0;
        return 0;
    }
    if (detents) {
        const int step = (shift ? 1 : 2) * detents;
        const int p = DM_PAGE_PARAM[g][encoder];
        if (p >= DATA) {
            int v = dm_data[p - DATA] + step;
            v = v < 0 ? 0 : v > 99 ? 99 : v;
            dm_data[p - DATA] = (unsigned char)v;
            data_save();
        } else {
            short now[8];
            opus_engine_knobs(now);
            opus_engine_knob_set(p, from99(k99(now[p]) + step));
        }
        dm_touch = g * 4 + encoder;
        dm_touch_age = 0;
    }
    return 1;
}

static int e_key(int key, int pressed, int page)
{
    if (page != 0 || key < KEY_BLUE_PUSH || key > KEY_ORANGE_PUSH)
        return 0;
    if (pressed) {
        dm_group = key - KEY_BLUE_PUSH;
        dm_touch = -1;
    }
    return 1;
}

/* ---------------------------------------------------------------- the presets in the browser */

static void p_list(int kind)
{
    if (kind != 0)
        return;
    for (int i = 0; i < DM_PRESETS; ++i)
        opus_preset_add(me, 0, DM_PRESET_FOLDER, DM_PRESET[i].name, "iter", (unsigned)i);
}

/* The chosen preset's whole patch: the OS's iter with this engine's mark in knob 4, as every sound of an Opus engine
 * is (api.md, Engines). The OS's effect and LFO are left off: the engine has its own space and movement. */
static int p_fill(unsigned tag, char *json, int max, const char *raw_path)
{
    (void)raw_path;
    if (tag >= DM_PRESETS || mark <= 0)
        return 0;
    const struct dm_preset *p = &DM_PRESET[tag];
    int k[8];
    for (int i = 0; i < 4; ++i)
        k[i] = from99(p->k[i]);
    k[4] = mark;
    k[5] = from99(p->k[4]);
    k[6] = from99(p->k[5]);
    k[7] = from99(p->k[6]);
    const int n = fw_snprintf(json, (unsigned)max,
        "{\"adsr\":[%i,%i,%i,%i,2048,5568,4000,4000],\"fx_active\":false,"
        "\"fx_params\":[9000,12000,5000,9000,0,0,0,0],\"fx_type\":\"delay\","
        "\"knobs\":[%i,%i,%i,%i,%i,%i,%i,%i],\"lfo_active\":false,"
        "\"lfo_params\":[16000,0,0,16000,0,0,0,0],\"lfo_type\":\"tremolo\","
        "\"name\":\"%s\",\"octave\":%i,\"synth_version\":2,\"type\":\"iter\"}",
        p->adsr[0], p->adsr[1], p->adsr[2], p->adsr[3],
        k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7],
        p->name, p->octave);
    return n > 0 && n < max ? n : 0;
}

static const struct opus_preset_source PRESETS = {
    .size = sizeof(struct opus_preset_source),
    .list = p_list,
    .fill = p_fill,
};

/* ---------------------------------------------------------------- the engine */

static void page_t1(void)
{
    dm_page_t1();
}

static const struct opus_engine DRIFTMAKER = {
    .size = sizeof(struct opus_engine),
    .name = "driftmaker",
    /* a new sound: organ-choir, unison, a little drift, half open; depth 1 25, depth 2 45, mix 55 (0-99) */
    .knobs = { 7614, 16549, 9929, 16549, 0, 8274, 14895, 18204 },
    .voice = sizeof(struct dm_voice),
    .init = v_init,
    .note_on = v_note_on,
    .release = v_release,
    .render = v_render,
    .block = e_block,
    .page = page_t1,
    .turn = e_turn,
    .key = e_key,
    .flags = OPUS_ENGINE_STATE_MOVES,      /* no pointer to a voice is kept past a call */
    .shift_knobs = { -1, -1, -1, -1 },     /* shift is a fine step here, not a second layer */
};

int opl_init(struct opl_self *self)
{
    me = self;
    for (int i = 0; i < DM_DATA_N; ++i)
        dm_data[i] = DM_DEFAULT_DATA[i];
    void *const fx = opus_alloc_audio(self, dm_fx_bytes());
    if (!fx)
        return -2;                         /* no room for the space's memory: refuse cleanly */
    dm_fx_init(fx);
    short *const own = (short *)opus_alloc_audio(self, dm_waves_bytes());
    if (own) {                             /* the waves in the audio pool, on a fast-memory lease while it plays */
        dm_waves_copy(own);
        dm_tables_at(own);
        wave_lease = opus_fast_lease(self, dm_waves_bytes(), OPUS_FAST_CONST, own);
    }
    void *const scratch = opus_alloc_audio(self, dm_scratch_bytes());
    if (scratch) {                         /* the render's two morphed waves, fast while it plays */
        dm_scratch_at(scratch);
        scratch_lease = opus_fast_lease(self, dm_scratch_bytes(), OPUS_FAST_SCRATCH, scratch);
    }
    dm_page_init(self);
    mark = opus_engine_add(self, &DRIFTMAKER);
    if (mark < 0)
        return -1;
    opus_on_block(self, resolve);
    opus_preset_source(self, &PRESETS);    /* the browser's "driftmaker" folder; without it the engine still plays */
    return 0;
}
