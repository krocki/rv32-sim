# RV32-SIM Project Makefile
# Builds both the RISC-V emulator and DOOM

.SUFFIXES:
.PHONY: all clean emulator doom doom-debug help

# Configuration
CROSS_PREFIX = riscv64-unknown-elf-
CC = $(CROSS_PREFIX)gcc
OBJCOPY = $(CROSS_PREFIX)objcopy
SIZE = $(CROSS_PREFIX)size

# Host compiler for emulator
HOST_CC = gcc
HOST_CXX = g++

# DOOM build configuration
DOOM_CFLAGS = -Wall -g -march=rv32im -mabi=ilp32 -ffreestanding -flto -nostartfiles
DOOM_CFLAGS += -fomit-frame-pointer -Wl,--gc-section --specs=nano.specs
DOOM_CFLAGS += -I.. -DNORMALUNIX

# Emulator configuration
EMU_CFLAGS = -O2 -DSDL2
EMU_LIBS = -lSDL2

# Default target
all: help

help:
	@echo "RV32-SIM Build System"
	@echo "Available targets:"
	@echo "  emulator     - Build the RISC-V emulator with SDL support"
	@echo "  doom         - Build DOOM for RISC-V"
	@echo "  doom-debug   - Build DOOM with extended debugging"
	@echo "  run-doom     - Build and run DOOM"
	@echo "  clean        - Clean all build artifacts"
	@echo "  help         - Show this help message"

# Build the RISC-V emulator
emulator: rv32ima_ref_sdl

rv32ima_ref_sdl: rv32ima_ref_sdl.c
	$(HOST_CC) $(EMU_CFLAGS) -o $@ $< $(EMU_LIBS)

# DOOM build targets
doom: src_doom/riscv/doom-riscv.bin

src_doom/riscv/doom-riscv.bin: src_doom/riscv/doom-riscv.elf
	cd src_doom/riscv && $(OBJCOPY) -O binary doom-riscv.elf doom-riscv.bin

src_doom/riscv/doom-riscv.elf: doom-sources
	cd src_doom/riscv && \
	$(CC) $(DOOM_CFLAGS) -Wl,-Bstatic,-T,riscv.lds -o doom-riscv.elf \
		$(DOOM_SOURCES) $(DOOM_ARCH_SOURCES) && \
	$(SIZE) doom-riscv.elf

# WAD object file - using real DOOM1.WAD
src_doom/riscv/wad_real.o: src_doom/riscv/doom1_real.wad
	cd src_doom/riscv && $(OBJCOPY) -I binary -O elf32-littleriscv -B riscv doom1_real.wad wad_real.o

# Define source files
DOOM_SOURCES = \
	../am_map.c ../d_items.c ../d_net.c ../doomdef.c ../doomstat.c ../dstrings.c \
	../f_finale.c ../f_wipe.c ../g_game.c ../hu_lib.c ../hu_stuff.c ../info.c \
	../m_argv.c ../m_bbox.c ../m_cheat.c ../m_fixed.c ../m_menu.c ../m_misc.c \
	../m_random.c ../m_swap.c ../p_ceilng.c ../p_doors.c ../p_enemy.c ../p_floor.c \
	../p_inter.c ../p_lights.c ../p_map.c ../p_maputl.c ../p_mobj.c ../p_plats.c \
	../p_pspr.c ../p_saveg.c ../p_setup.c ../p_sight.c ../p_spec.c ../p_switch.c \
	../p_telept.c ../p_tick.c ../p_user.c ../r_bsp.c ../r_data.c ../r_draw.c \
	../r_main.c ../r_plane.c ../r_segs.c ../r_sky.c ../r_things.c ../sounds.c \
	../st_lib.c ../st_stuff.c ../tables.c ../v_video.c ../wi_stuff.c ../w_wad.c \
	../z_zone.c

DOOM_ARCH_SOURCES = \
	d_main.c i_main.c i_net.c i_sound.c i_system.c i_video.c s_sound.c \
	start.S console.c libc_backend.c mini-printf.c wad_real.o

# Ensure WAD object exists before building
doom-sources: src_doom/riscv/wad_real.o

# Debug version with extended instruction count
doom-debug: doom
	@echo "DOOM built with debugging support"

# Run DOOM
run-doom: doom emulator
	@echo "Running DOOM..."
	./rv32ima_ref_sdl -f src_doom/riscv/doom-riscv.bin -c 500000000

# Continue running DOOM for more progress
continue-doom: doom emulator
	@echo "Running DOOM with extended instruction count..."
	./rv32ima_ref_sdl -f src_doom/riscv/doom-riscv.bin -c 800000000

# Clean build artifacts
clean:
	rm -f rv32ima_ref_sdl
	cd src_doom/riscv && rm -f *.bin *.elf *.o *.hex *.gen.h
	@echo "Clean complete"

# Status target to show current state
status:
	@echo "Build Status:"
	@echo "Emulator: $(shell [ -f rv32ima_ref_sdl ] && echo "Built" || echo "Not built")"
	@echo "DOOM Binary: $(shell [ -f src_doom/riscv/doom-riscv.bin ] && echo "Built" || echo "Not built")"
	@echo "DOOM ELF: $(shell [ -f src_doom/riscv/doom-riscv.elf ] && echo "Built" || echo "Not built")"
	@echo "WAD Object: $(shell [ -f src_doom/riscv/wad.o ] && echo "Built" || echo "Not built")"