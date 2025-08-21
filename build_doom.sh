#!/bin/bash

# Build script for DOOM image for RV32IMA simulator
# This script handles the environment setup and build process

set -e  # Exit on any error

echo "=========================================="
echo "Building DOOM image for RV32IMA simulator"
echo "=========================================="

# Check if we're in the right directory
if [ ! -d "buildroot-doom" ]; then
    echo "Error: buildroot-doom directory not found!"
    echo "Please run this script from the rv32-sim directory"
    exit 1
fi

# Function to run commands with clean environment
run_clean() {
    env -i PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
           TERM="$TERM" \
           HOME="$HOME" \
           "$@"
}

echo "Entering buildroot-doom directory..."
cd buildroot-doom

echo
echo "Step 1/3: Cleaning previous builds..."
run_clean make clean

echo
echo "Step 2/3: Configuring for RV32 DOOM..."
run_clean make rv32_doom_defconfig

echo
echo "Step 3/3: Building DOOM image (this will take 30-60 minutes)..."
echo "Building with single thread for reliability..."
echo

# Start the build with progress indication
run_clean make -j1

echo
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo
echo "DOOM image location: buildroot-doom/output/images/Image"
echo
echo "To run DOOM:"
echo "  ./rv32ima_sdl buildroot-doom/output/images/Image"
echo
echo "SDL2 simulator controls:"
echo "  - ESC key or close window to exit"
echo "  - Framebuffer: 640x480 at 0x50000000"
echo "  - Control registers at 0x50001000"
echo