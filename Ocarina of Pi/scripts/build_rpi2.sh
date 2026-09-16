#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
TARGET_NAME="ocarina_of_pi"
CROSS_COMPILE="arm-linux-gnueabihf-"

echo "============================================"
echo "  Ocarina of Pi - RPi2B Build Script"
echo "============================================"

echo "[1/6] Checking dependencies..."

REQUIRED_PKGS=(
    cmake
    g++-arm-linux-gnueabihf
    libsdl2-dev:armhf
    libgles2-mesa-dev:armhf
    libegl1-mesa-dev:armhf
    libasound2-dev:armhf
    libpthread-stubs0-dev:armhf
    libbcm-host-dev:armhf
)

for pkg in "${REQUIRED_PKGS[@]}"; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        echo "  WARNING: $pkg not found - may need cross-compile toolchain"
    fi
done

echo "[2/6] Creating build directory..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "[3/6] Checking for SDL2-RPI..."
if [ ! -d "${PROJECT_DIR}/../SDL2-RPI" ]; then
    echo "  WARNING: SDL2-RPI directory not found"
    echo "  The pre-packaged SDL2-RPI.tar.gz may be unavailable."
    echo "  Install SDL2 via system packages on the Pi:"
    echo "    sudo apt install -y libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev"
    echo "  Or build from source (see docs/SDL2_SETUP.md)"
    echo "  Continuing with system SDL2..."
    SDL2_PATH="/usr"
else
    SDL2_PATH="${PROJECT_DIR}/../SDL2-RPI"
fi

echo "[4/6] Configuring CMake..."
cmake "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=armv7l \
    -DCMAKE_C_COMPILER="${CROSS_COMPILE}gcc" \
    -DCMAKE_CXX_COMPILER="${CROSS_COMPILE}g++" \
    -DSDL2_PATH="${SDL2_PATH}" \
    -DCMAKE_C_FLAGS="-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto" \
    -DCMAKE_CXX_FLAGS="-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3 -ftree-vectorize -fomit-frame-pointer -flto -fno-exceptions -fno-rtti" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-O3 -Wl,--as-needed"

echo "[5/6] Compiling ${TARGET_NAME}..."
cmake --build . --config Release -- -j4

echo "[6/6] Deployment preparation..."

mkdir -p "${BUILD_DIR}/deploy"
cp "${BUILD_DIR}/${TARGET_NAME}" "${BUILD_DIR}/deploy/"
cp "${PROJECT_DIR}/config/config.txt" "${BUILD_DIR}/deploy/"

echo ""
echo "============================================"
echo "  Build Complete!"
echo "============================================"
echo "  Binary:  ${BUILD_DIR}/deploy/${TARGET_NAME}"
echo "  Config:  ${BUILD_DIR}/deploy/config.txt"
echo ""
echo "  Deploy to SD card:"
echo "    cp -r ${BUILD_DIR}/deploy/* /mnt/sdcard/"
echo ""
echo "  Run natively on RPi2B:"
echo "    ./${TARGET_NAME}"
echo ""

echo "[POST-BUILD] Running post-build automation..."
if [ -x "${PROJECT_DIR}/scripts/post_build.sh" ]; then
    "${PROJECT_DIR}/scripts/post_build.sh"
else
    echo "  WARNING: post_build.sh not found or not executable"
fi

echo "[Optional] Cross-compile verification..."
if command -v qemu-arm &>/dev/null; then
    echo "  qemu-arm found - can verify binary architecture:"
    file "${BUILD_DIR}/${TARGET_NAME}"
    qemu-arm -L /usr/arm-linux-gnueabihf "${BUILD_DIR}/${TARGET_NAME}" --version 2>/dev/null && echo "  Binary architecture OK" || echo "  Note: qemu-arm functional check skipped (no display available)"
else
    echo "  qemu-arm not found - skipping binary verification"
fi
