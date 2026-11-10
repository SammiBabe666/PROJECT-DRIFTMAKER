/* The Opus core's and the firmware's calls DRIFTMAKER makes, stubbed for the computer and the Blackfin simulator.
 * Freestanding: no C library, so the same file runs bare-metal in bfin-elf-run. What the stubs do is a model of the
 * documented behaviour (docs/api.md), never the OP-1's: they record the engine and preset source, keep one sound's
 * knobs, data and name, and log page drawing as text lines for tools/render_page.py.
 * Copyright (C) 2026 Sammi. GPL-3.0-or-later. */
/* the compiler's own varargs (the Blackfin toolchain carries no C headers) */
typedef __builtin_va_list va_list;
#define va_start(a, b) __builtin_va_start(a, b)
#define va_arg(a, t) __builtin_va_arg(a, t)
#define va_end(a) __builtin_va_end(a)
#include "opus_plugin.h"
#include "op1fw.h"
#include "emul.h"

struct emul E;

/* ---------------------------------------------------------------- a small snprintf: %i %d %u %s %c %% */
static int fmt(char *d, unsigned n, const char *f, va_list ap)
{
    unsigned o = 0;
#define PUT(ch) do { if (o + 1 < n) d[o] = (ch); ++o; } while (0)
    for (; *f; ++f) {
        if (*f != '%') { PUT(*f); continue; }
        ++f;
        if (*f == 's') { const char *s = va_arg(ap, const char *); while (*s) PUT(*s++); }
        else if (*f == 'c') { PUT((char)va_arg(ap, int)); }
        else if (*f == '%') { PUT('%'); }
        else if (*f == 'i' || *f == 'd' || *f == 'u') {
            char t[12]; int k = 0;
            unsigned v;
            if (*f == 'u') v = va_arg(ap, unsigned);
            else { int x = va_arg(ap, int); if (x < 0) { PUT('-'); v = (unsigned)(-x); } else v = (unsigned)x; }
            do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (k) PUT(t[--k]);
        }
    }
    if (n) d[o < n ? o : n - 1] = 0;
    return (int)o;
#undef PUT
}

int fw_snprintf(char *dst, unsigned int size, const char *f, ...)
{
    va_list ap; va_start(ap, f);
    const int r = fmt(dst, size, f, ap);
    va_end(ap);
    return r;
}

int emul_fmt(char *dst, unsigned size, const char *f, ...)
{
    va_list ap; va_start(ap, f);
    const int r = fmt(dst, size, f, ap);
    va_end(ap);
    return r;
}

int fw_shift_held(void) { return E.shift; }

/* ---------------------------------------------------------------- memory */
static unsigned char pool[1 << 20] __attribute__((aligned(32)));
static unsigned pool_used;
void *opus_alloc(struct opl_self *s, unsigned bytes)
{
    (void)s;
    bytes = (bytes + 31u) & ~31u;
    if (pool_used + bytes > sizeof pool) return 0;
    void *p = pool + pool_used;
    pool_used += bytes;
    E.alloc_bytes = pool_used;
    return p;                               /* pool is static: zeroed */
}
void *opus_alloc_audio(struct opl_self *s, unsigned bytes) { return opus_alloc(s, bytes); }
struct opus_fast_lease *opus_fast_lease(struct opl_self *s, unsigned b, int k, void *slow)
{ (void)s; (void)b; (void)k; (void)slow; return 0; }
void *opus_fast_use(struct opus_fast_lease *l) { (void)l; return 0; }
void opus_log(struct opl_self *s, const char *t) { (void)s; (void)t; }

/* ---------------------------------------------------------------- the engine and the sound */
int opus_engine_add(struct opl_self *s, const struct opus_engine *e)
{
    (void)s;
    E.engine = e;
    return EMUL_MARK;
}
void opus_on_block(struct opl_self *s, void (*fn)(void)) { (void)s; E.on_block = fn; }
int opus_preset_source(struct opl_self *s, const struct opus_preset_source *src) { (void)s; E.source = src; return 0; }
int opus_preset_add(struct opl_self *s, int kind, const char *folder, const char *name, const char *type, unsigned tag)
{
    (void)s; (void)kind;
    if (E.n_entries >= EMUL_ENTRIES) return -2;
    E.entry[E.n_entries].folder = folder;
    E.entry[E.n_entries].name = name;
    E.entry[E.n_entries].type = type;
    E.entry[E.n_entries].tag = tag;
    return E.n_entries++;
}

int opus_engine_knobs(short par[8])
{
    for (int i = 0; i < 8; ++i) par[i] = E.knobs[i];
    return 1;
}
int opus_engine_live(short par[8]) { return opus_engine_knobs(par); }
int opus_engine_knob_set(int k, int value)
{
    if (k < 0 || k > 7 || k == 4) return 0;
    E.knobs[k] = (short)(value < 0 ? 0 : value > 32767 ? 32767 : value);
    ++E.knob_sets;
    return 1;
}
int opus_engine_data(void *buf, int max)
{
    unsigned char *b = buf;
    int n = E.data_len < max ? E.data_len : max;
    for (int i = 0; i < n; ++i) b[i] = E.data[i];
    return n;
}
int opus_engine_data_set(const void *buf, int n)
{
    if (n < 0 || n > 256) return 0;
    const unsigned char *b = buf;
    for (int i = 0; i < n; ++i) E.data[i] = b[i];
    E.data_len = n;
    ++E.data_sets;
    return 1;
}
int opus_engine_sound_name(char *buf, int max)
{
    int n = 0;
    while (E.name[n] && n < max - 1) { buf[n] = E.name[n]; ++n; }
    buf[n] = 0;
    return n;
}

/* ---------------------------------------------------------------- drawing: one text line per call */
static void out(const char *f, ...)
{
    if (!E.draw_out) return;
    char b[160];
    va_list ap; va_start(ap, f);
    const int n = fmt(b, sizeof b, f, ap);
    va_end(ap);
    E.draw_out(b, n < (int)sizeof b ? n : (int)sizeof b - 1);
}
static struct { int x, y, size, align, set; } slot[32];
static void text(int sl, int x, int y, int size, int colour, const char *t, int align)
{
    if (sl < 0 || sl > 31) return;
    if (!slot[sl].set) { slot[sl].x = x; slot[sl].y = y; slot[sl].size = size; slot[sl].align = align; slot[sl].set = 1; }
    out("text %i %i %i %i %i %s\n", slot[sl].x, slot[sl].y, slot[sl].size, slot[sl].align, colour, t);
}
void opus_page_text(struct opl_self *s, int sl, int x, int y, int z, int c, const char *t) { (void)s; text(sl, x, y, z, c, t, 0); }
void opus_page_text_right(struct opl_self *s, int sl, int x, int y, int z, int c, const char *t) { (void)s; text(sl, x, y, z, c, t, 1); }
void opus_page_text_centre(struct opl_self *s, int sl, int x, int y, int z, int c, const char *t) { (void)s; text(sl, x, y, z, c, t, 2); }
void opus_page_fill(int x0, int y0, int x1, int y1, int c) { out("fill %i %i %i %i %i\n", x0, y0, x1, y1, c); }
void opus_page_frame(int x0, int y0, int x1, int y1, int c) { out("frame %i %i %i %i %i\n", x0, y0, x1, y1, c); }
void opus_page_bar(int x0, int y0, int x1, int y1, unsigned v, unsigned m, int c) { out("bar %i %i %i %i %u %u %i\n", x0, y0, x1, y1, v, m, c); }
void opus_page_line(int x0, int y0, int x1, int y1, int c) { out("line %i %i %i %i %i\n", x0 * 256, y0 * 256, x1 * 256, y1 * 256, c); }
void opus_page_stroke_line(int x0, int y0, int x1, int y1, int w, int c) { out("sline %i %i %i %i %i %i\n", x0, y0, x1, y1, w, c); }
void opus_page_dot(int x, int y, int r, int f, int c) { out("dot %i %i %i %i %i\n", x, y, r, f, c); }
void opus_page_path(void) { out("path\n"); }
void opus_page_move(int x, int y) { out("move %i %i\n", x, y); }
void opus_page_line_to(int x, int y) { out("lineto %i %i\n", x, y); }
void opus_page_curve_to(int a, int b, int c, int d, int x, int y) { out("curveto %i %i %i %i %i %i\n", a, b, c, d, x, y); }
void opus_page_close(void) { out("close\n"); }
void opus_page_stroke(int w, int c, int a, int cap, int join) { (void)cap; (void)join; out("stroke %i %i %i\n", w, c, a); }
void opus_page_fill_path(int c, int a) { out("fillpath %i %i\n", c, a); }
int opus_page_still(struct opl_self *s, unsigned key) { (void)s; (void)key; return 0; }
void opus_page_still_done(struct opl_self *s) { (void)s; }

/* opus_sincos: x 4096 of a turn in 1/4096; integer (a quarter-wave table by symmetry from the engine's sine) */
#include "dm_tables.h"
void opus_sincos(int t, int *s, int *c)
{
    const unsigned ph = (unsigned)t << 20;          /* 4096 a turn -> 2^32 */
    const unsigned pc = ph + 0x40000000u;
    *s = DM_SINE[ph >> 24] >> 3;
    *c = DM_SINE[pc >> 24] >> 3;
}
