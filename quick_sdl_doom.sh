#!/bin/bash

# Quick SDL DOOM Setup - Downloads pre-configured buildroot and builds

set -e

echo "======================================"
echo "  Quick SDL DOOM Builder for RISC-V"
echo "======================================"
echo ""
echo "This script will:"
echo "1. Download a minimal buildroot configuration"
echo "2. Build a Linux kernel with framebuffer support"
echo "3. Include chocolate-doom and SDL2"
echo "4. Create an image for our rv32ima_doom emulator"
echo ""
echo "Estimated time: 30-60 minutes"
echo "Required space: ~5GB"
echo ""
echo "Continue? (y/n)"
read -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled"
    exit 0
fi

# First ensure our SDL emulator is built
echo "Building SDL emulator..."
if [ ! -f "rv32ima_doom" ]; then
    if command -v sdl2-config &> /dev/null; then
        g++ -O3 -o rv32ima_doom rv32ima_doom.cc $(sdl2-config --cflags --libs) -lm
        echo "✓ SDL emulator built"
    else
        echo "Error: SDL2 not found. Please install it first:"
        echo "  macOS: brew install sdl2"
        echo "  Linux: sudo apt-get install libsdl2-dev"
        exit 1
    fi
fi

# Run the automated builder
./build_sdl_doom_auto.sh

echo ""
echo "======================================"
echo "  Setup Complete!"
echo "======================================"
echo ""
echo "To run SDL DOOM:"
echo "  ./rv32ima_doom sdl_doom.bin"
echo ""
echo "Controls in DOOM:"
echo "  Arrow Keys - Move/Turn"
echo "  Ctrl - Fire"
echo "  Space - Use/Open"
echo "  Shift - Run"
echo "  Tab - Map"
echo "  ESC - Menu"