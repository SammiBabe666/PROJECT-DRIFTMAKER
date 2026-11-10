# Test and performance report

DRIFTMAKER 1.0.0, 10 November 2026.

## Summary

| what | how it was tested | result |
|---|---|---|
| engine, effects, controls, presets, saving | 659 checks on a computer, in a model of the OP-1's voice handling, built with AddressSanitizer and UBSan | **all pass**, no sanitizer findings |
| Blackfin build | the Opus kit's `plugin_tool.py build`, warnings as errors, shared toolchain (GCC 13.3.0, binutils 2.38) | **builds clean**, 39,168 bytes |
| Blackfin code runs the same | the Blackfin build in `bfin-elf-run` vs the computer build, same scenario | **bit-identical** audio (360,448 bytes) |
| CPU | cycle counts in `bfin-elf-run` | worst block **122,545 cycles** (21 % of the block) |
| on an OP-1 | | **not tested yet** |

Nothing here was run on an OP-1. The host model and the simulator test the engine's own code, but not the OP-1's OS,
its real envelope and gains, how its browser takes the presets, or memory timing. The device checklist in
[INSTALL.md](INSTALL.md#first-run-what-to-check) covers those.

## The host tests (`make -C host test`)

The engine's own C files, unchanged, compiled for the computer with a stand-in for the Opus API (`host/stubs.c`) and a
model of how the OS drives an engine (`host/emul.c`: six voices, an ADSR envelope, knob turns at 2 per detent, the
88.2 kHz render brought down to 44.1 kHz). The full output is in `evidence/host_test.txt`.

What the 659 checks cover:

- **Start-up**: the engine registers as `driftmaker`, its struct and voice sizes are what Opus expects (voice state 76
  bytes, the limit is 256), and its memory comes from the audio pool (248,864 bytes in all).
- **The presets in the browser**: 16 entries, folder `driftmaker`, type `iter`, lower-case names of 23 characters or
  fewer.
- **Every preset**: it loads, through the same patch JSON the browser would get, with the engine's marker in knob 5. Its
  seven knobs and nine data values arrive exactly. It sounds with a two-note chord. It doesn't clip (fewer than 64
  full-scale samples in 6 s; all 16 had 0), has no DC offset, and is silent again 25 s after release.
- **The controls**: each encoder push selects its page, and other keys are left to the OS. On CORE the OS turns the
  knob (the engine leaves it alone). On the other three pages, every one of the 12 values moves by the right amount,
  the right store is written (OS knob or the sound's engine data), and the data bytes are correct. Shift gives a fine
  step. Values stop at 0 and 99. Edited data survives the preset-name lookup that runs every 16 blocks.
- **Freeze**: with the keys let go, a frozen texture is still sounding 12 s later (rms 590), and the sound is silent
  again after freeze is turned off.
- **Stress**: six voices with every value at its top (feedback, density, degrade, reverb all at 99) stays stable. 36
  samples in 8 s reached full scale, all through the final soft limiter.
- **The page**: draws on all four pages without error.

Preset levels with a two-note chord (rms, 32767 is full scale):

| | | | |
|---|---|---|---|
| cathedral sleep 3657 | bronze choir 3036 | hollow reliquary 3966 | drowned gurdy 4115 |
| buried bell 3575 | ashen vespers 4340 | wailing room 3332 | blackened strings 4102 |
| mothlight 2829 | chapel of rain 5064 | hollow saint 4326 | funeral engine 5135 |
| witchglass 2573 | last procession 2946 | void mother 7555 | sleepwalker 3111 |

Void mother is the loudest because it sits an octave down with the reverb at 99. Its sustain is already lowered.

## The Blackfin build and the simulator (`bash sim/run.sh`)

Built with the kit's own compiler flags (`-mcpu=bf524-any -O2 ... -Wall -Wextra -Werror`) and run in GDB's Blackfin
instruction-set simulator. The same scenario rendered by the Blackfin build and by the computer build gives the same
audio, byte for byte. That shows the integer code behaves the same on the Blackfin (word sizes, shifts, rounding).
Evidence: `evidence/sim_cycles.txt`, `evidence/build.log`, `evidence/opmod_show.txt`.

## CPU

Cycles per 64-sample audio block. The OP-1's block is 580,525 cycles.

| scenario | mean | worst | voices | effects |
|---|---|---|---|---|
| cathedral sleep, two notes | 55,752 | 58,302 | 24,615 | 33,687 |
| witchglass, six voices | 116,183 | 117,047 | 80,861 | 36,186 |
| six high voices, every value 99, frozen | 117,930 | **122,545** | 78,094 | 44,451 |
| six high voices, every value 99, grains | 117,634 | 119,643 | 78,094 | 41,549 |

The simulator doesn't model memory stalls, so the manifest's figure (`cpu.txt`) is the worst block plus 30 %:
**160,000 cycles, 28 % of the block**. It's marked in `cpu.txt` as an estimate. The device's own reading
(developer mode, the mods list, orange push on DRIFTMAKER's row: "audio: worst N") should replace it.

How it got there: the first build took 270k cycles a block. Splitting the effects into one loop per stage, running the
reverb at half rate, morphing the waves once per block instead of per sample, using 16-bit multiplies, and computing the
voices at 44.1 kHz (then doubling to 88.2 kHz) brought it to 122.5k.

### Memory

- Audio pool: about 243 KB (the effects take 228 KB: a 1.49 s grain ring, a 0.74 s delay line and the reverb's lines).
- Fast memory: the wave tables (a constant lease) and the block's two morphed waves (a scratch lease). Both fall back to
  normal memory when no lease is available.
- Code and data: 31,572 bytes, plus 1,548 bytes zeroed.

## Phase log

| phase | what | result |
|---|---|---|
| 1 | Toolchain check | Passed. The shared toolchain on the Mac (`~/Documents/op1-toolchain`): GCC 13.3.0, binutils 2.38, `bfin-elf-run`. No other compiler, nothing rebuilt. |
| 2 | Opus 2 beta 4 kit | Passed. Vendored from LizardMan245/opus at e47b95f, without its toolchain builder. |
| 3 | Engine and effects | Passed on the host. Fixed along the way: tails stuck at a DC level (rounding in feedback, now toward zero), a filter that never closed (a wrong shift), modulation too slow to hear, uneven preset levels, a bell interval that chirped with modulation, and an integer overflow found by UBSan. |
| 4 | Controls | Passed on the host: pushes, turns on all four pages, shift, range ends, saving. |
| 5 | Blackfin build and CPU | Passed in the simulator: clean build, bit-identical audio, 122.5k worst block (from 270k). |
| 6 | Interface and art | Previews only. They're drawn from the page's own drawing calls on a computer, not on the OP-1's screen. |
| 7 | Presets | Passed on the host: 16 in the browser source, all load, sound and end cleanly. Acceptance by the real browser is untested. |
| 8 | Report | This document. |
| 9 | Docs and demo | Done. The demo's audio is the engine's code on a computer and its screen is a preview, which every frame says. |

## Not tested

- Anything on an OP-1: loading, sound, controls, presets in the real browser, saving and loading sounds, the CPU
  reading, screen drawing, other mods alongside.
- The OS's real envelope, voice gains and voice stealing. The host model approximates them.
- How the OS's LFO moving the CORE knobs sounds in practice.
