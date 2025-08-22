/* Linker script for bare-metal RISC-V */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS {
    . = 0x80000000;
    
    .text : {
        *(.text.init)
        *(.text*)
    } > RAM
    
    .rodata : {
        *(.rodata*)
    } > RAM
    
    .data : {
        *(.data*)
    } > RAM
    
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM
    
    . = ALIGN(16);
    __stack_top = 0x83FF0000;
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}