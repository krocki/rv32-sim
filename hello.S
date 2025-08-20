.section .text
.global _start

_start:
    # Print "Hello, World!\n"
    li a7, 64           # sys_write
    li a0, 1            # stdout
    la a1, msg          # buffer
    li a2, 14           # length
    ecall
    
    # Exit
    li a7, 93           # sys_exit
    li a0, 0            # exit code
    ecall

.section .data
msg:
    .ascii "Hello, World!\n"