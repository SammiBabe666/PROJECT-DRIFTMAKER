# Building DRIFTMAKER

## The `.opmod`

DRIFTMAKER is built with the Opus 2 plugin kit (beta 4, vendored in `third_party/opus-plugin-kit`) and the shared
Blackfin toolchain: **bfin-elf GCC 13.3.0 with binutils 2.38**, C only, no C library, as the kit specifies. This
project doesn't bundle or build a compiler. Point the kit at the one you already have.

On the Mac's Linux shell where the shared toolchain lives:

```sh
. ~/mnt/Documents/op1-toolchain/env.sh          # sets OP1_BFIN_TOOLCHAIN, PATH and BFIN_RUN
bfin-elf-gcc --version | head -1                # bfin-elf-gcc (GCC) 13.3.0
python3 third_party/opus-plugin-kit/plugin_tool.py build engine/sammi.driftmaker --out dist/driftmaker.opmod
python3 third_party/opus-plugin-kit/plugin_tool.py show dist/driftmaker.opmod
```

Anywhere else, set `OP1_BFIN_TOOLCHAIN` to your toolchain's folder (the one holding `bin/bfin-elf-gcc`). The kit's
`plugin_tool.py doctor` checks the setup.

The kit builds with warnings as errors. `docs/evidence/build.log` and `docs/evidence/opmod_show.txt` are the record of
the build in `dist/`.

### The CPU figure

`engine/sammi.driftmaker/cpu.txt` holds the manifest's `cycles N` line. It's currently the simulator's worst block
plus a 30 % margin (160,000). After the device test, replace it with the OP-1's own "audio: worst" reading and rebuild.

## The tables

`engine/sammi.driftmaker/dm_tables.h` is generated. After changing `tools/make_tables.py`:

```sh
python3 tools/make_tables.py > engine/sammi.driftmaker/dm_tables.h
```

## Tests on a computer

Any C compiler with AddressSanitizer and UBSan (gcc or clang):

```sh
make -C host test          # 659 checks, built with the sanitizers
make -C host dm_host       # the host tool: renders, scores, page drawing calls
```

`host/dm_host` commands:

| command | what it does |
|---|---|
| `test` | the full check suite |
| `render TAG SECS HOLD N...` | preset TAG with notes N... held for HOLD s, SECS s in all: raw s16 mono 44.1 kHz on stdout |
| `score FILE` | plays a score (see `demo/make_score.py`), raw audio on stdout |
| `video SCORE AUDIO.raw` | plays a score into `AUDIO.raw` and prints the page's drawing calls every 30th of a second |
| `page TAG GROUP FRAMES [TOUCH [FREEZE]]` | one page's drawing calls after a held chord; pipe into `tools/render_page.py` |

Raw audio becomes a WAV with `python3 tools/wav.py IN.raw OUT.wav`.

## The Blackfin simulator

This builds the engine for the Blackfin, runs it in `bfin-elf-run`, prints cycle counts for four scenarios, and compares
its audio with the computer build's byte for byte:

```sh
. ~/mnt/Documents/op1-toolchain/env.sh
make -C host simdump       # the computer's audio for the same scenario (build/sim/audio_host.raw)
bash sim/run.sh
```

## The demo video

Needs Python 3 with Pillow, and ffmpeg with libx264.

```sh
python3 demo/make_score.py > demo/score.txt
make -C host dm_host
mkdir -p build
host/dm_host video demo/score.txt build/demo.raw > build/frames.txt
python3 demo/make_video.py demo/score.txt build/frames.txt build/demo.raw build/DRIFTMAKER_DEMO.mp4
# the preset tour: the sixteen presets section of the same audio (114.9 s to 244.5 s)
ffmpeg -f s16le -ar 44100 -ac 1 -i build/demo.raw -ss 114.9 -to 244.5 \
       -af "afade=t=in:d=0.5,afade=t=out:st=128.6:d=1" build/driftmaker_preset_tour.wav
```

`python3 demo/make_video.py ... OUT.png --still SECONDS` draws a single frame, for checking a layout.
