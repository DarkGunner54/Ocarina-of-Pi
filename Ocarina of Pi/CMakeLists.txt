cmake_minimum_required(VERSION 3.16)
project(OcarinaOfPi CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- SDL2 Path Configuration ---
# Can point to SDL2-RPI build, or /usr for system packages
set(SDL2_PATH "/usr" CACHE PATH "System SDL2 prefix; use the distribution package on Raspberry Pi OS")

# --- Compiler Flags (appended to toolchain defaults) ---
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ftree-vectorize -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -fomit-frame-pointer -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-O3 -Wl,--as-needed")

# --- Library Search Paths ---
# Combine SDL2 path, system ARM paths, and VideoCore paths
set(PI_LIB_PATHS
    ${SDL2_PATH}/lib
    /usr/lib/arm-linux-gnueabihf
    /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
    /usr/lib
    /usr/arm-linux-gnueabihf/lib
    /opt/vc/lib
)
set(PI_INCLUDE_PATHS
    ${SDL2_PATH}/include
    /usr/include/SDL2
    /usr/include/arm-linux-gnueabihf
    /usr/include
    /opt/vc/include
    /usr/include/freetype2
    /usr/include/libdrm
    /usr/include/drm
)

include_directories(${PI_INCLUDE_PATHS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Find Libraries ---
# Use PATHS (not HINTS) for reliable cross-compilation discovery
find_library(GLES2_LIB NAMES GLESv2 GLES2 PATHS ${PI_LIB_PATHS})
find_library(EGL_LIB NAMES EGL PATHS ${PI_LIB_PATHS})
find_library(BCM_HOST_LIB NAMES bcm_host PATHS ${PI_LIB_PATHS})
find_library(SDL2_LIB NAMES SDL2 PATHS ${PI_LIB_PATHS})
find_library(SDL2GFX_LIB NAMES SDL2_gfx PATHS ${PI_LIB_PATHS})
find_library(PTHREAD_LIB NAMES pthread PATHS ${PI_LIB_PATHS})
find_library(DL_LIB NAMES dl PATHS ${PI_LIB_PATHS})
find_library(RT_LIB NAMES rt PATHS ${PI_LIB_PATHS})
find_library(ASOUND_LIB NAMES asound PATHS ${PI_LIB_PATHS})
find_library(M_LIB NAMES m)
find_library(DRM_LIB NAMES drm PATHS ${PI_LIB_PATHS})

# Raspberry Pi OS Bookworm supplies these through pkg-config, not /opt/vc.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEM_SDL2 REQUIRED sdl2)
pkg_check_modules(SYSTEM_GLES2 REQUIRED glesv2)
pkg_check_modules(SYSTEM_EGL REQUIRED egl)
pkg_check_modules(SYSTEM_ALSA REQUIRED alsa)
pkg_check_modules(SYSTEM_DRM REQUIRED libdrm)
set(SDL2_LIB ${SYSTEM_SDL2_LIBRARIES})
set(GLES2_LIB ${SYSTEM_GLES2_LIBRARIES})
set(EGL_LIB ${SYSTEM_EGL_LIBRARIES})
set(ASOUND_LIB ${SYSTEM_ALSA_LIBRARIES})
set(DRM_LIB ${SYSTEM_DRM_LIBRARIES})

# --- Report Found Libraries ---
message(STATUS "")
message(STATUS "=== Library Discovery ===")
message(STATUS "  SDL2: resolved through pkg-config")
message(STATUS "  GLES2/EGL: resolved through pkg-config")
message(STATUS "  ALSA/DRM: resolved through pkg-config")
message(STATUS "")

# --- Required Libraries Check ---
if("${GLES2_LIB}" STREQUAL "" OR "${SDL2_LIB}" STREQUAL "")
    message(FATAL_ERROR "Required Raspberry Pi OS development libraries are missing. Install libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev libasound2-dev libdrm-dev.")
endif()

# --- Source Files ---
set(SOURCES
    src/main.cpp
    src/video_rpi.cpp
    src/audio_alsa.cpp
    src/threading.cpp
    src/mem_manager.cpp
    src/game_input.cpp
)

set(HEADERS
    include/video_rpi.h
    include/audio_alsa.h
    include/threading.h
    include/mem_manager.h
    include/game_input.h
    include/main.h
)

add_executable(ocarina_of_pi ${SOURCES} ${HEADERS})

# --- Link Libraries ---
target_link_libraries(ocarina_of_pi
    GLESv2 EGL SDL2 asound drm
    pthread dl m
)

# Native compatibility boundary for the zeldaret-based ARM port.
add_library(oot_pi_platform STATIC native_port/src/oot_pi_target.c)
target_include_directories(oot_pi_platform PUBLIC native_port/include)
target_compile_features(oot_pi_platform PUBLIC c_std_11)
target_compile_options(oot_pi_platform PRIVATE -Wall -Wextra -Wpedantic)

# --- Install Rules ---
install(TARGETS ocarina_of_pi RUNTIME DESTINATION bin)
install(FILES config/config.txt DESTINATION .)