#!/bin/bash

# Script to run RISC-V rv32ui tests against your simulator

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
RISCV_TESTS_DIR="./riscv-tests"
SIMULATOR="./rv32i"
TEST_DIR="$RISCV_TESTS_DIR/isa/rv32ui"
TEMP_DIR="./test_temp"
CROSS_COMPILE="riscv64-unknown-elf-"

# Create temp directory
mkdir -p "$TEMP_DIR"

# Check if simulator exists
if [ ! -f "$SIMULATOR" ]; then
    echo -e "${RED}Error: Simulator not found at $SIMULATOR${NC}"
    echo "Please compile your simulator first: g++ -o rv32i rv32i.cc"
    exit 1
fi

# Check if cross compiler exists
if ! command -v "${CROSS_COMPILE}gcc" &> /dev/null; then
    echo -e "${RED}Error: RISC-V cross compiler not found${NC}"
    echo "Please install riscv64-unknown-elf-gcc toolchain"
    exit 1
fi

# Function to compile and run a single test
run_test() {
    local test_name="$1"
    local test_file="$TEST_DIR/$test_name.S"
    
    if [ ! -f "$test_file" ]; then
        echo -e "${RED}SKIP${NC} $test_name (file not found)"
        return 2
    fi
    
    echo -n "Testing $test_name... "
    
    # Compile the test
    "${CROSS_COMPILE}gcc" -march=rv32i_zicsr -mabi=ilp32 \
        -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles \
        -I"$RISCV_TESTS_DIR/env/p" \
        -I"$RISCV_TESTS_DIR/isa/macros/scalar" \
        -T"./simple_link.ld" \
        "$test_file" -o "$TEMP_DIR/$test_name.elf" 2>/dev/null
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAIL${NC} (compilation failed)"
        return 1
    fi
    
    # Convert to binary
    "${CROSS_COMPILE}objcopy" -O binary "$TEMP_DIR/$test_name.elf" "$TEMP_DIR/$test_name.bin" 2>/dev/null
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAIL${NC} (objcopy failed)"
        return 1
    fi
    
    # Run the test with timeout and capture output
    # Use perl for timeout on macOS (works universally)
    perl -e 'alarm 10; exec @ARGV' "$SIMULATOR" "$TEMP_DIR/$test_name.bin" > "$TEMP_DIR/$test_name.log" 2>&1
    local exit_code=$?
    
    # Check if test passed
    # RISC-V tests use ECALL with specific exit codes
    # - Test passes if it reaches ECALL with a0=0 (exit code 0)
    # - Test fails if it reaches ECALL with a0≠0 or times out
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}PASS${NC}"
        return 0
    elif [ $exit_code -eq 124 ]; then
        echo -e "${RED}FAIL${NC} (timeout)"
        return 1
    else
        echo -e "${RED}FAIL${NC} (exit code: $exit_code)"
        return 1
    fi
}

# List of basic rv32ui tests to run
BASIC_TESTS=(
    "simple"
    "add" 
    "addi"
    "and"
    "andi"
    "auipc"
    "beq"
    "bge"
    "bgeu" 
    "blt"
    "bltu"
    "bne"
    "jal"
    "jalr"
    "lb"
    "lbu"
    "lh"
    "lhu"
    "lui"
    "lw"
    "or"
    "ori"
    "sb"
    "sh"
    "sw"
    "sll"
    "slli"
    "slt"
    "slti"
    "sltiu"
    "sltu"
    "sra"
    "srai"
    "srl"
    "srli"
    "sub"
    "xor"
    "xori"
)

# If arguments provided, run only those tests
if [ $# -gt 0 ]; then
    TESTS_TO_RUN=("$@")
else
    TESTS_TO_RUN=("${BASIC_TESTS[@]}")
fi

echo "Running RISC-V rv32ui tests..."
echo "==============================="

# Counters
passed=0
failed=0
skipped=0

# Run tests
for test in "${TESTS_TO_RUN[@]}"; do
    run_test "$test"
    case $? in
        0) ((passed++)) ;;
        1) ((failed++)) ;;
        2) ((skipped++)) ;;
    esac
done

# Summary
echo "==============================="
echo "Results:"
echo -e "  ${GREEN}Passed: $passed${NC}"
echo -e "  ${RED}Failed: $failed${NC}"
echo -e "  ${YELLOW}Skipped: $skipped${NC}"
echo "  Total: $((passed + failed + skipped))"

# Cleanup
rm -rf "$TEMP_DIR"

# Exit with error if any tests failed
if [ $failed -gt 0 ]; then
    exit 1
else
    exit 0
fi