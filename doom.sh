#!/bin/bash

# Simple DOOM launcher for RISC-V emulator

echo "Starting DOOM on RISC-V..."
echo ""
echo "Instructions:"
echo "1. Wait for 'buildroot login:' prompt"
echo "2. Type: root"
echo "3. Type: /root/emdoom"
echo ""

# Check if image exists
if [ ! -f "./images/Image-emdoom-MAX_ORDER_14" ]; then
    echo "Error: DOOM image not found!"
    echo "Download from: https://github.com/cnlohr/mini-rv32ima"
    exit 1
fi

# Check for reference emulator
if [ ! -f "./mini-rv32ima-ref" ]; then
    echo "Building reference emulator..."
    if [ -f "mini-rv32ima-ref.c" ]; then
        gcc -O3 -o mini-rv32ima-ref mini-rv32ima-ref.c -lm
    else
        echo "Downloading reference emulator source..."
        curl -LO https://raw.githubusercontent.com/cnlohr/mini-rv32ima/master/mini-rv32ima/mini-rv32ima.c
        mv mini-rv32ima.c mini-rv32ima-ref.c
        gcc -O3 -o mini-rv32ima-ref mini-rv32ima-ref.c -lm
    fi
fi

# Run DOOM
./mini-rv32ima-ref -m 0x4000000 -f ./images/Image-emdoom-MAX_ORDER_14