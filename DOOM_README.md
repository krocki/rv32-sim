# DOOM on RISC-V Simulator

This guide explains how to run DOOM on our RV32IMA RISC-V simulator.

## Quick Start

### 1. Prerequisites

Install SDL2 for graphics support:

```bash
# macOS
brew install sdl2 pkg-config

# Ubuntu/Linux
sudo apt-get install libsdl2-dev pkg-config
```

### 2. Build the DOOM-capable Emulator

```bash
./build_doom.sh
```

This creates `rv32ima_doom` - a RISC-V emulator with:
- SDL2 framebuffer support (640x480)
- UART console I/O
- Memory-mapped I/O (MMIO) devices
- Timer support
- 64MB RAM

### 3. Download Test Images

```bash
./download_doom_image.sh
```

This downloads a basic Linux kernel. For actual DOOM, you need a complete filesystem image.

## Architecture

### Memory Map

- `0x80000000` - RAM base (Linux entry point)
- `0x10000000` - CLINT (Core Local Interruptor)
  - `0x1000BFF8` - Timer register
- `0x11000000` - UART (console I/O)
- `0x11100000` - Framebuffer (640x480x32)
- `0x11300000` - System control

### Emulator Features

1. **Graphics**: SDL2-based framebuffer at 640x480 resolution
2. **Input**: Keyboard input via SDL2 (ESC to quit)
3. **Console**: UART emulation for Linux console output
4. **Timer**: System timer for Linux scheduling

## Building a Complete DOOM Image

To actually run DOOM, you need to build a complete Linux system with DOOM included:

### Option 1: Use Pre-built Image

```bash
# Download a complete DOOM image (if available)
wget https://github.com/cnlohr/mini-rv32ima-images/releases/download/v1.0/doom.img
./rv32ima_doom doom.img
```

### Option 2: Build Your Own

1. **Clone mini-rv32ima**:
```bash
git clone https://github.com/cnlohr/mini-rv32ima
cd mini-rv32ima
```

2. **Install Buildroot**:
```bash
wget https://buildroot.org/downloads/buildroot-2023.02.tar.gz
tar xzf buildroot-2023.02.tar.gz
cd buildroot-2023.02
```

3. **Configure for RISC-V with DOOM**:
```bash
make menuconfig
# Select:
# - Target Architecture: RISCV
# - Target Architecture Variant: RV32IMA
# - Target packages -> Games -> doom-wad
# - Target packages -> Games -> chocolate-doom (or prboom)
```

4. **Build the Image**:
```bash
make
# This takes 1-2 hours
```

5. **Create Combined Image**:
```bash
# Combine kernel and rootfs
cat output/images/Image output/images/rootfs.cpio > doom_complete.img
```

6. **Run DOOM**:
```bash
./rv32ima_doom doom_complete.img
```

## Emulator Controls

- **ESC**: Quit emulator
- **Console Input**: Type in terminal window
- **DOOM Controls** (when running):
  - Arrow keys: Move
  - Ctrl: Fire
  - Space: Open doors
  - Shift: Run

## Performance Notes

- The emulator runs at approximately 3 MIPS
- DOOM requires about 25-35 MHz equivalent
- Use `-f` flag for fixed update rate
- Use `-s` flag to slow down time

## Troubleshooting

### SDL2 Window Not Opening
- Ensure SDL2 is properly installed
- Check `DISPLAY` environment variable on Linux
- On macOS, allow terminal to control computer in Security settings

### Kernel Panic or Halt
- The kernel needs a proper rootfs to boot
- Ensure you have a complete image, not just the kernel

### Slow Performance
- Compile with `-O3` optimization
- Close other applications
- Try the `-n` flag for no sleep mode

## Technical Details

### How It Works

1. **CPU Emulation**: Uses cnlohr's mini-rv32ima engine
2. **Memory Management**: 64MB RAM with MMIO regions
3. **Graphics**: Framebuffer writes trigger SDL2 texture updates
4. **Timing**: Uses system clock for timer interrupts

### MMIO Implementation

The emulator implements several MMIO devices:

```c
// UART - Console I/O
if (addr == 0x11000000) {
    putchar(value & 0xFF);
}

// Framebuffer - Graphics
if (addr >= 0x11100000 && addr < 0x11500000) {
    framebuffer[(addr - 0x11100000) / 4] = value;
    UpdateDisplay();
}

// Timer - System timing
if (addr == 0x1000BFF8) {
    return GetTimeMicroseconds();
}
```

## Files

- `rv32ima_doom.cc` - Main DOOM emulator source
- `mini-rv32ima.h` - RISC-V CPU emulation header
- `build_doom.sh` - Build script
- `download_doom_image.sh` - Image download script
- `images/` - Downloaded kernel and filesystem images

## Credits

- Based on [cnlohr's mini-rv32ima](https://github.com/cnlohr/mini-rv32ima)
- RISC-V ISA specification
- SDL2 for graphics
- DOOM by id Software

## License

Educational and research purposes only. DOOM is property of id Software.