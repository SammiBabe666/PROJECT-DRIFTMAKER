# DRIFTMAKER on the computer: make -C host test | make -C host dm_host
KIT ?= ../third_party/opus-plugin-kit
ENG = ../engine/sammi.driftmaker
CC ?= cc
CFLAGS = -O2 -std=gnu11 -Wall -Wextra -Werror -fno-strict-aliasing -DDM_HOST -I$(KIT)/sdk -I$(ENG) -I.
SRC = $(ENG)/driftmaker.c $(ENG)/dm_dsp.c $(ENG)/dm_fx.c $(ENG)/dm_page.c stubs.c emul.c main_host.c
dm_host: $(SRC) $(wildcard $(ENG)/*.h) emul.h
	$(CC) $(CFLAGS) $(SRC) -lm -o $@
dm_host_san: $(SRC)
	$(CC) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $(SRC) -lm -o $@
test: dm_host dm_host_san
	./dm_host_san test
.PHONY: test
simdump: ../sim/main_sim.c $(SRC)
	$(CC) $(CFLAGS) $(ENG)/driftmaker.c $(ENG)/dm_dsp.c $(ENG)/dm_fx.c $(ENG)/dm_page.c stubs.c emul.c ../sim/main_sim.c -o dm_simdump
	mkdir -p ../build/sim && ./dm_simdump > ../build/sim/audio_host.raw
.PHONY: simdump
