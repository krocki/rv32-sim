// Very simple framebuffer test
#include <stdint.h>

#define VID_BASE 0x11100000
#define UART_BASE 0x10000000

void _start(void) {
    // Write to UART
    volatile uint8_t *uart = (uint8_t *)UART_BASE;
    *uart = 'H';
    *uart = 'I';
    *uart = '\n';
    
    // Write to framebuffer
    volatile uint32_t *fb = (uint32_t *)VID_BASE;
    
    // Fill first 100 pixels with red
    for (int i = 0; i < 100; i++) {
        fb[i] = 0xFFFF0000; // Red
    }
    
    // Infinite loop
    while(1);
}