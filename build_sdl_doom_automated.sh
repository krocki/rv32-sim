#!/bin/bash

# Fully Automated SDL DOOM Image Builder for RISC-V
# No user interaction required

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  SDL DOOM Auto-Builder for RISC-V${NC}"
echo -e "${GREEN}  (Fully Automated - No Input Needed)${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=""

for cmd in git make gcc g++ wget; do
    if ! command -v $cmd &> /dev/null; then
        MISSING_DEPS="$MISSING_DEPS $cmd"
    fi
done

if [ ! -z "$MISSING_DEPS" ]; then
    echo -e "${RED}Missing dependencies:$MISSING_DEPS${NC}"
    echo "Please install them first:"
    echo "  macOS: brew install$MISSING_DEPS"
    echo "  Linux: sudo apt-get install$MISSING_DEPS"
    exit 1
fi

echo -e "${GREEN}✓ Dependencies OK${NC}"

# Create build directory (always clean)
BUILD_DIR="sdl_doom_build"
echo -e "${YELLOW}Creating clean build directory...${NC}"
rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Step 1: Download buildroot
echo -e "${YELLOW}[1/7] Downloading buildroot (this may take a minute)...${NC}"
git clone --depth=1 --branch 2023.02.x https://github.com/buildroot/buildroot.git
cd buildroot

# Step 2: Create board directory structure first
echo -e "${YELLOW}[2/7] Creating board directory structure...${NC}"
mkdir -p board/rv32ima_doom

# Step 3: Create kernel configuration
echo -e "${YELLOW}[3/7] Creating kernel configuration with framebuffer...${NC}"

cat > board/rv32ima_doom/linux.config << 'EOF'
CONFIG_RISCV=y
CONFIG_32BIT=y
CONFIG_ARCH_RV32I=y
CONFIG_RISCV_ISA_M=y
CONFIG_RISCV_ISA_A=y
CONFIG_MMU=n
CONFIG_FPU=n

# Basic kernel features
CONFIG_PRINTK=y
CONFIG_EARLY_PRINTK=y
CONFIG_PANIC_TIMEOUT=10
CONFIG_BLK_DEV_INITRD=y
CONFIG_INITRAMFS_SOURCE=""
CONFIG_RD_GZIP=y

# Serial console
CONFIG_SERIAL_8250=y
CONFIG_SERIAL_8250_CONSOLE=y
CONFIG_SERIAL_OF_PLATFORM=y

# Framebuffer support - KEY FOR SDL!
CONFIG_FB=y
CONFIG_FB_SIMPLE=y
CONFIG_FRAMEBUFFER_CONSOLE=y
CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY=y
CONFIG_LOGO=n
CONFIG_FB_CFB_FILLRECT=y
CONFIG_FB_CFB_COPYAREA=y
CONFIG_FB_CFB_IMAGEBLIT=y
CONFIG_FB_SYS_FILLRECT=y
CONFIG_FB_SYS_COPYAREA=y
CONFIG_FB_SYS_IMAGEBLIT=y
CONFIG_FB_SYS_FOPS=y
CONFIG_FB_DEFERRED_IO=y

# Input for DOOM controls
CONFIG_INPUT=y
CONFIG_INPUT_EVDEV=y
CONFIG_INPUT_KEYBOARD=y

# Memory settings
CONFIG_SLOB=y
CONFIG_EMBEDDED=y
CONFIG_EXPERT=y

# Disable unnecessary features
CONFIG_MODULES=n
CONFIG_BLOCK=y
CONFIG_NETWORK=n
CONFIG_FW_LOADER=n
EOF

# Step 4: Create device tree with framebuffer
echo -e "${YELLOW}[4/7] Creating device tree with framebuffer at 0x11100000...${NC}"

cat > board/rv32ima_doom/rv32ima.dts << 'EOF'
/dts-v1/;

/ {
    #address-cells = <1>;
    #size-cells = <1>;
    compatible = "riscv-minimal-nommu,qemu";
    model = "riscv-minimal-nommu,qemu";

    chosen {
        bootargs = "earlycon=uart8250,mmio,0x10000000,115200n8 console=ttyS0 fb=simple";
        stdout-path = "uart0:115200n8";
    };

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x04000000>; /* 64MB */
    };

    cpus {
        #address-cells = <1>;
        #size-cells = <0>;
        timebase-frequency = <1000000>;
        
        cpu@0 {
            device_type = "cpu";
            reg = <0>;
            status = "okay";
            compatible = "riscv";
            riscv,isa = "rv32ima";
            mmu-type = "riscv,none";
            clock-frequency = <1000000>;
        };
    };

    soc {
        #address-cells = <1>;
        #size-cells = <1>;
        compatible = "simple-bus";
        ranges;

        uart0: uart@10000000 {
            compatible = "ns16850";
            reg = <0x10000000 0x100>;
            clock-frequency = <10000000>;
            reg-shift = <0>;
            reg-io-width = <1>;
        };

        framebuffer@11100000 {
            compatible = "simple-framebuffer";
            reg = <0x11100000 0x4B000>; /* 640x480x32bpp = 1228800 bytes */
            width = <640>;
            height = <480>;
            stride = <2560>; /* 640 * 4 bytes per pixel */
            format = "a8r8g8b8";
            status = "okay";
        };

        syscon@11100000 {
            compatible = "syscon", "simple-mfd";
            reg = <0x11100000 0x1000>;
            
            reboot {
                compatible = "syscon-reboot";
                regmap = <&syscon>;
                offset = <0x0>;
                value = <0x7777>;
                mask = <0xffff>;
            };

            poweroff {
                compatible = "syscon-poweroff";
                regmap = <&syscon>;
                offset = <0x0>;
                value = <0x5555>;
                mask = <0xffff>;
            };
        };
    };
};
EOF

# Step 5: Create post-build script to add DOOM WADs
echo -e "${YELLOW}[5/7] Setting up DOOM WAD files...${NC}"

cat > board/rv32ima_doom/post_build.sh << 'EOF'
#!/bin/bash
TARGET_DIR=$1

# Create doom directory
mkdir -p $TARGET_DIR/usr/share/games/doom

# Download shareware DOOM WAD if not present
if [ ! -f "$TARGET_DIR/usr/share/games/doom/doom1.wad" ]; then
    echo "Downloading DOOM shareware WAD..."
    # Try to download from archive.org
    wget -q -O /tmp/doom_shareware.zip "https://archive.org/download/DoomsharewareEpisode/doom.ZIP" || true
    if [ -f /tmp/doom_shareware.zip ]; then
        unzip -j -o /tmp/doom_shareware.zip "*/DOOM1.WAD" -d $TARGET_DIR/usr/share/games/doom/ 2>/dev/null || true
        # Make lowercase
        mv $TARGET_DIR/usr/share/games/doom/DOOM1.WAD $TARGET_DIR/usr/share/games/doom/doom1.wad 2>/dev/null || true
    fi
    
    if [ ! -f "$TARGET_DIR/usr/share/games/doom/doom1.wad" ]; then
        echo "Note: DOOM WAD download failed. You'll need to add doom1.wad manually"
    fi
fi

# Create startup script
cat > $TARGET_DIR/etc/init.d/S99doom << 'SCRIPT'
#!/bin/sh
case "$1" in
    start)
        echo "Starting framebuffer..."
        if [ -e /dev/fb0 ]; then
            echo "Framebuffer available at /dev/fb0"
            # Clear framebuffer to red (test pattern)
            dd if=/dev/zero of=/dev/fb0 bs=1024 count=1200 2>/dev/null || true
        else
            echo "WARNING: No framebuffer found at /dev/fb0"
        fi
        
        echo ""
        echo "====================================="
        echo "    DOOM is ready to run!"
        echo "====================================="
        echo ""
        echo "Type: chocolate-doom"
        echo "Or:   doom"
        echo ""
        ;;
    *)
        ;;
esac
SCRIPT

chmod +x $TARGET_DIR/etc/init.d/S99doom

# Create helper script
cat > $TARGET_DIR/usr/bin/doom << 'DOOMSCRIPT'
#!/bin/sh
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb0
export SDL_NOMOUSE=1

if [ -f /usr/share/games/doom/doom1.wad ]; then
    chocolate-doom -iwad /usr/share/games/doom/doom1.wad $@
else
    echo "Error: doom1.wad not found!"
    echo "DOOM WAD files should be in /usr/share/games/doom/"
    chocolate-doom $@
fi
DOOMSCRIPT
chmod +x $TARGET_DIR/usr/bin/doom

echo "DOOM setup complete!"
EOF

chmod +x board/rv32ima_doom/post_build.sh

# Step 6: Create custom configuration
echo -e "${YELLOW}[6/7] Creating RISC-V SDL DOOM configuration...${NC}"

cat > configs/rv32ima_sdl_doom_defconfig << 'EOF'
# Architecture
BR2_riscv=y
BR2_RISCV_32=y
BR2_RISCV_ABI_ILP32=y
BR2_RISCV_ISA_RVM=y
BR2_RISCV_ISA_RVA=y

# System
BR2_SYSTEM_DHCP="eth0"
BR2_TARGET_GENERIC_GETTY_PORT="console"
BR2_TARGET_GENERIC_GETTY_BAUDRATE_115200=y
BR2_TARGET_GENERIC_HOSTNAME="rv32doom"
BR2_TARGET_GENERIC_ISSUE="Welcome to RISC-V DOOM"

# Toolchain
BR2_TOOLCHAIN_BUILDROOT_GLIBC=y
BR2_TOOLCHAIN_BUILDROOT_CXX=y

# Kernel
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.19.0"
BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG=y
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="board/rv32ima_doom/linux.config"
BR2_LINUX_KERNEL_IMAGE=y
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_CUSTOM_DTS_PATH="board/rv32ima_doom/rv32ima.dts"

# Packages - Core
BR2_PACKAGE_BUSYBOX_SHOW_OTHERS=y

# Graphics/SDL2 - Minimal configuration
BR2_PACKAGE_SDL2=y

# DOOM! 
BR2_PACKAGE_CHOCOLATE_DOOM=y

# Filesystem - Embed everything in kernel
BR2_TARGET_ROOTFS_INITRAMFS=y

# Post-build scripts
BR2_ROOTFS_POST_BUILD_SCRIPT="board/rv32ima_doom/post_build.sh"

# Optimize for size
BR2_OPTIMIZE_S=y
EOF

# Step 7: Apply configuration and build
echo -e "${YELLOW}[7/7] Configuring buildroot...${NC}"
make rv32ima_sdl_doom_defconfig

# Start build automatically
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Starting build process...${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}This will take 30-90 minutes.${NC}"

# Detect number of CPU cores (macOS and Linux compatible)
if command -v nproc &> /dev/null; then
    JOBS=$(nproc)
elif command -v sysctl &> /dev/null; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi

echo -e "${YELLOW}Building with $JOBS parallel jobs...${NC}"
echo ""

# Build with reduced verbosity
make -j$JOBS 2>&1 | while IFS= read -r line; do
    # Show only important messages
    if [[ $line == *">>>"* ]] || [[ $line == *"Installing"* ]] || [[ $line == *"Building"* ]] || [[ $line == *"ERROR"* ]] || [[ $line == *"Warning"* ]]; then
        echo "$line"
    fi
done

# Check if build succeeded
if [ -f "output/images/Image" ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  BUILD SUCCESSFUL!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    # Copy to main directory
    cp output/images/Image ../../sdl_doom.bin
    
    echo -e "${GREEN}Image created: sdl_doom.bin${NC}"
    echo ""
    echo "To run SDL DOOM:"
    echo "  ./rv32ima_doom sdl_doom.bin"
    echo ""
    echo "In the emulator:"
    echo "  1. Wait for Linux to boot"
    echo "  2. Login as: root"
    echo "  3. Type: doom"
    echo ""
    echo "DOOM Controls:"
    echo "  Arrow Keys - Move/Turn"
    echo "  Ctrl - Fire"
    echo "  Space - Use/Open"
    echo "  Tab - Map"
    
    # Also create a launcher script
    cd ../..
    cat > run_sdl_doom.sh << 'LAUNCHER'
#!/bin/bash
echo "Starting SDL DOOM on RISC-V..."
echo ""
echo "Instructions:"
echo "1. Wait for boot (5-10 seconds)"
echo "2. Login as: root"
echo "3. Type: doom"
echo ""
./rv32ima_doom sdl_doom.bin
LAUNCHER
    chmod +x run_sdl_doom.sh
    
    echo ""
    echo -e "${GREEN}Quick launcher created: ./run_sdl_doom.sh${NC}"
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}  BUILD FAILED!${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "Check the build output above for errors."
    echo "Common issues:"
    echo "  - Missing dependencies (try: make menuconfig)"
    echo "  - Network issues downloading packages"
    echo "  - Insufficient disk space (need ~5GB)"
    exit 1
fi