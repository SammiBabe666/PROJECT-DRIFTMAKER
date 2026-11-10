#!/usr/bin/env python3
"""DRIFTMAKER's tables: every curve the engine reads, made once here so the audio code is integer-only.
    python3 tools/make_tables.py > engine/sammi.driftmaker/dm_tables.h
Waves: 8 single-cycle sources x 3 band limits x 256 points (Q15). The rest: exp2, the filter's cutoff, the LFO
rates, the grain window, the delay times, the reverb decays. Standard library only. GPL-3.0-or-later."""
import math

FS_VOICE = 88200.0      # an engine voice renders at 88.2 kHz
FS_BLOCK = 44100.0      # the block (the voices' sum) runs at 44.1 kHz
BLOCKS_PER_S = FS_BLOCK / 64
N = 256
MIPS = (48, 16, 6)      # harmonics kept: below ~900 Hz, ~2.7 kHz, above

def norm(vals):
    m = max(abs(v) for v in vals) or 1.0
    return [int(round(v / m * 30000)) for v in vals]

def additive(amps, h_max, phases=None):
    out = []
    for i in range(N):
        t = 2 * math.pi * i / N
        s = 0.0
        for h, a in amps.items():
            if h <= h_max and a:
                s += a * math.sin(h * t + (phases.get(h, 0) if phases else 0))
        out.append(s)
    return out

def formant(h, centres, width):
    return sum(g * math.exp(-((h - c) / width) ** 2) for c, g in centres)

# The eight sources, left to right on the character knob.
def w_hollow(h):  return {1: 1.0, 2: 0.35, 3: 0.08, 4: 0.12}.get(h, 0)          # stopped flute pipe
def w_organ(h):   return {1: 1.0, 2: 0.7, 3: 0.55, 4: 0.5, 6: 0.35, 8: 0.3, 10: 0.12, 12: 0.15, 16: 0.1}.get(h, 0)
def w_ah(h):      return (1.0 / h) * (0.25 + formant(h, [(3.5, 1.4), (6, 0.9), (11, 0.35)], 1.6))   # choir "ah"
def w_oo(h):      return (1.0 / h) * (0.35 + formant(h, [(1.5, 1.2), (3.5, 0.5)], 1.0))            # choir "oo"
def w_bowed(h):   return (1.0 / h) * (1.0 if h % 2 else 0.8) * math.exp(-h / 30)                   # bowed string
def w_gurdy(h):   return (1.0 / h ** 0.75) * (1 + 1.5 * formant(h, [(6, 1.0), (13, 0.6)], 2.0))    # buzzing chanterelle
def w_reed(h):    return (1.0 / h) if h % 2 else 0.06 / h                                         # regal reed
def w_bell(h):    return {1: 0.6, 2: 1.0, 3: 0.45, 4: 0.7, 5: 0.2, 6: 0.5, 8: 0.35, 9: 0.15, 11: 0.25, 13: 0.12}.get(h, 0)
WAVES = [w_hollow, w_organ, w_ah, w_oo, w_bowed, w_gurdy, w_reed, w_bell]
NAMES = ["hollow", "organ", "choir ah", "choir oo", "bowed", "gurdy", "reed", "bell"]

def c_array(name, ctype, vals, per=12):
    s = [f"static const {ctype} {name}[{len(vals)}] __attribute__((unused)) = {{"]
    for i in range(0, len(vals), per):
        s.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    s.append("};")
    return "\n".join(s)

def main():
    out = ["/* Made by tools/make_tables.py: do not edit. DRIFTMAKER's curves (integer only in the audio). */",
           "#ifndef DM_TABLES_H", "#define DM_TABLES_H", "",
           f"#define DM_WAVES {len(WAVES)}", f"#define DM_MIPS {len(MIPS)}", f"#define DM_WAVE_LEN {N}", ""]
    waves = []
    for w in WAVES:
        rng = 0x5EED
        ph = {}
        for h in range(1, 49):            # fixed, gentle phases: lower crest factor than all-sine
            rng = (rng * 1103515245 + 12345) & 0x7FFFFFFF
            ph[h] = (rng / 0x7FFFFFFF) * 2 * math.pi * 0.5
        amps = {h: w(h) for h in range(1, 49)}
        full = norm(additive(amps, MIPS[0], ph))
        for m in MIPS:
            vals = additive(amps, m, ph)
            scale = max(abs(v) for v in additive(amps, MIPS[0], ph))
            waves += [int(round(v / scale * 30000)) for v in vals]
    out.append("/* [wave][mip][point]: " + ", ".join(NAMES) + "; mips keep " + ", ".join(map(str, MIPS)) + " harmonics */")
    out.append(c_array("DM_WAVE", "short", waves, 16))
    # exp2 over one octave, 256 steps, Q16 minus 65536 (fits an unsigned short)
    out.append(c_array("DM_EXP2", "unsigned short", [min(65535, int(round(65536 * 2 ** (i / 256)) - 65536)) for i in range(257)]))
    # sine, 256 points, Q15
    out.append(c_array("DM_SINE", "short", [int(round(32767 * math.sin(2 * math.pi * i / 256))) for i in range(257)]))
    # Chamberlin SVF f = 2 sin(pi fc / fs) at 44.1 kHz (the voice computes at half its output rate), Q15 (unsigned),
    # one entry a semitone from 30 Hz, capped at 8 kHz where the filter stays stable at its strongest resonance
    cut = []
    for i in range(128):
        fc = min(8000.0, 30.0 * 2 ** (i / 12))
        cut.append(int(round(2 * math.sin(math.pi * fc / FS_BLOCK) * 32768)))
    out.append("/* svf coefficient (44.1 kHz) a semitone a step from 30 Hz (index 0) to 8 kHz */")
    out.append(c_array("DM_CUTOFF", "unsigned short", cut))
    # LFO phase steps per audio block (2^32 a cycle): mod 1 0.02-5 Hz, mod 2 0.01-2 Hz, exponential over 0-99
    def rates(lo, hi):
        return [int(round((lo * (hi / lo) ** (i / 99)) / BLOCKS_PER_S * 2 ** 32)) for i in range(100)]
    out.append("/* mod 1: 0.02-5 Hz; mod 2: 0.01-2 Hz (phase step a block) */")
    out.append(c_array("DM_RATE1", "unsigned", rates(0.02, 5.0), 8))
    out.append(c_array("DM_RATE2", "unsigned", rates(0.01, 2.0), 8))
    # Hann window, 256 points + guard, Q15
    out.append(c_array("DM_HANN", "short", [int(round(32767 * (0.5 - 0.5 * math.cos(2 * math.pi * i / 256)))) for i in range(257)]))
    # delay time in samples at 44.1 kHz: 0-99 -> 60 ms .. 720 ms (exp), the line holds 32768
    out.append(c_array("DM_DELAY", "unsigned short", [int(44100 * 0.06 * (12.0 ** (i / 99))) for i in range(100)], 10))
    # reverb loop gain, Q15: 0-99 -> 0.62 .. 0.965
    out.append(c_array("DM_DECAY", "short", [int(32767 * (0.62 + 0.345 * (i / 99) ** 0.7)) for i in range(100)], 10))
    out.append("\n#endif")
    print("\n".join(out))

if __name__ == "__main__":
    main()
