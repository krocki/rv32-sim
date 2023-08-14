.SUFFIXES:

RISCV=${HOME}/opt/riscv
TARGET=func
CC=${RISCV}/bin/riscv32-unknown-linux-gnu-gcc
AS=${RISCV}/bin/riscv32-unknown-linux-gnu-as
OBJDUMP=${RISCV}/bin/riscv32-unknown-linux-gnu-objdump
CFLAGS=-O0

all: $(TARGET)

%.lst: %.o
	$(OBJDUMP) -d $< > $@

%.o: %.s
	$(AS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET)
