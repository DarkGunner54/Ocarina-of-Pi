#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo: sudo $0 /path/to/your/baserom.z64 [user]" >&2
    exit 1
fi

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: sudo $0 /path/to/your/baserom.z64 [user]" >&2
    exit 1
fi

ROM_SOURCE=$1
RUN_USER=${2:-${SUDO_USER:-pi}}
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_DIR=/opt/ocarina-of-pi
UNIT_FILE=/etc/systemd/system/ocarina-of-pi.service

if [ ! -r "$ROM_SOURCE" ]; then
    echo "ROM is not readable: $ROM_SOURCE" >&2
    exit 1
fi

if ! id "$RUN_USER" >/dev/null 2>&1; then
    echo "Unknown service user: $RUN_USER" >&2
    exit 1
fi

if getent group input >/dev/null 2>&1; then
    usermod -a -G input "$RUN_USER"
fi

if [ ! -x "$BUILD_DIR/ocarina_of_pi" ]; then
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --parallel 4
fi

install -d -m 0755 "$INSTALL_DIR/bin" "$INSTALL_DIR/roms"
install -m 0755 "$BUILD_DIR/ocarina_of_pi" "$INSTALL_DIR/bin/ocarina_of_pi"
install -m 0755 "$PROJECT_DIR/deploy/launch.sh" "$INSTALL_DIR/launch.sh"
install -m 0644 "$ROM_SOURCE" "$INSTALL_DIR/roms/baserom.z64"
sed "s/@USER@/$RUN_USER/g" "$PROJECT_DIR/deploy/ocarina-of-pi.service" > "$UNIT_FILE"

if command -v raspi-config >/dev/null 2>&1; then
    raspi-config nonint do_composite 0
fi

systemctl daemon-reload
systemctl enable ocarina-of-pi.service

echo "Installed to $INSTALL_DIR. Your ROM is stored as roms/baserom.z64."
echo "Reboot to start automatically: sudo reboot"
