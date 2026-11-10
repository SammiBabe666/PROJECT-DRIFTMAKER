# Installing DRIFTMAKER

## You need

- An original OP-1 (not the OP-1 field) with **Opus 2 beta 4** installed. The engine asks for Opus core 2.21 or later.
- `dist/driftmaker.opmod` from this repository.

## Steps

1. Turn the OP-1 on into disk mode and connect it to your computer.
2. Copy `driftmaker.opmod` into the **mods** folder on the OP-1's disk. Leave your other mods where they are.
3. Eject the disk and restart the OP-1.
4. Open Opus's mods list. **DRIFTMAKER** should be listed as loaded.
5. In synth mode, choose **driftmaker** from the engine list like any other engine.
6. The presets are in the synth preset browser, in a folder called **driftmaker**.

To remove it, delete `driftmaker.opmod` from the mods folder and restart.

## First run: what to check

This engine has not been on an OP-1 yet, so the first run is also its first real test. Please check these, and note
what you see and hear:

1. **It loads.** The mods list shows DRIFTMAKER as loaded, with no warning.
2. **It sounds.** Pick the engine and hold a chord. You should hear a slow drone that keeps changing.
3. **The pages.** Push each of the four encoders. The right side of the screen should show CORE, MOTION, TEXTURE or
   SPACE, and turning each encoder should move the value under it. Shift and a turn should give a finer step.
4. **The presets.** Open the preset browser and find the driftmaker folder with sixteen presets. Load a few. Each
   should change the sound and the values on the pages.
5. **Saving.** Change something on TEXTURE or SPACE, save the sound, load another sound, and load yours again. Your
   changes should come back.
6. **Freeze.** On TEXTURE, turn freeze up while holding notes, then let go. The texture should keep sounding until you
   turn freeze back to off.
7. **CPU.** Turn on developer mode, open the mods list and push orange on DRIFTMAKER's row to see **audio: worst N**.
   Play six-note chords with every value turned up (preset *void mother* or *witchglass* with grain, density, degrade,
   reverb and feedback near 99, and freeze on). Note the worst figure. That number goes into
   `engine/sammi.driftmaker/cpu.txt` as `cycles N`, and the `.opmod` is then rebuilt ([BUILD.md](BUILD.md)).
8. **Other mods.** Check your other mods still load alongside it.

If something goes wrong, please note what you did and what you saw (a photo of the screen helps), and open an issue
on the repository.

## If the OP-1 warns about CPU

Opus times every engine block by block. Outside developer mode, if DRIFTMAKER keeps going over its figure, Opus warns
and can switch it off until the next restart. Lower grain, density and reverb, play fewer notes, and report the
"audio: worst" reading from developer mode.
