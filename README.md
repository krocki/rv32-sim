# RV32IMAC RISC-V Simulator

A complete RISC-V RV32IMAC processor simulator with full instruction trace output and comprehensive test suite support.

## Features

### Supported Instruction Set Extensions

- **RV32I** - Base Integer Instruction Set (32-bit)
- **RV32M** - Standard Extension for Integer Multiplication and Division
- **RV32A** - Standard Extension for Atomic Instructions
- **RV32C** - Standard Extension for Compressed Instructions (16-bit)

### Additional Features

- **Full instruction tracing** with cycle-accurate execution
- **Register state display** after each instruction
- **Comprehensive disassembly** for both 32-bit and 16-bit instructions
- **Memory-mapped I/O** support
- **Atomic operations** with reservation tracking
- **Compressed instruction expansion** with proper encoding

## Instruction Set Support

### RV32I Base Instructions (38 instructions)
- **Arithmetic**: ADD, ADDI, SUB, SLT, SLTI, SLTU, SLTIU
- **Logical**: AND, ANDI, OR, ORI, XOR, XORI
- **Shift**: SLL, SLLI, SRL, SRLI, SRA, SRAI
- **Load/Store**: LB, LH, LW, LBU, LHU, SB, SH, SW
- **Branch**: BEQ, BNE, BLT, BGE, BLTU, BGEU
- **Jump**: JAL, JALR
- **Upper Immediate**: LUI, AUIPC
- **System**: ECALL, EBREAK
- **Fence**: FENCE, FENCE.I

### RV32M Extension (8 instructions)
- **Multiply**: MUL, MULH, MULHSU, MULHU
- **Divide**: DIV, DIVU, REM, REMU

### RV32A Extension (10 instructions)
- **Load-Reserved/Store-Conditional**: LR.W, SC.W
- **Atomic Memory Operations**: 
  - AMOADD.W, AMOAND.W, AMOOR.W, AMOXOR.W
  - AMOSWAP.W, AMOMIN.W, AMOMAX.W
  - AMOMINU.W, AMOMAXU.W

### RV32C Extension (Compressed - 16-bit instructions)
- **Quadrant 0**: C.ADDI4SPN, C.LW, C.SW
- **Quadrant 1**: C.ADDI, C.JAL, C.LI, C.LUI, C.ADDI16SP, C.SRLI, C.SRAI, C.ANDI, C.SUB, C.XOR, C.OR, C.AND, C.J, C.BEQZ, C.BNEZ
- **Quadrant 2**: C.SLLI, C.LWSP, C.JR, C.MV, C.EBREAK, C.JALR, C.ADD, C.SWSP

## Building

```bash
g++ -o rv32imac rv32imac.cc
```

## Usage

### Basic Execution
```bash
./rv32imac program.bin
```

### Example Output
```
[cycle 0] pc=0x00000000 ins=0x00100e13  addi x28,x0,1
x00:0x00000000  x01:0x00000000  x02:0x00000000  x03:0x00000000
x04:0x00000000  x05:0x00000000  x06:0x00000000  x07:0x00000000
...

[cycle 1] pc=0x00000004 ins=0xa091  c.j
x00:0x00000000  x01:0x00000000  x02:0x00000000  x03:0x00000000
...
```

### Preparing RISC-V Programs

Compile RISC-V assembly or C code:
```bash
# Assembly
riscv64-unknown-elf-as -march=rv32imac -mabi=ilp32 -o program.o program.S
riscv64-unknown-elf-objcopy -O binary program.o program.bin

# C code with compressed instructions
riscv64-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -static -nostdlib \
    -T linker.ld -o program.elf program.c
riscv64-unknown-elf-objcopy -O binary program.elf program.bin
```

## Testing

### Official RISC-V Test Suite

Clone and test with the official RISC-V compliance tests:

```bash
# Clone official test suite
git clone https://github.com/riscv-software-src/riscv-tests.git

# Run basic RV32IMA tests (57 tests)
./run_tests.sh

# Run comprehensive tests including compressed instructions (38 tests)  
./run_compressed_tests.sh

# Run all available test suites (83 tests across multiple categories)
./test_all.sh
```

### Test Categories

1. **RV32UI Tests** (38 tests) - Base integer instructions
2. **RV32UM Tests** (8 tests) - Multiply/divide instructions  
3. **RV32UA Tests** (10 tests) - Atomic instructions
4. **RV32UC Tests** (1 test) - Compressed instructions
5. **RV32SI Tests** (19 tests) - Supervisor-mode instructions
6. **RV32MI Tests** (7 tests) - Machine-mode instructions

### Current Test Results
- **Basic Tests**: 57/57 passing (RV32IMA)
- **Compressed Tests**: 38/38 passing (RV32IMAC)  
- **All Tests**: 83/83 passing (Complete test suite)

## Architecture Details

### Memory Layout
- Flat 32-bit address space
- Little-endian byte ordering
- 1MB default memory size
- Memory-mapped execution starting at address 0x0

### Register File
- 32 general-purpose registers (x0-x31)
- x0 hardwired to zero
- Full register state displayed in trace output

### Atomic Operations
- Reservation tracking for LR/SC instructions
- Support for all AMO operations
- Proper memory ordering semantics

### Compressed Instructions
- Automatic detection of 16-bit vs 32-bit instructions
- Real-time expansion to 32-bit equivalents
- Supports all standard RVC instruction encodings

## Implementation Details

### Key Components
- `CPU` struct with complete processor state
- `expand_compressed()` - Expands 16-bit to 32-bit instructions
- `disasm()` - Comprehensive disassembler for both formats
- `step()` - Single instruction execution with full tracing

### Instruction Decoding
- Automatic compressed/uncompressed detection
- Proper immediate field extraction and sign extension
- Complete opcode dispatch for all supported instructions

### Error Handling
- Invalid instruction detection
- Proper exit codes for test integration
- Comprehensive error messages with PC context

## Files

- `rv32imac.cc` - Main simulator implementation
- `rv32i_backup.cc` - Backup of original RV32I-only implementation
- `run_tests.sh` - Basic RV32IMA test runner
- `run_compressed_tests.sh` - Compressed instruction test runner  
- `test_all.sh` - Comprehensive test suite runner
- `simple_link.ld` - Linker script for test programs
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
./configure --prefix=/opt/riscv --with-arch=rv32imac --with-abi=ilp32
make
```

## License

This simulator is provided for educational and research purposes.