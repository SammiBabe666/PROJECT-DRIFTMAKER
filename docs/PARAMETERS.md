# DRIFTMAKER's parameters

Push an encoder to choose its page. The four encoders then turn the page's four values, from 0 to 99. Hold shift for
a fine step (one instead of two per detent). The top left of the screen shows the value you just turned, by name.

| page (push) | blue | green | white | orange |
|---|---|---|---|---|
| **CORE** (blue) | character | tone | detune | filter |
| **MOTION** (green) | rate 1 | depth 1 | rate 2 | depth 2 |
| **TEXTURE** (white) | grain | freeze | density | degrade |
| **SPACE** (orange) | reverb | delay | feedback | mix |

## Where the values are kept

- **Seven of them are OS knobs**: character, tone, detune and filter (knobs 1-4), depth 1, depth 2 and mix (knobs
  6-8). The OS saves them in the sound, and the OP-1's LFO can move them. On CORE the OS turns them itself, exactly as
  with any engine.
- **Nine are kept in the sound's engine data**: rate 1, rate 2, grain, freeze, density, degrade, reverb, delay and
  feedback. They're saved with the sound too. A sound that has no DRIFTMAKER data yet (a fresh preset load) takes these
  values from the preset whose name it carries, or from the defaults.
- Knob 5 holds the engine's own marker, which is how a saved sound is recognised. It isn't a parameter.

## CORE: the sound itself

| value | what it does |
|---|---|
| **character** | Morphs through eight sources: hollow (a stopped pipe), organ, choir ah, choir oo, bowed, gurdy, reed and bell. The screen names the nearest one. Between them the waves blend. Past about 5.5 sources the second oscillator glides toward an inharmonic 2.756 times its pitch, which gives the bell zone its metallic sound. Choirs and strings carry a little breath noise. |
| **tone** | The layer voice, a third oscillator. Below the middle it's a sub octave, getting louder toward 0. Around 50 it's off (unison). Above that it's a fifth, then from about 75 a twelfth (an octave and a fifth). |
| **detune** | Spreads the unison pair up to 25 cents apart, and scales each oscillator's slow random pitch drift. Some drift stays even at 0. |
| **filter** | A resonant low-pass from 30 Hz up (capped at 8 kHz), following the keyboard by half. The darker it is, the more it resonates. |

## MOTION: two slow modulators per note

Each note has its own two modulators, so chords don't move in step.

| value | what it does |
|---|---|
| **rate 1** | Mod 1's speed, 0.02 to 5 Hz (the screen shows Hz). Mod 1 is 70 % sine and 30 % random wander. |
| **depth 1** | Mod 1 bends the pitch up to ±18 cents and sweeps the filter up to ±24 semitones. |
| **rate 2** | Mod 2's speed, 0.01 to 2 Hz. |
| **depth 2** | Mod 2 swells the level, moves the character by up to ±1.4 sources, and carries the grains through the captured sound. It also sets how far each note **blooms**: over its first twelve seconds the filter opens and the character moves on. |

The two traces at the bottom left of the screen are the newest note's mod 1 (blue) and mod 2 (green).

## TEXTURE: grains, freeze and wear

These work on the sum of all voices.

| value | what it does |
|---|---|
| **grain** | How much of the grain cloud you hear. Four Hann-windowed grains replay the last 1.49 seconds. |
| **freeze** | 0 is off. Any value above 0 stops the capture, so the grains keep replaying what was there, even after you let go of the keys. The value sets how far the frozen cloud replaces the live sound (at 99 the live sound is down to a tenth). A star appears on the wheel while frozen. |
| **density** | Grain length and overlap: from long, sparse grains (186 ms) to short, dense ones (35 ms). Low values add grains an octave down, high values an octave up (a shimmer). |
| **degrade** | Tape wear: wow, saturation, sample-and-hold, lost bits, darkening, and hiss that follows the sound and fades with it. |

## SPACE: room and repeats

| value | what it does |
|---|---|
| **reverb** | A dark reverb (four diffusers into a four-line feedback network). Its send and its length both grow with the value. |
| **delay** | The delay time, 60 ms to 0.72 s. 0 turns the delay off. |
| **feedback** | The delay's repeats, up to 0.92, darkened and soft-limited in the loop so they never run away. |
| **mix** | How much of the reverb and delay you hear. The dry sound dips by up to half as the mix rises. |

## The OS's own pages

T2 (envelope) and T4 (LFO) are the OS's and work as with any engine. Each preset sets its own envelope and octave.
