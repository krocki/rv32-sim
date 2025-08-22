#!/bin/bash

# Docker-based buildroot DOOM build for RISC-V

echo "Building DOOM for RISC-V using Docker..."

# Create Dockerfile
cat > Dockerfile.doom << 'EOF'
FROM ubuntu:22.04

# Install buildroot dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    wget \
    git \
    python3 \
    python3-pip \
    unzip \
    bc \
    cpio \
    rsync \
    file \
    libncurses5-dev \
    libssl-dev \
    patch \
    bison \
    flex \
    && rm -rf /var/lib/apt/lists/*

# Copy buildroot directory
COPY buildroot-doom /buildroot-doom

# Set working directory
WORKDIR /buildroot-doom

# Build command
CMD ["make", "-j8"]
EOF

# Build Docker image
docker build -f Dockerfile.doom -t buildroot-doom .

# Run build in Docker
docker run -v "$(pwd)/buildroot-doom:/buildroot-doom" buildroot-doom

echo "Build complete! Check buildroot-doom/output/images/ for the resulting image."