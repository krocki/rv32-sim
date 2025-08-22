#!/bin/bash

echo "Starting DOOM with reference emulator..."
echo "This should boot Linux and run DOOM!"
echo "Once you see 'buildroot login:', try typing 'doom' or just press Enter"
echo "============================================"

# Use the reference emulator which works properly
./mini-rv32ima-ref -f images/Image-emdoom-MAX_ORDER_14 -m 67108864 -t 1

echo "DOOM session ended."