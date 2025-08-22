// Simple framebuffer test for DOOM
#include <stdint.h>

#define FB_BASE 0x11100000
#define FB_WIDTH 640
#define FB_HEIGHT 480
#define UART_BASE 0x10000000

static volatile uint32_t* fb = (uint32_t*)FB_BASE;
static volatile uint8_t* uart = (uint8_t*)UART_BASE;

void uart_putc(char c) {
    *uart = c;
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

void draw_test_pattern() {
    uart_puts("Drawing test pattern...\r\n");
    
    // Fill screen with gradient
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint32_t r = (x * 255 / FB_WIDTH) & 0xFF;
            uint32_t g = (y * 255 / FB_HEIGHT) & 0xFF;
            uint32_t b = 128;
            fb[y * FB_WIDTH + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
    
    uart_puts("Test pattern complete!\r\n");
}

void main() {
    uart_puts("DOOM framebuffer test starting...\r\n");
    draw_test_pattern();
    
    // Call real DOOM
    extern void D_DoomMain(void);
    uart_puts("Starting DOOM...\r\n");
    D_DoomMain();
}