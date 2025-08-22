#!/bin/bash

# Build script for RISC-V DOOM emulator

set -e

echo "Building RISC-V DOOM Emulator..."

# Check for SDL2
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    if ! pkg-config --exists sdl2 2>/dev/null; then
        echo "SDL2 not found. Installing via Homebrew..."
        brew install sdl2
    fi
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
    SDL_LIBS=$(pkg-config --libs sdl2)
else
    # Linux
    if ! pkg-config --exists sdl2 2>/dev/null; then
        echo "SDL2 not found. Please install: sudo apt-get install libsdl2-dev"
        exit 1
    fi
    SDL_CFLAGS=$(pkg-config --cflags sdl2)
    SDL_LIBS=$(pkg-config --libs sdl2)
fi

# Compile the emulator
echo "Compiling rv32ima_doom..."
gcc -O3 -Wall -o rv32ima_doom rv32ima_doom.cc $SDL_CFLAGS $SDL_LIBS -lm

if [ $? -eq 0 ]; then
    echo "Build successful! Executable: rv32ima_doom"
    echo ""
    echo "To run DOOM:"
    echo "  1. Download a Linux image with DOOM:"
    echo "     ./download_doom_image.sh"
    echo "  2. Run the emulator:"
    echo "     ./rv32ima_doom doom.bin"
else
    echo "Build failed!"
    exit 1
fi