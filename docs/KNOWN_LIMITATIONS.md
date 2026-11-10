# Known limitations

## Still unknown until it's tried on an OP-1

- **It hasn't run on an OP-1.** It was built and tested on a computer and in the Blackfin simulator only
  ([TESTING.md](TESTING.md)). The first device run may show problems the model couldn't.
- **The CPU figure is an estimate.** 160,000 cycles is the simulator's worst block plus 30 %. Real memory timing may be
  better or worse. Until the figure is replaced with a device reading, heavy settings could trip Opus's CPU guard
  outside developer mode.
- **Loudness.** Levels were balanced in a model of the OS's envelope and voice gains. On the device, presets may sit
  louder or quieter than expected relative to each other and to other engines.
- **Preset loading** follows the kit's documented preset-source format. Whether the real browser accepts every field
  as written hasn't been seen yet.
- **The screen.** The page was checked as drawing calls rendered on a computer with a stand-in font. Text sizes,
  spacing and line weights may look different on the OP-1's display.

## By design

- **Nine values aren't OS knobs.** Rate 1, rate 2, grain, freeze, density, degrade, reverb, delay and feedback live in
  the sound's engine data. They're saved with the sound, but the OP-1's LFO can't reach them. Only the seven knob
  values (CORE, depth 1, depth 2, mix) can be modulated by the OS.
- **A sound with no DRIFTMAKER data takes its values by name.** If a sound's name matches a preset and you haven't
  changed any of the nine data values, it uses that preset's values. If it matches none, it uses the defaults.
- **The effects are shared.** Grains, freeze, delay and reverb work on the sum of all voices, not per note.
- **Freeze holds the last 1.49 seconds.** Longer textures aren't captured.
- **The delay is free-running**, 60 ms to 0.72 s. It doesn't follow the tempo.
- **Mono.** The engine renders one channel, like the OS's own engines.
- **Six voices**, as the OS gives them. Voice stealing is the OS's.

## Not included

- **No sample playback.** All sources are synthesised. No sounds, samples or code from Fabula Audio's Drone Maker are
  used.
- **No OP-1 field version.** This targets the original OP-1 with Opus only.
- **No hardware footage.** The demo video's audio is the engine's own code rendered on a computer through a model of
  the OP-1, and its screen is a preview drawn from the page code. Every frame says so.
