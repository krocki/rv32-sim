# Basic ADD test for RV32I simulator
# Tests basic arithmetic operations

_start:
    # Test 1: 0 + 0 = 0
    li x1, 0
    li x2, 0
    add x3, x1, x2
    li x4, 0
    bne x3, x4, fail
    
    # Test 2: 1 + 1 = 2  
    li x1, 1
    li x2, 1
    add x3, x1, x2
    li x4, 2
    bne x3, x4, fail
    
    # Test 3: 3 + 7 = 10
    li x1, 3
    li x2, 7
    add x3, x1, x2
    li x4, 10
    bne x3, x4, fail
    
    # Test 4: Negative numbers
    li x1, -1    # 0xffffffff
    li x2, 1
    add x3, x1, x2
    li x4, 0
    bne x3, x4, fail
    
    # Test 5: ADDI instruction
    li x1, 5
    addi x2, x1, 10
    li x4, 15
    bne x2, x4, fail
    
    # All tests passed - call ecall to exit with success
    li a0, 0      # Exit code 0 (success)
    ecall

fail:
    # Test failed - call ecall to exit with error
    li a0, 1      # Exit code 1 (failure)
    ecall