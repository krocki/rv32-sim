/*
 * start.S
 *
 * Startup code
 *
 * Copyright (C) 2021 Sylvain Munaut
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

    .section .text.start
    .global _start
_start:

    // Set stack pointer
    la sp, __stacktop

    // Copy data section from ROM to RAM
    la t0, _sdata       // RAM destination
    la t1, _edata       // RAM end
    la t2, _sidata      // ROM source
2:
    beq t0, t1, 3f      // If we've reached the end, exit loop
    lw t3, 0(t2)        // Load word from ROM
    sw t3, 0(t0)        // Store word to RAM
    addi t0, t0, 4      // Increment RAM pointer
    addi t2, t2, 4      // Increment ROM pointer
    j 2b
3:

    // Clear BSS section
    la t0, _sbss
    la t1, _ebss
1:
    sw zero, 0(t0)
    addi t0, t0, 4
    bltu t0, t1, 1b

    // call main
    call main

.global	_exit
_exit:
    j _exit
