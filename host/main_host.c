/* DRIFTMAKER on the computer: the engine's own code (engine/sammi.driftmaker) run in the OP-1 model (emul.c).
 *   dm_host test                         the functional tests (exit 0 when all pass)
 *   dm_host render TAG SECS HOLD N...     preset TAG, notes N... held HOLD s, SECS s in all: raw s16 44.1 kHz to stdout
 *   dm_host score FILE                    a score (demo/): raw audio to stdout
 *   dm_host video SCORE AUDIO.raw        a score: audio to AUDIO.raw, the page's drawing calls every 1/30 s to stdout
 *   dm_host page TAG GROUP FRAMES [TOUCH [FREEZE]]  the T1 page's drawing calls, as text, after FRAMES frames of a chord
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "opus_plugin.h"
#include "emul.h"
#include "../engine/sammi.driftmaker/dm_dsp.h"
#include "../engine/sammi.driftmaker/dm_presets.h"
#include "../engine/sammi.driftmaker/dm_page.h"


static int fails, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { ++fails; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void draw_print(const char *l, int n) { fwrite(l, 1, (size_t)n, stdout); }

static short buf[64 * 689 * 31];

static double rms(const short *b, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i) s += (double)b[i] * b[i];
    return sqrt(s / (n ? n : 1));
}

static int k99(int v) { return (v * 99 + 16383) / 32767; }

static int test(void)
{
    CHECK(emul_start() == 0, "opl_init");
    CHECK(E.engine && strcmp(E.engine->name, "driftmaker") == 0, "engine name");
    CHECK(E.engine->size == sizeof(struct opus_engine), "struct size");
    CHECK(E.engine->voice <= 256, "voice bytes %u", E.engine->voice);
    printf("voice state %u bytes, memory %u bytes\n", E.engine->voice, E.alloc_bytes);
    /* the presets in the browser */
    CHECK(E.n_entries == DM_PRESETS, "%d entries", E.n_entries);
    for (int i = 0; i < E.n_entries; ++i) {
        CHECK(strcmp(E.entry[i].folder, "driftmaker") == 0, "folder");
        CHECK(strcmp(E.entry[i].type, "iter") == 0, "type");
        CHECK(strlen(E.entry[i].name) <= 23, "name length %s", E.entry[i].name);
        for (const char *p = E.entry[i].name; *p; ++p)
            CHECK((*p >= 'a' && *p <= 'z') || *p == ' ', "lower case name %s", E.entry[i].name);
    }
    /* a new sound: defaults */
    emul_blocks(buf, 16);
    CHECK(dm_data[DM_D_REVERB] == DM_DEFAULT_DATA[DM_D_REVERB], "default data");
    /* each preset loads, its values arrive, it sounds, it never clips hard, it stays centred */
    for (int t = 0; t < DM_PRESETS; ++t) {
        CHECK(emul_load(t) == 0, "load %d", t);
        CHECK(strcmp(E.name, DM_PRESET[t].name) == 0, "name %s", E.name);
        for (int i = 0; i < 4; ++i) CHECK(k99(E.knobs[i]) == DM_PRESET[t].k[i], "knob %d of %d", i, t);
        CHECK(k99(E.knobs[5]) == DM_PRESET[t].k[4] && k99(E.knobs[6]) == DM_PRESET[t].k[5] &&
              k99(E.knobs[7]) == DM_PRESET[t].k[6], "knobs 5-7 of %d", t);
        emul_blocks(buf, 32);
        for (int i = 0; i < DM_DATA_N; ++i) CHECK(dm_data[i] == DM_PRESET[t].d[i], "data %d of preset %d", i, t);
        CHECK(dm_preset_now == t, "preset seen %d", t);
        emul_note(48, 1); emul_note(55, 1);
        int clip = 0; double sum = 0; double peak_rms = 0;
        for (int s = 0; s < 8; ++s) {      /* 8 x 0.74 s */
            emul_blocks(buf, 512);
            for (int i = 0; i < 512 * 64; ++i) { if (buf[i] >= 32767 || buf[i] <= -32767) ++clip; sum += buf[i]; }
            const double r = rms(buf, 512 * 64);
            if (r > peak_rms) peak_rms = r;
        }
        emul_all_off();
        CHECK(peak_rms > 500, "preset %d sounds (rms %.0f)", t, peak_rms);
        CHECK(clip < 64, "preset %d clips %d samples", t, clip);
        CHECK(fabs(sum / (8.0 * 512 * 64)) < 400, "preset %d dc %.0f", t, sum / (8.0 * 512 * 64));
        printf("preset %2d %-18s rms %5.0f  clipped %d\n", t + 1, DM_PRESET[t].name, peak_rms, clip);
        emul_blocks(buf, 689 * 25);        /* let the tail end: 25 s */
        const double tail = rms(buf + 689 * 24 * 64, 689 * 64);
        CHECK(tail < 30, "preset %d silent after its tail (rms %.1f)", t, tail);
    }
    /* the controls: pushes choose the page; turns reach the right value; shift is a fine step */
    emul_load(0);
    emul_blocks(buf, 32);
    for (int g = 0; g < 4; ++g) {
        CHECK(emul_key(0x3B + g, 1) == 1 && emul_key(0x3B + g, 0) == 1, "push %d taken", g);
        CHECK(dm_group == g, "page %d", g);
    }
    CHECK(emul_key(0x06, 1) == 0, "T1 key left to the OS");
    emul_key(0x3B, 1);                     /* CORE: the OS turns knobs 0-3 */
    int before = k99(E.knobs[3]);
    CHECK(emul_turn(3, 2) == 0, "core turn is the OS's");
    CHECK(k99(E.knobs[3]) == before + 4, "filter moved by the OS model");
    E.shift = 1;
    before = k99(E.knobs[3]);
    CHECK(emul_turn(3, 1) == 1 && k99(E.knobs[3]) == before + 1, "shift fine step on core");
    E.shift = 0;
    for (int g = 1; g < 4; ++g) {
        emul_key(0x3B + g, 1);
        for (int e = 0; e < 4; ++e) {
            const int p = DM_PAGE_PARAM[g][e];
            const int sets = E.data_sets, ks = E.knob_sets;
            const int was = p >= 16 ? dm_data[p - 16] : k99(E.knobs[p]);
            const int dir = was > 90 ? -1 : 1;
            CHECK(emul_turn(e, 3 * dir) == 1, "turn taken g%d e%d", g, e);
            const int now = p >= 16 ? dm_data[p - 16] : k99(E.knobs[p]);
            CHECK(now == was + 6 * dir, "g%d e%d moved %d -> %d", g, e, was, now);
            if (p >= 16) {
                CHECK(E.data_sets == sets + 1, "data kept g%d e%d", g, e);
                CHECK(E.data_len == 12 && E.data[0] == 'd' && E.data[1] == 'm' && E.data[2] == 1 &&
                      E.data[3 + p - 16] == now, "data bytes g%d e%d", g, e);
            } else {
                CHECK(E.knob_sets == ks + 1, "knob kept g%d e%d", g, e);
            }
        }
    }
    /* the data wins over the preset's name once it exists: persistence across the resolver */
    const int rv = dm_data[DM_D_REVERB];
    emul_blocks(buf, 64);
    CHECK(dm_data[DM_D_REVERB] == rv, "edited data survives the resolver");
    /* range ends: never past 0 or 99 */
    emul_key(0x3E, 1);
    for (int i = 0; i < 80; ++i) emul_turn(0, 1);
    CHECK(dm_data[DM_D_REVERB] == 99, "top end");
    for (int i = 0; i < 80; ++i) emul_turn(0, -1);
    CHECK(dm_data[DM_D_REVERB] == 0, "bottom end");
    /* freeze: the texture holds after the keys are let go */
    emul_load(9);
    emul_blocks(buf, 32);
    emul_note(48, 1);
    emul_blocks(buf, 689 * 3);
    emul_key(0x3D, 1);
    emul_turn(1, 40);                      /* freeze on */
    CHECK(dm_data[DM_D_FREEZE] > 0, "freeze on");
    emul_all_off();
    emul_blocks(buf, 689 * 12);
    const double held = rms(buf + 689 * 11 * 64, 689 * 64);
    CHECK(held > 200, "frozen texture sustains after release (rms %.0f)", held);
    printf("freeze: rms %.0f twelve seconds after the keys were let go\n", held);
    emul_turn(1, -60);                     /* freeze off */
    CHECK(dm_data[DM_D_FREEZE] == 0, "freeze off");
    emul_blocks(buf, 689 * 30);
    CHECK(rms(buf + 689 * 29 * 64, 689 * 64) < 30, "silent again after freeze off");
    /* six voices at the heaviest settings: stable */
    emul_load(14);
    emul_blocks(buf, 32);
    for (int i = 0; i < DM_DATA_N; ++i) dm_data[i] = i == DM_D_FREEZE ? 0 : 99;
    for (int i = 0; i < 8; ++i) if (i != 4) E.knobs[i] = 32767;
    for (int n = 0; n < 6; ++n) emul_note(36 + n * 7, 1);
    int clip = 0;
    for (int s = 0; s < 8; ++s) {
        emul_blocks(buf, 689);
        for (int i = 0; i < 689 * 64; ++i) if (buf[i] >= 32767 || buf[i] <= -32767) ++clip;
    }
    printf("six voices, every value at its top: %d samples at full scale in 8 s\n", clip);
    emul_all_off();
    /* the page draws on every page */
    for (int g = 0; g < 4; ++g) { emul_key(0x3B + g, 1); emul_page(); }
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

static FILE *audio_out;                    /* where the audio goes (stdout, or a file in video mode) */
static int video;                          /* video mode: a frame's drawing every 1/30 s of audio */
static long samples, next_frame;

static void write_raw(int n)
{
    fwrite(buf, sizeof(short), (size_t)n * 64, audio_out ? audio_out : stdout);
}

/* n blocks, and in video mode the page's drawing at each 30th of a second (1470 samples) */
static void run_blocks(int n)
{
    if (!video) { emul_blocks(buf, n); write_raw(n); samples += 64L * n; return; }
    for (int b = 0; b < n; ++b) {
        emul_blocks(buf, 1);
        fwrite(buf, sizeof(short), 64, audio_out);
        samples += 64;
        if (samples >= next_frame) {
            next_frame += 1470;
            printf("frame %ld\n", samples);
            emul_page();
        }
    }
}

/* a score: lines "t SECONDS", "preset TAG", "on NOTE", "off NOTE", "alloff", "key K", "turn E D", "shift 0|1",
 * "end SECONDS"; time runs forward only */
static int score(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 2;
    char line[256];
    double now = 0;
    (void)now;
    while (fgets(line, sizeof line, f)) {
        double t; int a, b;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "t %lf", &t) == 1 || sscanf(line, "end %lf", &t) == 1) {
            long target = (long)(t * 44100.0 / 64 + 0.5) * 64;
            while (samples < target) {
                const long left = (target - samples) / 64;
                run_blocks(left > 1024 ? 1024 : (int)left);
            }
            now = t;
        } else if (sscanf(line, "preset %d", &a) == 1) emul_load(a);
        else if (sscanf(line, "on %d", &a) == 1) emul_note(a, 1);
        else if (sscanf(line, "off %d", &a) == 1) emul_note(a, 0);
        else if (strncmp(line, "alloff", 6) == 0) emul_all_off();
        else if (sscanf(line, "key %d", &a) == 1) { emul_key(a, 1); emul_key(a, 0); }
        else if (sscanf(line, "turn %d %d", &a, &b) == 2) emul_turn(a, b);
        else if (sscanf(line, "shift %d", &a) == 1) E.shift = a;
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "test") == 0)
        return test();
    if (emul_start() != 0) return 3;
    if (argc >= 5 && strcmp(argv[1], "render") == 0) {
        emul_load(atoi(argv[2]));
        const double secs = atof(argv[3]), hold = atof(argv[4]);
        emul_blocks(buf, 32);              /* the resolver sees the preset first */
        for (int i = 5; i < argc; ++i) emul_note(atoi(argv[i]), 1);
        int total = (int)(secs * 44100 / 64), held = (int)(hold * 44100 / 64), done = 0;
        while (done < total) {
            int n = total - done > 1024 ? 1024 : total - done;
            if (done < held && done + n > held) n = held - done;
            emul_blocks(buf, n);
            write_raw(n);
            done += n;
            if (done == held) emul_all_off();
        }
        return 0;
    }
    if (argc >= 3 && strcmp(argv[1], "score") == 0)
        return score(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "video") == 0) {
        /* video SCORE AUDIO.raw: the audio to the file, every frame's page drawing to stdout ("frame N" first) */
        audio_out = fopen(argv[3], "wb");
        if (!audio_out) return 2;
        video = 1;
        E.draw_out = draw_print;
        const int r = score(argv[2]);
        fclose(audio_out);
        return r;
    }
    if (argc >= 5 && strcmp(argv[1], "page") == 0) {
        /* page TAG GROUP FRAMES [TOUCH_ENCODER [FREEZE]]: a chord held, the page drawn after FRAMES frames */
        emul_load(atoi(argv[2]));
        emul_blocks(buf, 32);
        emul_key(0x3B + atoi(argv[3]), 1);
        emul_note(48, 1); emul_note(55, 1); emul_note(60, 1);
        const int frames = atoi(argv[4]);
        for (int f = 0; f < frames; ++f) {
            emul_blocks(buf, 24);          /* 1/28.5 s */
            if (argc >= 7 && atoi(argv[6]) && f == frames / 2) { emul_key(0x3D, 1); emul_turn(1, 20); emul_key(0x3B + atoi(argv[3]), 1); }
            if (argc >= 6 && atoi(argv[5]) >= 0 && f == frames - 10) emul_turn(atoi(argv[5]), 1);
            if (f < frames - 1) emul_page();
        }
        E.draw_out = draw_print;
        emul_page();
        return 0;
    }
    fprintf(stderr, "usage: see the top of main_host.c\n");
    return 2;
}
