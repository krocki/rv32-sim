// Simple framebuffer test for RV32 emulator
#include <stdint.h>

#define VID_BASE 0x11100000
#define SCREENWIDTH 320
#define SCREENHEIGHT 200

void main() {
    uint32_t *fb = (uint32_t *)VID_BASE;
    
    // Draw a colorful pattern
    for (int y = 0; y < SCREENHEIGHT; y++) {
        for (int x = 0; x < SCREENWIDTH; x++) {
            int idx = y * SCREENWIDTH + x;
            // Create a gradient pattern
            fb[idx] = 0xFF000000 | ((x & 0xFF) << 16) | ((y & 0xFF) << 8) | ((x + y) & 0xFF);
        }
    }
    
    // Infinite loop
    while(1);
}

// Minimal startup
__attribute__((section(".text.start")))
__attribute__((naked))
void _start() {
    __asm__ volatile(
        "la sp, 0x80100000\n"  // Set stack pointer
        "call main\n"
    );
}
