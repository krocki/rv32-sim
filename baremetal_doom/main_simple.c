// Bare-metal DOOM-like demo for RISC-V
// Simple integer-only version for testing

#include <stdint.h>

// Hardware addresses
#define UART_BASE    0x10000000
#define FB_BASE      0x11100000

// Framebuffer configuration
#define FB_WIDTH     640
#define FB_HEIGHT    480

// Framebuffer pointer
static volatile uint32_t* fb = (uint32_t*)FB_BASE;

// Draw a pixel
static void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fb[y * FB_WIDTH + x] = color;
    }
}

// Fill rectangle
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            draw_pixel(x + dx, y + dy, color);
        }
    }
}

// Simple UART output
static void uart_putc(char c) {
    volatile uint8_t* uart = (uint8_t*)UART_BASE;
    *uart = c;
}

static void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

// Main function - draws a colorful test pattern
void main(void) {
    uart_puts("Bare-metal DOOM test starting...\r\n");
    uart_puts("Drawing test pattern to framebuffer\r\n");
    
    // Draw gradient test pattern
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint32_t r = (x * 255 / FB_WIDTH) & 0xFF;
            uint32_t g = (y * 255 / FB_HEIGHT) & 0xFF;
            uint32_t b = ((x + y) * 255 / (FB_WIDTH + FB_HEIGHT)) & 0xFF;
            fb[y * FB_WIDTH + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }
    
    // Draw some rectangles
    fill_rect(100, 100, 200, 150, 0xFFFF0000);  // Red
    fill_rect(340, 100, 200, 150, 0xFF00FF00);  // Green
    fill_rect(220, 280, 200, 150, 0xFF0000FF);  // Blue
    
    // Draw border
    for (int x = 0; x < FB_WIDTH; x++) {
        draw_pixel(x, 0, 0xFFFFFFFF);
        draw_pixel(x, FB_HEIGHT-1, 0xFFFFFFFF);
    }
    for (int y = 0; y < FB_HEIGHT; y++) {
        draw_pixel(0, y, 0xFFFFFFFF);
        draw_pixel(FB_WIDTH-1, y, 0xFFFFFFFF);
    }
    
    uart_puts("Test pattern complete!\r\n");
    
    // Animation loop
    int offset = 0;
    while (1) {
        // Animate a moving bar
        for (int x = 0; x < 50; x++) {
            for (int y = 200; y < 280; y++) {
                draw_pixel((x + offset) % FB_WIDTH, y, 0xFFFFFF00);
            }
        }
        
        offset++;
        
        // Simple delay
        for (volatile int i = 0; i < 1000000; i++);
    }
}