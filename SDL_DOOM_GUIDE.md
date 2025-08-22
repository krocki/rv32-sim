# Running SDL DOOM on RISC-V Simulator

## The Current Situation

The `Image-emdoom-MAX_ORDER_14` from cnlohr's project uses **emdoom**, which renders DOOM in the terminal using ASCII characters. To get SDL DOOM with real graphics, you need a different Linux image.

## Option 1: Build Your Own SDL DOOM Image

### Prerequisites
```bash
# Install buildroot
git clone https://github.com/buildroot/buildroot.git
cd buildroot
```

### Configure Buildroot for RISC-V with SDL DOOM

1. Create a custom buildroot configuration:

```bash
make menuconfig
```

2. Configure these settings:
   - Target Architecture: RISCV
   - Target Architecture Variant: RV32IMA
   - Target ABI: ilp32
   - System configuration → Init system: BusyBox
   - Kernel → Linux Kernel: Enable
   - Kernel → Kernel version: 5.19.x
   - Target packages → Games → chocolate-doom or prboom-plus
   - Target packages → Graphics → SDL2
   - Target packages → Graphics → SDL2_image
   - Target packages → Graphics → SDL2_mixer
   - Target packages → Graphics → SDL2_net
   - Filesystem images → ext2/3/4 root filesystem

3. Add framebuffer support to kernel:
```bash
make linux-menuconfig
```
Enable:
- Device Drivers → Graphics support → Frame buffer Devices
- Device Drivers → Graphics support → Support for simple framebuffer

4. Build the image:
```bash
make -j$(nproc)
```

5. The output will be in `output/images/rootfs.ext2` and `output/images/Image`

## Option 2: Use Pre-built SDL DOOM Images

Check these repositories for pre-built images with graphical DOOM:
- https://github.com/cnlohr/mini-rv32ima (may have other images)
- https://github.com/sifive/freedom-u-sdk
- RISC-V buildroot examples

## Option 3: Modify Our Emulator for Graphical Linux

Our `rv32ima_doom.cc` already has SDL2 framebuffer support at 0x11100000. To use it:

1. Build a Linux kernel with framebuffer driver that writes to 0x11100000
2. Configure the device tree (DTB) to include:
```dts
framebuffer@11100000 {
    compatible = "simple-framebuffer";
    reg = <0x11100000 0x4B000>;  /* 640x480x4 */
    width = <640>;
    height = <480>;
    stride = <2560>;
    format = "a8r8g8b8";
};
```

3. Install chocolate-doom or prboom in the rootfs
4. Configure DOOM to use /dev/fb0

## Building the SDL Emulator

We already have `rv32ima_doom.cc` with SDL support:

```bash
# Ensure SDL2 is installed
brew install sdl2  # macOS
# or
sudo apt-get install libsdl2-dev  # Linux

# Build the SDL version
g++ -O3 -o rv32ima_doom rv32ima_doom.cc `sdl2-config --cflags --libs` -lm

# Run with a framebuffer-enabled Linux image
./rv32ima_doom your-custom-image.bin
```

## Memory Map for SDL Version

Our SDL emulator (`rv32ima_doom.cc`) uses:
- 0x10000000: UART (console I/O)
- 0x11000000: System control
- 0x11100000: Framebuffer (640x480, 32-bit color)
- 0x11004000: Timer
- 0x11003000: Interrupt controller

## Quick Test with Framebuffer

To test if an image has framebuffer support:

```bash
# Boot the image
./rv32ima_doom image.bin

# Inside Linux, check for framebuffer
ls /dev/fb*
cat /proc/fb

# Write test pattern (if fb0 exists)
dd if=/dev/urandom of=/dev/fb0 bs=1024 count=100
```

## Why emdoom Uses Console Instead

The `Image-emdoom-MAX_ORDER_14` was specifically built for minimal systems without graphics hardware. It's actually quite clever - DOOM rendered in ASCII art! For true SDL graphics, you need:

1. Kernel with framebuffer driver
2. SDL libraries in userspace  
3. Graphical DOOM port (chocolate-doom, prboom, etc.)
4. Proper memory mapping between emulator and kernel

## Next Steps

1. **Easiest**: Look for other pre-built images from cnlohr's repo that might include graphical DOOM
2. **Educational**: Build your own image with buildroot (takes 1-2 hours)
3. **Advanced**: Modify the kernel to use our emulator's framebuffer at 0x11100000

The SDL infrastructure is ready in `rv32ima_doom.cc` - you just need a Linux image that knows how to use it!