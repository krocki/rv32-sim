#!/bin/bash

# Download pre-built RISC-V image with DOOM support

echo "Downloading pre-built RISC-V Linux image with DOOM..."

# Check if we have the SDL emulator ready
if [ ! -f "/Users/kamil/git/rv32-sim/rv32ima_ref_sdl" ]; then
    echo "Building SDL emulator first..."
    gcc -o /Users/kamil/git/rv32-sim/rv32ima_ref_sdl \
        /Users/kamil/git/rv32-sim/rv32ima_ref_sdl.c \
        -lSDL2 -O3 -march=native
    if [ $? -ne 0 ]; then
        echo "Failed to build SDL emulator"
        exit 1
    fi
fi

# Option 1: Use the existing console DOOM image and add framebuffer wrapper
echo "Using existing DOOM image with framebuffer wrapper..."

# Create a simple framebuffer test program
cat > fb_test.c << 'EOF'
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
EOF

# Compile test program
riscv64-unknown-elf-gcc -march=rv32ima -mabi=ilp32 -nostdlib -o fb_test.elf fb_test.c

echo "Setup complete!"
echo ""
echo "To test the SDL emulator with existing DOOM image, run:"
echo "  ./rv32ima_ref_sdl Image-emdoom-MAX_ORDER_14"
echo ""
echo "Note: The current image outputs to console/UART, not framebuffer."
echo "A proper framebuffer-enabled DOOM image needs to be built with buildroot."