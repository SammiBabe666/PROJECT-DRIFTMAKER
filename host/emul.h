/* A model of what the OP-1 and the Opus core do around an engine, for the computer and the Blackfin simulator:
 * six voices, a note's voice choice, an ADSR envelope, the voices' sum (88.2 kHz to 44.1 kHz), the block handler,
 * the preset browser's load (the source's fill, its JSON read back), turns and keys on T1, and page frames.
 * It is a MODEL: the OS's own envelope curve, voice gain and resampler are not public, so levels and envelope times
 * here are approximations. The engine code it runs is the plugin's own, unchanged. Freestanding (no C library).
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#ifndef EMUL_H
#define EMUL_H

#define EMUL_MARK 0x1D37                   /* the stub core's mark for the engine (the real one is the core's) */
#define EMUL_ENTRIES 32
#define EMUL_VOICES 6

struct opus_engine;
struct opus_preset_source;

struct emul {
    const struct opus_engine *engine;
    const struct opus_preset_source *source;
    void (*on_block)(void);
    struct { const char *folder, *name, *type; unsigned tag; } entry[EMUL_ENTRIES];
    int n_entries;
    short knobs[8];
    unsigned char data[256];
    int data_len;
    char name[24];
    int adsr[4];
    int octave;
    int shift;
    int knob_sets, data_sets;
    unsigned alloc_bytes;
    void (*draw_out)(const char *line, int n);
    /* voices */
    unsigned char *vstate;
    int vnote[EMUL_VOICES];                /* midi note, -1 free */
    int vheld[EMUL_VOICES];
    int venv[EMUL_VOICES];                 /* Q24 */
    int vstage[EMUL_VOICES];               /* 0 off, 1 attack, 2 sustain, 3 release */
    unsigned vage[EMUL_VOICES];
    unsigned clock;
    unsigned cyc_render, cyc_block, cyc_worst;   /* the simulator's figures */
};
extern struct emul E;

int emul_fmt(char *dst, unsigned size, const char *f, ...);
int emul_start(void);                      /* opl_init, the preset list: 0 or <0 */
int emul_load(int tag);                    /* the browser loads entry tag: 0 ok */
void emul_note(int midi, int on);
void emul_all_off(void);
/* n blocks of 64 samples at 44.1 kHz into out (n x 64 shorts) */
void emul_blocks(short *out, int n);
int emul_turn(int encoder, int detents);
int emul_key(int key, int pressed);
void emul_page(void);
/* a cycle counter for the simulator; 0 on the computer */
extern unsigned (*emul_cycles)(void);

#endif
