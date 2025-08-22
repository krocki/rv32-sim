/*
 * Simplified main that just draws DOOM logo
 */

#include <stdint.h>
#include "config.h"

#define FB_WIDTH 640
#define FB_HEIGHT 480

// Simple DOOM logo pattern
void draw_doom_logo() {
    volatile uint32_t *fb = (uint32_t*)VID_BASE;
    
    // Clear screen to dark red
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = 0xFF400000;
    }
    
    // Draw "DOOM" with blocks
    // D
    for (int y = 100; y < 200; y++) {
        for (int x = 50; x < 60; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
        for (int x = 90; x < 100; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    }
    for (int y = 100; y < 110; y++)
        for (int x = 50; x < 100; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    for (int y = 190; y < 200; y++)
        for (int x = 50; x < 100; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    
    // O
    for (int y = 100; y < 200; y++) {
        for (int x = 150; x < 160; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
        for (int x = 190; x < 200; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    }
    for (int y = 100; y < 110; y++)
        for (int x = 150; x < 200; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    for (int y = 190; y < 200; y++)
        for (int x = 150; x < 200; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    
    // O
    for (int y = 100; y < 200; y++) {
        for (int x = 250; x < 260; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
        for (int x = 290; x < 300; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    }
    for (int y = 100; y < 110; y++)
        for (int x = 250; x < 300; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    for (int y = 190; y < 200; y++)
        for (int x = 250; x < 300; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    
    // M
    for (int y = 100; y < 200; y++) {
        for (int x = 350; x < 360; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
        for (int x = 390; x < 400; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
        for (int x = 430; x < 440; x++) fb[y * FB_WIDTH + x] = 0xFFFF0000;
    }
    for (int y = 100; y < 130; y++) {
        int offset = (y - 100);
        fb[y * FB_WIDTH + 360 + offset] = 0xFFFF0000;
        fb[y * FB_WIDTH + 390 - offset] = 0xFFFF0000;
    }
    
    // Draw subtitle
    for (int y = 250; y < 260; y++)
        for (int x = 200; x < 440; x++)
            if ((x % 10) < 7) fb[y * FB_WIDTH + x] = 0xFFFFFF00;
    
    // Draw animated fire effect at bottom
    static int frame = 0;
    frame++;
    for (int y = 400; y < 480; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            int intensity = ((y - 400) * 255 / 80) + ((x + frame) % 50);
            if (intensity > 255) intensity = 255;
            uint32_t color = 0xFF000000 | (intensity << 16) | ((intensity/2) << 8);
            fb[y * FB_WIDTH + x] = color;
        }
    }
}

int main(void) {
    // Draw DOOM logo
    while (1) {
        draw_doom_logo();
        
        // Delay
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    return 0;
}