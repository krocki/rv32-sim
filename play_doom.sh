#!/bin/bash

# Interactive DOOM launcher for RISC-V

clear
cat << "EOF"
     ____   ___   ___  __  __    ___  _   _    ____  ___ ____   ____     __     __
    |  _ \ / _ \ / _ \|  \/  |  / _ \| \ | |  |  _ \|_ _/ ___| / ___|    \ \   / /
    | | | | | | | | | | |\/| | | | | |  \| |  | |_) || |\___ \| |   _____ \ \ / / 
    | |_| | |_| | |_| | |  | | | |_| | |\  |  |  _ < | | ___) | |__|_____| \ V /  
    |____/ \___/ \___/|_|  |_|  \___/|_| \_|  |_| \_\___|____/ \____|       \_/   
                                                                                    
                        Press ENTER to descend into Hell...
EOF

read -p ""

echo ""
echo "Checking system..."

# Check for the DOOM image
if [ ! -f "./images/Image-emdoom-MAX_ORDER_14" ]; then
    echo "❌ DOOM image not found!"
    echo ""
    echo "Download it from: https://github.com/cnlohr/mini-rv32ima"
    echo "Or build your own with buildroot"
    exit 1
fi

# Check for emulator
EMULATOR=""
if [ -f "./mini-rv32ima-ref" ]; then
    EMULATOR="./mini-rv32ima-ref"
    EMULATOR_ARGS="-m 0x4000000 -f ./images/Image-emdoom-MAX_ORDER_14"
    echo "✓ Found reference emulator"
elif [ -f "./rv32ima_doom_console" ]; then
    EMULATOR="./rv32ima_doom_console"
    EMULATOR_ARGS="./images/Image-emdoom-MAX_ORDER_14"
    echo "✓ Found console emulator"
else
    echo "❌ No emulator found!"
    echo ""
    echo "Building reference emulator..."
    if [ -f "mini-rv32ima-ref.c" ] && [ -f "mini-rv32ima.h" ]; then
        gcc -O3 -o mini-rv32ima-ref mini-rv32ima-ref.c -lm
        if [ $? -eq 0 ]; then
            EMULATOR="./mini-rv32ima-ref"
            EMULATOR_ARGS="-m 0x4000000 -f ./images/Image-emdoom-MAX_ORDER_14"
            echo "✓ Built reference emulator"
        else
            echo "❌ Build failed!"
            exit 1
        fi
    else
        echo "❌ Source files not found!"
        echo "Run: curl -LO https://raw.githubusercontent.com/cnlohr/mini-rv32ima/master/mini-rv32ima/mini-rv32ima.c"
        exit 1
    fi
fi

echo ""
echo "================== INSTRUCTIONS =================="
echo "1. Wait for 'buildroot login:' prompt (~5 seconds)"
echo "2. Login as: root (just type 'root' and press Enter)"
echo "3. Type: /root/emdoom (and press Enter)"
echo "4. DOOM will start!"
echo ""
echo "CONTROLS:"
echo "  ↑↓←→  = Move/Turn"
echo "  Ctrl  = Fire"
echo "  Space = Use/Open"
echo "  Shift = Run"
echo "  Tab   = Map"
echo "  ESC   = Menu"
echo "=================================================="
echo ""
echo "Starting Linux in 3 seconds..."
echo "(This will take about 5-10 seconds to boot)"
sleep 3

# Run the emulator
$EMULATOR $EMULATOR_ARGS

echo ""
echo "Thanks for playing DOOM on RISC-V!"