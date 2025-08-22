/*
 * Minimal DOOM-like test for SDL framebuffer
 * This creates a simple graphical demo to test the SDL emulator
 */

#include <stdint.h>

// Framebuffer configuration
#define FB_BASE   0x11100000
#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT)

// System calls
#define SYS_exit  93
#define SYS_write 64

void sys_exit(int code) {
    asm volatile ("li a7, %0; mv a0, %1; ecall" :: "i"(SYS_exit), "r"(code) : "a7", "a0");
}

void sys_write(const char *buf, int len) {
    asm volatile ("li a7, %0; li a0, 1; mv a1, %1; mv a2, %2; ecall" 
                  :: "i"(SYS_write), "r"(buf), "r"(len) 
                  : "a7", "a0", "a1", "a2");
}

void draw_doom_logo() {
    volatile uint32_t *fb = (volatile uint32_t *)FB_BASE;
    
    sys_write("Drawing DOOM logo to framebuffer...\n", 37);
    
    // Clear screen to black
    for (int i = 0; i < FB_SIZE; i++) {
        fb[i] = 0xFF000000; // Black
    }
    
    // Draw simple DOOM-like logo (red letters on black background)
    int start_y = FB_HEIGHT / 2 - 50;
    int start_x = FB_WIDTH / 2 - 150;
    
    // Draw "DOOM" letters as colored rectangles
    uint32_t red = 0xFFFF0000;
    uint32_t yellow = 0xFFFFFF00;
    
    // Letter D
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 20; x++) {
            if (x < 15 && (y < 15 || y > 85 || x < 5)) {
                fb[(start_y + y) * FB_WIDTH + (start_x + x)] = red;
            }
        }
    }
    
    // Letter O (first)
    for (int y = 0; y < 100; y++) {
        for (int x = 40; x < 90; x++) {
            if ((y < 15 || y > 85) || (x < 55 || x > 75)) {
                if (x < 90 && y < 100) {
                    fb[(start_y + y) * FB_WIDTH + (start_x + x)] = yellow;
                }
            }
        }
    }
    
    // Letter O (second) 
    for (int y = 0; y < 100; y++) {
        for (int x = 110; x < 160; x++) {
            if ((y < 15 || y > 85) || (x < 125 || x > 145)) {
                if (x < 160 && y < 100) {
                    fb[(start_y + y) * FB_WIDTH + (start_x + x)] = red;
                }
            }
        }
    }
    
    // Letter M
    for (int y = 0; y < 100; y++) {
        for (int x = 180; x < 250; x++) {
            if (x < 195 || x > 235 || (y < 50 && x > 210 && x < 225)) {
                fb[(start_y + y) * FB_WIDTH + (start_x + x)] = yellow;
            }
        }
    }
    
    // Add some animated fire effect at bottom
    for (int y = FB_HEIGHT - 100; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            uint32_t intensity = (y - (FB_HEIGHT - 100)) * 2;
            if (intensity > 255) intensity = 255;
            uint32_t fire_color = 0xFF000000 | (intensity << 16) | ((intensity/2) << 8);
            fb[y * FB_WIDTH + x] = fire_color;
        }
    }
    
    sys_write("DOOM logo complete! Check your SDL window.\n", 43);
}

void _start() {
    draw_doom_logo();
    
    // Keep running so the image stays visible
    while(1) {
        // Infinite loop
    }
}