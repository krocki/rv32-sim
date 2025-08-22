MEMORY
{
    ROM   (xr)  : ORIGIN = 0x80000000, LENGTH = 0x500000	/* 5120k */
    PSRAM (xrw) : ORIGIN = 0x80500000, LENGTH = 0x1300000	/* ~19MB */
}
ENTRY(_start)
SECTIONS {
    __stacktop = ORIGIN(PSRAM) + LENGTH(PSRAM);
    .text :
    {
        . = ALIGN(4);
        *(.text.start)
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        . = ALIGN(4);
        _etext = .;
        _sidata = _etext;
    } >ROM
    .data : AT ( _sidata )
    {
        . = ALIGN(4);
        _sdata = ABSOLUTE(.);
        _ram_start = ABSOLUTE(.);
        . = ALIGN(4);
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    } > PSRAM
    .bss :
    {
        . = ALIGN(4);
        _sbss = ABSOLUTE(.);
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = ABSOLUTE(.);
    } > PSRAM
    .heap :
    {
        . = ALIGN(4);
        _heap_start = ABSOLUTE(.);
    } > PSRAM
}
