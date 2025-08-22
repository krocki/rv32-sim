.section .text
.global _start

_start:
    # Test negative addition
    li      a1, -1       # Load -1
    li      a2, -1       # Load -1  
    add     a4, a1, a2   # -1 + -1 = -2
    
    li      t2, -2       # Expected result
    bne     a4, t2, fail
    
    # Success
    li      a7, 93
    li      a0, 0
    ecall
    
fail:
    li      a7, 93
    li      a0, 1
    ecall