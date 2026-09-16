# SDL2 Setup Guide for Raspberry Pi 2B

> **Purpose**: Resolve the issue where `SDL2-RPI.tar.gz` is unavailable, and provide reliable methods to install and configure SDL2 with VideoCore IV hardware acceleration on Raspberry Pi OS.

This guide covers three approaches — from easiest (system packages) to most flexible (source build). Choose the method that fits your needs.

---

## Table of Contents

1. [Method A: System Package Installation (Recommended)](#method-a-system-package-installation)
2. [Method B: Clone SDL2-RPI from GitHub](#method-b-clone-sdl2-rpi-from-github)
3. [Method C: Build SDL2 from Official Source](#method-c-build-sdl2-from-official-source)
4. [KMSDRM Video Pipeline Setup](#kmsdrm-video-pipeline-setup)
5. [Verification](#verification)
6. [Integration with Ocarina of Pi](#integration-with-ocarina-of-pi)

---

## Method A: System Package Installation (Recommended)

This is the fastest and most reliable method. Raspberry Pi OS includes pre-compiled SDL2 libraries that are already linked against the VideoCore IV GPU drivers.

### Step A.1 — Update Package Manager

```bash
sudo apt update
sudo apt upgrade -y
sudo apt autoremove -y
sudo apt clean
```

### Step A.2 — Install SDL2 and Dependencies

```bash
sudo apt install -y libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

### Step A.3 — Install GPU and Display Dependencies

```bash
sudo apt install -y libgles2-mesa-dev libegl1-mesa-dev libbcm-host-dev
sudo apt install -y libdrm-dev libgbm-dev libexpat1-dev
sudo apt install -y libraspberrypi-dev
```

### Step A.4 — Verify Installation

```bash
sdl2-config --version
sdl2-config --cflags
sdl2-config --libs
```

Expected output (version may vary):

```
2.24.1
-I/usr/include/SDL2 -D_GNU_SOURCE=1 -D_REENTRANT
-L/usr/lib/arm-linux-gnueabihf -lSDL2
```

### Step A.5 — Verify GPU Access

```bash
ls /dev/dri/card0          # Should exist (KMSDRM)
ls /dev/fb0                # Should exist (framebuffer)
vcdbg dispinfo             # Should show GPU status
/opt/vc/bin/vcosctl status # Alternative GPU status check
```

### Step A.6 — Configure Memory Split

Edit `/boot/config.txt` to allocate sufficient GPU memory:

```bash
sudo nano /boot/config.txt
```

Add or modify:

```
gpu_mem=256
```

> **Note**: 256 MB is sufficient for 240p output. The default of 64 or 128 MB may cause failures with GLES2 contexts.

Reboot to apply:

```bash
sudo reboot
```

---

## Method B: Clone SDL2-RPI from GitHub

If you need the VideoCore IV-specific optimizations from the SDL2-RPI fork (better GLES2 performance, lower-latency display), clone the git repository directly instead of using the unavailable tarball.

### Step B.1 — Install Build Dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libgles2-mesa-dev libegl1-mesa-dev \
    libbcm-host-dev libraspberrypi-dev \
    libasound2-dev libpulse-dev libdbus-1-dev \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols
```

### Step B.2 — Clone SDL2-RPI

```bash
cd ~
git clone https://github.com/psifidotos/SDL2-RPI.git
cd SDL2-RPI
```

### Step B.3 — Build SDL2-RPI

```bash
mkdir build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DSDL2_BUILDING_FRAMEWORK=OFF \
    -DSDL_TESTS=OFF
make -j4
sudo make install
```

### Step B.4 — Verify SDL2-RPI Installation

```bash
# Check the installed library location
ls -la /usr/local/lib/libSDL2*

# Verify headers
ls /usr/local/include/SDL2/SDL.h

# Check version
/usr/local/bin/sdl2-config --version 2>/dev/null || pkg-config --modversion sdl2
```

### Step B.5 — Update Library Cache

```bash
sudo ldconfig
```

### Step B.6 — Set Environment Variables (Add to ~/.bashrc)

```bash
echo 'export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

---

## Method C: Build SDL2 from Official Source

Use this method if you need the latest SDL2 from the official repository with VideoCore IV support enabled through CMake configuration.

### Step C.1 — Install Build Dependencies

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build \
    libgles2-mesa-dev libegl1-mesa-dev libdrm-dev libgbm-dev \
    libasound2-dev libpulse-dev libudev-dev libdbus-1-dev \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libxss-dev libxkbcommon-dev \
    wayland-protocols libwayland-dev libwayland-cursor-dev
```

### Step C.2 — Clone SDL2 Official Repository

```bash
cd ~
git clone https://github.com/libsdl-org/SDL.git
cd SDL
git checkout release-2.24.2
```

### Step C.3 — Configure with VideoCore IV Support

```bash
mkdir build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DSDL_TESTS=OFF \
    -DSDL_SHARED=ON \
    -DSDL_STATIC=OFF \
    -DSDL_VIDEO_DRIVER_KMSDRM=ON \
    -DSDL_VIDEO_DRIVER_KMSDRM_DEVICE_DEFAULT=/dev/dri/card0 \
    -DSDL_VIDEO_DRIVER_RPI=ON \
    -DSDL_AUDIO_DRIVER_ALSA=ON \
    -DSDL_AUDIO_DRIVER_PULSE=ON \
    -DSDL_JOYSTICK_HIDAPI=ON \
    -DSDL_HAPTIC=ON \
    -DSDL_POWER=ON \
    -DSDL_FILESYSTEM=ON \
    -DSDL_CPUINFO=ON \
    -DSDL_TESTS=OFF \
    -DSDL_INSTALL_TESTS=OFF
```

### Step C.4 — Compile and Install

```bash
cmake --build . --config Release --parallel 4
sudo cmake --install .
sudo ldconfig
```

### Step C.5 — Verify Installation

```bash
pkg-config --modversion sdl2
pkg-config --cflags sdl2
pkg-config --libs sdl2
```

### Step C.6 — Check Enabled Drivers

```bash
# Create a small test to print enabled drivers
cat > /tmp/sdl2_drivers.c << 'EOF'
#include <SDL2/SDL.h>
#include <stdio.h>
int main(int argc, char* argv[]) {
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL Video drivers:\n");
    int count = SDL_GetNumVideoDrivers();
    for (int i = 0; i < count; i++) {
        printf("  %s\n", SDL_GetVideoDriver(i));
    }
    printf("SDL Audio drivers:\n");
    count = SDL_GetNumAudioDrivers();
    for (int i = 0; i < count; i++) {
        printf("  %s\n", SDL_GetAudioDriver(i));
    }
    SDL_Quit();
    return 0;
}
EOF
gcc /tmp/sdl2_drivers.c -o /tmp/sdl2_drivers $(pkg-config --cflags --libs sdl2)
/tmp/sdl2_drivers 2>&1 | grep -E "SDL (Video|Audio) drivers|  "
```

Expected output on Raspberry Pi 2B:

```
SDL Video drivers:
  kmsdrm
  rpi
  x11
SDL Audio drivers:
  alsa
  pulse
  disk
```

> **Important**: The `rpi` and `kmsdrm` video drivers are the ones that provide hardware acceleration. If you see `offscreen` only, SDL2 was not configured correctly.

---

## KMSDRM Video Pipeline Setup

For native 240p analog composite output (the Ocarina of Pi project's target), KMSDRM must be properly configured.

### Step K.1 — Ensure DRM Kernel Module is Loaded

```bash
# Check if DRM module is loaded
lsmod | grep drm

# If not loaded, load it:
sudo modprobe bcm2835-drm

# Verify:
ls /dev/dri/card0
```

### Step K.2 — Make DRM Module Persistent

```bash
echo "bcm2835-drm" | sudo tee /etc/modules-load.d/drm.conf
```

### Step K.3 — Set 240p Output via config.txt

Edit `/boot/config.txt`:

```bash
sudo nano /boot/config.txt
```

Add these lines:

```ini
# Force composite video output
hdmi_force_hotplug=1
hdmi_ignore_edid=0xa5000080
hdmi_group=2
hdmi_mode=20
hdmi_drive=2

# NTSC artifact 240p (use 16 for PAL 240p)
sdtv_mode=20
sdtv_aspect=2

# Enable TV output
enable_tvout=1
```

### Step K.4 — Reboot and Verify

```bash
sudo reboot
```

After reboot:

```bash
# Check display status
/opt/vc/bin/vcdbg dispinfo
cat /sys/class/dispmanx/results/0/status
```

---

## Verification

### Full Verification Script

Run this after completing any method above:

```bash
#!/bin/bash
echo "=== SDL2 Installation Check ==="
echo "Version: $(pkg-config --modversion sdl2 2>/dev/null || echo NOT FOUND)"
echo "C Flags: $(pkg-config --cflags sdl2 2>/dev/null)"
echo "Link Flags: $(pkg-config --libs sdl2 2>/dev/null)"

echo ""
echo "=== GPU & Display Check ==="
echo "/dev/dri/card0: $([ -e /dev/dri/card0 ] && echo EXISTS || echo MISSING)"
echo "/dev/fb0: $([ -e /dev/fb0 ] && echo EXISTS || echo MISSING)"

echo ""
echo "=== Kernel Modules ==="
lsmod | grep -E "bcm2835-drm|drm_kms_helper|vcfb" || echo "No DRM modules loaded"

echo ""
echo "=== Library Check ==="
ls /usr/lib/arm-linux-gnueabihf/libSDL2* 2>/dev/null | head -5 || echo "No SDL2 libraries found in default path"

echo ""
echo "=== Hardware Acceleration ==="
ls /opt/vc/lib/libbrcmEGL.so 2>/dev/null && echo "Broadcom EGL: OK" || echo "Broadcom EGL: MISSING"
ls /opt/vc/lib/libbrcmGLESv2.so 2>/dev/null && echo "Broadcom GLES2: OK" || echo "Broadcom GLES2: MISSING"

echo ""
echo "=== Audio (ALSA) ==="
aplay -L 2>/dev/null | grep "default" && echo "ALSA default: OK" || echo "ALSA default: MISSING"
ls /dev/snd/controlC0 2>/dev/null && echo "Sound card: OK" || echo "Sound card: MISSING"
```

Save it as `verify_sdl2.sh` and run:

```bash
chmod +x verify_sdl2.sh
./verify_sdl2.sh
```

All items should report **OK** or **EXISTS**. If any show **MISSING**, revisit the corresponding step above.

---

## Integration with Ocarina of Pi

### CMakeLists.txt Update

If you used **Method B** or **Method C** (custom SDL2 build), update `CMakeLists.txt` to point to the new SDL2 location:

```cmake
# For Method B (SDL2-RPI installed to /usr/local):
set(SDL2_PATH "/usr/local")

# For Method C (official SDL2 installed to /usr/local):
set(SDL2_PATH "/usr/local")

# For Method A (system packages, default):
set(SDL2_PATH "")

find_package(SDL2 REQUIRED)
find_package(SDL2 REQUIRED COMPONENTS video audio joystick)
```

### Environment Variables

Add these to `~/.bashrc` or your build script:

```bash
# Method B or C custom installation:
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Force SDL2 to use KMSDRM driver (bypasses desktop compositor):
export SDL_VIDEODRIVER=kmsdrm

# Force SDL2 to use ALSA audio:
export SDL_AUDIODRIVER=alsa

# Low latency settings:
export SDL_AUDIO_ALLOW_CHANGE_FREQUENCY=1
```

### Cross-Compilation from Host PC

If cross-compiling on another machine, you need the **Method A** system packages installed on the Pi, and a matching toolchain file:

```cmake
# toolchain-armpi.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7l)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf /opt/vc)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Then build:

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain-armpi.cmake \
    -DSDL2_PATH=/path/to/SDL2-RPI \
    -DCMAKE_C_FLAGS="-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3" \
    -DCMAKE_CXX_FLAGS="-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -O3"
cmake --build . --parallel 4
```

---

## Troubleshooting SDL2 on Raspberry Pi 2B

### SDL2 Video Driver Not Detected

**Symptom**: Only `offscreen` driver is available.

**Solution**: You need to rebuild SDL2 with KMSDRM/RPI drivers enabled (Method B or C), or install `libdrm-dev`:

```bash
sudo apt install -y libdrm-dev
```

### KMSDRM Fails to Initialize

**Symptom**: Application starts but screen is black.

**Solution**: Verify the DRM connector is active:

```bash
/opt/vc/bin/vcdbg dispinfo
# Look for "HDMI-1" or "SDTV" with "EDID: OK"

# If using composite only:
echo "hdmi_ignore_edid=0xa5000080" | sudo tee -a /boot/config.txt
sudo reboot
```

### Segfault on SDL2 Initialization

**Symptom**: Application crashes immediately on `SDL_Init()`.

**Solution**: This usually means the memory split is too low:

```bash
# Check current split
grep "gpu_mem" /boot/config.txt

# Increase to 256 MB minimum
sudo sed -i 's/gpu_mem=.*/gpu_mem=256/' /boot/config.txt
sudo reboot
```

### Audio Device Not Found

**Symptom**: `hw:0,0` fails in ALSA.

**Solution**: Check the sound card:

```bash
aplay -l
# If no sound cards listed, load the ALSA module:
sudo modprobe snd_bcm2835
echo "snd_bcm2835" | sudo tee /etc/modules-load.d/alsa.conf
```

### SDL2-RPI Build Fails with "BCM host library not found"

**Symptom**: CMake error during SDL2-RPI build.

**Solution**: Install the Broadcom host library:

```bash
sudo apt install -y libbcm-host-dev
# Or if building from the Pi's VC firmware:
sudo rpi-eeprom-update
sudo apt install -y libraspberrypi-dev
```

---

## Quick Reference — All Commands

```bash
# === Complete Setup in Order ===

# 1. Update system
sudo apt update && sudo apt upgrade -y

# 2. Install SDL2 (system package — Method A)
sudo apt install -y libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev \
    libsdl2-ttf-dev libgles2-mesa-dev libegl1-mesa-dev libbcm-host-dev \
    libdrm-dev libgbm-dev libraspberrypi-dev libasound2-dev

# 3. Configure GPU memory
echo "gpu_mem=256" | sudo tee -a /boot/config.txt
sudo reboot

# 4. Verify
pkg-config --modversion sdl2
ls /dev/dri/card0
ls /opt/vc/lib/libbrcmEGL.so

# 5. Build project
cd "Ocarina of Pi"
bash scripts/build_rpi2.sh
```

---

## Summary: Which Method to Choose?

| Method | Time | Best For |
|--------|------|----------|
| **A: System packages** | 2 minutes | Most users; reliable; hardware acceleration via `libbcm-host` |
| **B: SDL2-RPI from git** | 15 minutes | Optimized VideoCore IV drivers; latest SDL2-RPI patches |
| **C: Official SDL2 from source** | 20 minutes | Latest upstream SDL2 with KMSDRM; full control over drivers |

For the Ocarina of Pi project, **Method A** is sufficient if the pre-packaged `libsdl2-dev` provides adequate VideoCore IV support. If performance is unsatisfactory or KMSDRM is required, upgrade to **Method B** or **Method C**.
