#!/bin/bash

cd buildroot-doom

echo "Fixing buildroot configuration..."

# Clean previous build
make clean

# Use the RISC-V 32-bit QEMU virtual machine configuration as base
make qemu_riscv32_virt_defconfig

# Enable required packages
cat >> .config << 'EOF'
BR2_PACKAGE_CHOCOLATE_DOOM=y
BR2_PACKAGE_SDL2=y
BR2_PACKAGE_SDL2_IMAGE=y
BR2_PACKAGE_SDL2_MIXER=y
BR2_PACKAGE_FBSET=y
BR2_PACKAGE_FB_TEST_APP=y
BR2_TARGET_ROOTFS_CPIO=y
BR2_TARGET_ROOTFS_CPIO_GZIP=y
EOF

# Update config
make olddefconfig

echo "Configuration fixed. Now build with:"
echo "  make -j8"
echo ""
echo "After building, the kernel will be at:"
echo "  output/images/Image"
echo ""
echo "Run with:"
echo "  ./rv32ima_ref_sdl -f buildroot-doom/output/images/Image"