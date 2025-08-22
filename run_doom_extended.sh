#!/bin/bash

# Run DOOM and capture more output
echo "Starting RISC-V DOOM..."
echo "You should see Linux booting, then DOOM starting!"
echo "======================================="

# Run with more verbose output
./rv32ima_doom_console images/Image-emdoom-MAX_ORDER_14

echo "DOOM session ended."