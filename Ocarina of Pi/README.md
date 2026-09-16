# Ocarina of Pi

> **The Legend of Zelda: Ocarina of Time** — Ported to the **Raspberry Pi 2B** with native **240p analog composite output**.

This guide walks you through everything from preparing your Raspberry Pi to playing the game. No prior experience with cross-compilation or embedded systems is required — just follow the steps in order.

---

## Table of Contents

1. [What You'll Need](#1-what-youll-need)
2. [Understanding the Project](#2-understanding-the-project)
3. [Preparing Your Raspberry Pi](#3-preparing-your-raspberry-pi)
4. [Setting Up the Build Environment](#4-setting-up-the-build-environment)
5. [Building the Project](#5-building-the-project)
6. [Configuring the Game ROM](#6-configuring-the-game-rom)
7. [Checking Controller Compatibility](#7-checking-controller-compatibility)
8. [Deploying to the SD Card](#8-deploying-to-the-sd-card)
9. [Booting and Playing](#9-booting-and-playing)
10. [Troubleshooting](#10-troubleshooting)
11. [Technical Reference](#11-technical-reference)

> **Also see**: [§6.5 — Automatic Controller Mapping](#65-automatic-controller-mapping-system) for controller mapping details.

---

## 1. What You'll Need

### Hardware

| Item | Details |
|------|---------|
| **Raspberry Pi 2B** | Broadcom BCM2836, 900 MHz quad-core ARM Cortex-A7, 1 GB RAM |
| **MicroSD card** | 8 GB or larger, Class 10 or faster |
| **MicroSD card reader** | For flashing the OS and copying files |
| **USB keyboard** | For initial setup (gameplay can use a controller) |
| **HDMI monitor** (optional) | For visual setup; the game outputs to analog composite by default |
| **Xbox 360 controller** (recommended) | For the best gaming experience; Xbox-compatible controllers work via XInput |
| **RCA composite cable** or **3.5 mm TRRS-to-RCA adapter** | For 240p analog video output to a CRT or composite-compatible display |
| **3.5 mm audio cable** or **headphones** | For analog audio via the Pi's TRRS jack |
| **USB power supply** | 5V, 2A (official Pi 2B supply recommended) |
| **USB hub** (if using controller) | Powered hub if your controller requires more power than the Pi's USB port provides |

> **SDL2 Setup**: The pre-packaged `SDL2-RPI.tar.gz` referenced in the build script may be unavailable. See **[SDL2 Setup Guide](docs/SDL2_SETUP.md)** for alternative installation methods including system packages, GitHub clone, and source build — all with VideoCore IV hardware acceleration.

### Software (on your main computer, for building)

- **Linux computer** (Ubuntu, Debian, or similar) — used for cross-compiling or building natively
- **Terminal / Bash** — all instructions use command-line tools
- **Git** (optional) — if cloning from a repository

---

## 2. Understanding the Project

This project runs **The Legend of Zelda: Ocarina of Time** on a Raspberry Pi 2B. Here is a simplified overview of how the pieces fit together:

```
┌────────────────────────────────────────────────────┐
│                   Your Computer                     │
│                                                     │
│  ┌──────────────┐     ┌────────────────────────┐   │
│  │ Source Code  │────▶│  build_rpi2.sh         │   │
│  │ (C++/CMake)  │     │  (compiles everything) │   │
│  └──────────────┘     └──────────┬─────────────┘   │
│                                  │                  │
│                                  ▼                  │
│                         ┌──────────────────┐        │
│                         │  ocarina_of_pi   │        │
│                         │  (ARM binary)    │        │
│                         └────────┬─────────┘        │
│                                  │                  │
│                                  ▼                  │
│  ┌─────────────────────┐  ┌──────────────────┐     │
│  │ post_build.sh       │◀─│ Auto-runs after  │     │
│  │ (ROM + XInput check)│  │ build completes  │     │
│  └──────────┬──────────┘  └──────────────────┘     │
│             │                                      │
└─────────────┼──────────────────────────────────────┘
              │  Copy files to SD card
              ▼
┌────────────────────────────────────────────────────┐
│              Raspberry Pi 2B                        │
│                                                     │
│  ┌────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │ config.txt │  │ ocarina_of_ │  │  roms/      │  │
│  │ (boot conf)│  │ pi (binary) │  │  (game ROM) │  │
│  └────────────┘  └──────┬──────┘  └─────────────┘  │
│                        │                            │
│                        ▼                            │
│              ┌─────────────────┐                    │
│              │  240p Analog    │                    │
│              │  Composite Out  │                    │
│              │  (3.5mm TRRS)   │                    │
│              └─────────────────┘                    │
│                                                     │
│              ┌─────────────────┐                    │
│              │  Analog Audio   │                    │
│              │  (3.5mm TRRS)   │                    │
│              └─────────────────┘                    │
└────────────────────────────────────────────────────┘
```

### Key Concepts

- **Cross-compilation**: Building software on one computer (e.g., your desktop PC) that runs on a different architecture (the Pi's ARM processor). This is faster than building directly on the Pi.
- **240p**: A low-resolution video signal (320×240 pixels) that is standard for older games and CRT displays. This project forces the Pi to output this resolution through its analog composite port.
- **XInput**: Microsoft's controller API. Xbox 360 and Xbox One/Series controllers use this protocol. On Linux, XInput compatibility is achieved through the `xpad` kernel driver or `xboxdrv` software bridge.
- **KMSDRM**: A Linux kernel framework that allows direct access to display hardware, bypassing the desktop compositor for raw framebuffer output.

---

## 3. Preparing Your Raspberry Pi

### Step 3.1 — Install the Operating System

We need a lightweight Linux operating system on the Pi. **Raspberry Pi OS Lite (32-bit)** is recommended.

1. **Download Raspberry Pi OS Lite (32-bit)**
   - Go to: [https://www.raspberrypi.com/software/operating-systems/](https://www.raspberrypi.com/software/operating-systems/)
   - Download the **Raspberry Pi OS (32-bit) Lite** image (the version without desktop)

2. **Flash the SD card**
   - Download and install **Raspberry Pi Imager** on your main computer:
     [https://www.raspberrypi.com/software/](https://www.raspberrypi.com/software/)
   - Insert your MicroSD card into your computer
   - Open Raspberry Pi Imager
   - Click **"Raspberry Pi OS (other)"** → select the downloaded image
   - Click **"Storage"** → select your MicroSD card
   - Click **"Write"** and wait for the process to finish
   - Safely eject the SD card

3. **Enable SSH (optional, for remote access)**
   - After flashing, the SD card will have a `boot` partition visible on your computer
   - Create an empty file named `ssh` (no extension) in the `boot` partition:
     ```bash
     touch /mnt/sdcard/boot/ssh
     ```
   - This lets you connect to the Pi remotely via SSH after first boot

### Step 3.2 — Boot the Pi for First Time

1. Insert the MicroSD card into the Raspberry Pi 2B
2. Connect the Pi to your router via Ethernet cable (recommended for initial setup)
3. Connect the Pi to power (5V, 2A USB-C or Micro-USB depending on your Pi 2B model)
4. Wait 1-2 minutes for the Pi to boot

5. **Find the Pi's IP address** — check your router's DHCP client list, or use a network scanning tool:
   ```bash
   # On your main computer:
   nmap -sn 192.168.1.0/24
   ```
   Look for a device named `raspberrypi` or similar.

6. **SSH into the Pi**:
   ```bash
   ssh pi@<IP_ADDRESS>
   ```
   Default password: `raspberry`

7. **Change the password** when prompted (recommended for security):
   ```bash
   passwd
   ```

### Step 3.3 — Update the System

```bash
sudo apt update && sudo apt upgrade -y
sudo reboot
```

After the reboot, SSH back in and continue to the next step.

---

## 4. Setting Up the Build Environment

You can build the project in two ways:
- **Method A: Cross-compile from your main PC** (faster, recommended)
- **Method B: Build directly on the Pi** (simpler, no cross-compilation needed)

### Method A: Cross-Compile from Your Main PC

This is recommended because your desktop PC is much faster than the Pi.

#### Step 4A.1 — Install the ARM Cross-Compiler

On a **Debian/Ubuntu** system:
```bash
sudo apt update
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    cmake build-essential pkg-config libsdl2-dev libgles2-mesa-dev \
    libegl1-mesa-dev libasound2-dev libpthread-stubs0-dev libdrm-dev
```

> **Note**: `libdrm-dev` provides the DRM/KMS headers (`drm.h`) required for the video pipeline. On the Pi target, these come standard with the Raspberry Pi OS development packages.

#### Step 4A.2 — Install SDL2 and Graphics Libraries

```bash
sudo apt install -y libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev \
    libasound2-dev libpthread-stubs0-dev libdrm-dev
```

> **Note**: `libdrm-dev` provides the DRM/KMS headers (`drm.h`) required for the video pipeline.

#### Step 4A.3 — Obtain SDL2-RPI (VideoCore IV SDL2)

The Pi's VideoCore IV GPU requires a special SDL2 build with hardware-accelerated GLES2 support. **The pre-packaged `SDL2-RPI.tar.gz` may be unavailable.** Use one of these alternatives:

> **If SDL2-RPI tarball is available** (fallback):
> 1. Download from: [https://github.com/psifidotos/SDL2-RPI](https://github.com/psifidotos/SDL2-RPI)
> 2. Extract into the project parent directory:
>    ```bash
>    cd /home/khalesis/Documentos/Ocarina\ of\ Pi/
>    tar xzf SDL2-RPI.tar.gz -C ..
>    ```

> **If SDL2-RPI tarball is unavailable** (recommended):
> Install SDL2 directly on the Raspberry Pi using system packages or build from source. See the **[SDL2 Setup Guide](docs/SDL2_SETUP.md)** for complete instructions covering:
> - System package installation (`libsdl2-dev`) — easiest
> - Cloning SDL2-RPI from GitHub
> - Building SDL2 from official source with VideoCore IV support
> - KMSDRM configuration and driver verification

> **Note**: If SDL2-RPI is not installed, the build script will warn you. On Raspberry Pi OS, `libsdl2-dev` provides adequate GLES2 support for the 240p composite target.

#### Step 4A.4 — Clone or Copy the Project

```bash
cd ~/
git clone <project-repository-url> "Ocarina of Pi"
cd "Ocarina of Pi"
```

If you don't have a Git repository, simply copy all the project files into a folder on your PC.

### Method B: Build Directly on the Pi

#### Step 4B.1 — Install Required Packages

```bash
sudo apt update
sudo apt install -y cmake g++ libsdl2-dev libgles2-mesa-dev \
    libegl1-mesa-dev libasound2-dev libpthread-stubs0-dev libdrm-dev
```

> **Note**: `libdrm-dev` provides the DRM/KMS headers (`drm.h`) required for the video pipeline.

#### Step 4B.2 — Clone or Copy the Project

```bash
cd ~
git clone <project-repository-url> "Ocarina of Pi"
cd "Ocarina of Pi"
```

---

## 5. Building the Project

### Step 5.1 — Prepare the ROM Directory

Create a directory where your game ROM will go:
```bash
mkdir -p roms
```

### Step 5.2 — Run the Build Script

```bash
bash scripts/build_rpi2.sh
```

The script will:
1. Check that all dependencies are present
2. Create a build directory
3. Configure CMake with ARM toolchain settings (cross-compile) or native settings (on Pi)
4. Compile the project using all 4 cores
5. Copy the binary and config files to `build/deploy/`
6. **Automatically run** the post-build script (see Section 6)

### Step 5.3 — Verify the Build

After the build completes, confirm the binary exists:
```bash
ls -lh build/deploy/ocarina_of_pi
file build/deploy/ocarina_of_pi
```

Expected output for cross-compiled binary:
```
ocarina_of_pi: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

If building natively on the Pi:
```
ocarina_of_pi: ELF 32-bit LSB pie executable, ARM, EABI5 version 1 (SYSV), dynamically linked, ...
```

---

## 6. Configuring the Game ROM

The post-build script needs to know where your game ROM is. This is configured through `config/rom_config.txt`.

### Step 6.1 — Edit the ROM Configuration

```bash
nano config/rom_config.txt
```

### Step 6.2 — Set the ROM Path

You have two options:

**Option A: Point to a specific ROM file**

Edit `ROM_FILE` to the full path of your Ocarina of Time ROM:
```
ROM_FILE="/home/khalesis/roms/ocarina_of_time.z64"
```

**Option B: Point to a ROM directory (auto-discovery)**

Edit `ROM_DIRECTORY` to the folder containing your ROMs:
```
ROM_DIRECTORY="/home/khalesis/roms"
```

Leave `ROM_FILE` empty when using directory mode. The script will automatically find `.z64`, `.n64`, or `.rom` files in that directory.

### Step 6.3 — Configure Controller Support

Ensure XInput controller support is enabled:
```
ENABLE_XINPUT=1
```

If you're using an Xbox 360 controller with the Linux `xpad` driver, this will be auto-detected. If using `xboxdrv` (software XInput bridge), ensure the path is correct:
```
XBOXDRV_PATH="/usr/bin/xboxdrv"
```

### Step 6.4 — (Optional) Configure ROM Validation

If you want the script to verify your ROM's integrity, add the CRC32 checksums:
```
CRC32_GOLD="DDE37965"
CRC32_SCRAMBLE="61715BDB"
```

Leave these empty to skip CRC validation.

### Step 6.5 — Save and Close

Save the file (`Ctrl+O`, `Enter`) and exit the editor (`Ctrl+X`).

---

## 6.5 — Automatic Controller Mapping System

The emulator includes an automatic controller mapping system that translates any SDL2-compatible controller into the N64 input layout. No manual configuration is required — the mapping is determined by the physical input device, not by button labels.

### How It Works

1. **Detection**: When the emulator starts, it opens any connected SDL2 game controller. If no controller is found, keyboard input is used as a fallback.

2. **Left Analog Stick → Movement**: The left stick is read as raw analog values (X and Y axes, range −32768 to +32767). These values represent Link's movement with full analog precision, matching the N64 control stick's range of motion.

3. **D-Pad → Standard D-Pad**: The physical D-pad buttons are mapped directly to N64 D-pad functions (up, down, left, right). These operate independently from the left stick — pressing the D-pad while moving with the stick sends both inputs simultaneously, exactly like the original N64 controller.

4. **Right Analog Stick → C-Buttons**: The right stick is converted from analog to digital using threshold detection. When the stick is pushed beyond the active zone (~76% of full deflection), the corresponding C-button activates. This emulates the N64 C-buttons which were digital (on/off) inputs:
   - Right stick pushed **right** → C-Right
   - Right stick pushed **left** → C-Left
   - Right stick pushed **up** → C-Up (zoom out in first-person)
   - Right stick pushed **down** → C-Down (zoom in on items)

5. **Shoulder Triggers → L/R Buttons**: The left and right triggers (axes) are held above the active threshold to register L and R buttons.

6. **A/B/Start/Back Buttons**: Mapped directly — A (A), B (B), Start (Start), Back (Z).

### Deadzone Settings

The default deadzone (8000/32767 ≈ 24%) filters out stick drift. The active zone (25000/32767 ≈ 76%) ensures intentional directional input before registering a button press. These values can be tuned in the source code (`game_input.cpp` → `ControllerConfig`).

### Running Without a Controller

If no controller is detected, the emulator falls back to keyboard input with the following mapping:

| Key | Action |
|-----|--------|
| Z | A Button |
| X | B Button |
| Return | Start |
| Left Shift | Z (Trigger) |
| [ | L Shoulder |
| ] | R Shoulder |
| W / Up | D-Pad Up |
| S / Down | D-Pad Down |
| A / Left | D-Pad Left |
| D / Right | D-Pad Right |

---

## 7. Checking Controller Compatibility

After the build and post-build script run, the script will automatically check for XInput-compatible controllers. Here is what you need to know about each check:

### Xbox 360 Controller (wired or wireless with receiver)

This is the recommended controller. On Linux, it works out of the box with the `xpad` kernel driver.

**Check if xpad is loaded:**
```bash
lsmod | grep xpad
```
If there's no output, load it manually:
```bash
sudo modprobe xpad
```

**Verify the controller is detected:**
```bash
ls /dev/input/js*
```
You should see at least `/dev/input/js0` when the controller is connected.

### Xbox One / Xbox Series Controller

These controllers require `xboxdrv` for XInput compatibility on older Raspberry Pi kernels.

**Install xboxdrv:**
```bash
sudo apt install -y xboxdrv
```

**Start xboxdrv (run this before launching the game):**
```bash
sudo xboxdrv --detach-kernel-driver &
```

### Generic Controllers

SDL2 supports any controller recognized by the Linux input subsystem (`/dev/input/event*`). If your controller appears in `/dev/input/`, it should work.

---

## 8. Deploying to the SD Card

### Step 8.1 — Prepare the SD Card

The SD card should already have Raspberry Pi OS installed (from Section 3). Now we need to copy the game files:

1. **Insert the MicroSD card into your main computer** (or use the Pi and mount the SD card image)

2. **Navigate to the boot partition** (visible on most systems as a removable drive):
   ```bash
   # On Linux with the SD card mounted:
   cd /mnt/sdcard
   ```

3. **Copy the deployment files:**
   ```bash
   cp /home/khalesis/Documentos/Ocarina\ of\ Pi/build/deploy/ocarina_of_pi .
   cp /home/khalesis/Documentos/Ocarina\ of\ Pi/build/deploy/config.txt .
   cp /home/khalesis/Documentos/Ocarina\ of\ Pi/build/deploy/launch.sh .
   ```

4. **Copy the ROM (if it was included by the post-build script):**
   ```bash
   mkdir -p roms
   cp /home/khalesis/Documentos/Ocarina\ of\ Pi/build/deploy/rom/* roms/
   ```

5. **Eject the SD card safely** and insert it into the Raspberry Pi.

### Step 8.2 — Verify the SD Card Contents

After inserting the SD card into the Pi, the file structure should look like:

```
SD Card
├── boot/
│   └── (Raspberry Pi OS files)
│   └── config.txt         ← Display configuration
├── ocarina_of_pi          ← Game binary
├── launch.sh              ← Launch script
└── roms/
    └── ocarina_of_time.z64 ← Game ROM (if included)
```

---

## 9. Booting and Playing

### Step 9.1 — Hardware Connections

Before powering on the Pi, make all physical connections:

1. **Video**: Connect the Pi's 3.5mm TRRS jack to your TV or monitor's composite (yellow RCA) input, or use a TRRS-to-RCA adapter.

2. **Audio**: Connect the Pi's 3.5mm TRRS jack to headphones, powered speakers, or a TV's audio input.

3. **Controller**: Plug your Xbox controller into a USB port.

4. **Power**: Connect the 5V power supply last.

### Step 9.2 — Boot the Pi

1. Plug in the power supply
2. The Pi will boot Raspberry Pi OS (about 30-60 seconds)
3. Wait for the desktop or login prompt (if using desktop) or SSH access (if using Lite)

### Step 9.3 — Launch the Game

**If you have a monitor/keyboard connected to the Pi:**

```bash
# Navigate to the game directory
cd ~

# Launch with the included script (sets SDL environment variables)
./launch.sh
```

Or directly:
```bash
./ocarina_of_pi
```

**If you're connected via SSH (headless):**

SSH does not provide a display. Instead, use SSH to launch the game and output will go to the Pi's composite port:

```bash
ssh pi@<IP_ADDRESS>
cd ~
./launch.sh
```

The game video and audio will appear on your TV/monitor connected to the Pi via composite, even though you're controlling via SSH.

### Step 9.4 — First Gameplay

1. **Wait for the game to initialize** — the first startup may take 5-10 seconds as the emulator loads the ROM and initializes the video/audio subsystems.

2. **Controller setup** — if your Xbox controller is connected and XInput is enabled, it should work automatically. If not, try pressing any button to wake the controller.

3. **You should now see the game** at 320×240 resolution on your composite display.

4. **Controller mapping** (automatic):

The emulator automatically maps your controller inputs to the N64 layout as follows:

| Physical Input | N64 Function | Description |
|----------------|-------------|-------------|
| **Left Analog Stick** | Movement (Control Stick) | Move Link through the game world. Analog precision — push gently for slow walking, push fully for running. |
| **D-Pad** | D-Pad | Standard directional inputs for menus and inventory cycling. Independent from the left stick. |
| **Right Analog Stick** | C-Buttons (Camera) | Emulates the N64 C-buttons (C-Up/C-Down/C-Left/C-Right) using threshold detection. Push the stick in a direction past the deadzone to trigger the corresponding C-button. Used for camera control in Ocarina of Time. |
| **A Button** | Confirm / Select | Interact with characters, pick up items, confirm choices. |
| **B Button** | Cancel / Back | Open menus, put away items, go back. |
| **Z (Shoulder)** | Zelda's Lullaby / Lock-on | Play Zelda's Lullaby, target enemies. |
| **L / R (Shoulders)** | Inventory Cycling | Cycle through inventory items left and right. |
| **Start** | Pause Menu | Open / close the pause menu. |

**Analog Stick Deadzone**: Both analog sticks have a deadzone of ~25% (8000/32767) to prevent accidental inputs from stick drift. An active zone of ~76% (25000/32767) must be reached before the direction registers. This is optimized for the N64's digital input model — the right stick is converted to digital C-button presses when pushed beyond the active threshold in each direction.

**Important**: The D-pad and left stick operate independently. Even if you are moving with the left stick, the D-pad can be pressed simultaneously for menu navigation — just like the original N64 controller. Similarly, the right stick for camera control is independent of both.

5. **Enjoy the game!**

---

## 10. Troubleshooting

### No Video Output

- **Check the cable**: Ensure the 3.5mm TRRS jack or RCA adapter is firmly connected to both the Pi and the display.
- **Check `config.txt`**: Verify `enable_tvout=1` and `sdtv_mode=20` are present.
- **Try a different display**: Some TVs may not detect 240p signals. A CRT television works best.
- **Verify the binary**: Run `file ocarina_of_pi` to confirm it's an ARM binary.

### No Audio

- **Check the audio jack**: Ensure headphones or speakers are plugged into the 3.5mm TRRS jack.
- **Force TRRS audio**: Edit `config.txt` and ensure `hdmi_ignore_cec_init=1` and `audio_pwm_mode=2` are set.
- **Test ALSA directly**: Run `speaker-test -D hw:0,0 -c 2` to verify audio hardware works.

### Controller Not Detected

- **Check the connection**: Unplug and replug the controller's USB cable.
- **Verify xpad driver**: Run `lsmod | grep xpad`. If not loaded, run `sudo modprobe xpad`.
- **Check device nodes**: Run `ls /dev/input/js*` — if no devices appear, the controller is not being recognized.
- **Try xboxdrv**: Run `sudo xboxdrv` to start the software XInput bridge.
- **Check mapping**: The emulator auto-detects any SDL2-compatible controller. If a controller is detected but inputs seem wrong, check the mapping output at startup: the emulator prints the SDL2 controller mapping string when it first connects.

### "ROM Not Found" Error

- **Edit `config/rom_config.txt`**: Ensure `ROM_DIRECTORY` points to the correct folder or `ROM_FILE` points to the exact ROM file path.
- **Check file permissions**: Run `chmod 644 <rom_file>` to make sure the ROM is readable.
- **Verify ROM filename**: Ensure the ROM has one of the accepted extensions: `.z64`, `.n64`, `.rom`.

### Slow Performance

- **Ensure overclocking is active**: Check `config.txt` has `arm_freq=900` and `over_voltage=6`.
- **Avoid desktop environment**: Use Raspberry Pi OS Lite for best performance.
- **Check RAM**: Close any unnecessary processes. The game needs the 768 MB memory budget to be available.
- **Verify CPU affinity**: The threads should be pinned to separate cores. Check with `htop` or `top`.

### Black Screen (but audio plays)

- The video subsystem may be failing to initialize. Check that:
  - KMSDRM is available (`ls /dev/dri/card0`)
  - SDL2 GLES2 drivers are installed
  - The display configuration in `video_rpi.cpp` is correct (320×240 with integer scaling)

---

## 11. Technical Reference

### File Descriptions

| File | Location | Purpose |
|------|----------|---------|
| `CMakeLists.txt` | Project root | Build configuration — compiler flags, libraries, targets |
| `config.txt` | `config/` | Raspberry Pi boot parameters — overclocking, 240p mode, memory split |
| `rom_config.txt` | `config/` | ROM path, XInput settings, validation parameters |
| `build_rpi2.sh` | `scripts/` | Main build automation script |
| `post_build.sh` | `scripts/` | Post-build: ROM detection, XInput check, deployment package creation |
| `video_rpi.cpp` | `src/` | KMSDRM display driver, OpenGL ES 2.0 pipeline, nearest-neighbor scaling |
| `audio_alsa.cpp` | `src/` | ALSA audio output via `hw:0,0`, low-latency buffering |
| `threading.cpp` | `src/` | CPU affinity — pins threads to individual Cortex-A7 cores |
| `mem_manager.cpp` | `src/` | Memory pool allocator with 768 MB budget enforcement |
| `game_input.cpp` | `src/` | SDL2 controller input → N64 button mapping (auto-mapping: left stick = movement, D-pad, right stick = C-buttons) |
| `game_input.h` | `include/` | Controller input interface with analog-to-digital conversion |
| `main.cpp` | `src/` | Entry point and orchestration — initializes all subsystems, starts threads |
| `README.md` | Project root | This guide — setup, build, controller mapping, troubleshooting |

### Build Flags Reference

| Flag | Purpose |
|------|---------|
| `-march=armv7-a` | Target ARMv7-A instruction set |
| `-mtune=cortex-a7` | Optimize for Cortex-A7 pipeline |
| `-mfpu=neon-vfpv4` | Enable NEON SIMD and VFPv4 FPU |
| `-mfloat-abi=hard` | Use hardware floating-point ABI |
| `-O3` | Maximum optimization level |
| `-ftree-vectorize` | Auto-vectorize loops with SIMD |
| `-flto` | Link-time optimization across compilation units |
| `-fno-exceptions` | Disable C++ exceptions (saves code size) |
| `-fno-rtti` | Disable RTTI (saves code size) |

### System Resource Allocation

| Resource | Allocation | Purpose |
|----------|-----------|---------|
| GPU Memory | 256 MB | VideoCore IV textures, display pipeline |
| Main Application | 560 MB | Game logic, ROM data, streaming buffers |
| Graphics Buffer | 128 MB | Frame buffers, texture memory |
| Audio Buffer | 16 MB | PCM samples, mixing buffers |
| Streaming | 64 MB | Asset streaming, level loading |
| Thread Stack (4 threads) | ~4 MB | Thread stacks at 1 MB each |

### Peripheral Pinout (3.5mm TRRS)

```
Raspberry Pi 2B 3.5mm TRRS Jack:
┌───────────────────┐
│                   │
│  Tip ────────── Video (composite, yellow RCA)
│  Ring1 ───────── Audio Left (white RCA)
│  Ring2 ───────── Audio Right (red RCA)
│  Sleeve ───────── Ground
│                   │
└───────────────────┘

Use a TRRS-to-3×RCA adapter cable for standard composite AV input.
```

---

## Project Structure Summary

```
Ocarina of Pi/
├── CMakeLists.txt                  # Build system
├── config/
│   ├── config.txt                  # Pi boot configuration (240p, overclock, memory)
│   └── rom_config.txt              # ROM path & XInput settings
├── include/
│   ├── video_rpi.h                 # Display pipeline interface
│   ├── audio_alsa.h                # Audio output interface
│   ├── threading.h                 # CPU affinity interface
│   ├── mem_manager.h               # Memory budget interface
│   ├── game_input.h                # Controller input interface
│   └── main.h                      # Top-level orchestration interface
├── src/
│   ├── main.cpp                    # Entry point & thread management
│   ├── video_rpi.cpp               # KMSDRM + GLES2 implementation
│   ├── audio_alsa.cpp              # ALSA hw:0,0 implementation
│   ├── threading.cpp               # Core pinning implementation
│   ├── mem_manager.cpp             # Pool allocator implementation
│   └── game_input.cpp              # Input mapping implementation
├── scripts/
│   ├── build_rpi2.sh               # Build automation (compiles + deploys)
│   └── post_build.sh               # Post-build (ROM + XInput + report)
├── roms/                           # (Create this directory for your ROMs)
│   └── your_rom.z64                # <-- Place your game ROM here
└── build/                          # (Created during build)
    └── deploy/                     # Final deployment package
        ├── ocarina_of_pi
        ├── config.txt
        ├── launch.sh
        ├── xinput_report.txt
        └── rom/                    # (ROM included if configured)
```

---

## Quick Reference — All Commands in Order

```bash
# 1. On your main PC (cross-compile setup):
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf cmake \
    libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev \
    libasound2-dev libpthread-stubs0-dev libdrm-dev

# 2. On the Pi (native build setup):
sudo apt update && sudo apt upgrade -y
sudo apt install -y cmake g++ libsdl2-dev libgles2-mesa-dev \
    libegl1-mesa-dev libasound2-dev libpthread-stubs0-dev libdrm-dev

# 3. Create ROM directory and add your ROM:
mkdir -p roms
# Copy your Ocarina of Time ROM (z64 format) to roms/

# 4. Edit ROM configuration:
nano config/rom_config.txt
# Set ROM_DIRECTORY="/home/khalesis/Documentos/Ocarina of Pi/roms"
# Controller mapping is automatic — no config needed.

# 5. Build:
bash scripts/build_rpi2.sh

# 6. Deploy to SD card:
cp build/deploy/ocarina_of_pi /mnt/sdcard/
cp build/deploy/config.txt /mnt/sdcard/
cp build/deploy/launch.sh /mnt/sdcard/

# 7. If ROM included by post-build:
cp -r build/deploy/rom/* /mnt/sdcard/

# 7. Boot Pi and launch:
cd ~
./launch.sh
```

---

**Happy gaming!** 🎮

For technical questions about the implementation, refer to the inline code documentation in the source files. Each module includes detailed comments explaining the hardware-specific optimizations for the Raspberry Pi 2B.
