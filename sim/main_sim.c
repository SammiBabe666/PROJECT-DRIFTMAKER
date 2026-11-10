/* DRIFTMAKER's compiled code in the Blackfin instruction-set simulator (bfin-elf-run), and the same program on the
 * computer, so the two can be compared byte for byte:
 *   dump:   two presets' chords, 2 s each, raw s16 to stdout
 *   cycles: cycles per audio block: a preset's two-note chord, six voices on a preset, and the worst case (six voices,
 *           every value at its top, grains and freeze on), each over 400 blocks: mean and worst
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include "opus_plugin.h"
#include "emul.h"
#include "dm_dsp.h"

#ifdef DM_HOST
#include <stdio.h>
static void out_bytes(const void *p, int n) { fwrite(p, 1, (size_t)n, stdout); }
#else
int sim_syscall(int nr, const int *args);
unsigned sim_cycles(void);
static void out_bytes(const void *p, int n) { int a[3] = { 1, (int)p, n }; sim_syscall(5, a); }
#endif

static void out_text(const char *s) { int n = 0; while (s[n]) n++; out_bytes(s, n); }
static void out_num(unsigned v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); out_text(b + i); }

static short buf[64 * 64];

static void __attribute__((unused)) measure(const char *what, int blocks)
{
    unsigned worst = 0, sum = 0, worst_r = 0, worst_b = 0;
    for (int b = 0; b < blocks; ++b) {
        E.cyc_render = E.cyc_block = E.cyc_worst = 0;
        emul_blocks(buf, 1);
        const unsigned t = E.cyc_render + E.cyc_block;
        sum += t;
        if (t > worst) { worst = t; worst_r = E.cyc_render; worst_b = E.cyc_block; }
    }
    out_text(what); out_text(": mean "); out_num(sum / (unsigned)blocks); out_text(" worst "); out_num(worst);
    out_text(" (voices "); out_num(worst_r); out_text(", block "); out_num(worst_b); out_text(") cycles a block\n");
}

int main(void);
int main(void)
{
#ifndef DM_HOST
    emul_cycles = sim_cycles;
#endif
    if (emul_start() != 0) return 1;
#ifdef SIM_CYCLES
    emul_load(0); emul_blocks(buf, 32);
    emul_note(48, 1); emul_note(55, 1); emul_blocks(buf, 64);
    measure("cathedral sleep, two notes", 400);
    emul_all_off();
    emul_load(12); emul_blocks(buf, 32);
    for (int n = 0; n < 6; ++n) emul_note(40 + n * 5, 1);
    emul_blocks(buf, 64);
    measure("witchglass, six voices", 400);
    emul_all_off();
    emul_load(14); emul_blocks(buf, 32);
    for (int i = 0; i < DM_DATA_N; ++i) dm_data[i] = 99;
    for (int i = 0; i < 8; ++i) if (i != 4) E.knobs[i] = 32767;
    for (int n = 0; n < 6; ++n) emul_note(72 + n * 5, 1);      /* high notes: the bell zone's widest steps */
    emul_blocks(buf, 64);
    measure("worst: six high voices, every value 99, frozen", 400);
    for (int i = 0; i < DM_DATA_N; ++i) dm_data[i] = i == DM_D_FREEZE ? 0 : 99;
    measure("worst: six high voices, every value 99, grains", 400);
#else
    static const int tags[2] = { 0, 12 };
    for (int p = 0; p < 2; ++p) {
        emul_load(tags[p]);
        emul_blocks(buf, 32);
        emul_note(48, 1); emul_note(55, 1); emul_note(60, 1);
        for (int b = 0; b < 1378; b += 64) {
            emul_blocks(buf, 64);
            out_bytes(buf, (int)sizeof buf);
        }
        emul_all_off();
    }
#endif
    return 0;
}
