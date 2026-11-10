#!/bin/bash
# DRIFTMAKER in the Blackfin simulator: the cycle figures, and its audio against the computer's build, byte for byte.
#   . ~/mnt/Documents/op1-toolchain/env.sh     (the shared toolchain: bfin-elf-gcc 13.3.0, bfin-elf-run)
#   bash sim/run.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
KIT=${OPUS_KIT:-$ROOT/third_party/opus-plugin-kit}
RUN=${BFIN_RUN:?set BFIN_RUN to bfin-elf-run (the shared toolchain env.sh)}
ENG=$ROOT/engine/sammi.driftmaker
B=$ROOT/build/sim
mkdir -p "$B"
F="-mcpu=bf524-any -O2 -fno-indirect-inlining -ffreestanding -fno-builtin -Wall -Wextra -Werror -fno-common"   # the kit's
M="--environment virtual --model bf524 --memory-region 0x0,0x4000000 --memory-region 0xFF800000,0x8000"
SRC="$ENG/driftmaker.c $ENG/dm_dsp.c $ENG/dm_fx.c $ENG/dm_page.c $ROOT/host/stubs.c $ROOT/host/emul.c $ROOT/sim/main_sim.c"
bfin-elf-gcc $F -c "$ROOT/sim/start.S" -o "$B/start.o"
for mode in dump cycles; do
    D=""; [ $mode = cycles ] && D=-DSIM_CYCLES
    objs=""
    for s in $SRC; do
        o="$B/$(basename "$s" .c)_$mode.o"
        bfin-elf-gcc $F $D -I "$KIT/sdk" -I "$ENG" -I "$ROOT/host" -c "$s" -o "$o"
        objs="$objs $o"
    done
    bfin-elf-gcc $F -nostdlib -T "$ROOT/sim/sim.ld" "$B/start.o" $objs -lgcc -o "$B/dm_$mode.elf"
done
"$RUN" $M "$B/dm_cycles.elf" | tee "$B/cycles.txt"
"$RUN" $M "$B/dm_dump.elf" > "$B/audio_bfin.raw"
if [ -f "$B/audio_host.raw" ]; then
    cmp "$B/audio_bfin.raw" "$B/audio_host.raw" && echo "audio: the Blackfin build == the computer's build, bit for bit ($(wc -c < "$B/audio_bfin.raw") bytes)"
else
    echo "no audio_host.raw: make -C host simdump on the computer, copy build/sim/audio_host.raw here"
fi
