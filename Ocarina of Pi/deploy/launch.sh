#!/bin/sh
set -eu

cd /opt/ocarina-of-pi
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_KMSDRM_DEVICE_INDEX="${SDL_KMSDRM_DEVICE_INDEX:-0}"
export OOT_PI_ROM_DIR="/opt/ocarina-of-pi/roms"
export OOT_PI_GAME_TICK_HZ="${OOT_PI_GAME_TICK_HZ:-20}"

exec /opt/ocarina-of-pi/bin/ocarina_of_pi "$@"
