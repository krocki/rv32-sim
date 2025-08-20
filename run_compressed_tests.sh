#!/bin/bash

# Script to run RISC-V tests with compressed instruction support

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
RISCV_TESTS_DIR="./riscv-tests"
SIMULATOR="./rv32imac"
TEMP_DIR="./test_temp"
CROSS_COMPILE="riscv64-unknown-elf-"

# Create temp directory
mkdir -p "$TEMP_DIR"

# Compile simulator if needed
if [ ! -f "$SIMULATOR" ] || [ "rv32imac.cc" -nt "$SIMULATOR" ]; then
    echo "Compiling RV32IMAC simulator..."
    g++ -o rv32imac rv32imac.cc
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Failed to compile simulator${NC}"
        exit 1
    fi
fi

# Function to run a single test
run_test() {
    local test_name="$1"
    local test_file="$2"
    local march="$3"
    
    if [ ! -f "$test_file" ]; then
        echo -e "${RED}SKIP${NC} $test_name (file not found)"
        return 2
    fi
    
    echo -n "Testing $test_name... "
    
    # Compile the test
    "${CROSS_COMPILE}gcc" -march="$march" -mabi=ilp32 \
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
    
    # Run the test with timeout
    perl -e 'alarm 10; exec @ARGV' "$SIMULATOR" "$TEMP_DIR/$test_name.bin" > "$TEMP_DIR/$test_name.log" 2>&1
    local exit_code=$?
    
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

echo "Running RISC-V Compressed Instruction Tests"
echo "============================================"

# Counters
passed=0
failed=0
skipped=0

# Test RVC (compressed instructions)
echo -e "\nTesting Compressed Instructions (C extension):"
run_test "rvc" "$RISCV_TESTS_DIR/isa/rv32uc/rvc.S" "rv32imac_zicsr"
case $? in
    0) ((passed++)) ;;
    1) ((failed++)) ;;
    2) ((skipped++)) ;;
esac

# Test some basic instructions with compressed enabled
echo -e "\nTesting Basic Instructions with Compressed Support:"
for test in add addi sub and or xor sll srl sra lui auipc jal jalr beq bne blt bge lw sw; do
    run_test "$test" "$RISCV_TESTS_DIR/isa/rv32ui/$test.S" "rv32imac_zicsr"
    case $? in
        0) ((passed++)) ;;
        1) ((failed++)) ;;
        2) ((skipped++)) ;;
    esac
done

# Test M extension with compressed
echo -e "\nTesting M Extension with Compressed Support:"
for test in mul mulh mulhsu mulhu div divu rem remu; do
    run_test "$test" "$RISCV_TESTS_DIR/isa/rv32um/$test.S" "rv32imac_zicsr"
    case $? in
        0) ((passed++)) ;;
        1) ((failed++)) ;;
        2) ((skipped++)) ;;
    esac
done

# Test A extension with compressed
echo -e "\nTesting A Extension with Compressed Support:"
for test in amoadd_w amoand_w amomax_w amomaxu_w amomin_w amominu_w amoor_w amoswap_w amoxor_w lrsc; do
    run_test "$test" "$RISCV_TESTS_DIR/isa/rv32ua/$test.S" "rv32imac_zicsr"
    case $? in
        0) ((passed++)) ;;
        1) ((failed++)) ;;
        2) ((skipped++)) ;;
    esac
done

# Summary
echo "============================================"
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