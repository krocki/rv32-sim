# RV32IMA RISC-V Simulator

A simple RISC-V RV32IMA instruction set simulator with optional trace output.

## Features

- **RV32I**: Base integer instruction set (32-bit)
- **RV32M**: Standard extension for integer multiplication and division
- **RV32A**: Standard extension for atomic instructions
- **Quiet by default**: Runs silently for testing and production use
- **Optional trace mode**: Full instruction and register state tracing with `--trace` flag
- **System calls**: Basic Linux syscall support (exit, write)
- **CSR support**: Basic Control and Status Register support

## Building

```bash
g++ -O3 -o rv32ima rv32ima.cc
```

## Usage

### Basic Execution (Quiet Mode)
```bash
./rv32ima program.bin
```

### Trace Mode (Verbose Output)
```bash
./rv32ima --trace program.bin
```

The simulator loads the binary at address 0x0 and starts execution from there.

### Example Output (Trace Mode)

```
[cycle 0] pc=0x00000000 ins=0x00100e13  addi x28,x0,1
x00:0x00000000  x01:0x00000000  x02:0x00000000  x03:0x00000000
x04:0x00000000  x05:0x00000000  x06:0x00000000  x07:0x00000000
...
```

## Hello World Example

1. Create a hello world program (`hello.S`):

```asm
.section .text
.global _start

_start:
    # Print "Hello, World!\n"
    li a7, 64           # sys_write
    li a0, 1            # stdout
    la a1, msg          # buffer
    li a2, 14           # length
    ecall
    
    # Exit
    li a7, 93           # sys_exit
    li a0, 0            # exit code
    ecall

.section .data
msg:
    .ascii "Hello, World!\n"
```

2. Assemble and link:

```bash
riscv64-unknown-elf-as -march=rv32ima -mabi=ilp32 hello.S -o hello.o
riscv64-unknown-elf-ld -m elf32lriscv -Ttext=0x10000 hello.o -o hello.elf
riscv64-unknown-elf-objcopy -O binary hello.elf hello.bin
```

3. Run:

```bash
# Quiet mode - just prints "Hello, World!"
./rv32ima hello.bin

# Trace mode - shows full execution trace
./rv32ima --trace hello.bin
```

## Testing

Run the official RISC-V test suite:

```bash
./run_tests.sh
```

This will compile and run the official RISC-V tests for the RV32IMA instruction set (57 tests).

## Implementation Notes

- The simulator implements a flat memory model with 2MB of RAM
- Quiet mode by default for efficient testing
- Optional trace mode shows PC, instruction, decoded instruction, and all registers
- The x0 register is hardwired to zero
- Atomic operations are simplified (no true atomicity with multiple cores)
- CSR instructions provide basic support for cycle counters

## Supported Instructions

### RV32I Base Integer Instructions (38 instructions)
- **Arithmetic**: ADD, SUB, ADDI
- **Logical**: AND, OR, XOR, ANDI, ORI, XORI  
- **Shifts**: SLL, SRL, SRA, SLLI, SRLI, SRAI
- **Comparison**: SLT, SLTU, SLTI, SLTIU
- **Load/Store**: LB, LH, LW, LBU, LHU, SB, SH, SW
- **Branches**: BEQ, BNE, BLT, BGE, BLTU, BGEU
- **Jumps**: JAL, JALR
- **Upper immediate**: LUI, AUIPC
- **System**: ECALL, EBREAK, FENCE
- **CSR**: CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI

### RV32M Multiply/Divide Extension (8 instructions)
- MUL, MULH, MULHSU, MULHU
- DIV, DIVU, REM, REMU

### RV32A Atomic Extension (11 instructions)
- LR.W, SC.W
- AMOADD.W, AMOSWAP.W, AMOXOR.W, AMOOR.W, AMOAND.W
- AMOMIN.W, AMOMAX.W, AMOMINU.W, AMOMAXU.W

## Performance

The simulator runs efficiently in quiet mode, making it suitable for:
- Automated testing
- Continuous integration
- Performance benchmarking
- Educational purposes

Use trace mode when you need to debug or understand program execution.

## Files

- `rv32ima.cc` - Main simulator implementation
- `run_tests.sh` - Test runner for official RISC-V tests
- `simple_link.ld` - Linker script for test programs
- `hello.S` - Example hello world program
- `riscv-tests/` - Official RISC-V test suite (git submodule)

## Prerequisites

### RISC-V Toolchain

```bash
# macOS with Homebrew
brew tap riscv-software-src/riscv
brew install riscv-tools

# Ubuntu/Debian
sudo apt-get install gcc-riscv64-unknown-elf

# From source
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv --with-arch=rv32ima --with-abi=ilp32
make
```

## License

This simulator is provided for educational and research purposes.