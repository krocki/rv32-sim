# Bare-metal DOOM for RISC-V 32-bit

CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -march=rv32ima -mabi=ilp32 -nostdlib -nostartfiles -ffreestanding
CFLAGS += -O2 -g -Wall
CFLAGS += -DBAREMETAL -DFB_WIDTH=640 -DFB_HEIGHT=480

LDFLAGS = -T link.ld -m elf32lriscv

# Simple DOOM-like demo
SRCS = start.S main.c
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

all: doom.bin

doom.bin: doom.elf
	$(OBJCOPY) -O binary $< $@
	@echo "Built bare-metal DOOM: $@"
	@echo "Size: `stat -f%z $@` bytes"

doom.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.elf *.bin

run: doom.bin
	../rv32ima_ref_sdl -f doom.bin

.PHONY: all clean run