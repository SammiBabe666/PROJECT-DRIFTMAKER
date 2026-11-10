# The sixteen presets

DRIFTMAKER adds its presets to the OP-1's own synth preset browser, in a folder called **driftmaker**, after the OS's
own folders. They're a *preset source* (Opus core 2.15 and later): the engine lists them when Opus starts, and fills
each one in when the browser loads it. No preset files need copying.

A loaded preset is an ordinary OP-1 sound: DRIFTMAKER, its seven knob values, its envelope and its octave. The other
nine values (rate 1, rate 2, grain, freeze, density, degrade, reverb, delay, feedback) aren't OS knobs. On a freshly
loaded preset, DRIFTMAKER recognises the sound's name and uses that preset's values for them. As soon as you change
any of them, they're written into the sound's own engine data and saved with it, under whatever name you give it.

You can hear all sixteen, about eight seconds each, in the demo's preset tour (`driftmaker_preset_tour.wav`, made by
[BUILD.md](BUILD.md#the-demo-video)). Each one plays a chord from `demo/make_score.py`.

## The values

All values are 0-99, as the pages show them. Character and tone show the source and layer voice that the value
gives. Octave is the OS's octave for the sound.

| # | preset | sound | character | tone | detune | filter | rate 1 | depth 1 | rate 2 | depth 2 | grain | density | degrade | reverb | delay | feedback | mix | octave |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | **cathedral sleep** | a deep church organ that slowly breathes | organ 16 | twelfth 82 | 22 | 48 | 32 | 20 | 38 | 45 | 10 | 30 | 0 | 85 | 0 | 0 | 55 | -1 |
| 2 | **bronze choir** | metallic, inharmonic voices from the bell zone | bell 93 | fifth 60 | 40 | 62 | 38 | 30 | 45 | 55 | 35 | 55 | 0 | 80 | 35 | 40 | 60 | +0 |
| 3 | **hollow reliquary** | a stopped pipe and a sub octave in the dark | hollow 4 | sub 20 | 30 | 38 | 28 | 35 | 35 | 50 | 20 | 20 | 15 | 70 | 20 | 30 | 50 | +0 |
| 4 | **drowned gurdy** | the hurdy-gurdy's buzz, warped under water | gurdy 71 | sub 30 | 55 | 55 | 50 | 60 | 40 | 40 | 25 | 40 | 45 | 60 | 30 | 35 | 45 | +0 |
| 5 | **buried bell** | a resonant bell with a long, dark tail | bell 99 | sub 10 | 25 | 45 | 25 | 15 | 30 | 35 | 30 | 15 | 10 | 95 | 45 | 55 | 70 | +0 |
| 6 | **ashen vespers** | a distant choir in a large stone room | choir ah 30 | unison 50 | 45 | 52 | 30 | 25 | 42 | 60 | 40 | 45 | 5 | 90 | 25 | 25 | 65 | +0 |
| 7 | **wailing room** | dissonant, wandering, never still | choir oo 45 | fifth 62 | 85 | 58 | 60 | 75 | 55 | 70 | 45 | 60 | 20 | 75 | 50 | 60 | 60 | +0 |
| 8 | **blackened strings** | dark bowed layers with tape wear | bowed 57 | sub 15 | 50 | 42 | 40 | 30 | 35 | 35 | 15 | 30 | 35 | 55 | 15 | 20 | 40 | -1 |
| 9 | **mothlight** | high, delicate harmonics shimmering | choir oo 40 | twelfth 95 | 35 | 80 | 45 | 40 | 50 | 65 | 70 | 85 | 0 | 85 | 55 | 45 | 70 | +1 |
| 10 | **chapel of rain** | a soft drone with grains like rain | organ 10 | unison 50 | 30 | 40 | 35 | 20 | 45 | 40 | 55 | 70 | 25 | 80 | 40 | 35 | 60 | +0 |
| 11 | **hollow saint** | a deep, voice-like sustained tone | choir oo 38 | sub 25 | 30 | 45 | 33 | 35 | 38 | 55 | 20 | 25 | 10 | 80 | 10 | 20 | 55 | -1 |
| 12 | **funeral engine** | a distorted industrial reed drone | reed 82 | sub 5 | 70 | 60 | 65 | 45 | 45 | 50 | 30 | 50 | 85 | 50 | 60 | 70 | 40 | -1 |
| 13 | **witchglass** | crystalline bells scattered into grains | bell 97 | twelfth 85 | 20 | 85 | 42 | 25 | 52 | 60 | 85 | 95 | 0 | 90 | 50 | 50 | 75 | +1 |
| 14 | **last procession** | a slow medieval march of strings and fifths | gurdy 64 | fifth 65 | 40 | 50 | 30 | 25 | 35 | 45 | 25 | 35 | 20 | 75 | 45 | 45 | 55 | +0 |
| 15 | **void mother** | an enormous, dark, evolving atmosphere | choir ah 24 | sub 0 | 65 | 35 | 22 | 50 | 28 | 80 | 60 | 40 | 30 | 99 | 65 | 65 | 80 | -1 |
| 16 | **sleepwalker** | a gentle, hypnotic, slowly shifting pad | bowed 50 | fifth 55 | 25 | 55 | 38 | 20 | 48 | 50 | 35 | 55 | 0 | 70 | 40 | 40 | 55 | +0 |

Every preset starts with freeze off. The values come from `engine/sammi.driftmaker/dm_presets.h`, which also holds
each preset's envelope (attack, decay, sustain, release). Some sustains are lowered to even out the presets' loudness
(the host tests print each preset's level: [TESTING.md](TESTING.md)).

## Adding or changing a preset

1. Edit `engine/sammi.driftmaker/dm_presets.h`. Names must be lower case letters and spaces, 23 characters at most
   (the OP-1's font draws capitals as icons). The host tests check this.
2. Raise `DM_PRESETS` if you add one.
3. Run `make -C host test`, then rebuild the `.opmod` ([BUILD.md](BUILD.md)).

Note that a saved sound with no DRIFTMAKER data takes the values of the preset whose name it carries. Renaming a
preset changes which saved sounds match it.
