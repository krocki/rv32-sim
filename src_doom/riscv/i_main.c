/*
 * i_main.c
 *
 * Main entry point
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

#include "doomdef.h"
#include "d_main.h"

// Debug output
#define UART_BASE 0x10000000
#define VID_BASE 0x11100000

static volatile unsigned char *const uart = (void *)(UART_BASE);

static void uart_puts(const char *str) {
    while (*str) {
        *uart = *str++;
    }
}

static void draw_debug_pattern() {
    volatile unsigned int *fb = (unsigned int *)VID_BASE;
    // Draw red stripe at top
    for (int i = 0; i < 640 * 10; i++) {
        fb[i] = 0xFFFF0000;
    }
}

// Global argument variables that DOOM expects (defined in m_argv.c)
extern int myargc;
extern char** myargv;

// Static argument array
static char *doom_argv[] = { "doom", NULL };

int main(void)
{
	uart_puts("=== DOOM Starting ===\n");
	draw_debug_pattern();
	uart_puts("Debug pattern drawn\n");
	
	// Initialize command line arguments
	myargc = 1;
	myargv = doom_argv;
	
	uart_puts("Calling D_DoomMain...\n");
	D_DoomMain();
	uart_puts("D_DoomMain returned (unexpected)\n");
	return 0;
}
