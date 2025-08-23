# RV32IMA RISC-V Emulator

A RISC-V RV32IMA instruction set emulator with SDL2 support for running bare-metal programs including DOOM.

## Quick Start

```bash
# Build the emulator
make emulator-sdl

# Run DOOM
make run-doom
```

![DOOM running on RV32IMA emulator](doom.png)

## Features

- **RV32IMA**: Full RISC-V 32-bit integer, multiply/divide, and atomic instructions
- **SDL2 Support**: Framebuffer rendering for graphics applications
- **Bare-metal DOOM**: Runs the classic game at ~20-30 FPS
- **Memory-mapped I/O**: UART at 0x10000000, framebuffer at 0x11100000
- **Fast emulation**: Optimized C implementation

## Building

### Prerequisites

```bash
# macOS
brew install sdl2
brew tap riscv-software-src/riscv
brew install riscv-tools

# Linux
sudo apt-get install libsdl2-dev gcc-riscv64-unknown-elf
```

### Build Commands

```bash
# Basic console emulator
make emulator

# SDL-enabled emulator  
make emulator-sdl

# Build hello world example
make hello

# Build DOOM
make doom
```

## Running Programs

### Hello World

```bash
make run-hello
```

### DOOM

```bash
# Build and run DOOM
make run-doom
```

**DOOM Controls:**
- Arrow keys: Move
- Ctrl: Fire
- Space: Use/Open doors
- Enter: Activate menu
- Tab: Map
- ESC: Menu
- 1-7: Select weapon

## Project Structure

```
rv32-sim/
├── rv32ima.cc             # Your original RV32IMA emulator
├── rv32ima_ref_sdl.c      # SDL-enabled emulator for DOOM
├── mini-rv32ima-ref.c     # Alternative console emulator
├── mini-rv32ima.h         # Mini emulator header
├── default64mbdtc.h       # Device tree configuration
├── hello.S                # Hello world example
├── src_doom/              # DOOM source code
│   └── riscv/            # RISC-V specific implementation
├── riscv-tests/          # Official RISC-V test suite
└── gui/                  # ImGui debugger (optional)
```

## Testing

```bash
# Run RISC-V compliance tests
make test
```

## Memory Map

- `0x00000000 - 0x03FFFFFF`: RAM (64MB)
- `0x10000000`: UART (console I/O)
- `0x11100000 - 0x111E0000`: Framebuffer (640x480x32)
- `0x11300000`: Timer/RTC registers

## Implementation Details

The emulator is based on mini-rv32ima with extensions for:
- SDL2 framebuffer rendering
- Memory-mapped I/O devices
- Bare-metal DOOM support using custom libc backend
- WAD file embedding via linker symbols

## License

This project is for educational and research purposes.