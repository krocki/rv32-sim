#!/bin/bash

# Script to help build a Linux image with SDL DOOM support

echo "SDL DOOM Image Builder for RISC-V"
echo "=================================="
echo ""

# Check if buildroot exists
if [ ! -d "buildroot" ]; then
    echo "Downloading buildroot..."
    git clone --depth=1 https://github.com/buildroot/buildroot.git
fi

cd buildroot

# Create our custom config
cat > configs/rv32ima_sdl_doom_defconfig << 'EOF'
BR2_riscv=y
BR2_RISCV_32=y
BR2_RISCV_ABI_ILP32=y
BR2_TOOLCHAIN_BUILDROOT_UCLIBC=y
BR2_KERNEL_HEADERS_5_19=y
BR2_BINUTILS_VERSION_2_38_X=y
BR2_GCC_VERSION_12_X=y
BR2_TOOLCHAIN_BUILDROOT_CXX=y
BR2_TARGET_GENERIC_GETTY_PORT="ttyS0"
BR2_TARGET_GENERIC_GETTY_BAUDRATE_115200=y
BR2_ROOTFS_POST_IMAGE_SCRIPT="support/scripts/genimage.sh"
BR2_LINUX_KERNEL=y
BR2_LINUX_KERNEL_CUSTOM_VERSION=y
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="5.19"
BR2_LINUX_KERNEL_DEFCONFIG="rv32_defconfig"
BR2_LINUX_KERNEL_IMAGE=y
BR2_PACKAGE_SDL2=y
BR2_PACKAGE_SDL2_IMAGE=y
BR2_PACKAGE_SDL2_MIXER=y
BR2_PACKAGE_SDL2_NET=y
BR2_PACKAGE_SDL2_TTF=y
BR2_PACKAGE_CHOCOLATE_DOOM=y
BR2_TARGET_ROOTFS_EXT2=y
BR2_TARGET_ROOTFS_EXT2_4=y
BR2_TARGET_ROOTFS_EXT2_SIZE="128M"
BR2_PACKAGE_HOST_GENIMAGE=y
EOF

echo "Configuration created!"
echo ""
echo "To build the image:"
echo "  cd buildroot"
echo "  make rv32ima_sdl_doom_defconfig"
echo "  make menuconfig  # (optional) adjust settings"
echo "  make -j\$(nproc)"
echo ""
echo "The build will take 1-2 hours depending on your system."
echo "Output will be in: buildroot/output/images/"
echo ""
echo "Note: You'll need to modify the kernel config to add framebuffer support:"
echo "  make linux-menuconfig"
echo "  Enable: Device Drivers -> Graphics support -> Frame buffer Devices"
echo ""
echo "For our emulator, you'll also need to patch the device tree to map"
echo "the framebuffer to address 0x11100000"