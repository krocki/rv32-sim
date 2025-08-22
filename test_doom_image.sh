#!/bin/bash

# Test script for DOOM SDL image

IMAGE_PATH="/Users/kamil/git/rv32-sim/buildroot-doom/output/images/rootfs.cpio"
EMULATOR="/Users/kamil/git/rv32-sim/rv32ima_ref_sdl"

if [ ! -f "$IMAGE_PATH" ]; then
    echo "Error: Image not found at $IMAGE_PATH"
    echo "Build may still be in progress..."
    exit 1
fi

if [ ! -f "$EMULATOR" ]; then
    echo "Error: SDL emulator not found at $EMULATOR"
    echo "Please compile it first with:"
    echo "  gcc -o rv32ima_ref_sdl rv32ima_ref_sdl.c -lSDL2 -O3"
    exit 1
fi

echo "Starting RISC-V SDL emulator with DOOM image..."
echo "Controls:"
echo "  - Arrow keys: Move"
echo "  - Ctrl: Fire"
echo "  - Space: Use/Open doors"
echo "  - Shift: Run"
echo "  - Tab: Map"
echo "  - ESC: Menu"
echo ""
echo "Starting emulator..."

$EMULATOR $IMAGE_PATH