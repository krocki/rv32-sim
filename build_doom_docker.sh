#!/bin/bash

# Docker-based SDL DOOM builder for macOS/Linux compatibility

set -e

echo "========================================="
echo "  Docker-based SDL DOOM Builder"
echo "========================================="
echo ""

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo "❌ Docker not found!"
    echo ""
    echo "Please install Docker:"
    echo "  macOS: https://docs.docker.com/desktop/mac/"
    echo "  Linux: sudo apt-get install docker.io"
    echo ""
    exit 1
fi

echo "✅ Docker found"

# Create Dockerfile for buildroot environment
cat > Dockerfile.doom << 'EOF'
FROM ubuntu:20.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install buildroot dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    wget \
    cpio \
    unzip \
    rsync \
    bc \
    libncurses5-dev \
    python3 \
    python3-distutils \
    file \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy our configuration
COPY configs/rv32_simple_doom_defconfig /build/
COPY board/ /build/board/

# Entry script
COPY build_in_docker.sh /build/
RUN chmod +x /build/build_in_docker.sh

CMD ["/build/build_in_docker.sh"]
EOF

# Create build script for inside Docker
cat > build_in_docker.sh << 'EOF'
#!/bin/bash
set -e

echo "Building SDL DOOM in Docker..."

# Clone buildroot if not present
if [ ! -d "buildroot" ]; then
    git clone --depth=1 --branch 2023.02.x https://github.com/buildroot/buildroot.git
fi

cd buildroot

# Copy our config
cp ../rv32_simple_doom_defconfig configs/
cp -r ../board/rv32ima_doom board/ || true

# Load config and build
make rv32_simple_doom_defconfig
make -j$(nproc)

# Copy output
cp output/images/Image /output/sdl_doom.bin
echo "Build complete! Output: /output/sdl_doom.bin"
EOF

chmod +x build_in_docker.sh

# Prepare build context
mkdir -p docker_build_context
cp -r sdl_doom_build/buildroot/configs docker_build_context/ 2>/dev/null || mkdir -p docker_build_context/configs
cp -r sdl_doom_build/buildroot/board docker_build_context/ 2>/dev/null || mkdir -p docker_build_context/board

# Copy our simple config
cp sdl_doom_build/buildroot/configs/rv32_simple_doom_defconfig docker_build_context/configs/

# Build Docker image
echo "Building Docker image..."
cp Dockerfile.doom docker_build_context/
cp build_in_docker.sh docker_build_context/

docker build -t rv32-doom-builder -f docker_build_context/Dockerfile.doom docker_build_context/

# Run build in Docker with output volume
echo ""
echo "Starting build in Docker container..."
echo "This will take 30-60 minutes..."

mkdir -p output
docker run --rm \
    -v "$(pwd)/output:/output" \
    rv32-doom-builder

if [ -f "output/sdl_doom.bin" ]; then
    echo ""
    echo "✅ Build successful!"
    echo "📁 Output: $(pwd)/output/sdl_doom.bin"
    echo ""
    echo "To run SDL DOOM:"
    echo "  ./rv32ima_doom output/sdl_doom.bin"
    echo ""
    
    # Create launcher
    cat > run_docker_doom.sh << 'LAUNCHER'
#!/bin/bash
echo "Starting SDL DOOM built in Docker..."
./rv32ima_doom output/sdl_doom.bin
LAUNCHER
    chmod +x run_docker_doom.sh
    
    echo "Quick launcher: ./run_docker_doom.sh"
else
    echo ""
    echo "❌ Build failed!"
    echo "Check Docker container logs above for errors."
    exit 1
fi

# Cleanup
rm -f Dockerfile.doom build_in_docker.sh
rm -rf docker_build_context

echo ""
echo "🎮 Ready to play DOOM on RISC-V!"