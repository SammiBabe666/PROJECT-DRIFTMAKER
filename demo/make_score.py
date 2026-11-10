#!/usr/bin/env python3
"""DRIFTMAKER's demo score: what is played, turned and pushed, and the captions, second by second.
    python3 demo/make_score.py > demo/score.txt
The host runner (host/dm_host video) plays the engine lines (preset, on, off, alloff, key, turn, shift, t); the video
maker (demo/make_video.py) reads the same file for the captions (title, section, caption, preset_card, end_card)."""

B, G, W, O = 59, 60, 61, 62            # the encoder pushes: blue, green, white, orange
L = []
now = 0.0


def at(t):
    global now
    assert t >= now, (t, now)
    now = t
    L.append(f"t {t:.2f}")


def do(*lines):
    L.extend(lines)


PRESETS = [
    ("cathedral sleep", "a deep church organ that slowly breathes", [48, 55, 60]),
    ("bronze choir", "metallic, inharmonic voices from the bell zone", [45, 52, 57]),
    ("hollow reliquary", "a stopped pipe and a sub octave in the dark", [50, 57]),
    ("drowned gurdy", "the hurdy-gurdy's buzz, warped under water", [43, 50, 55]),
    ("buried bell", "a resonant bell with a long, dark tail", [40, 47]),
    ("ashen vespers", "a distant choir in a large stone room", [50, 53, 57]),
    ("wailing room", "dissonant, wandering, never still", [47, 48, 54]),
    ("blackened strings", "dark bowed layers with tape wear", [45, 52, 53]),
    ("mothlight", "high, delicate harmonics shimmering", [52, 59, 64]),
    ("chapel of rain", "a soft drone with grains like rain", [48, 55, 64]),
    ("hollow saint", "a deep, voice-like sustained tone", [50, 57, 60]),
    ("funeral engine", "a distorted industrial reed drone", [41, 48, 49]),
    ("witchglass", "crystalline bells scattered into grains", [53, 60, 64]),
    ("last procession", "a slow medieval march of strings and fifths", [43, 50, 58]),
    ("void mother", "an enormous, dark, evolving atmosphere", [38, 45]),
    ("sleepwalker", "a gentle, hypnotic, slowly shifting pad", [48, 55, 62]),
]


def main():
    # ---- opening: the title over the artwork, a drone fading in
    do("title driftmaker|an atmospheric drone engine for the op-1|opus 2 beta 4")
    do("preset 0")
    at(0.30); do("on 48", "on 55", "on 60")
    at(6.0)
    do("section intro")
    do("caption one held note becomes a whole environment.|eight sources, two slow modulators, grains, freeze and space.")
    at(12.0)
    # ---- the interface tour
    do("section the interface: core")
    do("caption push an encoder for its page. blue: core.|character morphs hollow pipe, organ, choirs, strings, gurdy, reed, bell.")
    do(f"key {B}")
    t = 12.4
    for i in range(28):
        at(t); do("turn 0 3"); t += 0.3
    at(t + 1.0)
    for i in range(14):
        at(now + 0.3); do("turn 0 -5")
    at(now + 0.6)
    do("caption tone adds the layer voice: sub octave, fifth or twelfth.|detune spreads the unison; filter darkens and resonates.")
    for i in range(10):
        at(now + 0.3); do("turn 1 -4")
    for i in range(16):
        at(now + 0.25); do("turn 1 4")
    for i in range(10):
        at(now + 0.3); do("turn 3 -3")
    for i in range(12):
        at(now + 0.3); do("turn 3 3")
    at(now + 1.0)
    # ---- motion
    do("section movement")
    do("caption green: motion. two modulators, 0.02 to 5 hz and 0.01 to 2 hz.|mod 1 bends pitch and sweeps the filter; mod 2 swells and drifts the timbre.")
    do(f"key {G}")
    for i in range(12):
        at(now + 0.3); do("turn 1 4")
    for i in range(8):
        at(now + 0.3); do("turn 0 2")
    for i in range(10):
        at(now + 0.3); do("turn 3 4")
    at(now + 4.0)
    do("caption every note also blooms over its first twelve seconds:|the filter opens and the timbre moves on.")
    do("alloff")
    at(now + 1.5); do("on 43", "on 50")
    at(now + 8.0)
    do("alloff")
    at(now + 1.5)
    # ---- texture: grains and freeze
    do("section grains and freeze")
    do("preset 9")
    do(f"key {W}")
    do("caption white: texture. grains replay the last second and a half,|scattered by density, an octave up or down.")
    at(now + 0.3); do("on 48", "on 55", "on 64")
    for i in range(10):
        at(now + 0.4); do("turn 0 4")
    for i in range(8):
        at(now + 0.4); do("turn 2 3")
    at(now + 4.0)
    do("caption freeze stops the capture: the texture holds,|even after the keys are let go.")
    at(now + 1.0); do("turn 1 30")
    at(now + 1.5); do("alloff")
    at(now + 6.0)
    do("caption degrade adds tape wow, saturation, lost bits and hiss.")
    for i in range(12):
        at(now + 0.3); do("turn 3 5")
    at(now + 4.0)
    for i in range(12):
        at(now + 0.15); do("turn 3 -5")
    do("turn 1 -60")
    at(now + 4.0)
    # ---- space
    do("section space")
    do("preset 4")
    do(f"key {O}")
    do("caption orange: space. a dark reverb, a delay up to 0.72 s with feedback,|and the mix.")
    at(now + 0.3)
    for k in range(4):
        do("on 52"); at(now + 0.35); do("off 52"); at(now + 0.9)
        do("on 59"); at(now + 0.35); do("off 59"); at(now + 0.9)
        if k == 1:
            for i in range(6):
                do("turn 2 4")
        if k == 3:
            for i in range(6):
                do("turn 1 4")
    at(now + 3.0)
    # ---- the sixteen presets
    do("section the sixteen presets")
    do("caption in the op-1's preset browser, folder driftmaker.")
    at(now + 3.0)
    for i, (name, line, notes) in enumerate(PRESETS):
        do(f"preset {i}", f"key {B}", f"preset_card {i + 1}|{name}|{line}")
        at(now + 0.3)
        do(*[f"on {n}" for n in notes])
        at(now + 6.5)
        do("alloff")
        at(now + 1.3)
    # ---- a short piece
    do("section a short piece: ashen vespers")
    do("caption a slow progression, d minor, b flat, f, c; freeze at the end.")
    do("preset 5", f"key {G}")
    chords = [[50, 57, 62], [46, 53, 62], [41, 48, 57], [48, 55, 64], [50, 57, 62]]
    prev = []
    at(now + 0.5)
    for c in chords:
        do(*[f"on {n}" for n in c])
        at(now + 0.6)
        do(*[f"off {n}" for n in prev])
        prev = c
        at(now + 5.0)
    do(f"key {W}", "turn 1 30")
    at(now + 1.0)
    do("alloff")
    at(now + 5.0)
    do("turn 1 -60")
    at(now + 4.0)
    do("end_card driftmaker 1.0.0|opus 2 beta 4 synth engine for the original op-1|inspired by fabula audio's drone maker")
    at(now + 7.0)
    print("# DRIFTMAKER demo score: made by demo/make_score.py")
    print("\n".join(L))


if __name__ == "__main__":
    main()
