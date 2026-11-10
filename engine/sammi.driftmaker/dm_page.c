/* DRIFTMAKER: the T1 page. A hurdy-gurdy, the instrument of Drone Maker's artwork, drawn as thin OP-1 line art on
 * the left; the shown page's four values on the right in the encoders' colours.
 *
 * Still (drawn once and kept by the core, opus_page_still): the body, the key box, the peg box and scroll, the wheel
 * cover, the sound holes, the tailpiece. Moving (every frame, cheap lines and dots): the wheel's spokes and the crank,
 * turning while it sounds (stopped and marked by a frost star while frozen); the three strings, shaking with the
 * level; the key box's twelve tangents, lit by the held notes; the two modulators' traces along the bottom; the
 * values. A value just turned is framed, and its name and value show top left for a second.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
#include "opus_plugin.h"
#include "op1fw.h"
#include "dm_dsp.h"
#include "dm_page.h"
#include "dm_tables.h"
#include "dm_presets.h"

static struct opl_self *me;

#define U(v) ((v) * 256)                   /* units to path units */
#define LINE 384                           /* 1.5 pixels */
#define FINE 256

static const char *const GROUP[4] = { "core", "motion", "texture", "space" };
static const char *const NAME[4][4] = {
    { "character", "tone", "detune", "filter" },
    { "rate 1", "depth 1", "rate 2", "depth 2" },
    { "grain", "freeze", "density", "degrade" },
    { "reverb", "delay", "feedback", "mix" },
};
static const char *const SOURCE[8] = { "hollow", "organ", "choir ah", "choir oo", "bowed", "gurdy", "reed", "bell" };

#define HIST 32
static short h1[HIST], h2[HIST];
static int hpos;
static int crank, strum;

void dm_page_init(struct opl_self *self)
{
    me = self;
}

/* ---------------------------------------------------------------- the still art */

static void art_still(void)
{
    /* the body: a pear-shaped sound box, its broad end at the tail (right) */
    opus_page_path();
    opus_page_move(U(70), U(68));
    opus_page_curve_to(U(98), U(52), U(150), U(48), U(176), U(60));
    opus_page_curve_to(U(194), U(70), U(194), U(104), U(176), U(114));
    opus_page_curve_to(U(150), U(126), U(98), U(122), U(70), U(108));
    opus_page_curve_to(U(60), U(100), U(60), U(76), U(70), U(68));
    opus_page_close();
    opus_page_stroke(LINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the key box along the neck, over the body (filled black first so the body's line hides under it) */
    opus_page_path();
    opus_page_move(U(28), U(58));
    opus_page_line_to(U(148), U(58));
    opus_page_line_to(U(148), U(72));
    opus_page_line_to(U(28), U(72));
    opus_page_close();
    opus_page_fill_path(OPUS_COLOR_BACK, 255);
    opus_page_stroke(LINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_MITER);
    /* the peg box and its scroll */
    opus_page_path();
    opus_page_move(U(28), U(60));
    opus_page_line_to(U(14), U(56));
    opus_page_curve_to(U(6), U(54), U(6), U(42), U(14), U(42));
    opus_page_curve_to(U(20), U(42), U(21), U(50), U(15), U(50));
    opus_page_move(U(28), U(70));
    opus_page_line_to(U(16), U(66));
    opus_page_line_to(U(14), U(56));
    opus_page_stroke(LINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the pegs */
    opus_page_path();
    opus_page_move(U(18), U(57)); opus_page_line_to(U(16), U(50));
    opus_page_move(U(23), U(59)); opus_page_line_to(U(22), U(51));
    opus_page_move(U(20), U(66)); opus_page_line_to(U(20), U(74));
    opus_page_stroke(FINE, OPUS_COLOR_DIM, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the wheel cover: an arch over the wheel near the tail */
    opus_page_path();
    opus_page_move(U(144), U(86));
    opus_page_curve_to(U(144), U(64), U(176), U(64), U(176), U(86));
    opus_page_stroke(LINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the sound holes: two small crescents */
    opus_page_path();
    opus_page_move(U(98), U(92));
    opus_page_curve_to(U(92), U(98), U(92), U(106), U(98), U(110));
    opus_page_move(U(118), U(96));
    opus_page_curve_to(U(112), U(102), U(112), U(110), U(118), U(113));
    opus_page_stroke(FINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the tailpiece */
    opus_page_path();
    opus_page_move(U(184), U(80));
    opus_page_line_to(U(184), U(96));
    opus_page_stroke(LINE, OPUS_COLOR_TEXT, 255, OPUS_CAP_ROUND, OPUS_JOIN_ROUND);
    /* the scope's baseline */
    opus_page_stroke_line(U(8), U(136), U(196), U(136), FINE, OPUS_COLOR_OFF);
    /* the right column's rule */
    opus_page_stroke_line(U(202), U(8), U(202), U(140), FINE, OPUS_COLOR_OFF);
}

/* ---------------------------------------------------------------- what moves */

static void art_moving(int level, int frozen)
{
    int s, c;
    /* the wheel (centre 160, 86, radius 12): its rim and three spokes */
    opus_page_dot(160, 86, 12, 0, OPUS_COLOR_E1);
    if (!frozen)
        crank = (crank + 18 + (dm_data[DM_D_RATE1] >> 1) + (level >> 9)) & 4095;
    for (int i = 0; i < 3; ++i) {
        int a = (crank + i * 1365) & 4095;
        opus_sincos(a > 2048 ? a - 4096 : a, &s, &c);
        opus_page_stroke_line(U(160), U(86), U(160) + c * 12 * 256 / 4096, U(86) + s * 12 * 256 / 4096, FINE,
                              OPUS_COLOR_E1);
    }
    /* the crank at the tail: its axle, its arm, its handle */
    {
        const int a = crank;
        opus_sincos(a > 2048 ? a - 4096 : a, &s, &c);
        const int ax = U(190), ay = U(88);
        const int hx = ax + c * 12 * 256 / 4096, hy = ay + s * 12 * 256 / 4096;
        opus_page_stroke_line(U(184), U(88), ax, ay, LINE, OPUS_COLOR_E4);
        opus_page_stroke_line(ax, ay, hx, hy, 512, OPUS_COLOR_E4);
        opus_page_dot(hx / 256, hy / 256, 2, 1, OPUS_COLOR_E4);
    }
    /* the strings: from the peg box to the tailpiece, shaking with the level */
    strum = (strum + 300) & 4095;
    const int amp = frozen ? 0 : level * 3 / 32767 + (level > 300);
    for (int k = 0; k < 3; ++k) {
        const int x0 = 28, y0 = 62 + k * 4, x1 = 184, y1 = 82 + k * 6;
        int px = U(x0), py = U(y0);
        for (int i = 1; i <= 8; ++i) {
            const int x = U(x0) + (U(x1 - x0) * i) / 8, y = U(y0) + (U(y1 - y0) * i) / 8;
            int d = 0;
            if (amp && i < 8) {
                opus_sincos(((i * 512 + strum * (k + 2)) & 4095) - 2048, &s, &c);
                d = s * amp * 256 / 4096;
            }
            opus_page_stroke_line(px, py, x, y + d, FINE, OPUS_COLOR_E2);
            px = x;
            py = y + d;
        }
    }
    /* the twelve tangents along the key box, lit by the notes held */
    unsigned lit = 0;
    for (int v = 0; v < 6; ++v)
        if (dm_notes[v] >= 0 && dm_notes[v] < 12)
            lit |= 1u << dm_notes[v];
    for (int i = 0; i < 12; ++i) {
        const int x = 36 + i * 9;
        if (lit >> i & 1)
            opus_page_stroke_line(U(x), U(60), U(x), U(70), 512, OPUS_COLOR_E3);
        else
            opus_page_stroke_line(U(x), U(66), U(x), U(70), FINE, OPUS_COLOR_DIM);
    }
    /* frozen: a frost star over the wheel */
    if (frozen)
        for (int i = 0; i < 3; ++i) {
            opus_sincos(i * 683 - 1024, &s, &c);
            const int dx = c * 9 * 256 / 4096, dy = s * 9 * 256 / 4096;
            opus_page_stroke_line(U(160) - dx, U(86) - dy, U(160) + dx, U(86) + dy, 512, OPUS_COLOR_E3);
        }
    /* the modulators' traces: mod 1 blue above the line, mod 2 green below it */
    static int every;
    if (++every >= 8) {                    /* a point every 8 frames: the traces span about 9 seconds */
        every = 0;
        h1[hpos] = (short)dm_vis.m1;
        h2[hpos] = (short)dm_vis.m2;
        hpos = (hpos + 1) % HIST;
    }
    for (int i = 1; i < HIST; ++i) {
        const int a = (hpos + i - 1) % HIST, b = (hpos + i) % HIST;
        const int xa = U(8) + U(188) * (i - 1) / (HIST - 1), xb = U(8) + U(188) * i / (HIST - 1);
        opus_page_stroke_line(xa, U(129) - h1[a] * 6 / 128, xb, U(129) - h1[b] * 6 / 128, FINE, OPUS_COLOR_E1);
        opus_page_stroke_line(xa, U(140) - h2[a] * 4 / 128, xb, U(140) - h2[b] * 4 / 128, FINE, OPUS_COLOR_E2);
    }
}

/* ---------------------------------------------------------------- the values */

static void value_text(char *b, unsigned n, int group, int enc, const short *k)
{
    const int v = dm_param99(group, enc, k);
    const int p = DM_PAGE_PARAM[group][enc];
    if (group == 0 && enc == 0) {          /* character: the source it is nearest */
        fw_snprintf(b, n, "%s", SOURCE[(v * 7 + 49) / 99]);
    } else if (group == 0 && enc == 1) {   /* tone: the layer voice */
        fw_snprintf(b, n, "%s", v < 47 ? "sub" : v < 53 ? "unison" : v < 74 ? "fifth" : "twelfth");
    } else if (p == 16 + DM_D_RATE1 || p == 16 + DM_D_RATE2) {
        const unsigned r = p == 16 + DM_D_RATE1 ? DM_RATE1[v] : DM_RATE2[v];
        const int c = (int)(((unsigned long long)r * 68906u) >> 32);   /* hundredths of a hertz */
        fw_snprintf(b, n, "%i.%i%i hz", c / 100, (c / 10) % 10, c % 10);
    } else if (p == 16 + DM_D_FREEZE) {
        fw_snprintf(b, n, "%s", v ? "held" : "off");
    } else if (p == 16 + DM_D_DELAY) {
        fw_snprintf(b, n, "%i ms", (int)DM_DELAY[v] * 10 / 441);
    } else {
        fw_snprintf(b, n, "%i", v);
    }
}

void dm_page_t1(void)
{
    short k[8];
    opus_engine_knobs(k);
    const int g = dm_group & 3;
    const int level = dm_vis.level;
    const int frozen = dm_data[DM_D_FREEZE] > 0;

    if (!opus_page_still(me, 0xD1F7A000u)) {
        art_still();
        opus_page_still_done(me);
    }
    art_moving(level, frozen);

    /* the right column: the page's name, four dots for the four pages, the four values */
    opus_page_text(me, 1, 208, 6, 14, OPUS_COLOR_E(g), GROUP[g]);
    for (int i = 0; i < 4; ++i)
        opus_page_dot(283 + i * 9, 13, 3, i == g, i == g ? OPUS_COLOR_E(i) : OPUS_COLOR_DIM);
    static char val[4][16];
    for (int i = 0; i < 4; ++i) {
        const int y = 32 + i * 27;
        value_text(val[i], sizeof val[i], g, i, k);
        opus_page_text(me, 2 + i, 208, y, 9, OPUS_COLOR_E(i), NAME[g][i]);
        opus_page_text_right(me, 6 + i, 314, y, 9, OPUS_COLOR_TEXT, val[i]);
        opus_page_bar(208, y + 14, 314, y + 17, (unsigned)dm_param99(g, i, k), 99, OPUS_COLOR_E(i));
        if (dm_touch == g * 4 + i && dm_touch_age < 30)
            opus_page_frame(205, y - 3, 317, y + 20, OPUS_COLOR_E(i));
    }

    /* top left: the engine's name, or for a second the value just turned */
    static char top[32];
    const int t = dm_touch;
    if (t >= 0 && dm_touch_age < 30) {
        const int tg = t >> 2, te = t & 3;
        char v[16];
        value_text(v, sizeof v, tg, te, k);
        fw_snprintf(top, sizeof top, "%s  %s", NAME[tg][te], v);
        opus_page_text(me, 0, 8, 6, 10, OPUS_COLOR_E(te), top);
    } else {
        opus_page_text(me, 0, 8, 6, 10, OPUS_COLOR_DIM, "driftmaker");
    }
    if (dm_touch_age < 1000)
        ++dm_touch_age;
    /* bottom right: the preset's name when the sound is one */
    const int p = dm_preset_now;
    opus_page_text(me, 10, 208, 137, 7, OPUS_COLOR_DIM, p >= 0 && p < DM_PRESETS ? DM_PRESET[p].name : " ");
}
