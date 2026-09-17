#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
TARGET_NAME="ocarina_of_pi"
CROSS_COMPILE=""

echo "============================================"
echo "  Ocarina of Pi - RPi2B Build Script"
echo "============================================"

echo "[1/5] Checking toolchain availability..."
if ! command -v "${CROSS_COMPILE}gcc" &>/dev/null; then
    echo "  ERROR: ${CROSS_COMPILE}gcc not found"
    echo "  Install with: sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf"
    exit 1
fi
echo "  Toolchain found: $(${CROSS_COMPILE}gcc --version | head -1)"

echo "[2/5] Creating build directory..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "[3/5] Checking for SDL2..."
SDL2_PATH_ARG=""
if [ -d "${PROJECT_DIR}/../SDL2-RPI" ]; then
    echo "  SDL2-RPI found at ${PROJECT_DIR}/../SDL2-RPI"
    SDL2_PATH_ARG="-DSDL2_PATH=${PROJECT_DIR}/../SDL2-RPI"
else
    echo "  SDL2-RPI not found - will use system packages"
    echo "  Install: sudo apt install -y libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev"
    SDL2_PATH_ARG="-DSDL2_PATH=/usr"
fi

echo "[4/5] Configuring native CMake build..."
cmake "${PROJECT_DIR}" \
    ${SDL2_PATH_ARG} \
    -DCMAKE_BUILD_TYPE=Release

echo "[5/5] Compiling ${TARGET_NAME}..."
cmake --build "${BUILD_DIR}" --config Release -- -j4

echo ""
echo "============================================"
echo "  Build Complete!"
echo "============================================"
echo "  Binary:  ${BUILD_DIR}/${TARGET_NAME}"
echo ""
echo "  Deploy to SD card:"
echo "    cp ${BUILD_DIR}/${TARGET_NAME} /mnt/sdcard/"
echo "    cp ${PROJECT_DIR}/config/config.txt /mnt/sdcard/"
echo ""
echo "  Run natively on RPi2B:"
echo "    cd / && ./ocarina_of_pi"
echo ""

echo "[POST-BUILD] Running post-build automation..."
if [ "${RUN_POST_BUILD:-0}" = "1" ] && [ -x "${PROJECT_DIR}/scripts/post_build.sh" ]; then
    "${PROJECT_DIR}/scripts/post_build.sh"
else
    echo "  Post-build deployment skipped (set RUN_POST_BUILD=1 after integrating a real port)."
fi