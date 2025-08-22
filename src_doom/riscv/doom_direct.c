// Direct framebuffer DOOM test - bypass engine
#include <stdint.h>
#include "config.h"

#define FB_WIDTH 320
#define FB_HEIGHT 200

// DOOM screen buffer
uint8_t screen[FB_WIDTH * FB_HEIGHT];

// Simple palette
uint32_t palette[256];

void init_palette() {
    // Create a simple red-based palette
    for (int i = 0; i < 256; i++) {
        int r = (i < 128) ? i * 2 : 255;
        int g = (i < 64) ? 0 : (i - 64);
        int b = 0;
        palette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
}

void draw_doom_screen() {
    // Draw DOOM-like screen
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            // Sky gradient
            if (y < 100) {
                screen[y * FB_WIDTH + x] = 64 + (y / 2);
            }
            // Floor
            else {
                screen[y * FB_WIDTH + x] = 32 + ((y - 100) / 4);
            }
        }
    }
    
    // Draw walls
    for (int x = 60; x < 80; x++) {
        for (int y = 50; y < 150; y++) {
            screen[y * FB_WIDTH + x] = 128;
        }
    }
    
    for (int x = 240; x < 260; x++) {
        for (int y = 50; y < 150; y++) {
            screen[y * FB_WIDTH + x] = 128;
        }
    }
    
    // Draw HUD bar at bottom
    for (int y = 180; y < 200; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            screen[y * FB_WIDTH + x] = 96;
        }
    }
}

void copy_to_framebuffer() {
    volatile uint32_t *fb = (uint32_t*)VID_BASE;
    
    // Scale 320x200 to 640x480 with pixel doubling
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t pixel = screen[y * 320 + x];
            uint32_t color = palette[pixel];
            
            // Write 2x2 block for each pixel
            int fb_x = x * 2;
            int fb_y = y * 2;
            
            fb[fb_y * 640 + fb_x] = color;
            fb[fb_y * 640 + fb_x + 1] = color;
            fb[(fb_y + 1) * 640 + fb_x] = color;
            fb[(fb_y + 1) * 640 + fb_x + 1] = color;
        }
    }
    
    // Fill remaining bottom area
    for (int y = 400; y < 480; y++) {
        for (int x = 0; x < 640; x++) {
            fb[y * 640 + x] = 0xFF000000;
        }
    }
}

int main() {
    init_palette();
    
    while (1) {
        draw_doom_screen();
        copy_to_framebuffer();
        
        // Simple animation
        static int frame = 0;
        frame++;
        
        // Animate wall positions
        for (int y = 50; y < 150; y++) {
            int offset = (frame / 10) % 20;
            screen[y * FB_WIDTH + 60 + offset] = 160;
            screen[y * FB_WIDTH + 240 - offset] = 160;
        }
        
        // Delay
        for (volatile int i = 0; i < 100000; i++);
    }
    
    return 0;
}