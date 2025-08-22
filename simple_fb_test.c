/*
 * Simple framebuffer test for RISC-V emulator
 * This will draw colored patterns to the framebuffer at 0x11100000
 */

#define FRAMEBUFFER_BASE 0x11100000
#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_SIZE   (FB_WIDTH * FB_HEIGHT * 4)  // 32-bit pixels

// System call numbers (Linux RISC-V)
#define SYS_exit  93
#define SYS_write 64

void sys_exit(int code) {
    register int a7 asm("a7") = SYS_exit;
    register int a0 asm("a0") = code;
    asm volatile ("ecall" : : "r"(a7), "r"(a0));
}

void sys_write(int fd, const void *buf, int count) {
    register int a7 asm("a7") = SYS_write;
    register int a0 asm("a0") = fd;
    register const void *a1 asm("a1") = buf;
    register int a2 asm("a2") = count;
    asm volatile ("ecall" : : "r"(a7), "r"(a0), "r"(a1), "r"(a2));
}

void draw_test_pattern() {
    volatile unsigned int *fb = (volatile unsigned int *)FRAMEBUFFER_BASE;
    
    // Draw vertical color bars
    for (int y = 0; y < FB_HEIGHT; y++) {
        for (int x = 0; x < FB_WIDTH; x++) {
            unsigned int color;
            int bar_width = FB_WIDTH / 8;
            int bar = x / bar_width;
            
            switch (bar) {
                case 0: color = 0xFFFF0000; break; // Red
                case 1: color = 0xFF00FF00; break; // Green  
                case 2: color = 0xFF0000FF; break; // Blue
                case 3: color = 0xFFFFFF00; break; // Yellow
                case 4: color = 0xFFFF00FF; break; // Magenta
                case 5: color = 0xFF00FFFF; break; // Cyan
                case 6: color = 0xFFFFFFFF; break; // White
                default: color = 0xFF000000; break; // Black
            }
            
            fb[y * FB_WIDTH + x] = color;
        }
    }
}

int main() {
    const char *msg = "Drawing framebuffer test pattern...\n";
    sys_write(1, msg, 33);
    
    // Draw test pattern to framebuffer
    draw_test_pattern();
    
    const char *done = "Framebuffer test complete!\n";
    sys_write(1, done, 27);
    
    // Keep running so we can see the pattern
    while(1) {
        // Infinite loop
    }
    
    sys_exit(0);
    return 0;
}