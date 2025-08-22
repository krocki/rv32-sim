/*
 * Test version with framebuffer debug
 */

#include <stdint.h>
#include "doomdef.h"
#include "d_main.h"
#include "config.h"

#define FB_WIDTH 640
#define FB_HEIGHT 480

void draw_test() {
    volatile uint32_t *fb = (uint32_t*)VID_BASE;
    
    // Draw test pattern
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            fb[y * FB_WIDTH + x] = 0xFFFF0000; // Red stripe
        }
    }
    
    // Draw border
    for (int x = 0; x < FB_WIDTH; x++) {
        fb[x] = 0xFFFFFFFF; // White top
        fb[(FB_HEIGHT-1) * FB_WIDTH + x] = 0xFFFFFFFF; // White bottom
    }
    for (int y = 0; y < FB_HEIGHT; y++) {
        fb[y * FB_WIDTH] = 0xFFFFFFFF; // White left
        fb[y * FB_WIDTH + (FB_WIDTH-1)] = 0xFFFFFFFF; // White right
    }
}

int main(void) {
    // Draw test pattern first
    draw_test();
    
    // Small delay to see test pattern
    for (volatile int i = 0; i < 10000000; i++);
    
    // Start DOOM
    D_DoomMain();
    return 0;
}