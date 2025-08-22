.section .text.init
.global _start

_start:
    # Set up stack pointer  
    li sp, 0x100000  # 1MB memory, stack at top
    
    # Clear BSS section
    la t0, __bss_start
    la t1, __bss_end
clear_bss:
    beq t0, t1, bss_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j clear_bss
bss_done:
    
    # Call main
    call main
    
    # Exit with return value
    mv a0, a0  # main's return value is already in a0
    li a7, 93  # SYS_EXIT
    ecall

.weak __bss_start
.weak __bss_end
__bss_start:
__bss_end: