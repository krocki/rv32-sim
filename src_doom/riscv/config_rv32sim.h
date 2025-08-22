/*
 * config.h for rv32-sim emulator
 * Modified to work with your SDL emulator
 */

#pragma once

// Your emulator's framebuffer address
#define VID_BASE 0x11100000

// UART address (same as original)
#define UART_BASE 0x10000000

// Control base (unused but kept for compatibility)
#define VID_CTRL_BASE (VID_BASE + 0x00000)

// Timer address for your emulator
#define CLINT_MTIME 0x1100BFF8