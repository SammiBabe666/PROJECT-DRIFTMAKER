/* DRIFTMAKER's sixteen presets. Each is a whole sound: the seven knobs, the nine values kept in the sound's data,
 * the OS's envelope (T2) and octave. They appear in the OP-1's preset browser in the folder "driftmaker" (the engine
 * is a preset source: dm_glue.c writes each one's patch when it is chosen). Values 0-99.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#ifndef DM_PRESETS_H
#define DM_PRESETS_H

#define DM_PRESETS 16
#define DM_PRESET_FOLDER "driftmaker"

struct dm_preset {
    const char *name;                      /* lower case: the OP-1's font draws capitals as icons */
    /*                knobs: character, tone, detune, filter, depth 1, depth 2, mix */
    unsigned char k[7];
    /*                data: rate 1, rate 2, grain, freeze, density, degrade, reverb, delay, feedback */
    unsigned char d[9];
    unsigned short adsr[4];                /* the OS's envelope: attack, decay, sustain, release */
    signed char octave;
};

static const struct dm_preset DM_PRESET[DM_PRESETS] __attribute__((unused)) = {
    /* name                  char tone det filt d1 d2 mix    r1 r2 grn frz den deg rev dly fb    adsr                       oct */
    { "cathedral sleep",   { 16, 82, 22, 48, 20, 45, 55 }, { 32, 38, 10, 0, 30,  0, 85,  0,  0 }, { 7000, 9000, 32767, 11000 }, -1 },
    { "bronze choir",      { 93, 60, 40, 62, 30, 55, 60 }, { 38, 45, 35, 0, 55,  0, 80, 35, 40 }, { 3500, 9000, 32767,  9000 },  0 },
    { "hollow reliquary",  {  4, 20, 30, 38, 35, 50, 50 }, { 28, 35, 20, 0, 20, 15, 70, 20, 30 }, { 5000, 9000, 27524,  9000 },  0 },
    { "drowned gurdy",     { 71, 30, 55, 55, 60, 40, 45 }, { 50, 40, 25, 0, 40, 45, 60, 30, 35 }, { 2500, 9000, 28834,  7000 },  0 },
    { "buried bell",       { 99, 10, 25, 45, 15, 35, 70 }, { 25, 30, 30, 0, 15, 10, 95, 45, 55 }, { 1200, 9000, 22936, 12000 },  0 },
    { "ashen vespers",     { 30, 50, 45, 52, 25, 60, 65 }, { 30, 42, 40, 0, 45,  5, 90, 25, 25 }, { 8000, 9000, 29490, 12000 },  0 },
    { "wailing room",      { 45, 62, 85, 58, 75, 70, 60 }, { 60, 55, 45, 0, 60, 20, 75, 50, 60 }, { 6000, 9000, 32767, 10000 },  0 },
    { "blackened strings", { 57, 15, 50, 42, 30, 35, 40 }, { 40, 35, 15, 0, 30, 35, 55, 15, 20 }, { 4000, 9000, 32767,  8000 }, -1 },
    { "mothlight",         { 40, 95, 35, 80, 40, 65, 70 }, { 45, 50, 70, 0, 85,  0, 85, 55, 45 }, { 6000, 9000, 32767, 11000 },  1 },
    { "chapel of rain",    { 10, 50, 30, 40, 20, 40, 60 }, { 35, 45, 55, 0, 70, 25, 80, 40, 35 }, { 7000, 9000, 20970, 11000 },  0 },
    { "hollow saint",      { 38, 25, 30, 45, 35, 55, 55 }, { 33, 38, 20, 0, 25, 10, 80, 10, 20 }, { 9000, 9000, 32767, 12000 }, -1 },
    { "funeral engine",    { 82,  5, 70, 60, 45, 50, 40 }, { 65, 45, 30, 0, 50, 85, 50, 60, 70 }, { 1500, 9000, 25558,  6000 }, -1 },
    { "witchglass",        { 97, 85, 20, 85, 25, 60, 75 }, { 42, 52, 85, 0, 95,  0, 90, 50, 50 }, { 4000, 9000, 32767, 12000 },  1 },
    { "last procession",   { 64, 65, 40, 50, 25, 45, 55 }, { 30, 35, 25, 0, 35, 20, 75, 45, 45 }, { 6000, 9000, 32767, 10000 },  0 },
    { "void mother",       { 24,  0, 65, 35, 50, 80, 80 }, { 22, 28, 60, 0, 40, 30, 99, 65, 65 }, { 9000, 9000, 16383, 14000 }, -1 },
    { "sleepwalker",       { 50, 55, 25, 55, 20, 50, 55 }, { 38, 48, 35, 0, 55,  0, 70, 40, 40 }, { 6500, 9000, 32767, 10000 },  0 },
};

/* A new sound's values (the engine list's "driftmaker"): a slow organ-choir drone with a little of everything. */
static const unsigned char DM_DEFAULT_DATA[9] __attribute__((unused)) = { 32, 40, 20, 0, 40, 0, 65, 30, 30 };

#endif
