#!/bin/bash

# Quick build script for RISC-V SDL DOOM on Linux
# Copy and run this on a Linux machine

set -e
echo "RISC-V SDL DOOM Build Script for Linux"

# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential gcc g++ make wget git python3 \
    unzip bc cpio rsync file libncurses5-dev libssl-dev patch bison flex

# Clone buildroot
git clone --depth=1 --branch=2023.02.x https://github.com/buildroot/buildroot.git buildroot-doom
cd buildroot-doom

# Setup directories
mkdir -p board/rv32-doom

# Download DOOM WAD
wget -O board/rv32-doom/doom1.wad https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad

# Create post-build script
cat > board/rv32-doom/post-build.sh << 'EEOF'
#!/bin/sh
set -e
TARGET_DIR=$1
mkdir -p ${TARGET_DIR}/usr/games/doom
cp board/rv32-doom/doom1.wad ${TARGET_DIR}/usr/games/doom/ || true
echo "#!/bin/sh" > ${TARGET_DIR}/usr/bin/start_doom
echo "cd /usr/games/doom && chocolate-doom -iwad doom1.wad" >> ${TARGET_DIR}/usr/bin/start_doom
chmod +x ${TARGET_DIR}/usr/bin/start_doom
EEOF
chmod +x board/rv32-doom/post-build.sh

# Create defconfig
cat > configs/rv32_doom_defconfig << 'EEOF'
BR2_riscv=y
BR2_RISCV_32=y
BR2_RISCV_ISA_RVM=y
BR2_RISCV_ISA_RVA=y
BR2_TOOLCHAIN_BUILDROOT_GLIBC=y
BR2_TOOLCHAIN_BUILDROOT_CXX=y
BR2_ROOTFS_POST_BUILD_SCRIPT="board/rv32-doom/post-build.sh"
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.15.18"
BR2_LINUX_KERNEL_DEFCONFIG="rv32"
BR2_LINUX_KERNEL_IMAGE=y
BR2_TARGET_ROOTFS_CPIO=y
BR2_TARGET_ROOTFS_CPIO_GZIP=y
BR2_PACKAGE_SDL2=y
BR2_PACKAGE_CHOCOLATE_DOOM=y
BR2_PACKAGE_FBSET=y
BR2_PACKAGE_FB_TEST_APP=y
EEOF

# Build
make rv32_doom_defconfig
make -j$(nproc)

echo "Build complete! Output: output/images/Image"
echo "Copy to Mac and run: ./rv32ima_ref_sdl -f Image"
