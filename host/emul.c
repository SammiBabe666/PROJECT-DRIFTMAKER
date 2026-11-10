/* The OP-1 model around the engine (emul.h). Freestanding; integer only. Copyright (C) 2026 Sammi.
 * GPL-3.0-or-later. */
#include "opus_plugin.h"
#include "emul.h"

int opl_init(struct opl_self *self);

static unsigned zero_cycles(void) { return 0; }
unsigned (*emul_cycles)(void) = zero_cycles;

static struct opl_self self = { sizeof(struct opl_self), "sammi.driftmaker", "/yaffs2/mod/sammi.driftmaker", 0 };
static unsigned char vmem[EMUL_VOICES * 256] __attribute__((aligned(32)));

/* C4 .. B4 as phase steps at 88.2 kHz */
static const unsigned STEP[12] = { 12740059u, 13497623u, 14300233u, 15150569u, 16051469u, 17005939u, 18017165u,
                                   19088521u, 20223584u, 21426141u, 22700205u, 24050030u };
static unsigned step_of(int midi)
{
    int o = (midi - 60) / 12, n = (midi - 60) % 12;
    if (n < 0) { n += 12; --o; }
    unsigned s = STEP[n];
    return o >= 0 ? s << o : s >> -o;
}

/* envelope: blocks for an ADSR value (64..16320). An approximation of the OS's curve: (v / 16320)^2 x 8 s for the
 * attack, x 10 s for the release. */
static int env_blocks(int v, int secs)
{
    long long b = (long long)v * v * secs * 689 / (16320LL * 16320);
    return b < 1 ? 1 : (int)b;
}

static void *voice(int i) { return vmem + i * ((E.engine->voice + 31u) & ~31u); }

int emul_start(void)
{
    for (int i = 0; i < EMUL_VOICES; ++i) { E.vnote[i] = -1; E.vstage[i] = 0; }
    const int r = opl_init(&self);
    if (r < 0 || !E.engine) return r < 0 ? r : -100;
    if (E.engine->voice > 256) return -101;
    for (int i = 0; i < 8; ++i) E.knobs[i] = E.engine->knobs[i];
    E.knobs[4] = EMUL_MARK;
    emul_fmt(E.name, sizeof E.name, "%s", E.engine->name);
    E.adsr[0] = 3000; E.adsr[1] = 9000; E.adsr[2] = 32767; E.adsr[3] = 8000;
    for (int i = 0; i < EMUL_VOICES; ++i) E.engine->init(voice(i), i);
    if (E.source) E.source->list(0);
    return 0;
}

/* the numbers of a JSON array member: "key":[a,b,...] */
static int json_ints(const char *j, const char *key, int *v, int max)
{
    for (const char *p = j; *p; ++p) {
        const char *k = key, *q = p;
        while (*k && *q == *k) { ++k; ++q; }
        if (*k) continue;
        int n = 0;
        if (*q == '[') ++q;
        while (*q && *q != ']' && *q != ',' && *q != '}' && n < max) {
            int neg = 0, x = 0;
            if (*q == '-') { neg = 1; ++q; }
            while (*q >= '0' && *q <= '9') x = x * 10 + (*q++ - '0');
            v[n++] = neg ? -x : x;
            if (*q == ',') ++q;
            if (*q == ']' || *q == '}') break;
        }
        return n;
    }
    return 0;
}

int emul_load(int tag)
{
    static char json[1024];
    if (!E.source) return -1;
    int n = E.source->fill((unsigned)tag, json, sizeof json, "/raw");
    if (n <= 0) return -2;
    int k[8], a[8], o[1];
    if (json_ints(json, "\"knobs\":", k, 8) != 8) return -3;
    if (json_ints(json, "\"adsr\":", a, 8) != 8) return -4;
    if (json_ints(json, "\"octave\":", o, 1) != 1) return -5;
    if (k[4] != EMUL_MARK) return -6;      /* the sound must carry the engine's mark */
    for (int i = 0; i < 8; ++i) E.knobs[i] = (short)k[i];
    for (int i = 0; i < 4; ++i) E.adsr[i] = a[i];
    E.octave = o[0];
    /* the name: "name":"..." */
    const char *p = json;
    while (*p && !(p[0] == '"' && p[1] == 'n' && p[2] == 'a' && p[3] == 'm' && p[4] == 'e' && p[5] == '"')) ++p;
    if (!*p) return -7;
    p += 8;
    int i = 0;
    while (*p && *p != '"' && i < 23) E.name[i++] = *p++;
    E.name[i] = 0;
    E.data_len = 0;                        /* a preset from a source carries no engine data */
    for (int v = 0; v < EMUL_VOICES; ++v) E.engine->init(voice(v), v);   /* the OS builds the voices again */
    return 0;
}

void emul_note(int midi, int on)
{
    if (on) {
        int best = -1;
        for (int i = 0; i < EMUL_VOICES; ++i)
            if (E.vstage[i] == 0) { best = i; break; }
        if (best < 0) {                    /* steal the oldest */
            unsigned old = 0xFFFFFFFFu;
            for (int i = 0; i < EMUL_VOICES; ++i)
                if (E.vage[i] < old) { old = E.vage[i]; best = i; }
        }
        E.vnote[best] = midi + 12 * E.octave;
        E.vheld[best] = 1;
        E.vstage[best] = 1;
        E.vage[best] = ++E.clock;
        E.engine->note_on(voice(best), E.knobs);
    } else {
        for (int i = 0; i < EMUL_VOICES; ++i)
            if (E.vheld[i] && E.vnote[i] == midi + 12 * E.octave) {
                E.vheld[i] = 0;
                E.vstage[i] = 3;
                E.engine->release(voice(i), E.knobs);
            }
    }
}

void emul_all_off(void)
{
    for (int i = 0; i < EMUL_VOICES; ++i)
        if (E.vheld[i]) { E.vheld[i] = 0; E.vstage[i] = 3; E.engine->release(voice(i), E.knobs); }
}

void emul_blocks(short *out, int nb)
{
    static short vout[128];
    static int acc[128];
    static unsigned pitch[128];
    const int att = env_blocks(E.adsr[0], 8), dec = env_blocks(E.adsr[1], 8), rel = env_blocks(E.adsr[3], 10);
    for (int b = 0; b < nb; ++b) {
        if (E.on_block) E.on_block();
        for (int n = 0; n < 128; ++n) acc[n] = 0;
        unsigned t0 = emul_cycles(), tr = 0;
        for (int i = 0; i < EMUL_VOICES; ++i) {
            if (!E.vstage[i]) continue;
            const int e0 = E.venv[i];
            if (E.vstage[i] == 1) {
                E.venv[i] += (1 << 24) / att;
                if (E.venv[i] >= (1 << 24)) { E.venv[i] = 1 << 24; E.vstage[i] = 2; }
            } else if (E.vstage[i] == 2) {     /* decay to the sustain level */
                const int sus = (int)(((long long)E.adsr[2] << 24) / 32767);
                if (E.venv[i] > sus) {
                    E.venv[i] -= (1 << 24) / dec;
                    if (E.venv[i] < sus) E.venv[i] = sus;
                }
            } else if (E.vstage[i] == 3) {
                E.venv[i] -= (1 << 24) / rel;
                if (E.venv[i] <= 0) { E.venv[i] = 0; E.vstage[i] = 0; }
            }
            const unsigned s = step_of(E.vnote[i]);
            for (int n = 0; n < 128; ++n) pitch[n] = s;
            const unsigned c0 = emul_cycles();
            E.engine->render(voice(i), vout, E.knobs, pitch);
            tr += emul_cycles() - c0;
            /* envelope ramped across the block (Q15), a squared curve for the ear */
            const int g0 = (int)(((long long)(e0 >> 9) * (e0 >> 9)) >> 15), g1 = (int)(((long long)(E.venv[i] >> 9) * (E.venv[i] >> 9)) >> 15);
            for (int n = 0; n < 128; ++n) {
                const int g = g0 + (((g1 - g0) * n) >> 7);
                acc[n] += (vout[n] * g) >> 15;
            }
        }
        short *o = out + b * 64;
        for (int n = 0; n < 64; ++n) {     /* to 44.1 kHz: the pair's mean, at half gain for six voices' room */
            int v = (acc[2 * n] + acc[2 * n + 1]) >> 2;
            o[n] = (short)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
        }
        const unsigned c1 = emul_cycles();
        if (E.engine->block) E.engine->block(o);
        const unsigned tb = emul_cycles() - c1;
        E.cyc_render += tr;
        E.cyc_block += tb;
        if (tr + tb > E.cyc_worst) E.cyc_worst = tr + tb;
        (void)t0;
    }
}

/* a turn on T1: the engine's handler first; when it leaves it, the OS turns knob 0-3 (modelled as 2 of 99 a detent) */
int emul_turn(int encoder, int detents)
{
    const int took = E.engine->turn ? E.engine->turn(encoder, detents, 0) : 0;
    if (!took && encoder >= 0 && encoder < 4) {
        int v = ((E.knobs[encoder] * 99 + 16383) / 32767) + 2 * detents;
        v = v < 0 ? 0 : v > 99 ? 99 : v;
        E.knobs[encoder] = (short)((v * 32767 + 98) / 99);
    }
    return took;
}
int emul_key(int key, int pressed) { return E.engine->key ? E.engine->key(key, pressed, 0) : 0; }
void emul_page(void) { if (E.engine->page) E.engine->page(); }
