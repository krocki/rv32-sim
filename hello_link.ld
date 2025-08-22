ENTRY(_start)

MEMORY
{
  ram (rwx) : ORIGIN = 0x00000000, LENGTH = 1M
}

SECTIONS
{
  . = 0x00000000;
  
  .text : {
    *(.text.init)
    *(.text)
    *(.text.*)
  } > ram
  
  .rodata : {
    *(.rodata)
    *(.rodata.*)
  } > ram
  
  .data : {
    *(.data)
    *(.data.*)
  } > ram
  
  .bss : {
    *(.bss)
    *(.bss.*)
    *(COMMON)
  } > ram
  
  _end = .;
}