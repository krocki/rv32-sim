# How to Actually Run DOOM on the RISC-V Emulator

## Quick Start

The `Image-emdoom-MAX_ORDER_14` file contains a complete Linux system with DOOM embedded. Here's how to run it:

### Step 1: Use the Reference Emulator

The most reliable way is to use cnlohr's reference implementation:

```bash
# Build the reference emulator
gcc -O3 -o mini-rv32ima-ref mini-rv32ima-ref.c -lm

# Run with the DOOM image
./mini-rv32ima-ref -m 0x4000000 -f ./images/Image-emdoom-MAX_ORDER_14
```

### Step 2: Login to Linux

When you see the prompt:
```
Welcome to Buildroot
buildroot login:
```

Type: `root` (press Enter)

No password is required.

### Step 3: Launch DOOM

Once logged in, type:
```bash
/root/emdoom
```

DOOM will start immediately!

## DOOM Controls

- **Arrow Keys**: Move forward/backward, turn left/right
- **Ctrl**: Fire weapon
- **Space**: Open doors/activate switches
- **Shift**: Run (move faster)
- **Tab**: Toggle map
- **ESC**: Menu
- **1-7**: Select weapons
- **Enter**: Select menu items

## What's Actually Happening

1. **Linux Boot**: The image contains a minimal Linux kernel (5.19.0) configured for RISC-V
2. **Buildroot System**: A tiny Linux distribution with just enough to run DOOM
3. **emdoom**: A special embedded version of DOOM optimized for small systems
4. **Console Graphics**: DOOM renders using ASCII/ANSI characters in the terminal

## Performance Tips

- The emulator runs at approximately 3 MIPS
- DOOM is playable but may be slow
- Use the `-l` flag to lock time base for more consistent performance
- Run on a fast machine for best results

## Troubleshooting

### "FAULT 8" Error
This usually means the DTB (Device Tree Blob) is incorrect. Use the reference emulator which handles this correctly.

### Black Screen in SDL Version
The SDL version expects a different framebuffer memory layout. The emdoom image uses console output, not a graphical framebuffer.

### Slow Performance
- Close other applications
- Compile with `-O3` optimization
- Try the reference emulator which is optimized

### Can't Find /root/emdoom
Make sure you're using the correct image file: `Image-emdoom-MAX_ORDER_14`

## Technical Details

### Memory Layout
- RAM: 64MB (0x4000000 bytes)
- Kernel loads at: 0x80000000
- DTB at end of RAM
- UART at: 0x10000000

### Why Console Instead of Graphics?
The emdoom version is designed for embedded systems without graphics hardware. It uses clever ASCII art rendering to display DOOM in the terminal. This is actually more authentic to the mini-rv32ima design philosophy - minimal, functional, and surprisingly capable.

## Alternative: Build Your Own DOOM Image

If you want a graphical version with SDL:

1. Clone mini-rv32ima
2. Use buildroot to configure a system with:
   - Framebuffer support
   - SDL libraries
   - Standard DOOM port (chocolate-doom or prboom)
3. Configure kernel with framebuffer device
4. Build and run with the SDL version of our emulator

## Fun Facts

- This runs DOOM on a simulated 32-bit RISC-V processor
- The entire system (kernel + DOOM) fits in ~8.5MB
- It's DOOM running on Linux running on RISC-V running on your computer
- The terminal-based rendering is actually quite playable!

## Credits

- Original DOOM by id Software
- emdoom port by cnlohr
- mini-rv32ima emulator by cnlohr
- RISC-V architecture by UC Berkeley