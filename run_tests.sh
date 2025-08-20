#!/bin/bash

# Script to run RISC-V rv32ui tests against your simulator

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
RISCV_TESTS_DIR="./riscv-tests"
SIMULATOR="./rv32imac"
TEST_DIR="$RISCV_TESTS_DIR/isa/rv32ui"
TEMP_DIR="./test_temp"
CROSS_COMPILE="riscv64-unknown-elf-"

# Create temp directory
mkdir -p "$TEMP_DIR"

# Check if simulator exists
if [ ! -f "$SIMULATOR" ]; then
    echo -e "${RED}Error: Simulator not found at $SIMULATOR${NC}"
    echo "Please compile your simulator first: g++ -o rv32imac rv32imac.cc"
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
    
    # Check if it's an M extension test
    if [[ " ${RV32UM_TESTS[@]} " =~ " ${test_name} " ]]; then
        test_file="$RISCV_TESTS_DIR/isa/rv32um/$test_name.S"
    # Check if it's an A extension test
    elif [[ " ${RV32UA_TESTS[@]} " =~ " ${test_name} " ]]; then
        test_file="$RISCV_TESTS_DIR/isa/rv32ua/$test_name.S"
    # Check if it's a C extension test
    elif [[ " ${RV32UC_TESTS[@]} " =~ " ${test_name} " ]]; then
        test_file="$RISCV_TESTS_DIR/isa/rv32uc/$test_name.S"
    fi
    
    if [ ! -f "$test_file" ]; then
        echo -e "${RED}SKIP${NC} $test_name (file not found)"
        return 2
    fi
    
    echo -n "Testing $test_name... "
    
    # Compile the test  
    # Note: We compile without C extension since our simulator doesn't support compressed instructions yet
    # Add zifencei extension for fence.i instruction
    MARCH="rv32ima_zicsr"
    if [ "$test_name" = "fence_i" ]; then
        MARCH="rv32ima_zicsr_zifencei"
    fi
    
    "${CROSS_COMPILE}gcc" -march="$MARCH" -mabi=ilp32 \
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
RV32UI_TESTS=(
    "simple"
    "add"
    "fence_i" 
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

# List of rv32um (M extension) tests
RV32UM_TESTS=(
    "mul"
    "mulh"
    "mulhsu"
    "mulhu"
    "div"
    "divu"
    "rem"
    "remu"
)

# List of rv32ua (A extension) tests
RV32UA_TESTS=(
    "amoadd_w"
    "amoand_w"
    "amomax_w"
    "amomaxu_w"
    "amomin_w"
    "amominu_w"
    "amoor_w"
    "amoswap_w"
    "amoxor_w"
    "lrsc"
)

# List of rv32uc (C extension) tests - commented out for now as full support is WIP
# RV32UC_TESTS=(
#     "rvc"
# )
RV32UC_TESTS=()

# Combine all tests
BASIC_TESTS=("${RV32UI_TESTS[@]}" "${RV32UM_TESTS[@]}" "${RV32UA_TESTS[@]}" "${RV32UC_TESTS[@]}")

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