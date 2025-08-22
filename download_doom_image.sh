#!/bin/bash

# Download pre-built Linux images for RISC-V DOOM

set -e

echo "Downloading RISC-V Linux images..."

# Create images directory
mkdir -p images

# Download basic Linux image
if [ ! -f "images/Image" ]; then
    echo "Downloading Linux kernel..."
    curl -L -o images/Image https://github.com/cnlohr/mini-rv32ima/raw/master/buildroot/Image
    echo "Kernel downloaded: images/Image"
else
    echo "Kernel already exists: images/Image"
fi

# Try to download a pre-built DOOM image
echo ""
echo "Checking for pre-built DOOM images..."

# Option 1: Try cnlohr's mini-rv32ima-images repository
if [ ! -f "images/doom_attempt.img" ]; then
    echo "Attempting to download DOOM image from mini-rv32ima-images..."
    curl -L -f -o images/doom_attempt.img https://github.com/cnlohr/mini-rv32ima-images/raw/master/linux.zip 2>/dev/null || true
    
    if [ -f "images/doom_attempt.img" ] && [ -s "images/doom_attempt.img" ]; then
        # Try to unzip if it's a zip file
        if file images/doom_attempt.img | grep -q "Zip archive"; then
            echo "Extracting zip archive..."
            cd images
            unzip -o doom_attempt.img
            rm doom_attempt.img
            cd ..
        fi
    fi
fi

# Create a simple test image if no DOOM image available
if [ ! -f "images/test.bin" ]; then
    echo ""
    echo "Creating a simple test image..."
    # This would be a basic Linux image for testing
    cp images/Image images/test.bin
    echo "Test image created: images/test.bin"
fi

echo ""
echo "Available images:"
ls -lh images/

echo ""
echo "To run the emulator with an image:"
echo "  ./rv32ima_doom images/Image"
echo ""
echo "Note: For a full DOOM experience, you'll need to build a custom"
echo "      Linux image with DOOM included using buildroot."
echo ""
echo "To build your own DOOM image:"
echo "  1. Clone mini-rv32ima: git clone https://github.com/cnlohr/mini-rv32ima"
echo "  2. Follow the buildroot instructions in the repository"
echo "  3. Enable DOOM in the buildroot menuconfig"