# RV32I Simulator

A cycle-accurate RISC-V RV32I simulator with full trace output.

## Features

- Complete RV32I base instruction set implementation
- Cycle-accurate execution
- Detailed trace output showing PC, instruction, disassembly, and register state
- Passes all 38 official RISC-V rv32ui tests

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

The simulator has been validated against the official RISC-V test suite and passes all 38 rv32ui tests.

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
./run_tests.sh add addi sub  # Test multiple instructions
```

3. **Run all RV32UI tests**:
```bash
./run_tests.sh
```

### Test Results

The simulator passes all 38 RV32UI tests:
- ✅ **Arithmetic**: add, addi, sub
- ✅ **Logical**: and, andi, or, ori, xor, xori  
- ✅ **Shifts**: sll, slli, srl, srli, sra, srai
- ✅ **Comparisons**: slt, slti, sltiu, sltu
- ✅ **Branches**: beq, bne, blt, bge, bltu, bgeu
- ✅ **Jumps**: jal, jalr
- ✅ **Loads**: lb, lbu, lh, lhu, lw
- ✅ **Stores**: sb, sh, sw
- ✅ **Upper immediates**: lui, auipc

### Writing Custom Tests

Example test program (`test.S`):
```assembly
_start:
    li x1, 5        # Load 5 into x1
    li x2, 10       # Load 10 into x2
    add x3, x1, x2  # x3 = x1 + x2 = 15
    
    # Exit with success
    li a0, 0        # Exit code 0
    ecall           # System call
```

Compile and run:
```bash
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 -c test.S -o test.o
riscv64-unknown-elf-objcopy -O binary test.o test.bin
./rv32i test.bin
```

## Implementation Details

- **Memory**: 1 MiB flat memory space
- **PC**: Starts at 0x00000000
- **Registers**: 32 general-purpose registers (x0 hardwired to zero)
- **Exit**: Programs terminate via ECALL instruction
