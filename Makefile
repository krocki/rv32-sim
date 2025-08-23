# RV32IMA Emulator Makefile

CC = gcc
CXX = g++
RISCV_CC = riscv64-unknown-elf-gcc
RISCV_OBJCOPY = riscv64-unknown-elf-objcopy
RISCV_AS = riscv64-unknown-elf-as
CFLAGS = -O3 -Wall
SDL_FLAGS = -lSDL2

# Default target
all: emulator emulator-sdl hello doom

# Basic console emulator (your original implementation)
emulator: rv32ima.cc
	$(CXX) $(CFLAGS) -o rv32ima rv32ima.cc

# SDL-enabled emulator for DOOM/graphics (modular version)
emulator-sdl: rv32ima_modular.cc memory_subsystem.h memory_subsystem_sdl.h
	$(CXX) $(CFLAGS) -o rv32ima_sdl rv32ima_modular.cc $(SDL_FLAGS)

# Build hello world example
hello: hello.S
	$(RISCV_AS) -march=rv32ima -o hello.o hello.S
	$(RISCV_OBJCOPY) -O binary hello.o hello.bin

# Build DOOM for RISC-V
doom: src_doom/riscv/doom-riscv.bin

src_doom/riscv/doom-riscv.bin: src_doom/riscv/doom-riscv.elf
	$(RISCV_OBJCOPY) -O binary src_doom/riscv/doom-riscv.elf src_doom/riscv/doom-riscv.bin

src_doom/riscv/doom-riscv.elf: src_doom/riscv/wad_real.o
	cd src_doom/riscv && \
	$(RISCV_CC) -Wall -g -march=rv32im -mabi=ilp32 -ffreestanding -flto -nostartfiles \
		-fomit-frame-pointer -Wl,--gc-section --specs=nano.specs \
		-I.. -DNORMALUNIX -Wl,-Bstatic,-T,riscv.lds -o doom-riscv.elf \
		../am_map.c ../d_items.c ../d_net.c ../doomdef.c ../doomstat.c ../dstrings.c \
		../f_finale.c ../f_wipe.c ../g_game.c ../hu_lib.c ../hu_stuff.c ../info.c \
		../m_argv.c ../m_bbox.c ../m_cheat.c ../m_fixed.c ../m_menu.c ../m_misc.c \
		../m_random.c ../m_swap.c ../p_ceilng.c ../p_doors.c ../p_enemy.c ../p_floor.c \
		../p_inter.c ../p_lights.c ../p_map.c ../p_maputl.c ../p_mobj.c ../p_plats.c \
		../p_pspr.c ../p_saveg.c ../p_setup.c ../p_sight.c ../p_spec.c ../p_switch.c \
		../p_telept.c ../p_tick.c ../p_user.c ../r_bsp.c ../r_data.c ../r_draw.c \
		../r_main.c ../r_plane.c ../r_segs.c ../r_sky.c ../r_things.c ../sounds.c \
		../st_lib.c ../st_stuff.c ../tables.c ../v_video.c ../wi_stuff.c ../w_wad.c \
		../z_zone.c d_main.c i_main.c i_net.c i_sound.c i_system.c i_video.c s_sound.c \
		start.S console.c libc_backend.c mini-printf.c wad_real.o

src_doom/riscv/wad_real.o: src_doom/riscv/doom1_real.wad
	cd src_doom/riscv && $(RISCV_OBJCOPY) -I binary -O elf32-littleriscv -B riscv --rename-section .data=.rodata,alloc,load,readonly,data,contents doom1_real.wad wad_real.o

# Run hello world
run-hello: emulator hello
	./rv32ima hello.bin

# Run DOOM using your rv32ima with SDL memory subsystem
run-doom: emulator-sdl doom
	./rv32ima_sdl --sdl -f src_doom/riscv/doom-riscv.bin

# Run tests
test: emulator
	./run_tests.sh

# Clean build artifacts
clean:
	rm -f rv32ima rv32ima_sdl *.o hello.bin
	rm -f src_doom/riscv/*.bin src_doom/riscv/*.elf src_doom/riscv/*.o

.PHONY: all emulator emulator-sdl hello doom run-hello run-doom test clean
