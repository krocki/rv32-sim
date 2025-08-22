// Debug wrapper for DOOM
#include <stdint.h>

#define UART_BASE 0x10000000
#define VID_BASE 0x11100000

static volatile uint8_t *const uart = (void *)(UART_BASE);

void uart_puts(const char *str) {
    while (*str) {
        *uart = *str++;
    }
}

void draw_test_pattern() {
    volatile uint32_t *fb = (uint32_t *)VID_BASE;
    
    // Draw red stripe at top
    for (int i = 0; i < 640 * 10; i++) {
        fb[i] = 0xFFFF0000;
    }
    
    uart_puts("Test pattern drawn\n");
}

extern void D_DoomMain(void);

void main(void) {
    uart_puts("=== DOOM Debug Start ===\n");
    draw_test_pattern();
    uart_puts("Calling D_DoomMain...\n");
    D_DoomMain();
    uart_puts("D_DoomMain returned (should not happen)\n");
    while(1);
}