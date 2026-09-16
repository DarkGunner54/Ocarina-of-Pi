#!/bin/bash
set -euo pipefail

#===============================================================
#  Ocarina of Pi - Post-Build Automation Script
#===============================================================
# This script runs after build_rpi2.sh completes.
# Tasks:
#   1) Locate and validate the generated N64 game ROM
#   2) Ensure XInput controller compatibility
#   3) Use configurable ROM path from rom_config.txt
#===============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
DEPLOY_DIR="${BUILD_DIR}/deploy"
ROM_CONFIG="${PROJECT_DIR}/config/rom_config.txt"

# Default values (overridden by rom_config.txt)
ROM_DIRECTORY=""
ROM_FILE=""
ROM_EXTENSIONS="z64 n64 rom zip 7z"
ENABLE_XINPUT=1
XBOXDRV_PATH="/usr/bin/xboxdrv"
SDL_JOYSTICK_DRIVER=""
MIN_ROM_SIZE=8388608
MAX_ROM_SIZE=16777216
CRC32_GOLD=""
CRC32_SCRAMBLE=""
DEPLOY_ROM_DIR="/roms"
INCLUDE_ROM_IN_DEPLOY=1
REGION_OVERRIDE=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }
log_section() { echo -e "${BLUE}--- $* ---${NC}"; }

#===============================================================
# Task 1: Parse ROM configuration
#===============================================================
parse_rom_config() {
    log_step "Parsing ROM configuration from ${ROM_CONFIG}"

    if [ ! -f "$ROM_CONFIG" ]; then
        log_error "ROM config file not found: ${ROM_CONFIG}"
        log_error "Creating default configuration..."
        mkdir -p "$(dirname "$ROM_CONFIG")"
        cp "${PROJECT_DIR}/config/rom_config.txt" "$ROM_CONFIG" 2>/dev/null || true
        log_warn "Please edit ${ROM_CONFIG} and re-run this script."
        return 1
    fi

    # Read configuration values (simple key=value parser)
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        [[ "$key" =~ ^#.*$ ]] && continue
        [[ -z "$key" ]] && continue
        # Trim whitespace
        key=$(echo "$key" | xargs)
        value=$(echo "$value" | xargs)

        case "$key" in
            ROM_DIRECTORY)  ROM_DIRECTORY="$value" ;;
            ROM_FILE)       ROM_FILE="$value" ;;
            ROM_EXTENSIONS) ROM_EXTENSIONS="$value" ;;
            ENABLE_XINPUT)  ENABLE_XINPUT="$value" ;;
            XBOXDRV_PATH)   XBOXDRV_PATH="$value" ;;
            SDL_JOYSTICK_DRIVER) SDL_JOYSTICK_DRIVER="$value" ;;
            MIN_ROM_SIZE)   MIN_ROM_SIZE="$value" ;;
            MAX_ROM_SIZE)   MAX_ROM_SIZE="$value" ;;
            CRC32_GOLD)     CRC32_GOLD="$value" ;;
            CRC32_SCRAMBLE) CRC32_SCRAMBLE="$value" ;;
            DEPLOY_ROM_DIR) DEPLOY_ROM_DIR="$value" ;;
            INCLUDE_ROM_IN_DEPLOY) INCLUDE_ROM_IN_DEPLOY="$value" ;;
            REGION_OVERRIDE) REGION_OVERRIDE="$value" ;;
        esac
    done < "$ROM_CONFIG"

    # Resolve relative paths relative to project directory
    if [[ "$ROM_DIRECTORY" != /* ]]; then
        ROM_DIRECTORY="${PROJECT_DIR}/${ROM_DIRECTORY}"
    fi

    log_info "ROM directory: ${ROM_DIRECTORY:-<not set>}"
    log_info "ROM file: ${ROM_FILE:-<auto-discovery>}"
    log_info "XInput enabled: ${ENABLE_XINPUT}"
    log_info "ROM size range: ${MIN_ROM_SIZE} - ${MAX_ROM_SIZE} bytes"

    return 0
}

#===============================================================
# Task 1a: Locate the game ROM
#===============================================================
locate_rom() {
    log_step "Locating game ROM"

    FOUND_ROM=""
    FOUND_ROM_PATH=""

    # Priority 1: Use explicitly specified ROM file
    if [ -n "$ROM_FILE" ]; then
        log_info "Checking specified ROM file: ${ROM_FILE}"
        if [ -f "$ROM_FILE" ]; then
            FOUND_ROM_PATH="$ROM_FILE"
        else
            log_error "Specified ROM file does not exist: ${ROM_FILE}"
            return 1
        fi
    # Priority 2: Search in ROM_DIRECTORY
    elif [ -n "$ROM_DIRECTORY" ] && [ -d "$ROM_DIRECTORY" ]; then
        log_info "Searching for ROM in: ${ROM_DIRECTORY}"

        local found=0
        while IFS= read -r -d '' rom; do
            FOUND_ROM_PATH="$rom"
            found=1
            break
        done < <(find "$ROM_DIRECTORY" -type f \( \
            -iname "*.z64" -o -iname "*.n64" -o -iname "*.rom" \
            -o -iname "*.zip" -o -iname "*.7z" \
        \) -print0 2>/dev/null | sort -z)

        if [ "$found" -eq 0 ]; then
            log_error "No ROM files found in ${ROM_DIRECTORY}"
            log_info "Searching extensions: ${ROM_EXTENSIONS}"
            return 1
        fi
    # Priority 3: Search common locations
    else
        log_warn "No ROM directory configured. Searching common locations..."
        local search_paths=(
            "${PROJECT_DIR}/roms"
            "${PROJECT_DIR}/games"
            "${PROJECT_DIR}"
            "/media/roms"
            "/home/pi/roms"
            "/opt/ocarina/roms"
        )

        local found=0
        for path in "${search_paths[@]}"; do
            if [ -d "$path" ]; then
                while IFS= read -r -d '' rom; do
                    FOUND_ROM_PATH="$rom"
                    found=1
                    break
                done < <(find "$path" -maxdepth 2 -type f \( \
                    -iname "*.z64" -o -iname "*.n64" -o -iname "*.rom" \
                \) -print0 2>/dev/null | sort -z)
                [ "$found" -eq 1 ] && break
            fi
        done

        if [ "$found" -eq 0 ]; then
            log_error "No ROM found in any common location."
            log_info "Please set ROM_FILE or ROM_DIRECTORY in ${ROM_CONFIG}"
            return 1
        fi
    fi

    FOUND_ROM=$(basename "$FOUND_ROM_PATH")
    log_info "Found ROM: ${FOUND_ROM}"
    log_info "Full path: ${FOUND_ROM_PATH}"

    return 0
}

#===============================================================
# Task 1b: Validate ROM integrity
#===============================================================
validate_rom() {
    log_step "Validating ROM: ${FOUND_ROM}"

    local rom_path="$FOUND_ROM_PATH"
    local errors=0

    # Check file exists and is readable
    if [ ! -r "$rom_path" ]; then
        log_error "ROM file is not readable: ${rom_path}"
        return 1
    fi

    # Check file size
    if command -v stat &>/dev/null; then
        file_size=$(stat --format=%s "$rom_path" 2>/dev/null || stat -f%z "$rom_path" 2>/dev/null)
    else
        file_size=$(wc -c < "$rom_path")
    fi

    log_info "ROM size: ${file_size} bytes"

    if [ "$file_size" -lt "$MIN_ROM_SIZE" ]; then
        log_error "ROM too small (${file_size} < ${MIN_ROM_SIZE}). Invalid or truncated."
        errors=$((errors + 1))
    fi

    if [ "$file_size" -gt "$MAX_ROM_SIZE" ]; then
        log_error "ROM too large (${file_size} > ${MAX_ROM_SIZE}). Suspicious."
        errors=$((errors + 1))
    fi

    # Check N64 ROM header signature
    # N64 z64 files typically start with specific patterns
    local header_bytes
    header_bytes=$(xxd -l 4 -p "$rom_path" 2>/dev/null || echo "")
    log_info "ROM header bytes: ${header_bytes}"

    # Validate CRC if configured
    if [ -n "$CRC32_GOLD" ]; then
        log_info "Validating CRC32 (gold: ${CRC32_GOLD})..."
        local actual_crc
        actual_crc=$(crc32 "$rom_path" 2>/dev/null | awk '{print $1}' || echo "")
        if [ -z "$actual_crc" ]; then
            log_warn "crc32 command not found - skipping CRC validation"
        elif [ "$actual_crc" != "$CRC32_GOLD" ] && [ "$actual_crc" != "$CRC32_SCRAMBLE" ]; then
            log_warn "CRC32 mismatch (got: ${actual_crc}, expected: ${CRC32_GOLD}/${CRC32_SCRAMBLE})"
            log_warn "This may be a different game or modified ROM"
        else
            log_info "CRC32 validated successfully"
        fi
    fi

    # Check for compressed archives
    case "$rom_path" in
        *.zip)
            log_info "ROM is a ZIP archive - checking contents..."
            if command -v unzip &>/dev/null; then
                local inner_rom
                inner_rom=$(unzip -l "$rom_path" 2>/dev/null | grep -iE '\.(z64|n64|rom)$' | awk '{print $4}' | head -1)
                if [ -n "$inner_rom" ]; then
                    log_info "Found inner ROM: ${inner_rom}"
                else
                    log_error "No valid ROM found inside ZIP archive"
                    errors=$((errors + 1))
                fi
            else
                log_warn "unzip not available - cannot verify archive contents"
            fi
            ;;
        *.7z)
            log_info "ROM is a 7Z archive - checking contents..."
            if command -v 7z &>/dev/null; then
                local inner_rom
                inner_rom=$(7z l "$rom_path" 2>/dev/null | grep -iE '\.(z64|n64|rom)$' | awk '{print $NF}' | head -1)
                if [ -n "$inner_rom" ]; then
                    log_info "Found inner ROM: ${inner_rom}"
                else
                    log_error "No valid ROM found inside 7Z archive"
                    errors=$((errors + 1))
                fi
            else
                log_warn "7z not available - cannot verify archive contents"
            fi
            ;;
    esac

    if [ "$errors" -gt 0 ]; then
        log_error "ROM validation failed with ${errors} error(s)"
        return 1
    fi

    log_info "ROM validation passed"
    return 0
}

#===============================================================
# Task 2: Check and ensure XInput compatibility
#===============================================================
check_xinput_compatibility() {
    log_step "Checking XInput controller compatibility"

    local xinput_ok=0

    # 2a: Check for xpad kernel driver (Linux XInput compatible driver)
    log_info "Checking kernel drivers..."
    if lsmod 2>/dev/null | grep -q "xpad"; then
        log_info "xpad kernel driver loaded"
        xinput_ok=1
    else
        log_warn "xpad kernel driver not loaded"
        if [ -f "/lib/modules/$(uname -r)/kernel/drivers/input/joystick/xpad.ko" ] || \
           [ -d "/lib/modules/$(uname -r)/kernel/drivers/input/joystick" ]; then
            log_info "xpad module available on system"
        fi
    fi

    # 2b: Check for xboxdrv
    log_info "Checking for xboxdrv..."
    if command -v xboxdrv &>/dev/null; then
        log_info "xboxdrv found at $(command -v xboxdrv)"
        xinput_ok=1
    else
        log_warn "xboxdrv not found in PATH"
    fi

    # 2c: Check for xinput utility (Linux evtest/input)
    log_info "Checking for xinput utility..."
    if command -v xinput &>/dev/null; then
        log_info "xinput utility found"
        local devices
        devices=$(xinput list 2>/dev/null | grep -iE "xbox|gamepad|controller|pad" || true)
        if [ -n "$devices" ]; then
            log_info "XInput-compatible devices detected:"
            echo "$devices" | while IFS= read -r line; do
                echo "  ${line}"
            done
            xinput_ok=1
        fi
    fi

    # 2d: Check SDL2 XInput support
    log_info "Checking SDL2 controller subsystem..."
    if pkg-config --exists sdl2 2>/dev/null; then
        local sdl_cflags
        sdl_cflags=$(pkg-config --cflags sdl2 2>/dev/null)
        log_info "SDL2 installed: ${sdl_cflags}"
    fi

    # Check if SDL2 joystick/controller subsystem is available at runtime
    # by looking for libSDL2_joystick in the binary's dependencies
    if [ -f "${DEPLOY_DIR}/ocarina_of_pi" ]; then
        if ldd "${DEPLOY_DIR}/ocarina_of_pi" 2>/dev/null | grep -qi "sdl"; then
            log_info "Binary links against SDL2 (includes controller/input subsystem)"
        fi
    fi

    # 2e: Check raw input devices
    log_info "Checking /dev/input for gamepad devices..."
    if [ -d "/dev/input" ]; then
        local js_devices
        js_devices=$(ls /dev/input/js* 2>/dev/null || true)
        local event_devices
        event_devices=$(ls /dev/input/event* 2>/dev/null | head -10 || true)

        if [ -n "$js_devices" ]; then
            log_info "Joystick devices found:"
            for js in $js_devices; do
                log_info "  ${js}"
            done
            xinput_ok=1
        else
            log_warn "No /dev/input/js* devices found"
        fi

        if [ -n "$event_devices" ]; then
            log_info "Event devices available ($(echo "$event_devices" | wc -l))"
        fi
    else
        log_warn "/dev/input directory not found (input subsystem unavailable)"
    fi

    # 2f: Configure SDL2 environment for XInput
    if [ "$ENABLE_XINPUT" -eq 1 ]; then
        log_info "Configuring XInput environment variables..."

        # Set SDL joystick driver
        if [ -n "$SDL_JOYSTICK_DRIVER" ]; then
            export SDL_JOYSTICK_DRIVER="$SDL_JOYSTICK_DRIVER"
            log_info "SDL_JOYSTICK_DRIVER=${SDL_JOYSTICK_DRIVER}"
        else
            # Default to SDL_CONTROLLER_DRIVER on Linux (evdev-based)
            export SDL_JOYSTICK_DRIVER="linux"
            log_info "SDL_JOYSTICK_DRIVER=linux (default)"
        fi

        # Enable controller mode in SDL2
        export SDL_CONTROLLER_CONFIG=""
        log_info "SDL controller mode enabled"

        # If on the Pi target, ensure the xpad driver is loaded
        if [ "$(uname -m)" = "armv7l" ] || [ "$(uname -m)" = "aarch64" ]; then
            if ! lsmod 2>/dev/null | grep -q "xpad"; then
                log_warn "Loading xpad kernel driver on Pi..."
                if [ -f /lib/modules/$(uname -r)/kernel/drivers/input/joystick/xpad.ko ]; then
                    insmod /lib/modules/$(uname -r)/kernel/drivers/input/joystick/xpad.ko 2>/dev/null || \
                        log_warn "Could not insmod xpad - check kernel module availability"
                else
                    log_warn "xpad module not found for this kernel - install linux-firmware"
                fi
            fi
        fi
    fi

    # Summary
    if [ "$xinput_ok" -eq 1 ]; then
        log_info "XInput compatibility: ENABLED"
    else
        log_warn "XInput compatibility: NO CONTROLLER DETECTED"
        log_warn "Install xboxdrv or connect an Xbox controller with xpad driver"
        log_warn "To enable software XInput bridge, install xboxdrv and restart with ENABLE_XINPUT=1"
    fi

    return 0
}

#===============================================================
# Task 3: Prepare deployment package
#===============================================================
prepare_deployment() {
    log_step "Preparing deployment package"

    mkdir -p "${DEPLOY_DIR}"

    # Copy binary
    if [ -f "${BUILD_DIR}/ocarina_of_pi" ]; then
        cp "${BUILD_DIR}/ocarina_of_pi" "${DEPLOY_DIR}/"
        log_info "Binary copied to deploy"
    else
        log_error "Binary not found: ${BUILD_DIR}/ocarina_of_pi"
        return 1
    fi

    # Copy config
    if [ -f "${PROJECT_DIR}/config/config.txt" ]; then
        cp "${PROJECT_DIR}/config/config.txt" "${DEPLOY_DIR}/"
        log_info "config.txt copied to deploy"
    fi

    # Copy ROM if configured
    if [ "$INCLUDE_ROM_IN_DEPLOY" -eq 1 ] && [ -n "$FOUND_ROM_PATH" ]; then
        log_info "Including ROM in deployment..."

        if [ "$FOUND_ROM_PATH" = *.zip ] || [ "$FOUND_ROM_PATH" = *.7z ]; then
            # For compressed ROMs, include as-is and note decompression method
            cp "$FOUND_ROM_PATH" "${DEPLOY_DIR}/rom/"
            log_info "Compressed ROM included - will be decompressed at runtime"
        else
            mkdir -p "${DEPLOY_DIR}${DEPLOY_ROM_DIR}"
            cp "$FOUND_ROM_PATH" "${DEPLOY_DIR}${DEPLOY_ROM_DIR}/${FOUND_ROM}"
            log_info "ROM copied to ${DEPLOY_DIR}${DEPLOY_ROM_DIR}/"
        fi
    fi

    # Generate launch script
    cat > "${DEPLOY_DIR}/launch.sh" << 'LAUNCH_EOF'
#!/bin/bash
cd "$(dirname "$0")"

# Set SDL2 controller environment
export SDL_JOYSTICK_DRIVER="${SDL_JOYSTICK_DRIVER:-linux}"

# Set audio parameters for low latency
export SDL_AUDIO_DRIVERS=alsa

# Launch the emulator
./ocarina_of_pi "$@"
LAUNCH_EOF
    chmod +x "${DEPLOY_DIR}/launch.sh"
    log_info "Launch script generated"

    return 0
}

#===============================================================
# Task 4: Create XInput compatibility report
#===============================================================
generate_report() {
    local report_file="${DEPLOY_DIR}/xinput_report.txt"

    log_step "Generating compatibility report"

    cat > "$report_file" << EOF
========================================
Ocarina of Pi - Compatibility Report
Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
========================================

ROM: ${FOUND_ROM_PATH:-not found}
ROM Size: ${file_size:-unknown} bytes
ROM Path Configured: ${ROM_FILE:-auto-discover}
ROM Directory: ${ROM_DIRECTORY:-not set}

XInput Status: $([ "$ENABLE_XINPUT" = "1" ] && echo "ENABLED" || echo "DISABLED")
XInput Driver: ${SDL_JOYSTICK_DRIVER:-auto}
xboxdrv Available: $([ -x "$XBOXDRV_PATH" ] && echo "YES" || echo "NO")

Deployment Target: ${DEPLOY_DIR}
ROM Deploy Path: ${DEPLOY_ROM_DIR}
ROM Included in Deploy: $([ "$INCLUDE_ROM_IN_DEPLOY" = "1" ] && echo "YES" || echo "NO")

========================================
EOF

    log_info "Report saved to ${report_file}"
}

#===============================================================
# Main execution
#===============================================================
main() {
    echo ""
    echo "============================================"
    echo "  Ocarina of Pi - Post-Build Automation"
    echo "============================================"
    echo ""

    local exit_code=0

    # Step 1: Parse configuration
    if ! parse_rom_config; then
        log_error "Failed to parse ROM configuration"
        exit_code=1
    fi

    # Step 2: Locate ROM
    if [ "$exit_code" -eq 0 ]; then
        if ! locate_rom; then
            log_error "ROM location failed"
            exit_code=1
        fi
    fi

    # Step 3: Validate ROM
    if [ "$exit_code" -eq 0 ]; then
        if ! validate_rom; then
            log_error "ROM validation failed"
            exit_code=1
        fi
    fi

    # Step 4: XInput compatibility check
    if [ "$exit_code" -eq 0 ]; then
        check_xinput_compatibility || log_warn "XInput check completed with warnings"
    fi

    # Step 5: Prepare deployment
    if [ "$exit_code" -eq 0 ]; then
        if ! prepare_deployment; then
            log_error "Deployment preparation failed"
            exit_code=1
        fi
    fi

    # Step 6: Generate report
    if [ "$exit_code" -eq 0 ]; then
        generate_report
    fi

    echo ""
    if [ "$exit_code" -eq 0 ]; then
        echo -e "${GREEN}============================================${NC}"
        echo -e "${GREEN}  Post-Build Automation: SUCCESS${NC}"
        echo -e "${GREEN}============================================${NC}"
        echo ""
        echo "  Deployment: ${DEPLOY_DIR}"
        echo "  Run with:   ${DEPLOY_DIR}/launch.sh"
        echo ""
    else
        echo -e "${RED}============================================${NC}"
        echo -e "${RED}  Post-Build Automation: FAILED (code ${exit_code})${NC}"
        echo -e "${RED}============================================${NC}"
    fi

    return $exit_code
}

main "$@"
