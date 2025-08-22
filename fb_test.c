#include <stdint.h>

#define FB_BASE 0x11100000
#define FB_WIDTH 640
#define FB_HEIGHT 480

void _start() {
    volatile uint32_t *fb = (uint32_t*)FB_BASE;
    
    // Draw test pattern
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint32_t color = ((x & 0xFF) << 16) | ((y & 0xFF) << 8) | 0xFF;
            fb[y * FB_WIDTH + x] = color;
        }
    }
    
    // Infinite loop
    while(1);
}
