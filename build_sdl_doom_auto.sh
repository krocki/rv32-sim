#!/bin/bash

# Automated SDL DOOM Image Builder for RISC-V
# This script automates the entire process of building a graphical DOOM for our emulator

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  SDL DOOM Auto-Builder for RISC-V${NC}"
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

# Create build directory
BUILD_DIR="sdl_doom_build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory exists. Clean build? (y/n)${NC}"
    read -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$BUILD_DIR"
        mkdir "$BUILD_DIR"
    fi
else
    mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Step 1: Download buildroot
if [ ! -d "buildroot" ]; then
    echo -e "${YELLOW}[1/6] Downloading buildroot...${NC}"
    git clone --depth=1 --branch 2023.02.x https://github.com/buildroot/buildroot.git
else
    echo -e "${GREEN}[1/6] Buildroot already downloaded${NC}"
fi

cd buildroot

# Step 2: Create custom configuration
echo -e "${YELLOW}[2/6] Creating RISC-V SDL DOOM configuration...${NC}"

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

# Kernel
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.19.0"
BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG=y
BR2_LINUX_KERNEL_IMAGE=y

# Packages - Core
BR2_PACKAGE_BUSYBOX_SHOW_OTHERS=y
BR2_PACKAGE_BASH=y

# Packages - Graphics/SDL2
BR2_PACKAGE_SDL2=y
BR2_PACKAGE_SDL2_KMSDRM=n
BR2_PACKAGE_SDL2_OPENGL=n
BR2_PACKAGE_SDL2_OPENGLES=n
BR2_PACKAGE_SDL2_X11=n
BR2_PACKAGE_DIRECTFB=n

# Simple framebuffer support
BR2_PACKAGE_FB_TEST_APP=y

# DOOM! 
BR2_PACKAGE_CHOCOLATE_DOOM=y

# Audio support (optional, might work)
BR2_PACKAGE_ALSA_UTILS=y
BR2_PACKAGE_ALSA_LIB=y

# Filesystem
BR2_TARGET_ROOTFS_INITRAMFS=y
BR2_TARGET_ROOTFS_INITRAMFS_CUSTOM_CONTENTS="dev dev/console c 5 1"

# Host tools
BR2_PACKAGE_HOST_DOSFSTOOLS=y
BR2_PACKAGE_HOST_GENIMAGE=y
BR2_PACKAGE_HOST_MTOOLS=y

# Optimize for size
BR2_OPTIMIZE_S=y
EOF

# Step 3: Create kernel configuration
echo -e "${YELLOW}[3/6] Creating kernel configuration with framebuffer...${NC}"

mkdir -p board/rv32ima_doom

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
echo -e "${YELLOW}[4/6] Creating device tree with framebuffer at 0x11100000...${NC}"

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
echo -e "${YELLOW}[5/6] Setting up DOOM WAD files...${NC}"

cat > board/rv32ima_doom/post_build.sh << 'EOF'
#!/bin/bash
TARGET_DIR=$1

# Create doom directory
mkdir -p $TARGET_DIR/usr/share/games/doom

# Download shareware DOOM WAD if not present
if [ ! -f "$TARGET_DIR/usr/share/games/doom/doom1.wad" ]; then
    echo "Downloading DOOM shareware WAD..."
    wget -q -O /tmp/doom19s.zip https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad
    if [ -f /tmp/doom1.wad ]; then
        cp /tmp/doom1.wad $TARGET_DIR/usr/share/games/doom/
    else
        # Alternative: extract from doom shareware
        echo "Note: DOOM WAD not found. You'll need to add doom1.wad manually"
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
            # Clear framebuffer
            dd if=/dev/zero of=/dev/fb0 2>/dev/null || true
        else
            echo "No framebuffer found"
        fi
        
        echo ""
        echo "====================================="
        echo "    DOOM is ready to run!"
        echo "====================================="
        echo ""
        echo "Type: chocolate-doom"
        echo "Or:   chocolate-doom -iwad /usr/share/games/doom/doom1.wad"
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
chocolate-doom -iwad /usr/share/games/doom/doom1.wad $@
DOOMSCRIPT
chmod +x $TARGET_DIR/usr/bin/doom

echo "DOOM setup complete!"
EOF

chmod +x board/rv32ima_doom/post_build.sh

# Step 6: Configure buildroot
echo -e "${YELLOW}[6/6] Configuring buildroot...${NC}"

# Update main config to use our board
cat >> configs/rv32ima_sdl_doom_defconfig << EOF

# Board specific
BR2_ROOTFS_POST_BUILD_SCRIPT="board/rv32ima_doom/post_build.sh"
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="board/rv32ima_doom/linux.config"
BR2_LINUX_KERNEL_DTS_SUPPORT=y
BR2_LINUX_KERNEL_CUSTOM_DTS_PATH="board/rv32ima_doom/rv32ima.dts"
EOF

# Apply configuration
make rv32ima_sdl_doom_defconfig

# Optional: Open menuconfig for manual adjustments
echo ""
echo -e "${YELLOW}Configuration ready! Adjust settings? (y/n)${NC}"
read -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    make menuconfig
fi

# Build!
echo ""
echo -e "${GREEN}Ready to build!${NC}"
echo -e "${YELLOW}This will take 30-90 minutes depending on your system.${NC}"
echo -e "${YELLOW}Start build now? (y/n)${NC}"
read -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${GREEN}Building SDL DOOM image...${NC}"
    make -j$(nproc) 2>&1 | tee build.log
    
    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}========================================${NC}"
        echo -e "${GREEN}  BUILD SUCCESSFUL!${NC}"
        echo -e "${GREEN}========================================${NC}"
        echo ""
        echo "Output image: $(pwd)/output/images/Image"
        echo ""
        echo "To run DOOM:"
        echo "  1. Go back to main directory: cd ../.."
        echo "  2. Copy the image: cp $BUILD_DIR/buildroot/output/images/Image ./sdl_doom.bin"
        echo "  3. Run with SDL emulator: ./rv32ima_doom sdl_doom.bin"
        echo ""
        echo "In the emulator:"
        echo "  - Login as: root"
        echo "  - Type: doom"
        echo ""
        
        # Copy to main directory
        cp output/images/Image ../../sdl_doom.bin
        echo -e "${GREEN}Image copied to: sdl_doom.bin${NC}"
    else
        echo -e "${RED}Build failed! Check build.log for details${NC}"
        exit 1
    fi
else
    echo "Build cancelled. To build later:"
    echo "  cd $BUILD_DIR/buildroot"
    echo "  make -j\$(nproc)"
fi