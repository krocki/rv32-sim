# Boot code for bare-metal RISC-V

.section .text.init
.global _start

_start:
    # Set up stack pointer
    la sp, __stack_top
    
    # Clear BSS section
    la t0, __bss_start
    la t1, __bss_end
clear_bss:
    bgeu t0, t1, done_clear
    sw zero, 0(t0)
    addi t0, t0, 4
    j clear_bss
done_clear:
    
    # Jump to main
    call main
    
    # Halt if main returns
halt:
    wfi
    j halt