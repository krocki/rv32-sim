#!/bin/bash

# Script to run DOOM on RISC-V emulator

echo "=================================="
echo "    DOOM on RISC-V Emulator"
echo "=================================="
echo ""
echo "Starting Linux with embedded DOOM..."
echo ""
echo "INSTRUCTIONS:"
echo "1. Wait for 'buildroot login:' prompt"
echo "2. Login as: root (no password)"
echo "3. Type: /root/emdoom"
echo "4. DOOM will start!"
echo ""
echo "DOOM CONTROLS:"
echo "  Arrow keys - Move"
echo "  Ctrl - Fire"
echo "  Space - Use/Open doors"
echo "  Shift - Run"
echo "  ESC - Menu"
echo "  Ctrl+C - Quit"
echo ""
echo "Starting emulator in 3 seconds..."
sleep 3

# Check if the DOOM image exists
if [ ! -f "./images/Image-emdoom-MAX_ORDER_14" ]; then
    echo "Error: DOOM image not found!"
    echo "Please download it first or build your own."
    exit 1
fi

# Check which emulator to use
if [ -f "./mini-rv32ima-ref" ]; then
    echo "Using reference emulator..."
    ./mini-rv32ima-ref -m 0x4000000 -f ./images/Image-emdoom-MAX_ORDER_14
elif [ -f "./rv32ima_doom_console" ]; then
    echo "Using console emulator..."
    ./rv32ima_doom_console ./images/Image-emdoom-MAX_ORDER_14
else
    echo "Error: No emulator found!"
    echo "Build one with: ./build_doom.sh"
    exit 1
fi