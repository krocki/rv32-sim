# Cleanup Instructions for RV32-SIM

## Current Status

The project has been restructured with:
1. **Modular headers created**: `rv32ima_core.h` and `rv32ima_sdl.h` for clean separation
2. **New Makefile**: `Makefile.new` with proper targets for all build variants
3. **Updated README**: `README_NEW.md` with comprehensive documentation
4. **Cleanup plan**: `cleanup_plan.txt` listing what to keep/remove

## Working Files

The current **working** emulator that runs DOOM successfully is:
- `rv32ima_ref_sdl.c` - This is the tested and working SDL emulator
- `src_doom/` - The DOOM source directory with all necessary files

## Recommended Actions

### 1. Backup Current State
```bash
git add -A
git commit -m "Backup before major cleanup"
git checkout -b cleanup-backup
```

### 2. Use the Working Emulator
Since `rv32ima_ref_sdl.c` is already working perfectly with DOOM, we should:
```bash
# Keep the working implementation
cp rv32ima_ref_sdl.c rv32ima.c

# Apply the new Makefile
mv Makefile Makefile.old
mv Makefile.new Makefile

# Update README
mv README.md README.old.md  
mv README_NEW.md README.md
```

### 3. Essential Files to Keep

```bash
# Core emulator
rv32ima.c (renamed from rv32ima_ref_sdl.c)
default64mbdtc.h
mini-rv32ima.h

# Build files
Makefile
run_tests.sh

# DOOM directory
src_doom/ (entire directory)

# Examples
hello.S
add.s
tests/

# GUI debugger (optional)
gui/imgui/
gui/rv32i-gui.cc

# Documentation
README.md
LICENSE
```

### 4. Remove Unnecessary Files

```bash
# Remove old emulator variants
rm -f rv32ima_doom*.cc rv32ima_mmio.cc rv32i*.cc
rm -f rv32ima_doom* rv32ima_mmio rv32i rv32im

# Remove buildroot/Linux files (not needed for bare-metal)
rm -rf buildroot* images/
rm -f *.tar.gz

# Remove Docker/build scripts
rm -f Dockerfile.doom build_*_doom*.sh download_doom*.sh

# Remove alternative implementations
rm -rf risc-666/ smunaut_doom/ doom-riscv/ baremetal_doom/

# Remove temp files
rm -rf test_temp/ sdl_doom_build/
rm -f *.o *.bin *.elf (except in src_doom/riscv/)

# Remove old docs
rm -f DOOM_*.md SDL_DOOM_GUIDE.md README_old.md
```

### 5. Final Directory Structure

After cleanup, you should have:
```
rv32-sim/
├── Makefile              # Build system
├── README.md             # Documentation
├── rv32ima.c            # Main emulator (with SDL support)
├── default64mbdtc.h     # Device tree
├── mini-rv32ima.h       # Core definitions
├── src_doom/            # DOOM source
│   ├── riscv/          # RISC-V specific
│   └── *.c/h           # Game source
├── tests/               # Test programs
├── gui/                 # Optional GUI debugger
│   └── imgui/
├── hello.S              # Example program
├── add.s                # Example program
└── run_tests.sh         # Test runner
```

### 6. Test After Cleanup

```bash
# Build emulator
make clean
make

# Test with hello world
make test-hello

# Run DOOM
make doom
make run-doom
```

### 7. Commit Clean State

```bash
git add -A
git commit -m "Major cleanup: streamlined project structure

- Consolidated emulator implementation
- Removed redundant files and old implementations  
- Updated build system and documentation
- Kept only essential files for building and running DOOM"
```

## Notes

- The current `rv32ima_ref_sdl.c` is the **working** implementation that successfully runs DOOM
- Don't modify the core emulation logic, just rename and organize
- The `src_doom/` directory should remain untouched as it's working
- Make sure to test DOOM after cleanup to verify nothing broke

## Quick Test Commands

After cleanup, these should work:
```bash
# Build and run DOOM
make && make doom && ./rv32ima -f src_doom/riscv/doom-riscv.bin -c 800000000
```