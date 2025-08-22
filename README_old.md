# RV32IMAC RISC-V Simulator

A complete RISC-V RV32IMAC processor simulator with full instruction trace output and comprehensive test suite support.

## Features

- Complete RV32I base instruction set implementation
- Complete RV32M (multiply/divide) extension implementation
- Cycle-accurate execution
- Detailed trace output showing PC, instruction, disassembly, and register state
- Passes all 46 official RISC-V tests (38 rv32ui + 8 rv32um)

## Building the Simulator

```bash
# Compile the simulator
g++ -o rv32i rv32i.cc

# Or with optimization
g++ -O2 -o rv32i rv32i.cc
```

## Running Programs

```bash
# Run a binary program
./rv32i program.bin

# Example: assemble and run a simple program
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 -c program.S -o program.o
riscv64-unknown-elf-objcopy -O binary program.o program.bin
./rv32i program.bin
```

## Testing with Official RISC-V Tests

The simulator has been validated against the official RISC-V test suite and passes all 46 tests (RV32I + RV32M).

### Prerequisites

#### RISC-V toolchain

```bash
# macOS
brew tap riscv-software-src/riscv
brew install riscv-tools

# Alternative installation
brew install --cc=gcc-10 riscv-tools
```

### Running Tests

1. **Clone the test suite** (already done in this repo):
```bash
git clone https://github.com/riscv-software-src/riscv-tests.git
```

2. **Run individual tests**:
```bash
./run_tests.sh add           # Test ADD instruction
./run_tests.sh mul div       # Test multiply and divide
./run_tests.sh add addi mul  # Test multiple instructions
```

3. **Run all RV32IM tests**:
```bash
./run_tests.sh               # Runs all 46 tests
```

### Test Results

The simulator passes all 46 RV32IM tests:

**RV32I Base Instructions (38 tests):**
- ✅ **Arithmetic**: add, addi, sub
- ✅ **Logical**: and, andi, or, ori, xor, xori  
- ✅ **Shifts**: sll, slli, srl, srli, sra, srai
- ✅ **Comparisons**: slt, slti, sltiu, sltu
- ✅ **Branches**: beq, bne, blt, bge, bltu, bgeu
- ✅ **Jumps**: jal, jalr
- ✅ **Loads**: lb, lbu, lh, lhu, lw
- ✅ **Stores**: sb, sh, sw
- ✅ **Upper immediates**: lui, auipc

**RV32M Extension (8 tests):**
- ✅ **Multiplication**: mul, mulh, mulhsu, mulhu
- ✅ **Division**: div, divu
- ✅ **Remainder**: rem, remu

### Writing Custom Tests

Example test program with M extension (`test.S`):
```assembly
_start:
    li x1, 6        # Load 6 into x1
    li x2, 7        # Load 7 into x2
    mul x3, x1, x2  # x3 = x1 * x2 = 42
    
    li x4, 100      # Load 100 into x4
    li x5, 3        # Load 3 into x5
    div x6, x4, x5  # x6 = x4 / x5 = 33
    rem x7, x4, x5  # x7 = x4 % x5 = 1
    
    # Exit with success
    li a0, 0        # Exit code 0
    ecall           # System call
```

Compile and run:
```bash
riscv64-unknown-elf-as -march=rv32im -mabi=ilp32 -c test.S -o test.o
riscv64-unknown-elf-objcopy -O binary test.o test.bin
./rv32i test.bin
```

## Implementation Details

- **ISA**: RV32IM (32-bit RISC-V with Integer and Multiply/Divide extensions)
- **Memory**: 1 MiB flat memory space
- **PC**: Starts at 0x00000000
- **Registers**: 32 general-purpose registers (x0 hardwired to zero)
- **Exit**: Programs terminate via ECALL instruction
- **M Extension**: Full support for multiplication and division operations with proper handling of edge cases (division by zero, overflow)
