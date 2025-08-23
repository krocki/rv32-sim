/*
 * console.c
 *
 * Copyright (C) 2019-2021 Sylvain Munaut
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

#include <stdint.h>

#include "config.h"
#include "mini-printf.h"

struct wb_uart {
  uint32_t tx;
  uint32_t rx;
} __attribute__((packed, aligned(4)));

static volatile uint8_t *const uart_regs = (void *)(UART_BASE);

void console_init(void) {}

void console_putchar(char c) { *uart_regs = c; }

char console_getchar(void) {
  // Wait for keyboard input
  volatile uint32_t *kbd_status = (volatile uint32_t *)0x11200000;
  volatile uint32_t *kbd_data = (volatile uint32_t *)0x11200004;
  
  // Wait until data is available
  while (!(*kbd_status & 1)) {
    // Busy wait
  }
  
  return *kbd_data & 0xFF;
}

int console_getchar_nowait(void) {
  // Read from keyboard MMIO at 0x11200000
  volatile uint32_t *kbd_status = (volatile uint32_t *)0x11200000;
  volatile uint32_t *kbd_data = (volatile uint32_t *)0x11200004;
  
  // Check if data is available
  if (*kbd_status & 1) {
    return *kbd_data & 0xFF;
  }
  
  return -1;
}

void console_puts(const char *p) {
  char c;
  while ((c = *(p++)) != 0x00)
    *uart_regs = c;
}

int console_printf(const char *fmt, ...) {
  static char _printf_buf[128];
  va_list va;
  int l;

  va_start(va, fmt);
  l = mini_vsnprintf(_printf_buf, 128, fmt, va);
  va_end(va);

  console_puts(_printf_buf);

  return l;
}
