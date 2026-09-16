cmake_minimum_required(VERSION 3.16)
project(OcarinaOfPi CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- SDL2 Path Configuration ---
# Can point to SDL2-RPI build, or /usr for system packages
set(SDL2_PATH "${CMAKE_SOURCE_DIR}/../SDL2-RPI" CACHE PATH "Path to SDL2 RPI installation (or /usr for system packages)")

# --- Compiler Flags (appended to toolchain defaults) ---
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ftree-vectorize -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -fomit-frame-pointer -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-O3 -Wl,--as-needed")

# --- Library Search Paths ---
# Combine SDL2 path, system ARM paths, and VideoCore paths
set(PI_LIB_PATHS
    ${SDL2_PATH}/lib
    /usr/lib/arm-linux-gnueabihf
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

include_directories(${PI_INCLUDE_PATHS})

# --- Find Libraries ---
# Use PATHS (not HINTS) for reliable cross-compilation discovery
find_library(GLES2_LIB NAMES GLESv2 GLES2 PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(EGL_LIB NAMES EGL PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(BCM_HOST_LIB NAMES bcm_host PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(SDL2_LIB NAMES SDL2 PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(SDL2GFX_LIB NAMES SDL2_gfx PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(PTHREAD_LIB NAMES pthread PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(DL_LIB NAMES dl PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(RT_LIB NAMES rt PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(ASOUND_LIB NAMES asound PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)
find_library(M_LIB NAMES m PATHS ${PI_LIB_PATHS} NO_DEFAULT_PATH)

# --- Report Found Libraries ---
message(STATUS "")
message(STATUS "=== Library Discovery ===")
foreach(lib GLES2_LIB EGL_LIB BCM_HOST_LIB SDL2_LIB SDL2GFX_LIB PTHREAD_LIB DL_LIB RT_LIB ASOUND_LIB M_LIB)
    if(${${lib}})
        message(STATUS "  ${lib}: ${${${lib}}}")
    else()
        message(STATUS "  ${lib}: NOT FOUND")
    endif()
endforeach()
message(STATUS "")

# --- Required Libraries Check ---
set(REQUIRED_LIBS GLES2_LIB SDL2_LIB)
set(MISSING_LIBS "")
foreach(lib ${REQUIRED_LIBS})
    if(NOT ${${lib}})
        list(APPEND MISSING_LIBS "${lib}")
    endif()
endforeach()

if(MISSING_LIBS)
    string(REPLACE ";" ", " MISSING_STR "${MISSING_LIBS}")
    message(FATAL_ERROR
        "\n"
        "Required libraries not found: ${MISSING_STR}\n"
        "\n"
        "Troubleshooting:\n"
        "  1. On Raspberry Pi (native build): Install SDL2 dev packages:\n"
        "       sudo apt install -y libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev\n"
        "       sudo apt install -y libbcm-host-dev libdrm-dev libasound2-dev\n"
        "  2. Cross-compilation: Ensure toolchain file is used:\n"
        "       cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-armpi.cmake ..\n"
        "  3. If SDL2-RPI is unavailable, use system packages on the Pi\n"
        "     or build SDL2 from source (see docs/SDL2_SETUP.md)\n"
        "  4. Verify SDL2_PATH points to correct location:\n"
        "       cmake .. -DSDL2_PATH=/usr\n"
    )
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
    ${GLES2_LIB}
    ${EGL_LIB}
    ${BCM_HOST_LIB}
    ${SDL2_LIB}
    ${SDL2GFX_LIB}
    ${PTHREAD_LIB}
    ${DL_LIB}
    ${RT_LIB}
    ${ASOUND_LIB}
    ${M_LIB}
    vcos
    vchiq_arm
)

# --- Install Rules ---
install(TARGETS ocarina_of_pi RUNTIME DESTINATION bin)
install(FILES config/config.txt DESTINATION .)