# RV32IMA SDL2 Simulator

This is an enhanced version of the rv32ima.cc RISC-V simulator with SDL2 framebuffer support for running graphical applications like DOOM.

## Features

- Complete RV32IMA (Integer, Multiply/Divide, Atomic) instruction set support
- Memory-mapped framebuffer at `0x50000000`
- Control registers at `0x50001000` 
- Real-time SDL2 window display (640x480)
- Keyboard input handling (ESC to exit)

## Building

### Install Dependencies
```bash
sudo apt-get install libsdl2-dev pkg-config
```

### Compile
```bash
make -f Makefile.sdl
```

## Usage

```bash
./rv32ima_sdl [--trace] program.bin
```

- `--trace`: Enable instruction tracing
- `program.bin`: RISC-V binary to execute

## Memory Map

| Address Range | Description |
|---------------|-------------|
| `0x00000000 - 0x00FFFFFF` | RAM (16 MiB) |
| `0x50000000 - 0x5012C000` | Framebuffer (640x480x32bit RGBA) |
| `0x50001000 - 0x500010FF` | Control registers |

## Control Registers

| Offset | Name | Access | Description |
|--------|------|--------|-------------|
| `0x00` | FB_WIDTH | R | Framebuffer width (640) |
| `0x04` | FB_HEIGHT | R | Framebuffer height (480) |
| `0x08` | FB_BPP | R | Bits per pixel (32) |
| `0x0C` | FB_ENABLE | R | Framebuffer enabled (1) |
| `0x10` | FB_FLUSH | W | Force display update |

## Framebuffer Format

- 32-bit RGBA pixels
- Linear layout: `FB_BASE + (y * width + x) * 4`
- Color format: `0xAABBGGRR` (little-endian)

## File Structure

- `rv32ima_sdl.cc` - Main simulator with SDL2 integration
- `sdl_framebuffer.h/.cc` - SDL2 framebuffer implementation  
- `Makefile.sdl` - Build configuration
- `buildroot-doom/` - DOOM buildroot configuration

## Building DOOM Image

The buildroot-doom directory contains a complete Linux build system configured for RV32I with:
- SDL2 graphics support
- Chocolate DOOM port
- DOOM1.WAD game data

### Prerequisites
No external RISC-V toolchain required - buildroot builds its own complete cross-compilation toolchain.

### Build Instructions

**Option 1: Use the build script (recommended)**
```bash
./build_doom.sh
```

**Option 2: Manual build**  
**Important**: Use a clean environment to avoid conflicts with system libraries:

```bash
cd buildroot-doom

# Clean any previous builds
env -i PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" TERM="$TERM" HOME="$HOME" make clean

# Configure for RV32 DOOM
env -i PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" TERM="$TERM" HOME="$HOME" make rv32_doom_defconfig

# Build (single-threaded for reliability)
env -i PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" TERM="$TERM" HOME="$HOME" make -j1
```

The resulting image will be in `output/images/Image`.

### Build Time
The complete build takes approximately 30-60 minutes and includes:
- Cross-compilation toolchain (binutils, GCC)
- Linux kernel for RV32IMA
- SDL2 library
- Chocolate DOOM
- Root filesystem with game assets

### Troubleshooting
If the build fails with library path errors, ensure you're using the clean environment commands above. The `env -i` command removes all environment variables that might interfere with the build process.

## Example Programs

Test the framebuffer with the provided hello.bin:
```bash
./rv32ima_sdl hello.bin
```

This will open an SDL2 window and print "Hello, World!" to the terminal while the framebuffer remains black (no graphics drawn).

## Implementation Notes

- The simulator maintains the original rv32ima.cc architecture
- SDL2 code is cleanly separated in dedicated files
- Framebuffer updates occur every 1000 CPU cycles for performance
- Window can be closed with ESC key or window close button
- All memory accesses to framebuffer addresses are intercepted and routed to SDL2